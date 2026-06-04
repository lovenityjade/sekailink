const { app, BrowserWindow, ipcMain, shell, screen, dialog, clipboard } = require("electron");
const { spawn, spawnSync } = require("child_process");
const readline = require("readline");
const fs = require("fs");
const os = require("os");
const crypto = require("crypto");
const https = require("https");
const http = require("http");
const net = require("net");
const path = require("path");
const { createLogger } = require("./lib/logger.cjs");
const { createPatchedRomCacheStore } = require("./lib/patched-rom-cache.cjs");
const {
  isSafeExternalUrl,
  isPlainObject,
  normalizeIpcString,
  normalizeIpcPath,
  normalizeGameId,
  createIpcSecurity,
} = require("./lib/ipc-security.cjs");

const isDev = !app.isPackaged;
const devServerUrl = process.env.VITE_DEV_SERVER_URL || "http://localhost:5173";
const shouldOpenDevTools = process.env.SEKAILINK_OPEN_DEVTOOLS === "1";

const firstExistingDir = (...candidates) => {
  for (const candidate of candidates) {
    const safe = String(candidate || "").trim();
    if (!safe) continue;
    try {
      if (fs.existsSync(safe) && fs.statSync(safe).isDirectory()) {
        return safe;
      }
    } catch (_err) {
      // keep scanning
    }
  }
  return "";
};

const getDevRuntimeCandidates = () => {
  const repoRuntime = path.join(__dirname, "..", "..", "..", "runtime");
  const localCoreRuntime = path.join(__dirname, "..", "..", "runtime");
  const siblingSekaiemuRuntime = path.join(__dirname, "..", "..", "..", "sekaiemu", "host", "runtime");
  const envRuntime = String(process.env.SEKAILINK_RUNTIME_DIR || "").trim();
  return [envRuntime, repoRuntime, localCoreRuntime, siblingSekaiemuRuntime].filter(Boolean);
};

const getBundledRuntimeDir = () => {
  // In packaged apps, runtime is shipped via electron-builder extraResources to process.resourcesPath/runtime.
  // In dev, prefer an explicit runtime dir, then the local repo runtime, then the sibling sekaiemu host/runtime.
  if (app.isPackaged) return path.join(process.resourcesPath, "runtime");
  const resolved = firstExistingDir(...getDevRuntimeCandidates());
  if (resolved) return resolved;
  return path.join(__dirname, "..", "..", "runtime");
};

const getRuntimePlatformId = () => {
  const arch = process.arch === "x64" ? "x64" : process.arch;
  return `${process.platform}-${arch}`;
};

const getRuntimePlatformDir = () => {
  return path.join(getBundledRuntimeDir(), "platforms", getRuntimePlatformId());
};

const getRuntimePlatformPath = (...parts) => {
  return path.join(getRuntimePlatformDir(), ...parts);
};

const getPlatformRuntimeDirCandidates = (...parts) => {
  const runtimeDir = getBundledRuntimeDir();
  return [
    getRuntimePlatformPath(...parts),
    path.join(runtimeDir, ...parts),
  ];
};

const getBundledThirdPartyDir = () => {
  // In packaged apps, third_party is shipped via extraResources to process.resourcesPath/third_party.
  // In dev, third_party lives at repo root (../../../third_party relative to client/app/electron).
  if (app.isPackaged) return path.join(process.resourcesPath, "third_party");
  return path.join(__dirname, "..", "..", "..", "third_party");
};

const isValidApRuntimeRoot = (dir) =>
  Boolean(
    dir &&
      fs.existsSync(path.join(dir, "Patch.py")) &&
      fs.existsSync(path.join(dir, "BaseClasses.py")) &&
      fs.existsSync(path.join(dir, "worlds", "Files.py"))
  );

const getBundledApRootDir = () => {
  // The curated AP/MWGG python sources are staged under runtime/ap so they ride
  // along with the same extraResources bundle as Sekaiemu/SKLMI/patcher tools.
  const envApRoot = String(process.env.SEKAILINK_AP_ROOT || "").trim();
  if ((!app.isPackaged || process.env.SEKAILINK_ALLOW_EXTERNAL_AP_ROOT === "1") && isValidApRuntimeRoot(envApRoot)) {
    return envApRoot;
  }
  const candidates = app.isPackaged
    ? [
        path.join(process.resourcesPath, "runtime", "ap"),
        path.join(process.resourcesPath, "ap"),
      ]
    : [
        path.join(__dirname, "..", "..", "runtime", "ap"),
        path.join(__dirname, "..", "..", "..", "runtime", "ap"),
        path.join(__dirname, "..", "..", ".."),
      ];
  const resolved = firstExistingDir(...candidates.filter(isValidApRuntimeRoot));
  if (resolved) return resolved;
  return app.isPackaged ? path.join(process.resourcesPath, "runtime", "ap") : path.join(__dirname, "..", "..", "runtime", "ap");
};

const resolveWindowIconPath = () => {
  const candidates = [
    path.join(__dirname, "..", "public", "sekailink-icon.png"),
    path.join(app.getAppPath(), "public", "sekailink-icon.png"),
    path.join(process.resourcesPath, "public", "sekailink-icon.png"),
  ];
  for (const p of candidates) {
    try {
      if (fs.existsSync(p)) return p;
    } catch (_err) {
      // ignore
    }
  }
  return undefined;
};

function getRuntimeOverlayRoot() {
  return path.join(app.getPath("userData"), "runtime", "overlay");
}

function getOverlayApRootDir() {
  return path.join(getRuntimeOverlayRoot(), "ap");
}

function getEffectiveApRootDirs() {
  const dirs = [];
  const overlay = getOverlayApRootDir();
  if (isValidApRuntimeRoot(overlay)) dirs.push(overlay);
  const bundled = getBundledApRootDir();
  if (isValidApRuntimeRoot(bundled)) dirs.push(bundled);
  return dirs;
}

const withApPythonEnv = (baseEnv = {}) => {
  const apRoots = getEffectiveApRootDirs();
  const apRoot = apRoots[0] || getBundledApRootDir();
  const prior = baseEnv.PYTHONPATH || process.env.PYTHONPATH || "";
  const injected = apRoots.filter(Boolean).join(path.delimiter);
  const nextPythonPath = prior
    ? `${injected}${path.delimiter}${prior}`
    : injected;
  return {
    ...baseEnv,
    SEKAILINK_AP_ROOT: apRoot,
    PYTHONPATH: nextPythonPath,
    PYTHONNOUSERSITE: "1",
    PYTHONDONTWRITEBYTECODE: "1",
    // We do not want pip/update flows to run inside the desktop client.
    SKIP_REQUIREMENTS_UPDATE: "1",
  };
};

let logFilePath = "";

let mainWindow;
let pendingAuthUrl = null;
const dashboardWindows = new Set();
let commonClientProc = null;
let commonClientRl = null;
let bizhawkClientProc = null;
let bizhawkClientRl = null;
let bizhawkClientKind = "bizhawk"; // "bizhawk" | "sni"
let sniBridgeProc = null;
const bizhawkProcs = new Map();
const trackerProcs = new Map();
const trackerWebWindows = new Set();
const nativeGameProcs = new Map();
const linkedWorldProcs = new Map();
const sekaiemuChatBridges = new Map();
let coupledRuntimeTeardownPromise = null;
const activeUpdaterDownloads = new Map();
const updaterDownloadResults = new Map();
const updaterDownloadWaiters = new Map();
const pendingTrackerVariantRequests = new Map();

const COMMONCLIENT_EVENT_CHANNEL = "commonclient:event";
const BIZHAWKCLIENT_EVENT_CHANNEL = "bizhawkclient:event";
const SESSION_EVENT_CHANNEL = "session:event";
const UPDATER_EVENT_CHANNEL = "updater:event";
const LOG_EVENT_CHANNEL = "log:event";

let _gamescopePathCache = undefined;
let _wmctrlPathCache = undefined;
let crashReportingOptIn = false;

const getWindowStatePath = () => path.join(app.getPath("userData"), "window-state.json");

const isBoundsVisible = (bounds) => {
  if (!bounds || !Number.isFinite(bounds.x) || !Number.isFinite(bounds.y)) return false;
  const displays = screen.getAllDisplays();
  const center = {
    x: bounds.x + Math.max(1, Number(bounds.width || 1)) / 2,
    y: bounds.y + Math.max(1, Number(bounds.height || 1)) / 2,
  };
  return displays.some((display) => {
    const area = display.workArea;
    return center.x >= area.x && center.x <= area.x + area.width && center.y >= area.y && center.y <= area.y + area.height;
  });
};

const readMainWindowBounds = (fallbackWidth, fallbackHeight) => {
  const fallbackDisplay = screen.getDisplayNearestPoint(screen.getCursorScreenPoint());
  const fallbackArea = fallbackDisplay?.workArea || screen.getPrimaryDisplay().workArea;
  const fallback = {
    width: fallbackWidth,
    height: fallbackHeight,
    x: Math.round(fallbackArea.x + (fallbackArea.width - fallbackWidth) / 2),
    y: Math.round(fallbackArea.y + (fallbackArea.height - fallbackHeight) / 2),
  };
  const parsed = readJsonFileSafe(getWindowStatePath());
  const saved = parsed?.mainWindow;
  if (!saved || !isBoundsVisible(saved)) return fallback;
  return {
    width: Math.max(fallbackWidth, Math.round(Number(saved.width) || fallbackWidth)),
    height: Math.max(fallbackHeight, Math.round(Number(saved.height) || fallbackHeight)),
    x: Math.round(Number(saved.x)),
    y: Math.round(Number(saved.y)),
  };
};

const saveMainWindowBounds = () => {
  try {
    if (!mainWindow || mainWindow.isDestroyed()) return;
    if (mainWindow.isMinimized() || mainWindow.isFullScreen()) return;
    fs.mkdirSync(path.dirname(getWindowStatePath()), { recursive: true });
    fs.writeFileSync(getWindowStatePath(), JSON.stringify({ mainWindow: mainWindow.getBounds() }, null, 2));
  } catch (_err) {
    // best-effort window placement persistence
  }
};

const nowIso = () => new Date().toISOString();

const logger = createLogger({ app, fs, path });

const writeLogLine = (level, scope, message) => {
  logger.writeLine(level, scope, message);
};

const writeLogJson = (scope, obj) => {
  logger.writeJson(scope, obj);
};

const { secureIpcHandle, openExternalSafely } = createIpcSecurity({
  ipcMain,
  shell,
  isDev,
  devServerUrl,
  writeLogLine,
});

const base64UrlDecodeUtf8 = (value) => {
  const raw = String(value || "").replace(/-/g, "+").replace(/_/g, "/");
  const padded = raw + "===".slice((raw.length + 3) % 4);
  return Buffer.from(padded, "base64").toString("utf8");
};

const getBootstrapLaunchTokenStatus = () => {
  const tokenRaw = String(process.env.SEKAILINK_BOOTSTRAP_TOKEN || "").trim();
  if (!tokenRaw || !tokenRaw.includes(".")) return { ok: false, present: false, error: "bootstrap_token_missing" };
  const [body, sig] = tokenRaw.split(".", 2);
  if (!body || !sig || !/^[a-f0-9]{64}$/i.test(sig)) return { ok: false, present: true, error: "bootstrap_token_invalid_format" };

  const installDirHint = String(process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
  const stateDirHint = String(process.env.SEKAILINK_BOOTSTRAP_STATE_DIR || "").trim();
  const exeDir = path.dirname(process.execPath);
  const secretCandidates = [
    stateDirHint ? path.join(stateDirHint, "launcher-secret.key") : "",
    installDirHint ? path.join(installDirHint, ".sekailink", "launcher-secret.key") : "",
    path.join(exeDir, ".sekailink", "launcher-secret.key"),
  ].filter(Boolean);

  let secret = "";
  for (const candidate of secretCandidates) {
    try {
      if (fs.existsSync(candidate)) {
        secret = String(fs.readFileSync(candidate, "utf8") || "").trim();
      }
    } catch (_err) {
      // ignore
    }
    if (secret.length >= 32) break;
  }
  if (secret.length < 32) return { ok: false, present: true, error: "bootstrap_secret_missing" };

  const expectedSig = crypto.createHmac("sha256", secret).update(body).digest("hex");
  if (expectedSig.toLowerCase() !== sig.toLowerCase()) return { ok: false, present: true, error: "bootstrap_token_bad_signature" };

  let payload = null;
  try {
    payload = JSON.parse(base64UrlDecodeUtf8(body));
  } catch (_err) {
    payload = null;
  }
  if (!payload || typeof payload !== "object") return { ok: false, present: true, error: "bootstrap_token_bad_payload" };
  if (String(payload.iss || "") !== "sekailink-bootstrapper") return { ok: false, present: true, error: "bootstrap_token_bad_issuer" };
  if (String(payload.aud || "") !== "sekailink-client") return { ok: false, present: true, error: "bootstrap_token_bad_audience" };

  const now = Date.now();
  const iat = Number(payload.iat || 0);
  const exp = Number(payload.exp || 0);
  if (!Number.isFinite(iat) || !Number.isFinite(exp) || exp <= now || iat > now + 10000) {
    return { ok: false, present: true, error: "bootstrap_token_expired" };
  }
  if (exp - iat > 10 * 60 * 1000) return { ok: false, present: true, error: "bootstrap_token_ttl_rejected" };

  return { ok: true, present: true, error: "" };
};

const validateBootstrapLaunchToken = () => {
  const requireBootstrap = app.isPackaged && String(process.env.SEKAILINK_REQUIRE_BOOTSTRAP || "0") !== "0";
  const token = getBootstrapLaunchTokenStatus();
  if (!requireBootstrap) {
    return {
      ok: true,
      enforced: false,
      token_present: Boolean(token.present),
      token_valid: Boolean(token.ok),
      error: token.ok ? "" : String(token.error || ""),
    };
  }
  if (!token.ok) {
    return {
      ok: false,
      enforced: true,
      token_present: Boolean(token.present),
      token_valid: false,
      error: String(token.error || "bootstrap_token_invalid"),
    };
  }
  return { ok: true, enforced: true, token_present: true, token_valid: true, error: "" };
};

const readJsonFileSafe = (filePath) => {
  try {
    if (!filePath || !fs.existsSync(filePath)) return null;
    const raw = fs.readFileSync(filePath, "utf8");
    const parsed = JSON.parse(raw);
    if (parsed && typeof parsed === "object") return parsed;
  } catch (_err) {
    // ignore
  }
  return null;
};

const getBootstrapInstallState = () => {
  const explicitStateDir = String(process.env.SEKAILINK_BOOTSTRAP_STATE_DIR || "").trim();
  const explicitInstallDir = String(process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
  const exeDir = path.dirname(process.execPath);
  const candidateStateFiles = [
    explicitStateDir ? path.join(explicitStateDir, "install-state.json") : "",
    explicitInstallDir ? path.join(explicitInstallDir, ".sekailink", "install-state.json") : "",
    path.join(exeDir, ".sekailink", "install-state.json"),
  ].filter(Boolean);
  for (const candidate of candidateStateFiles) {
    const parsed = readJsonFileSafe(candidate);
    if (parsed) return parsed;
  }
  return null;
};

const resolveBootstrapperExecutable = () => {
  const envExplicit = String(process.env.SEKAILINK_BOOTSTRAPPER_PATH || "").trim();
  if (envExplicit && fs.existsSync(envExplicit)) return envExplicit;

  const installState = getBootstrapInstallState();
  const stateInstallDir = String(installState?.installDir || process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
  const exeDir = path.dirname(process.execPath);
  const roots = [stateInstallDir, exeDir].filter(Boolean);
  const candidates = [];

  for (const root of roots) {
    if (process.platform === "win32") {
      candidates.push(path.join(root, "SekaiLink-bootstrapper.cmd"));
      candidates.push(path.join(root, "SekaiLink-bootstrapper.ps1"));
      candidates.push(path.join(root, "SekaiLink-bootstrapper.exe"));
      candidates.push(path.join(root, "SekaiLink Bootstrapper.exe"));
    } else {
      candidates.push(path.join(root, "sekailink-bootstrapper.sh"));
      candidates.push(path.join(root, "SekaiLink-bootstrapper.AppImage"));
      candidates.push(path.join(root, "SekaiLink Bootstrapper.AppImage"));
    }
  }

  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }

  // Fallback: scan install directory for versioned bootstrapper names.
  for (const root of roots) {
    try {
      const entries = fs.readdirSync(root, { withFileTypes: true });
      for (const entry of entries) {
        if (!entry.isFile()) continue;
        const name = String(entry.name || "");
        const lower = name.toLowerCase();
        if (!lower.includes("sekailink-bootstrapper")) continue;
        if (!(lower.endsWith(".cmd") || lower.endsWith(".ps1") || lower.endsWith(".sh") || lower.endsWith(".exe") || lower.endsWith(".appimage"))) continue;
        const full = path.join(root, name);
        if (fs.existsSync(full)) return full;
      }
    } catch (_err) {
      // ignore
    }
  }
  return "";
};

const getBootstrapperDownloadUrl = () => {
  if (process.platform === "win32") {
    return "https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-windows.zip";
  }
  return "https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-linux.tar.gz";
};

const spawnDetachedBootstrapper = (bootstrapperPath) => {
  const p = String(bootstrapperPath || "").trim();
  const ext = path.extname(p).toLowerCase();
  let command = p;
  let args = [];

  if (process.platform === "win32" && (ext === ".cmd" || ext === ".bat")) {
    command = "cmd.exe";
    args = ["/d", "/s", "/c", `"${p.replace(/"/g, '""')}"`];
  } else if (process.platform === "win32" && ext === ".ps1") {
    command = "powershell.exe";
    args = ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", p];
  } else if (process.platform !== "win32" && ext === ".sh") {
    try {
      fs.chmodSync(p, fs.statSync(p).mode | 0o111);
    } catch (_err) {
      // best effort
    }
    command = "/bin/bash";
    args = [p];
  }

  return spawn(command, args, {
    detached: true,
    stdio: "ignore",
    windowsHide: true,
    cwd: path.dirname(p),
    env: { ...process.env, SEKAILINK_UPDATE_TRIGGER: "client" },
  });
};

const launchBootstrapperAndQuit = async () => {
  const bootstrapperPath = resolveBootstrapperExecutable();
  if (!bootstrapperPath) return { ok: false, error: "bootstrapper_not_found" };
  try {
    const child = spawnDetachedBootstrapper(bootstrapperPath);
    child.unref();
    setTimeout(() => app.quit(), 25);
    return { ok: true, path: bootstrapperPath };
  } catch (err) {
    return { ok: false, error: String(err || "bootstrapper_launch_failed") };
  }
};

const waitForTcpPort = (host, port, timeoutMs = 8000) => {
  const h = String(host || "127.0.0.1");
  const p = Number(port || 0);
  const deadline = Date.now() + Math.max(0, Number(timeoutMs || 0));
  return new Promise((resolve) => {
    const tick = () => {
      const sock = new net.Socket();
      let done = false;
      const finish = (ok) => {
        if (done) return;
        done = true;
        try { sock.destroy(); } catch (_e) {}
        if (ok) return resolve(true);
        if (Date.now() >= deadline) return resolve(false);
        setTimeout(tick, 120);
      };
      sock.setTimeout(500);
      sock.once("connect", () => finish(true));
      sock.once("timeout", () => finish(false));
      sock.once("error", () => finish(false));
      try {
        sock.connect(p, h);
      } catch (_err) {
        finish(false);
      }
    };
    tick();
  });
};

const waitForAnyTcpPort = async (host, ports, timeoutMs = 8000) => {
  const list = Array.isArray(ports) ? ports.map((v) => Number(v)).filter((v) => Number.isFinite(v) && v > 0) : [];
  const deadline = Date.now() + Math.max(0, Number(timeoutMs || 0));
  while (Date.now() < deadline) {
    for (const p of list) {
      const ok = await waitForTcpPort(host, p, 600);
      if (ok) return p;
    }
    await new Promise((r) => setTimeout(r, 120));
  }
  return 0;
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, Math.max(0, Number(ms || 0))));

const isPidAlive = (pid) => {
  const p = Number(pid || 0);
  if (!Number.isFinite(p) || p <= 0) return false;
  try {
    process.kill(p, 0);
    return true;
  } catch (_err) {
    return false;
  }
};

const waitForChildExit = async (proc, timeoutMs = 1200) => {
  if (!proc) return true;
  if (proc.exitCode !== null) return true;
  return await new Promise((resolve) => {
    let done = false;
    const finish = (value) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      resolve(Boolean(value));
    };
    const timer = setTimeout(() => finish(false), Math.max(100, Number(timeoutMs || 0)));
    proc.once("exit", () => finish(true));
  });
};

const terminateChildProcess = async (proc, scope = "proc", opts = {}) => {
  if (!proc) return;
  const pid = Number(proc.pid || 0);
  const graceMs = Math.max(200, Number(opts.graceMs || 1200));
  try {
    proc.kill("SIGTERM");
  } catch (_err) {
    try {
      proc.kill();
    } catch (_err2) {
      // ignore
    }
  }
  const exited = await waitForChildExit(proc, graceMs);
  if (exited) return;
  if (pid > 0 && isPidAlive(pid)) {
    writeLogLine("warn", scope, `forcing SIGKILL pid=${pid}`);
    try {
      process.kill(pid, "SIGKILL");
    } catch (_err) {
      // ignore
    }
    await sleep(120);
  }
};

const getListeningPidsOnTcpPort = (port) => {
  const p = Number(port || 0);
  if (!Number.isFinite(p) || p <= 0) return [];
  try {
    if (process.platform === "win32") {
      const out = spawnSync("netstat", ["-ano", "-p", "tcp"], { encoding: "utf-8", windowsHide: true });
      const text = `${out.stdout || ""}\n${out.stderr || ""}`;
      const lines = text.split(/\r?\n/).filter(Boolean);
      const pids = new Set();
      for (const line of lines) {
        const row = line.trim();
        if (!/\bLISTENING\b/i.test(row)) continue;
        const m = row.match(new RegExp(`[:\\.]${p}\\s+`));
        if (!m) continue;
        const parts = row.split(/\s+/);
        const pid = Number(parts[parts.length - 1] || 0);
        if (pid > 0) pids.add(pid);
      }
      return Array.from(pids);
    }

    const out = spawnSync("ss", ["-ltnp", `sport = :${p}`], { encoding: "utf-8" });
    const text = `${out.stdout || ""}\n${out.stderr || ""}`;
    const pids = new Set();
    const re = /pid=(\d+)/g;
    let m;
    while ((m = re.exec(text)) !== null) {
      const pid = Number(m[1] || 0);
      if (pid > 0) pids.add(pid);
    }
    return Array.from(pids);
  } catch (_err) {
    return [];
  }
};

const getPidCommandLine = (pid) => {
  const p = Number(pid || 0);
  if (!Number.isFinite(p) || p <= 0) return "";
  try {
    if (process.platform === "win32") {
      const cmd = `(Get-CimInstance Win32_Process -Filter \"ProcessId=${p}\").CommandLine`;
      const out = spawnSync("powershell.exe", ["-NoProfile", "-Command", cmd], { encoding: "utf-8", windowsHide: true });
      return String(out.stdout || "").trim();
    }
    const out = spawnSync("ps", ["-p", String(p), "-o", "args="], { encoding: "utf-8" });
    return String(out.stdout || "").trim();
  } catch (_err) {
    return "";
  }
};

const isSekaiLinkBridgeCommand = (cmdline) => {
  const s = String(cmdline || "").toLowerCase();
  return s.includes("sni_bridge.py")
    || s.includes("sniclient_wrapper.py")
    || s.includes("bizhawkclient_wrapper.py")
    || s.includes("sekailink");
};

const killPidGracefully = async (pid, scope = "proc") => {
  const p = Number(pid || 0);
  if (!Number.isFinite(p) || p <= 0) return false;
  if (!isPidAlive(p)) return true;
  writeLogLine("warn", scope, `terminating stale pid=${p}`);
  try {
    process.kill(p, "SIGTERM");
  } catch (_err) {
    try {
      process.kill(p);
    } catch (_err2) {
      // ignore
    }
  }
  for (let i = 0; i < 12; i += 1) {
    if (!isPidAlive(p)) return true;
    await sleep(100);
  }
  try {
    process.kill(p, "SIGKILL");
  } catch (_err) {
    // ignore
  }
  for (let i = 0; i < 8; i += 1) {
    if (!isPidAlive(p)) return true;
    await sleep(80);
  }
  return !isPidAlive(p);
};

const purgeStaleSniBridgePortHolders = async (port, currentPid = 0) => {
  const keepPid = Number(currentPid || 0);
  const pids = getListeningPidsOnTcpPort(port);
  if (!pids.length) return { ok: true, killed: [], blocked: [] };
  const killed = [];
  const blocked = [];
  for (const pid of pids) {
    if (pid <= 0 || pid === process.pid || (keepPid > 0 && pid === keepPid)) continue;
    const cmdline = getPidCommandLine(pid);
    if (!isSekaiLinkBridgeCommand(cmdline)) {
      blocked.push({ pid, cmdline });
      continue;
    }
    const ok = await killPidGracefully(pid, "sni-bridge");
    if (ok) killed.push(pid);
    else blocked.push({ pid, cmdline });
  }
  if (blocked.length) {
    writeLogLine("warn", "sni-bridge", `port cleanup blocked on ${port}: ${blocked.map((b) => `${b.pid}`).join(",")}`);
  }
  if (killed.length) {
    writeLogLine("info", "sni-bridge", `port cleanup killed stale holders on ${port}: ${killed.join(",")}`);
  }
  return { ok: blocked.length === 0, killed, blocked };
};

const probeSniBridge = (pythonBin, host, port, timeoutMs = 2500) => {
  const py = String(pythonBin || "").trim();
  if (!py) return { ok: false, error: "python_missing_for_probe" };
  const script = [
    "import asyncio, json, sys",
    "import websockets",
    "async def run():",
    "  host = sys.argv[1]",
    "  port = int(sys.argv[2])",
    "  timeout = float(sys.argv[3])",
    "  uri = f'ws://{host}:{port}'",
    "  async with websockets.connect(uri, ping_interval=None, ping_timeout=None, close_timeout=1) as ws:",
    "    await ws.send(json.dumps({'Opcode':'DeviceList','Space':'SNES','Operands':[]}))",
    "    msg = await asyncio.wait_for(ws.recv(), timeout=timeout)",
    "    if isinstance(msg, (bytes, bytearray)):",
    "      raise RuntimeError('device_list_binary_reply')",
    "    data = json.loads(msg)",
    "    arr = data.get('Results') if isinstance(data, dict) else None",
    "    if not isinstance(arr, list) or not arr or arr[0] != 'SekaiLink BizHawk':",
    "      raise RuntimeError('device_list_invalid')",
    "  print('OK')",
    "asyncio.run(run())",
  ].join("\n");
  try {
    const sec = Math.max(0.4, Number(timeoutMs || 0) / 1000).toFixed(2);
    const out = spawnSync(py, ["-c", script, String(host || "127.0.0.1"), String(port || 23074), sec], {
      encoding: "utf-8",
      windowsHide: true,
      timeout: Math.max(1000, Number(timeoutMs || 0) + 1500),
      env: withApPythonEnv(process.env),
    });
    const stdout = String(out.stdout || "").trim();
    const stderr = String(out.stderr || "").trim();
    if (out.status === 0 && stdout.includes("OK")) return { ok: true };
    return { ok: false, error: "sni_bridge_probe_failed", detail: stderr || stdout || `status=${out.status}` };
  } catch (err) {
    return { ok: false, error: "sni_bridge_probe_exception", detail: String(err || "") };
  }
};

const startFileLogging = () => {
  logger.start();
  logFilePath = logger.getLogFilePath();
};

const getLatestLogFilePath = () => {
  try {
    if (logFilePath && fs.existsSync(logFilePath)) return logFilePath;
    const logsDir = path.join(app.getPath("userData"), "logs");
    if (!fs.existsSync(logsDir)) return "";
    const entries = fs
      .readdirSync(logsDir)
      .filter((name) => name.startsWith("sekailink-") && name.endsWith(".log"))
      .map((name) => {
        const full = path.join(logsDir, name);
        let mtimeMs = 0;
        try {
          mtimeMs = fs.statSync(full).mtimeMs;
        } catch (_err) {
          mtimeMs = 0;
        }
        return { full, mtimeMs };
      })
      .sort((a, b) => b.mtimeMs - a.mtimeMs);
    return entries[0]?.full || "";
  } catch (_err) {
    return "";
  }
};

const readFileTailUtf8 = (filePath, maxBytes = 200 * 1024) => {
  try {
    const stat = fs.statSync(filePath);
    const size = Number(stat.size || 0);
    if (!Number.isFinite(size) || size <= 0) return "";
    const tailBytes = Math.max(1024, Math.min(Number(maxBytes || 0), size));
    const fd = fs.openSync(filePath, "r");
    try {
      const buffer = Buffer.alloc(tailBytes);
      const start = Math.max(0, size - tailBytes);
      const bytesRead = fs.readSync(fd, buffer, 0, tailBytes, start);
      return buffer.slice(0, bytesRead).toString("utf8");
    } finally {
      fs.closeSync(fd);
    }
  } catch (_err) {
    return "";
  }
};

const collectDiagnosticsReport = async (options = {}) => {
  const includeLogTail = options?.includeLogTail !== false;
  const maxLogBytesRaw = Number(options?.maxLogBytes || 200 * 1024);
  const maxLogBytes = Number.isFinite(maxLogBytesRaw) ? Math.min(1024 * 1024, Math.max(4096, maxLogBytesRaw)) : 200 * 1024;
  const latest = getLatestLogFilePath();
  const logTail = includeLogTail && latest ? readFileTailUtf8(latest, maxLogBytes) : "";
  return {
    reportId: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
    createdAt: nowIso(),
    app: {
      version: app.getVersion ? String(app.getVersion() || "") : "",
      platform: process.platform,
      arch: process.arch,
      electron: process.versions?.electron,
      chrome: process.versions?.chrome,
      node: process.versions?.node,
    },
    log: {
      path: latest || "",
      tail: logTail || "",
    },
    meta: isPlainObject(options?.meta) ? options.meta : {},
  };
};

const collectBugReportArtifacts = async (options = {}) => {
  const includeScreenshot = options?.includeScreenshot !== false;
  const maxLogBytesRaw = Number(options?.maxLogBytes || 700 * 1024);
  const maxLogBytes = Number.isFinite(maxLogBytesRaw) ? Math.min(1024 * 1024, Math.max(32 * 1024, maxLogBytesRaw)) : 700 * 1024;
  const latest = logFilePath && fs.existsSync(logFilePath) ? logFilePath : getLatestLogFilePath();
  const logTail = latest ? readFileTailUtf8(latest, maxLogBytes) : "";

  let screenshotBase64 = "";
  if (includeScreenshot && mainWindow && !mainWindow.isDestroyed()) {
    try {
      const image = await mainWindow.capturePage();
      const png = image.toPNG();
      screenshotBase64 = png.toString("base64");
    } catch (_err) {
      screenshotBase64 = "";
    }
  }

  let diskTotal = 0;
  let diskFree = 0;
  try {
    const installDir = path.dirname(process.execPath);
    if (typeof fs.statfsSync === "function") {
      const info = fs.statfsSync(installDir);
      const bsize = Number(info?.bsize || 0);
      const blocks = Number(info?.blocks || 0);
      const bavail = Number(info?.bavail || 0);
      if (bsize > 0 && blocks > 0) diskTotal = Math.max(0, Math.floor(bsize * blocks));
      if (bsize > 0 && bavail > 0) diskFree = Math.max(0, Math.floor(bsize * bavail));
    }
  } catch (_err) {
    diskTotal = 0;
    diskFree = 0;
  }

  const cpus = Array.isArray(os.cpus?.()) ? os.cpus() : [];
  const cpuModel = cpus[0] && typeof cpus[0].model === "string" ? cpus[0].model : "";
  const bootstrap = getBootstrapInstallState() || {};
  return {
    screenshotBase64,
    logsText: logTail || "",
    systemInfo: {
      os: os.type(),
      os_release: os.release(),
      platform: process.platform,
      arch: process.arch,
      hostname: os.hostname(),
      cpu_model: cpuModel,
      cpu_count: cpus.length || 0,
      memory_total: Number(os.totalmem?.() || 0),
      memory_free: Number(os.freemem?.() || 0),
      disk_total: diskTotal,
      disk_free: diskFree,
    },
    appInfo: {
      sekailink_version: app.getVersion ? String(app.getVersion() || "") : "",
      bootstrapper_version: String(bootstrap.releaseVersion || ""),
      bootstrap_channel: String(bootstrap.channel || ""),
      bootstrap_build: String(bootstrap.buildType || ""),
      electron: process.versions?.electron || "",
      chrome: process.versions?.chrome || "",
      node: process.versions?.node || "",
    },
  };
};

const postJson = async (targetUrl, body, extraHeaders = {}, timeoutMs = 10000) => {
  return new Promise((resolve) => {
    try {
      const parsed = new URL(String(targetUrl || ""));
      const transport = parsed.protocol === "http:" ? http : https;
      const payload = Buffer.from(JSON.stringify(body || {}), "utf8");
      const req = transport.request(
        parsed,
        {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "Content-Length": String(payload.length),
            "User-Agent": "SekaiLink-CrashReporter/1.0",
            ...extraHeaders,
          },
          timeout: Math.max(1000, Number(timeoutMs || 0)),
        },
        (res) => {
          let responseBody = "";
          res.on("data", (chunk) => {
            responseBody += String(chunk || "");
          });
          res.on("end", () => {
            const status = Number(res.statusCode || 0);
            const ok = status >= 200 && status < 300;
            resolve({ ok, status, body: responseBody });
          });
        }
      );
      req.on("timeout", () => {
        req.destroy(new Error("request_timeout"));
      });
      req.on("error", (err) => {
        resolve({ ok: false, status: 0, error: String(err || "request_error"), body: "" });
      });
      req.write(payload);
      req.end();
    } catch (err) {
      resolve({ ok: false, status: 0, error: String(err || "invalid_request"), body: "" });
    }
  });
};

const submitDiagnosticsReport = async (options = {}) => {
  const uploadUrl = normalizeIpcString(options?.uploadUrl, 2048);
  if (!uploadUrl || !isSafeExternalUrl(uploadUrl)) return { ok: false, error: "invalid_upload_url" };
  const report = isPlainObject(options?.report) ? options.report : await collectDiagnosticsReport(options);
  const authToken = normalizeIpcString(options?.authToken, 4096);
  const headers = {};
  if (authToken) headers.Authorization = `Bearer ${authToken}`;
  const timeoutMsRaw = Number(options?.timeoutMs || 12000);
  const timeoutMs = Number.isFinite(timeoutMsRaw) ? Math.max(2000, timeoutMsRaw) : 12000;
  const result = await postJson(uploadUrl, report, headers, timeoutMs);
  if (!result.ok) return { ok: false, error: result.error || `upload_http_${result.status}`, status: result.status || 0 };
  return { ok: true, status: result.status || 200 };
};

const getPythonCommand = () => {
  const fromEnv = process.env.SEKAILINK_PYTHON || process.env.PYTHON;
  if (fromEnv) return fromEnv;
  return process.platform === "win32" ? "python" : "python3";
};

let _pythonRuntimePromise = null;
let _pythonBootstrapCache = null;

const getPythonVenvDir = () => {
  return path.join(getRuntimeToolsDir(), "python", "venv");
};

const getPythonVenvPythonPath = () => {
  const venvDir = getPythonVenvDir();
  if (process.platform === "win32") return path.join(venvDir, "Scripts", "python.exe");
  // venv typically creates `bin/python`; keep it explicit.
  return path.join(venvDir, "bin", "python");
};

const runProcess = (cmd, args, options = {}) => {
  return new Promise((resolve) => {
    let settled = false;
    const done = (result) => {
      if (settled) return;
      settled = true;
      resolve(result);
    };
    const proc = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"], ...options });
    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (c) => (stdout += String(c)));
    proc.stderr.on("data", (c) => (stderr += String(c)));
    proc.on("error", (err) => done({ code: 1, stdout, stderr: `${stderr}${String(err || "")}` }));
    proc.on("exit", (code) => done({ code: code ?? 0, stdout, stderr }));
  });
};

const getWindowsPortablePythonVersion = () => {
  const value = String(process.env.SEKAILINK_WIN_PYTHON_VERSION || "3.12.8").trim();
  return value || "3.12.8";
};

const resolveNugetPackageSha512 = async (packageId, version) => {
  const pkg = String(packageId || "").trim().toLowerCase();
  const ver = String(version || "").trim().toLowerCase();
  if (!pkg || !ver) throw new Error("missing_nuget_package");
  const shaUrl = `https://api.nuget.org/v3-flatcontainer/${pkg}/${ver}/${pkg}.${ver}.nupkg.sha512`;
  return new Promise((resolve, reject) => {
    const req = https.get(shaUrl, { headers: { "User-Agent": "SekaiLink/1.0", "Accept": "text/plain" } }, (res) => {
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error(`nuget_hash_http_${res.statusCode || "unknown"}`));
      }
      let body = "";
      res.on("data", (chunk) => {
        body += String(chunk || "");
      });
      res.on("end", () => {
        const raw = body.trim();
        try {
          const hex = Buffer.from(raw, "base64").toString("hex").toLowerCase();
          if (!/^[a-f0-9]{128}$/.test(hex)) return reject(new Error("nuget_hash_invalid"));
          resolve(hex);
        } catch (err) {
          reject(err);
        }
      });
    });
    req.on("error", reject);
  });
};

const getWindowsPortablePythonRoot = () => {
  return path.join(getRuntimeToolsDir(), "python", "portable-win");
};

const getWindowsPortablePythonExe = () => {
  return path.join(getWindowsPortablePythonRoot(), "tools", "python.exe");
};

const getRuntimePortableWindowsPythonRoot = () => {
  return getRuntimeFilePath(path.join("tools", "python", "portable-win"));
};

const getPrivatePythonCandidates = () => {
  const candidates = [];
  const add = (candidate, label) => {
    const p = String(candidate || "").trim();
    if (!p) return;
    candidates.push({ cmd: p, args: [], label });
  };
  const arch =
    process.arch === "x64" ? "x86_64" : process.arch === "arm64" ? "arm64" : String(process.arch || "unknown");

  if (process.platform === "win32") {
    for (const portable of listExistingWindowsPortablePythonExes()) {
      add(portable, "private-python-win");
    }
  } else if (process.platform === "linux") {
    add(path.join(getRuntimeFilePath(path.join("tools", "python", "portable-linux")), "bin", "python3"), "private-python-linux");
    add(path.join(getRuntimeFilePath(path.join("tools", "python", `linux-${arch}`)), "bin", "python3"), "private-python-linux");
    add(path.join(getBundledThirdPartyDir(), "python", `sekailink-python-linux-${arch}`, "bin", "python3"), "private-python-linux");
  } else if (process.platform === "darwin") {
    add(path.join(getRuntimeFilePath(path.join("tools", "python", `macos-${arch}`)), "bin", "python3"), "private-python-macos");
    add(path.join(getBundledThirdPartyDir(), "python", `sekailink-python-macos-${arch}`, "bin", "python3"), "private-python-macos");
  }
  return candidates.filter((candidate) => fs.existsSync(candidate.cmd));
};

const pythonExeProbeOk = (pythonPath) => {
  const p = String(pythonPath || "").trim();
  if (!p) return false;
  try {
    if (!fs.existsSync(p)) return false;
    const probe = spawnSync(p, ["-c", "import sys; print(sys.executable); raise SystemExit(0 if (3, 12) <= sys.version_info[:2] < (3, 14) else 2)"], {
      stdio: ["ignore", "ignore", "ignore"],
      env: { ...process.env, PYTHONNOUSERSITE: "1" },
    });
    return probe.status === 0;
  } catch (_err) {
    return false;
  }
};

const listExistingWindowsPortablePythonExes = () => {
  if (process.platform !== "win32") return [];
  const roots = [getWindowsPortablePythonRoot(), getRuntimePortableWindowsPythonRoot()];
  const seen = new Set();
  const out = [];
  const add = (candidate) => {
    const p = String(candidate || "").trim();
    if (!p) return;
    const k = p.toLowerCase();
    if (seen.has(k)) return;
    if (!fs.existsSync(p)) return;
    seen.add(k);
    out.push(p);
  };

  for (const root of roots) {
    if (!root || !fs.existsSync(root)) continue;
    add(path.join(root, "tools", "python.exe"));
    add(path.join(root, "python.exe"));
    const discovered = findPathByBasename(root, "python.exe", 8);
    if (discovered && !String(discovered).toLowerCase().includes(`${path.sep}venv${path.sep}`)) add(discovered);
  }
  return out;
};

const ensurePortableWindowsPython = async () => {
  if (process.platform !== "win32") return "";
  const existingPortable = listExistingWindowsPortablePythonExes();
  if (existingPortable.length) {
    for (const candidate of existingPortable) {
      if (pythonExeProbeOk(candidate)) return candidate;
    }
  }

  const root = getWindowsPortablePythonRoot();

  emitSessionEvent({ event: "status", status: "Preparing SekaiLink runtime (Windows, one-time)..." });
  const version = getWindowsPortablePythonVersion();
  const url = `https://www.nuget.org/api/v2/package/python/${version}`;
  const expectedSha512 = await resolveNugetPackageSha512("python", version);
  const dlDir = path.join(getRuntimeToolsDir(), "python", "downloads");
  ensureDir(dlDir);
  const pkgPath = path.join(dlDir, `python-${version}-win64.nupkg`);
  await downloadToFile(url, pkgPath, {
    expectedSha512,
    requireHash: true,
  });

  // Replace any broken partial runtime with a fresh portable install.
  fs.rmSync(root, { recursive: true, force: true });
  ensureDir(root);
  extractZip(pkgPath, root);

  const direct = getWindowsPortablePythonExe();
  if (fs.existsSync(direct)) return direct;
  const discovered = findPathByBasename(root, "python.exe", 8);
  if (discovered && fs.existsSync(discovered)) return discovered;
  throw new Error(`python_portable_install_failed: python.exe missing under ${root}`);
};

const getPythonBootstrapCandidates = () => {
  const candidates = [];
  const explicit = String(process.env.SEKAILINK_PYTHON || process.env.PYTHON || "").trim();
  if (explicit) candidates.push({ cmd: explicit, args: [], label: "env" });
  candidates.push(...getPrivatePythonCandidates());

  if (process.platform === "win32") {
    if (!app.isPackaged || process.env.SEKAILINK_ALLOW_SYSTEM_PYTHON === "1") {
      candidates.push(
        { cmd: "py", args: ["-3"], label: "py3" },
        { cmd: "py", args: [], label: "py" },
        { cmd: "python3", args: [], label: "python3" },
        { cmd: "python", args: [], label: "python" }
      );
    }
  } else if (!app.isPackaged || process.env.SEKAILINK_ALLOW_SYSTEM_PYTHON === "1") {
    candidates.push(
      { cmd: "python3.13", args: [], label: "python3.13" },
      { cmd: "python3.12", args: [], label: "python3.12" },
      { cmd: "python3", args: [], label: "python3" },
      { cmd: "python", args: [], label: "python" }
    );
  }
  return candidates;
};

const probePythonCandidate = async (candidate) => {
  const cmd = String(candidate?.cmd || "").trim();
  const args = Array.isArray(candidate?.args) ? candidate.args : [];
  if (!cmd) return false;
  const probe = await runProcess(cmd, [...args, "-c", "import sys; print(sys.executable); raise SystemExit(0 if (3, 12) <= sys.version_info[:2] < (3, 14) else 2)"], {
    env: { ...process.env, PYTHONNOUSERSITE: "1" },
  });
  return probe.code === 0;
};

const resolveBootstrapPython = async () => {
  if (_pythonBootstrapCache) return _pythonBootstrapCache;
  for (const candidate of getPythonBootstrapCandidates()) {
    if (await probePythonCandidate(candidate)) {
      _pythonBootstrapCache = candidate;
      return _pythonBootstrapCache;
    }
  }
  if (process.platform === "win32" && (!app.isPackaged || process.env.SEKAILINK_ALLOW_RUNTIME_DOWNLOAD === "1")) {
    try {
      const portable = await ensurePortableWindowsPython();
      const candidate = { cmd: portable, args: [], label: "portable-win" };
      if (await probePythonCandidate(candidate)) {
        _pythonBootstrapCache = candidate;
        return _pythonBootstrapCache;
      }
    } catch (err) {
      writeLogLine("warn", "python-runtime", `portable python install failed: ${String(err || "")}`);
    }
  }
  return null;
};

const parseMissingModule = (text) => {
  const s = String(text || "");
  const m =
    s.match(/MISSING_MODULE:([A-Za-z0-9_\\.\\-]+)/) ||
    s.match(/ModuleNotFoundError:\\s*No module named ['\\\"]([^'\\\"]+)['\\\"]/);
  if (!m) return "";
  return String(m[1] || "").trim();
};

const getPipSpecForModule = (moduleName) => {
  const name = String(moduleName || "").trim();
  if (!name) return "";
  // Common mismatches between import name and pip package.
  const map = {
    yaml: "pyyaml",
    PIL: "pillow",
    dotenv: "python-dotenv",
    pkg_resources: "setuptools<70", // pkg_resources removed in setuptools 70+ on Python 3.12+
  };
  return map[name] || name;
};

const ensurePythonRuntime = async () => {
  if (_pythonRuntimePromise) return _pythonRuntimePromise;
  _pythonRuntimePromise = (async () => {
    const bootstrap = await resolveBootstrapPython();
    if (!bootstrap) {
      throw new Error("python_missing: no usable Python interpreter found");
    }
    const bootstrapPython = bootstrap.cmd;
    const bootstrapArgs = bootstrap.args || [];
    const venvDir = getPythonVenvDir();
    const venvPython = getPythonVenvPythonPath();

    // Fast path: venv exists and can import required modules.
    if (fs.existsSync(venvPython)) {
      const check = await runProcess(
        venvPython,
        [
          "-c",
          "import sys; assert (3, 12) <= sys.version_info[:2] < (3, 14); import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, bsdiff4, websockets, certifi, orjson, PIL, dotenv, pyaes; print('ok')",
        ],
        {
          env: { ...process.env, PYTHONNOUSERSITE: "1" },
        }
      );
      if (check.code === 0) return venvPython;
      writeLogLine("warn", "python-runtime", `venv present but missing deps; will reinstall. stderr=${String(check.stderr || "").trim()}`);
      fs.rmSync(venvDir, { recursive: true, force: true });
    }

    emitSessionEvent({ event: "status", status: "Preparing launch runtime (one-time)..." });

    ensureDir(path.dirname(venvDir));
    if (!fs.existsSync(venvPython)) {
      let created = await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "venv", venvDir], {
        env: { ...process.env, PYTHONNOUSERSITE: "1" },
      });
      if (created.code !== 0 && process.platform === "win32") {
        // On some Windows Python builds, ensurepip must be initialized before venv creation.
        await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "ensurepip", "--upgrade"], {
          env: { ...process.env, PYTHONNOUSERSITE: "1" },
        });
        created = await runProcess(bootstrapPython, [...bootstrapArgs, "-m", "venv", venvDir], {
          env: { ...process.env, PYTHONNOUSERSITE: "1" },
        });
      }
      if (created.code !== 0) {
        const msg = String(created.stderr || created.stdout || "venv_failed").trim();
        writeLogLine("error", "python-runtime", `failed to create venv: ${msg}`);
        throw new Error(`python_venv_failed: ${msg}`);
      }
    }

    // Install minimal deps required by the bundled AP python sources.
    // Packaged Windows builds must use the bundled wheelhouse only; no user Python
    // install and no network dependency are allowed there.
    const wheelhouseDir = getPythonRuntimeWheelhouseDir();
    const hasWheelhouse = Boolean(wheelhouseDir && fs.existsSync(wheelhouseDir));
    if (process.platform === "win32" && app.isPackaged && !hasWheelhouse) {
      throw new Error(`python_wheelhouse_missing:${wheelhouseDir || "win-amd64-cp312"}`);
    }
    const pipEnv = {
      ...process.env,
      PYTHONNOUSERSITE: "1",
      PYTHONDONTWRITEBYTECODE: "1",
      SKIP_REQUIREMENTS_UPDATE: "1",
      PIP_DISABLE_PIP_VERSION_CHECK: "1",
      PIP_NO_INPUT: "1",
      ...(hasWheelhouse ? { PIP_NO_INDEX: "1", PIP_FIND_LINKS: wheelhouseDir } : {}),
    };
    // Pin setuptools<70 to ensure pkg_resources is included (removed in setuptools 70+ on Python 3.12+)
    const upgraded = await runProcess(venvPython, getPythonPipInstallArgs(["setuptools<70", "wheel"]), { env: pipEnv });
    if (upgraded.code !== 0) {
      const msg = String(upgraded.stderr || upgraded.stdout || "pip_bootstrap_failed").trim();
      writeLogLine("warn", "python-runtime", `pip bootstrap failed (continuing best-effort): ${msg}`);
    }

    // Keep this list minimal: only what is required to import and run the bundled AP python sources
    // for patching + headless clients. Avoid heavy GUI/server deps (kivy/gevent/flask/etc).
    const deps = new Set([
      "setuptools<70", // provides pkg_resources (removed in 70+ on Python 3.12+)
      "wheel",
      "pyyaml",
      "pathspec",
      "platformdirs",
      "schema",
      "jinja2",
      "colorama",
      "typing_extensions",
      "jellyfish",
      "bsdiff4",
      "websockets",
      "certifi",
      "orjson",
      // FF4FE local generator runtime
      "pillow",
      "python-dotenv",
      "pyaes",
    ]);

    const installOne = async (spec) => {
      const s = String(spec || "").trim();
      if (!s) return { ok: true };
      const res = await runProcess(venvPython, getPythonPipInstallArgs([s]), { env: pipEnv });
      if (res.code !== 0) {
        const msg = String(res.stderr || res.stdout || "pip_failed").trim();
        writeLogLine("error", "python-runtime", `pip install failed spec=${s}: ${msg}`);
        return { ok: false, error: msg };
      }
      return { ok: true };
    };

    // Install base deps, then auto-heal any missing imports we discover.
    const base = await runProcess(venvPython, getPythonPipInstallArgs(Array.from(deps)), { env: pipEnv });
    if (base.code !== 0) {
      const msg = String(base.stderr || base.stdout || "pip_failed").trim();
      writeLogLine("error", "python-runtime", `pip base install failed: ${msg}`);
      throw new Error(`python_deps_install_failed: ${msg}`);
    }

    const importCheckCode = `
import importlib, sys
mods = [
  "pkg_resources",
  "yaml",
  "pathspec",
  "platformdirs",
  "schema",
  "jinja2",
  "colorama",
  "typing_extensions",
  "jellyfish",
  "bsdiff4",
  "websockets",
  "certifi",
  "orjson",
  "PIL",
  "dotenv",
  "pyaes",
]
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or m), flush=True)
    raise
print("ok")
`.trim();

    for (let attempt = 0; attempt < 8; attempt += 1) {
      const check = await runProcess(venvPython, ["-c", importCheckCode], { env: { ...pipEnv } });
      if (check.code === 0) break;
      const missing = parseMissingModule(check.stdout) || parseMissingModule(check.stderr);
      if (!missing) {
        const msg = String(check.stderr || check.stdout || "deps_verify_failed").trim();
        throw new Error(`python_deps_verify_failed: ${msg}`);
      }
      const spec = getPipSpecForModule(missing);
      emitSessionEvent({ event: "status", status: "Preparing launch runtime component..." });
      const one = await installOne(spec);
      if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
    }

    // Probe only the common AP runtime surface here. Individual patchers verify
    // their own target world module later, from the selected module manifest.
    // Importing every bundled world here pulls optional game deps into unrelated
    // launches and can trigger upstream ModuleUpdate paths.
    const apRoot = getBundledApRootDir();
    const apProbeModules = ["Patch", "NetUtils", "Utils", "worlds.Files"];
    const apProbeCode = `
import importlib, os, sys, warnings
import types
warnings.filterwarnings("ignore", message=r"_speedups not available.*", category=UserWarning)
ap_root = os.environ.get("SEKAILINK_AP_ROOT","")
if ap_root and ap_root not in sys.path:
  sys.path.insert(0, ap_root)
worlds_dir = os.path.join(ap_root, "worlds")
if os.path.isdir(worlds_dir):
  pkg = types.ModuleType("worlds")
  pkg.__path__ = [worlds_dir]
  pkg.__package__ = "worlds"
  pkg.__sekailink_scoped__ = True
  sys.modules["worlds"] = pkg
mods = ${JSON.stringify(apProbeModules)}
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or ""), flush=True)
    raise
print("ok")
`.trim();

    for (let attempt = 0; attempt < 20; attempt += 1) {
      const probe = await runProcess(venvPython, ["-c", apProbeCode], {
        env: { ...pipEnv, SEKAILINK_AP_ROOT: apRoot, PYTHONPATH: apRoot },
      });
      if (probe.code === 0) break;
      const missing = parseMissingModule(probe.stdout) || parseMissingModule(probe.stderr);
      if (!missing) {
        const msg = String(probe.stderr || probe.stdout || "ap_probe_failed").trim();
        throw new Error(`python_deps_verify_failed: ${msg}`);
      }
      const spec = getPipSpecForModule(missing);
      emitSessionEvent({ event: "status", status: "Preparing launch runtime component..." });
      const one = await installOne(spec);
      if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
    }

    // Verify.
    const verify = await runProcess(
      venvPython,
        [
          "-c",
          "import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, jellyfish, bsdiff4, websockets, certifi, orjson, PIL, dotenv, pyaes; print('ok')",
        ],
      {
        env: { ...pipEnv },
      }
    );
    if (verify.code !== 0) {
      const msg = String(verify.stderr || verify.stdout || "verify_failed").trim();
      writeLogLine("error", "python-runtime", `deps verify failed: ${msg}`);
      throw new Error(`python_deps_verify_failed: ${msg}`);
    }

    emitSessionEvent({ event: "status", status: "Launch runtime ready." });
    return venvPython;
  })().catch((err) => {
    // Allow retry after transient bootstrap failures (download/network/corrupt partial install).
    _pythonRuntimePromise = null;
    throw err;
  });
  return _pythonRuntimePromise;
};

const verifyPatcherPythonWorld = async (python, patchWorldModule) => {
  const world = normalizeIpcString(patchWorldModule || "", 300);
  if (!world) return { ok: true };

  const modules = Array.from(new Set(["Patch", "worlds.Files", world]));
  const probeCode = `
import importlib, os, sys, warnings
import types
warnings.filterwarnings("ignore", message=r"_speedups not available.*", category=UserWarning)
ap_root = os.environ.get("SEKAILINK_AP_ROOT","")
if ap_root and ap_root not in sys.path:
  sys.path.insert(0, ap_root)
worlds_dir = os.path.join(ap_root, "worlds")
if os.path.isdir(worlds_dir):
  pkg = types.ModuleType("worlds")
  pkg.__path__ = [worlds_dir]
  pkg.__package__ = "worlds"
  pkg.__sekailink_scoped__ = True
  sys.modules["worlds"] = pkg
mods = ${JSON.stringify(modules)}
for m in mods:
  try:
    importlib.import_module(m)
  except ModuleNotFoundError as e:
    print("MISSING_MODULE:" + (e.name or ""), flush=True)
    raise
print("ok")
`.trim();

  const probe = await runProcess(python, ["-c", probeCode], {
    env: withApPythonEnv(process.env),
  });
  if (probe.code === 0) return { ok: true };

  const msg = String(probe.stderr || probe.stdout || "world_probe_failed").trim();
  writeLogLine("error", "python-runtime", `patcher world probe failed world=${world}: ${msg}`);
  return { ok: false, error: msg };
};

let _monoRuntimePromise = null;

const monoProbeOk = (monoPath) => {
  const p = String(monoPath || "").trim();
  if (!p) return false;
  try {
    if (!fs.existsSync(p)) return false;
    const probe = spawnSync(p, ["--version"], { stdio: ["ignore", "ignore", "ignore"] });
    return probe.status === 0;
  } catch (_err) {
    return false;
  }
};

const getBundledMonoInstallRoot = () => {
  // SekaiLink bundles a pinned mono runtime for Linux so BizHawk can run without system package installs.
  // Expected layout: third_party/mono/sekailink-mono-linux-<arch>/bin/mono
  if (process.platform !== "linux") return "";
  const arch =
    process.arch === "x64" ? "x86_64" : process.arch === "arm64" ? "arm64" : String(process.arch || "x86_64");
  return path.join(getBundledThirdPartyDir(), "mono", `sekailink-mono-linux-${arch}`);
};

const getMonoFromInstallRoot = (rootDir) => {
  const base = String(rootDir || "").trim();
  if (!base) return "";
  const direct = path.join(base, "bin", "mono");
  if (fs.existsSync(direct)) return direct;
  // Fallback: find any "mono" binary under the extracted tree.
  const found = findPathByBasename(base, "mono", 8);
  return found && fs.existsSync(found) ? found : "";
};

const ensureMonoRuntime = async () => {
  if (_monoRuntimePromise) return _monoRuntimePromise;
  _monoRuntimePromise = (async () => {
    // 1) Explicit override
    const override = String(process.env.SEKAILINK_MONO || "").trim();
    if (override && monoProbeOk(override)) return override;

    // 2) Bundled mono runtime (Linux)
    const bundledRoot = getBundledMonoInstallRoot();
    if (bundledRoot && fs.existsSync(bundledRoot)) {
      const monoPath = getMonoFromInstallRoot(bundledRoot);
      try {
        if (monoPath) fs.chmodSync(monoPath, 0o755);
      } catch (_err) {
        // ignore chmod failures
      }
      if (monoPath && monoProbeOk(monoPath)) return monoPath;
      writeLogLine("error", "mono-runtime", `bundled mono not runnable: rootDir=${bundledRoot} monoPath=${monoPath}`);
    }

    // 3) System mono (fallback)
    const sys = which("mono");
    if (sys && monoProbeOk(sys)) return sys;

    throw new Error(`mono_missing: expected bundled mono at ${bundledRoot || "(n/a)"}`);
  })().catch((err) => {
    // Allow retry if the user installs mono after a failure.
    _monoRuntimePromise = null;
    throw err;
  });
  return _monoRuntimePromise;
};

const getOverlayRuntimeDir = () => {
  return path.join(getRuntimeOverlayRoot(), "runtime");
};

const getRuntimeFilePath = (relativePath) => {
  const rel = String(relativePath || "").replace(/\\/g, "/").replace(/^\/+/, "");
  const overlayPath = path.join(getOverlayRuntimeDir(), rel);
  if (fs.existsSync(overlayPath)) return overlayPath;
  return path.join(getBundledRuntimeDir(), rel);
};

const getPythonRuntimeWheelhouseTag = () => {
  if (process.platform === "win32" && process.arch === "x64") return "win-amd64-cp312";
  return "";
};

const getPythonRuntimeWheelhouseDir = () => {
  const tag = getPythonRuntimeWheelhouseTag();
  if (!tag) return "";
  return getRuntimeFilePath(path.join("tools", "python", "wheelhouse", tag));
};

const getPythonPipInstallArgs = (specs = []) => {
  const normalizedSpecs = Array.isArray(specs)
    ? specs.map((spec) => String(spec || "").trim()).filter(Boolean)
    : [];
  const wheelhouseDir = getPythonRuntimeWheelhouseDir();
  if (wheelhouseDir && fs.existsSync(wheelhouseDir)) {
    return ["-m", "pip", "install", "--no-index", "--find-links", wheelhouseDir, ...normalizedSpecs];
  }
  if (process.platform === "win32" && app.isPackaged) {
    throw new Error(`python_wheelhouse_missing:${wheelhouseDir || "win-amd64-cp312"}`);
  }
  return ["-m", "pip", "install", ...normalizedSpecs];
};

const getRuntimeModulesDirs = () => {
  const dirs = [];
  const overlayModulesDir = path.join(getOverlayRuntimeDir(), "modules");
  const bundledModulesDir = path.join(getBundledRuntimeDir(), "modules");
  if (fs.existsSync(overlayModulesDir)) dirs.push(overlayModulesDir);
  if (fs.existsSync(bundledModulesDir)) dirs.push(bundledModulesDir);
  if (!dirs.length) dirs.push(overlayModulesDir, bundledModulesDir);
  return dirs;
};

const getRuntimeModuleDir = (moduleId) => {
  const id = String(moduleId || "").trim();
  if (!id) return "";
  for (const dir of getRuntimeModulesDirs()) {
    const candidate = path.join(dir, id);
    if (fs.existsSync(candidate)) return candidate;
  }
  return "";
};

const getRuntimeModulePath = (moduleId, ...parts) => {
  const base = getRuntimeModuleDir(moduleId);
  if (base) return path.join(base, ...parts);
  const fallbackDir = path.join(getRuntimeModulesDirs()[0], String(moduleId || ""));
  return path.join(fallbackDir, ...parts);
};

const getCommonClientWrapperPath = () => {
  return getRuntimeFilePath("commonclient_wrapper.py");
};

const getBizHawkClientWrapperPath = () => {
  return getRuntimeFilePath("bizhawkclient_wrapper.py");
};

const getSniClientWrapperPath = () => {
  return getRuntimeFilePath("sniclient_wrapper.py");
};

const getSniBridgePath = () => {
  return getRuntimeFilePath("sni_bridge.py");
};

const getPatcherWrapperPath = () => {
  return getRuntimeFilePath("patcher_wrapper.py");
};

const getPopTrackerDir = () => {
  const resolved = firstExistingDir(...getPlatformRuntimeDirCandidates("poptracker"));
  return resolved || getRuntimeFilePath("poptracker");
};

const getPopTrackerInstalledDir = () => {
  // PopTracker must run from a writable, self-contained directory (AppImage resources are mounted read-only;
  // Windows install dirs can also be constrained). We stage PopTracker + assets here.
  return path.join(getRuntimeToolsDir(), "poptracker", "app");
};

const ensurePopTrackerPortableConfig = (baseDir) => {
  // PopTracker stores config in user profile by default. For SekaiLink we want deterministic, 0-interaction
  // behavior per session (and to avoid user config forcing wss:// or non-local usb2snes endpoints).
  try {
    fs.writeFileSync(path.join(baseDir, "portable.txt"), "");
  } catch (_err) {
    // best-effort
  }

  const cfgRoot = process.platform === "win32"
    ? path.join(baseDir, "portable-config")
    : path.join(baseDir, ".portable-config");
  const cfgDir = path.join(cfgRoot, "PopTracker");
  const cfgPath = path.join(cfgDir, "PopTracker.json");
  ensureDir(cfgDir);

  let cfg = {};
  try {
    if (fs.existsSync(cfgPath)) {
      const parsed = JSON.parse(fs.readFileSync(cfgPath, "utf-8"));
      if (parsed && typeof parsed === "object") cfg = parsed;
    }
  } catch (_err) {
    cfg = {};
  }

  // Force local usb2snes/SNI endpoint (SekaiLink bridge) and ensure it's plain ws://.
  cfg.usb2snes = "ws://127.0.0.1:23074";
  try {
    fs.writeFileSync(cfgPath, JSON.stringify(cfg, null, 2), "utf-8");
  } catch (_err) {
    // best-effort
  }
};

const clearPopTrackerAutosaveForPack = (baseDir, packPath) => {
  try {
    const manifestPath = path.join(String(packPath || ""), "manifest.json");
    if (!fs.existsSync(manifestPath)) return;
    const raw = fs.readFileSync(manifestPath, "utf-8").replace(/^\uFEFF/, "");
    const manifest = JSON.parse(raw);
    const uid = String(manifest?.package_uid || "").trim();
    if (!uid) return;

    const cfgRoot = process.platform === "win32"
      ? path.join(baseDir, "portable-config")
      : path.join(baseDir, ".portable-config");
    const savesDir = path.join(cfgRoot, "PopTracker", "saves", uid);
    if (!fs.existsSync(savesDir)) return;
    fs.rmSync(savesDir, { recursive: true, force: true });
    writeLogLine("info", "poptracker", `cleared autosave state for pack uid=${uid}`);
  } catch (err) {
    writeLogLine("warn", "poptracker", `autosave cleanup skipped: ${String(err || "")}`);
  }
};

const stagePopTrackerToDir = (sourceDir, destDir) => {
  try {
    if (!fs.existsSync(sourceDir)) {
      return { ok: false, error: "poptracker_missing", detail: `source_missing: ${sourceDir}` };
    }
    // Always stage into a clean directory to avoid mismatched/leftover DLLs causing 0xc000007b.
    fs.rmSync(destDir, { recursive: true, force: true });
    ensureDir(destDir);

    // Copy assets/api/schema/key if missing (cheap / incremental).
    for (const dirName of ["api", "assets", "schema", "key"]) {
      const src = path.join(sourceDir, dirName);
      const dst = path.join(destDir, dirName);
      if (!fs.existsSync(src)) continue;
      fs.cpSync(src, dst, { recursive: true });
    }

    // Copy root metadata files (best-effort).
    for (const fileName of ["LICENSE", "README.md", "CHANGELOG.md", "CREDITS.md"]) {
      const src = path.join(sourceDir, fileName);
      const dst = path.join(destDir, fileName);
      if (fs.existsSync(src) && !fs.existsSync(dst)) {
        try {
          fs.copyFileSync(src, dst);
        } catch (_err) {
          // ignore
        }
      }
    }

    if (process.platform === "win32") {
      const dstExe = path.join(destDir, "poptracker.exe");
      const bundledExe = path.join(sourceDir, "poptracker.exe");
      const thirdPartyExe = getThirdPartyPopTrackerExePath();
      const exeSrc = fs.existsSync(bundledExe) ? bundledExe : (thirdPartyExe && fs.existsSync(thirdPartyExe) ? thirdPartyExe : "");
      if (exeSrc) fs.copyFileSync(exeSrc, dstExe);

      // PopTracker depends on multiple DLLs (SDL2, OpenSSL, zlib, mingw runtime). Copy everything we can
      // from the exe directory, plus any DLLs shipped alongside the runtime sourceDir.
      const copyDllsFromDir = (dir) => {
        try {
          if (!dir || !fs.existsSync(dir)) return;
          const items = fs.readdirSync(dir);
          for (const name of items) {
            if (!name || typeof name !== "string") continue;
            if (!name.toLowerCase().endsWith(".dll")) continue;
            const src = path.join(dir, name);
            const dst = path.join(destDir, name);
            try {
              if (fs.existsSync(src)) fs.copyFileSync(src, dst);
            } catch (_err) {
              // ignore
            }
          }
        } catch (_err) {
          // ignore
        }
      };
      if (exeSrc) copyDllsFromDir(path.dirname(exeSrc));
      copyDllsFromDir(sourceDir);

      return { ok: fs.existsSync(dstExe), exePath: dstExe, baseDir: destDir };
    }

    const dstExe = path.join(destDir, "poptracker");
    const bundledExe = path.join(sourceDir, "poptracker");
    if (!fs.existsSync(dstExe) && fs.existsSync(bundledExe)) fs.copyFileSync(bundledExe, dstExe);
    try {
      if (fs.existsSync(dstExe)) fs.chmodSync(dstExe, 0o755);
    } catch (_err) {
      // ignore chmod failures
    }
    return { ok: fs.existsSync(dstExe), exePath: dstExe, baseDir: destDir };
  } catch (err) {
    const detail = String(err || "");
    writeLogLine("error", "poptracker", `staging failed: ${detail}`);
    return { ok: false, error: "poptracker_stage_failed", detail };
  }
};

const getThirdPartyPopTrackerExePath = () => {
  const buildRoot = path.join(getBundledThirdPartyDir(), "PopTracker", "build");
  const candidates = [];
  if (process.platform === "win32") {
    candidates.push(
      path.join(buildRoot, "win64", "poptracker.exe"),
      path.join(buildRoot, "win32", "poptracker.exe")
    );
  } else {
    const linuxArch = process.arch === "arm64" ? "linux-arm64" : "linux-x86_64";
    candidates.push(
      path.join(buildRoot, linuxArch, "poptracker"),
      path.join(buildRoot, "linux-x86_64", "poptracker"),
      path.join(buildRoot, "linux-arm64", "poptracker")
    );
  }
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) return candidate;
  }
  return "";
};

let _popTrackerExeCache = "";
let _popTrackerExeSource = "";

const getBundledPopTrackerLibsDir = () => {
  const resolved = firstExistingDir(...getPlatformRuntimeDirCandidates("_bundled_libs", "poptracker"));
  return resolved || getRuntimeFilePath(path.join("_bundled_libs", "poptracker"));
};

const getPopTrackerExePath = () => {
  if (_popTrackerExeCache && fs.existsSync(_popTrackerExeCache)) return _popTrackerExeCache;

  const sourceDir = getPopTrackerDir();
  const destDir = getPopTrackerInstalledDir();
  const staged = stagePopTrackerToDir(sourceDir, destDir);
  if (staged?.ok && staged.exePath) {
    _popTrackerExeCache = staged.exePath;
    _popTrackerExeSource = "staged";
    return _popTrackerExeCache;
  }

  const exeName = process.platform === "win32" ? "poptracker.exe" : "poptracker";
  _popTrackerExeCache = path.join(sourceDir, exeName);
  _popTrackerExeSource = "runtime";
  return _popTrackerExeCache;
};

const getConfigPath = () => {
  return path.join(app.getPath("home"), ".sekailink", "config.json");
};

const normalizeConfig = (value) => {
  const config = value && typeof value === "object" ? value : {};
  if (!config.windowing || typeof config.windowing !== "object") config.windowing = {};
  if (!config.layout || typeof config.layout !== "object") config.layout = {};
  // Back-compat: if older builds stored gamescope at top-level, migrate to windowing.gamescope.
  if (config.gamescope && !config.windowing.gamescope) {
    config.windowing.gamescope = config.gamescope;
  }
  return config;
};

const readConfig = () => {
  const configPath = getConfigPath();
  try {
    if (!fs.existsSync(configPath)) return {};
    return normalizeConfig(JSON.parse(fs.readFileSync(configPath, "utf-8")));
  } catch (_err) {
    return {};
  }
};

const writeConfig = (config) => {
  const configPath = getConfigPath();
  ensureDir(path.dirname(configPath));
  fs.writeFileSync(configPath, JSON.stringify(normalizeConfig(config), null, 2));
};

const ensureDir = (dirPath) => {
  fs.mkdirSync(dirPath, { recursive: true });
};

const isWritablePath = (p) => {
  try {
    fs.accessSync(String(p || ""), fs.constants.W_OK);
    return true;
  } catch (_err) {
    return false;
  }
};

const getRuntimeModulesDir = () => {
  return getRuntimeModulesDirs()[0];
};

const getRomsDir = () => {
  return path.join(app.getPath("userData"), "roms");
};

const getPopTrackerPacksDir = () => {
  return path.join(app.getPath("userData"), "poptracker", "packs");
};

const getRuntimeDownloadsDir = () => {
  return path.join(app.getPath("userData"), "runtime", "downloads");
};

const getRuntimeToolsDir = () => {
  return path.join(app.getPath("userData"), "runtime", "tools");
};

const getClientUpdateDownloadsDir = () => {
  return path.join(app.getPath("downloads"), "SekaiLink");
};

const getClientUpdateStagingDir = () => {
  return path.join(app.getPath("userData"), "updates", "staging");
};

const computeFileHash = (filePath, algo = "sha256") => {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash(String(algo || "sha256"));
    const stream = fs.createReadStream(filePath);
    stream.on("error", reject);
    stream.on("data", (chunk) => hash.update(chunk));
    stream.on("end", () => resolve(hash.digest("hex")));
  });
};

const verifyDownloadedFileHash = async (filePath, options = {}) => {
  const expectedSha256 = normalizeSha256(options?.expectedSha256);
  const expectedSha512 = String(options?.expectedSha512 || "").trim().toLowerCase();
  if (expectedSha256) {
    const actual = String(await computeFileHash(filePath, "sha256")).toLowerCase();
    if (actual !== expectedSha256) throw new Error("checksum_mismatch_sha256");
    return { algo: "sha256", value: actual };
  }
  if (expectedSha512) {
    const actual = String(await computeFileHash(filePath, "sha512")).toLowerCase();
    if (actual !== expectedSha512) throw new Error("checksum_mismatch_sha512");
    return { algo: "sha512", value: actual };
  }
  if (options?.requireHash) {
    throw new Error("missing_expected_hash");
  }
  return { algo: "", value: "" };
};

const downloadToFile = (url, destPath, options = {}) => {
  return new Promise((resolve, reject) => {
    ensureDir(path.dirname(destPath));
    const out = fs.createWriteStream(destPath);
    const client = url.startsWith("https://") ? https : http;
    const req = client.get(url, (res) => {
      if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        out.close();
        let nextUrl = String(res.headers.location || "");
        try {
          nextUrl = new URL(nextUrl, String(url || "")).toString();
        } catch (_err) {
          // keep raw Location header when URL resolution fails
        }
        return resolve(downloadToFile(nextUrl, destPath, options));
      }
      if (res.statusCode !== 200) {
        out.close();
        return reject(new Error(`Download failed: ${res.statusCode}`));
      }
      res.pipe(out);
      out.on("finish", () => out.close(async () => {
        try {
          const digest = await verifyDownloadedFileHash(destPath, options);
          resolve({ ok: true, path: destPath, ...digest });
        } catch (err) {
          try {
            fs.rmSync(destPath, { force: true });
          } catch (_err) {
            // ignore
          }
          reject(err);
        }
      }));
    });
    req.on("error", (err) => {
      out.close();
      reject(err);
    });
  });
};

const sanitizeFilename = (value) => {
  const s = String(value || "").trim();
  if (!s) return "";
  // Keep it simple: portable filenames across Linux/Windows.
  return s
    .replace(/[<>:"/\\|?*\x00-\x1F]/g, "_")
    .replace(/\s+/g, " ")
    .slice(0, 180)
    .trim();
};

const parseContentDispositionFilename = (headerValue) => {
  const cd = String(headerValue || "");
  if (!cd) return "";

  // Try RFC 5987: filename*=UTF-8''...
  const mStar = cd.match(/filename\*\s*=\s*([^']*)''([^;]+)/i);
  if (mStar) {
    const encoded = mStar[2];
    try {
      return decodeURIComponent(encoded);
    } catch (_err) {
      return encoded;
    }
  }

  const m = cd.match(/filename\s*=\s*\"([^\"]+)\"/i) || cd.match(/filename\s*=\s*([^;]+)/i);
  if (m) return String(m[1] || "").trim();
  return "";
};

const uniquePathInDir = (dirPath, baseName) => {
  const safe = sanitizeFilename(baseName) || `download-${Date.now()}`;
  const ext = path.extname(safe);
  const stem = ext ? safe.slice(0, -ext.length) : safe;
  let candidate = path.join(dirPath, safe);
  let n = 1;
  while (fs.existsSync(candidate)) {
    candidate = path.join(dirPath, `${stem} (${n})${ext}`);
    n += 1;
  }
  return candidate;
};

const downloadToDir = (url, destDir, options = {}, depth = 0) => {
  return new Promise((resolve, reject) => {
    if (!url) return reject(new Error("missing_url"));
    if (depth > 6) return reject(new Error("too_many_redirects"));
    ensureDir(destDir);

    const client = url.startsWith("https://") ? https : http;
    const req = client.get(url, (res) => {
      if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        res.resume();
        return resolve(downloadToDir(res.headers.location, destDir, options, depth + 1));
      }
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error(`Download failed: ${res.statusCode}`));
      }

      const cd = res.headers["content-disposition"];
      const nameFromHeader = parseContentDispositionFilename(cd);
      let nameFromUrl = "";
      try {
        nameFromUrl = path.basename(new URL(url).pathname || "");
      } catch (_err) {
        nameFromUrl = "";
      }
      const baseName = sanitizeFilename(nameFromHeader) || sanitizeFilename(nameFromUrl) || String(options.defaultBasename || "download");
      const finalPath = uniquePathInDir(destDir, baseName);

      const out = fs.createWriteStream(finalPath);
      res.pipe(out);
      out.on("finish", () => out.close(() => resolve({ ok: true, path: finalPath, filename: path.basename(finalPath) })));
      out.on("error", (err) => {
        try {
          out.close();
        } catch (_err) {
          // ignore
        }
        reject(err);
      });
    });
    req.on("error", reject);
  });
};

const resolveRedirectUrl = (baseUrl, location) => {
  try {
    return new URL(String(location || ""), String(baseUrl || "")).toString();
  } catch (_err) {
    return String(location || "");
  }
};

const sanitizeHeaders = (value) => {
  if (!value || typeof value !== "object") return {};
  const out = {};
  for (const [k, v] of Object.entries(value)) {
    const key = String(k || "").trim();
    if (!key) continue;
    const lower = key.toLowerCase();
    if (lower === "connection" || lower === "host" || lower === "content-length") continue;
    out[key] = String(v ?? "");
  }
  return out;
};

const downloadToDirWithProgress = (url, destDir, options = {}, depth = 0) => {
  return new Promise((resolve, reject) => {
    if (!url) return reject(new Error("missing_url"));
    if (depth > 8) return reject(new Error("too_many_redirects"));
    ensureDir(destDir);

    const onProgress = typeof options.onProgress === "function" ? options.onProgress : null;
    const headers = sanitizeHeaders(options.headers);
    const client = url.startsWith("https://") ? https : http;
    const req = client.get(url, { headers }, (res) => {
      const status = Number(res.statusCode || 0);
      if (status >= 300 && status < 400 && res.headers.location) {
        const redirected = resolveRedirectUrl(url, res.headers.location);
        res.resume();
        return resolve(downloadToDirWithProgress(redirected, destDir, options, depth + 1));
      }
      if (status !== 200) {
        res.resume();
        return reject(new Error(`download_http_${status || "unknown"}`));
      }

      const cd = res.headers["content-disposition"];
      const nameFromHeader = parseContentDispositionFilename(cd);
      let nameFromUrl = "";
      try {
        nameFromUrl = path.basename(new URL(url).pathname || "");
      } catch (_err) {
        nameFromUrl = "";
      }
      const forced = sanitizeFilename(String(options.fileName || ""));
      const baseName = forced
        || sanitizeFilename(nameFromHeader)
        || sanitizeFilename(nameFromUrl)
        || String(options.defaultBasename || "download");
      const finalPath = uniquePathInDir(destDir, baseName);
      const totalBytes = Number.parseInt(String(res.headers["content-length"] || "0"), 10) || 0;
      let receivedBytes = 0;
      let lastSentAt = 0;
      const sendProgress = (force = false) => {
        if (!onProgress) return;
        const now = Date.now();
        if (!force && now - lastSentAt < 120) return;
        lastSentAt = now;
        const percent = totalBytes > 0 ? Math.min(100, Number(((receivedBytes / totalBytes) * 100).toFixed(2))) : null;
        onProgress({ receivedBytes, totalBytes, percent, path: finalPath });
      };

      const out = fs.createWriteStream(finalPath);
      let settled = false;
      const finish = (err) => {
        if (settled) return;
        settled = true;
        try {
          out.close();
        } catch (_err) {
          // ignore
        }
        if (err) {
          try {
            if (fs.existsSync(finalPath)) fs.unlinkSync(finalPath);
          } catch (_err) {
            // ignore
          }
          reject(err);
          return;
        }
        sendProgress(true);
        resolve({ ok: true, path: finalPath, filename: path.basename(finalPath), bytes: receivedBytes, totalBytes });
      };

      res.on("data", (chunk) => {
        receivedBytes += chunk?.length || 0;
        sendProgress(false);
      });
      res.on("error", (err) => finish(err));
      res.on("aborted", () => finish(new Error("download_aborted")));
      out.on("finish", () => finish(null));
      out.on("error", (err) => finish(err));
      res.pipe(out);
      sendProgress(false);
    });
    req.on("error", reject);
  });
};

const normalizeSha256 = (value) => {
  const s = String(value || "").trim().toLowerCase();
  if (/^[a-f0-9]{64}$/.test(s)) return s;
  return "";
};

const normalizeArtifactType = (value) => {
  return String(value || "")
    .trim()
    .toLowerCase()
    .replace(/_/g, "-");
};

const decodeSignature = (value) => {
  const raw = String(value || "").trim();
  if (!raw) return null;
  if (/^[a-f0-9]+$/i.test(raw) && raw.length % 2 === 0) {
    try {
      return Buffer.from(raw, "hex");
    } catch (_err) {
      return null;
    }
  }
  try {
    return Buffer.from(raw, "base64");
  } catch (_err) {
    return null;
  }
};

const verifyFileSignature = (filePath, signatureValue, publicKeyPem) => {
  try {
    const sig = decodeSignature(signatureValue);
    const pub = String(publicKeyPem || "").trim();
    if (!sig || !pub) return { ok: false, error: "missing_signature_material" };
    const payload = fs.readFileSync(filePath);
    const valid = crypto.verify(null, payload, pub, sig);
    return valid ? { ok: true } : { ok: false, error: "signature_invalid" };
  } catch (err) {
    return { ok: false, error: String(err || "signature_verify_failed") };
  }
};

const settleUpdaterDownload = (downloadId, result) => {
  if (!downloadId) return;
  const payload = result && typeof result === "object" ? result : { ok: false, error: "download_failed" };
  updaterDownloadResults.set(downloadId, payload);
  const waiters = updaterDownloadWaiters.get(downloadId);
  if (Array.isArray(waiters) && waiters.length) {
    for (const waiter of waiters) {
      try {
        waiter(payload);
      } catch (_err) {
        // ignore waiter failures
      }
    }
  }
  updaterDownloadWaiters.delete(downloadId);
};

const waitForUpdaterDownload = async (downloadId, timeoutMs = 0) => {
  const id = String(downloadId || "").trim();
  if (!id) return { ok: false, error: "missing_download_id" };
  if (updaterDownloadResults.has(id)) {
    const done = updaterDownloadResults.get(id);
    updaterDownloadResults.delete(id);
    return done;
  }
  return new Promise((resolve) => {
    let timer = null;
    const done = (value) => {
      try {
        clearTimeout(timer);
      } catch (_err) {
        // ignore
      }
      updaterDownloadResults.delete(id);
      resolve(value && typeof value === "object" ? value : { ok: false, error: "download_failed" });
    };

    const list = updaterDownloadWaiters.get(id) || [];
    list.push(done);
    updaterDownloadWaiters.set(id, list);

    if (timeoutMs > 0) {
      timer = setTimeout(() => {
        const waiters = updaterDownloadWaiters.get(id) || [];
        updaterDownloadWaiters.set(id, waiters.filter((fn) => fn !== done));
        done({ ok: false, error: "download_timeout" });
      }, timeoutMs);
      if (typeof timer.unref === "function") timer.unref();
    }
  });
};

const startClientUpdaterDownload = async (options) => {
  const downloadUrl = String(options?.downloadUrl || options?.url || "").trim();
  if (!downloadUrl) return { ok: false, error: "missing_download_url" };
  let parsedUrl = null;
  try {
    parsedUrl = new URL(downloadUrl);
  } catch (_err) {
    return { ok: false, error: "invalid_download_url" };
  }
  if (!/^https?:$/i.test(parsedUrl.protocol)) {
    return { ok: false, error: "unsupported_download_protocol" };
  }
  if (activeUpdaterDownloads.size > 0) {
    return { ok: false, error: "download_in_progress" };
  }

  const expectedSha256 = normalizeSha256(options?.sha256);
  if (!expectedSha256) {
    return { ok: false, error: "missing_expected_hash" };
  }
  const expectedSignature = normalizeIpcString(options?.signature, 4096);
  const publicKeyPem = normalizeIpcString(
    options?.publicKey || process.env.SEKAILINK_UPDATE_PUBLIC_KEY || "",
    8192
  );
  if (expectedSignature && !publicKeyPem) {
    return { ok: false, error: "missing_signature_public_key" };
  }
  const hintedVersion = sanitizeFilename(String(options?.version || options?.latest || "").trim());
  const pathExt = sanitizeFilename(path.extname(parsedUrl.pathname || ""));
  const defaultBasename = hintedVersion ? `SekaiLink-client-${hintedVersion}${pathExt || ""}` : "SekaiLink-client-update";
  const authToken = String(options?.authToken || "").trim();
  const downloadId = `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
  const destDir = getClientUpdateDownloadsDir();
  const headers = {
    "User-Agent": "SekaiLink-Updater/1.0",
    "Accept": "application/zip,application/octet-stream,application/x-msdownload,application/x-iso9660-image,*/*",
  };
  if (authToken) headers.Authorization = `Bearer ${authToken}`;

  activeUpdaterDownloads.set(downloadId, { startedAt: Date.now() });
  emitUpdaterEvent({
    event: "download-started",
    downloadId,
    version: hintedVersion || "",
    status: "Downloading update...",
  });

  (async () => {
    try {
      const downloaded = await downloadToDirWithProgress(downloadUrl, destDir, {
        defaultBasename,
        headers,
        onProgress: ({ receivedBytes, totalBytes, percent }) => {
          emitUpdaterEvent({
            event: "download-progress",
            downloadId,
            receivedBytes,
            totalBytes,
            percent,
          });
        },
      });

      const actualSha256 = String(await hashFile(downloaded.path, "sha256")).toLowerCase();
      if (actualSha256 !== expectedSha256) {
        try {
          fs.rmSync(downloaded.path, { force: true });
        } catch (_err) {
          // ignore
        }
        throw new Error("checksum_mismatch");
      }

      if (expectedSignature) {
        const verified = verifyFileSignature(downloaded.path, expectedSignature, publicKeyPem);
        if (!verified.ok) {
          try {
            fs.rmSync(downloaded.path, { force: true });
          } catch (_err) {
            // ignore
          }
          throw new Error(verified.error || "signature_invalid");
        }
      }

      emitUpdaterEvent({
        event: "download-complete",
        downloadId,
        path: downloaded.path,
        filename: downloaded.filename,
        totalBytes: downloaded.totalBytes || 0,
        receivedBytes: downloaded.bytes || 0,
        sha256: actualSha256 || expectedSha256 || "",
      });
      settleUpdaterDownload(downloadId, {
        ok: true,
        downloadId,
        path: downloaded.path,
        filename: downloaded.filename,
        totalBytes: downloaded.totalBytes || 0,
        receivedBytes: downloaded.bytes || 0,
        sha256: actualSha256 || expectedSha256 || "",
      });
    } catch (err) {
      const message = String(err || "download_failed");
      emitUpdaterEvent({
        event: "download-error",
        downloadId,
        error: message,
      });
      settleUpdaterDownload(downloadId, { ok: false, downloadId, error: message });
    } finally {
      activeUpdaterDownloads.delete(downloadId);
    }
  })();

  return { ok: true, downloadId };
};

const runDetachedSelfUpdateAndQuit = async (options = {}) => {
  const sourcePath = String(options.sourcePath || "").trim();
  const targetPath = String(options.targetPath || "").trim();
  if (!sourcePath || !targetPath) return { ok: false, error: "missing_paths" };
  if (!fs.existsSync(sourcePath)) return { ok: false, error: "source_missing" };

  const samePath = path.resolve(sourcePath) === path.resolve(targetPath);
  if (samePath) {
    try {
      app.relaunch();
      setTimeout(() => app.quit(), 25);
      return { ok: true, method: "relaunch" };
    } catch (err) {
      return { ok: false, error: String(err || "relaunch_failed") };
    }
  }

  try {
    fs.accessSync(path.dirname(targetPath), fs.constants.W_OK);
  } catch (_err) {
    return { ok: false, error: "target_not_writable" };
  }

  if (process.platform === "win32") {
    try {
      // Windows builds are shipped as NSIS installers. Apply update silently to avoid user interaction,
      // then quit so NSIS can replace the installed app files.
      const installDir = path.dirname(targetPath);
      const quotePs = (value) => String(value || "").replace(/'/g, "''");
      // Delay the installer start to avoid "app is still running" detection and file locks.
      // Force /D to keep upgrades in-place (otherwise some NSIS configs can re-install elsewhere).
      const script = [
        `$installer='${quotePs(sourcePath)}'`,
        `$exe='${quotePs(targetPath)}'`,
        `$dir='${quotePs(installDir)}'`,
        "Start-Sleep -Milliseconds 800",
        "try {",
        "  Start-Process -FilePath $installer -ArgumentList @('/S', ('/D=' + $dir)) -Wait | Out-Null",
        "} catch {",
        "  exit 1",
        "}",
        "Start-Sleep -Milliseconds 250",
        "try { Start-Process -FilePath $exe | Out-Null } catch { }",
      ].join("; ");
      const proc = spawn("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script], {
        detached: true,
        stdio: "ignore",
        windowsHide: true,
      });
      proc.unref();
      setTimeout(() => app.quit(), 50);
      return { ok: true, method: "nsis-silent", installerPath: sourcePath, installDir };
    } catch (err) {
      return { ok: false, error: String(err || "nsis_silent_failed") };
    }
  }

  if (process.platform === "linux") {
    try {
      const script = [
        "src=\"$1\"",
        "dst=\"$2\"",
        "tmp=\"${dst}.new\"",
        "bak=\"${dst}.bak\"",
        "for i in $(seq 1 120); do",
        "  cp -f \"$src\" \"$tmp\" 2>/dev/null || { sleep 0.25; continue; }",
        "  chmod +x \"$tmp\" 2>/dev/null || { sleep 0.25; continue; }",
        "  if [ -f \"$dst\" ]; then cp -f \"$dst\" \"$bak\" 2>/dev/null || true; fi",
        "  if mv -f \"$tmp\" \"$dst\" 2>/dev/null; then",
        "    nohup \"$dst\" >/dev/null 2>&1 &",
        "    launch_rc=$?",
        "    if [ \"$launch_rc\" -eq 0 ]; then exit 0; fi",
        "    if [ -f \"$bak\" ]; then mv -f \"$bak\" \"$dst\" 2>/dev/null || true; fi",
        "  fi",
        "  sleep 0.25",
        "done",
        "exit 1",
      ].join("\n");
      const proc = spawn("/bin/bash", ["-lc", script, "--", sourcePath, targetPath], {
        detached: true,
        stdio: "ignore",
      });
      proc.unref();
      setTimeout(() => app.quit(), 50);
      return { ok: true, method: "self-replace", targetPath };
    } catch (err) {
      return { ok: false, error: String(err || "self_replace_failed") };
    }
  }

  return { ok: false, error: "platform_not_supported" };
};

const getRunningClientBinaryPath = () => {
  if (process.platform === "linux") {
    const appImage = String(process.env.APPIMAGE || "").trim();
    if (appImage && fs.existsSync(appImage)) return appImage;
  }
  return String(process.execPath || "").trim();
};

const getClientBundleInstallDir = () => {
  const installState = getBootstrapInstallState() || {};
  const explicitInstallDir = String(installState.installDir || process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim();
  if (explicitInstallDir && fs.existsSync(explicitInstallDir)) return explicitInstallDir;
  if (process.platform === "linux" && String(process.env.APPIMAGE || "").trim()) return "";
  const exePath = String(process.execPath || "").trim();
  return exePath ? path.dirname(exePath) : "";
};

const findClientBundleExecutable = (bundleRoot) => {
  const root = String(bundleRoot || "").trim();
  if (!root || !fs.existsSync(root)) return "";
  const candidates = process.platform === "win32"
    ? ["SekaiLink Client.exe", "SekaiLink-client.exe", "sekailink-client.exe"]
    : ["sekailink-client", "SekaiLink Client", "SekaiLink-client"];
  for (const candidate of candidates) {
    const full = path.join(root, candidate);
    if (fs.existsSync(full)) return candidate;
  }
  try {
    const entries = fs.readdirSync(root, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isFile()) continue;
      const lower = entry.name.toLowerCase();
      if (process.platform === "win32") {
        if (lower.endsWith(".exe") && lower.includes("sekailink")) return entry.name;
      } else if (lower.includes("sekailink")) {
        return entry.name;
      }
    }
  } catch (_err) {
    // ignore
  }
  return "";
};

const resolveClientBundleRoot = (extractDir) => {
  let root = String(extractDir || "").trim();
  if (!root || !fs.existsSync(root)) return "";
  try {
    const entries = fs.readdirSync(root, { withFileTypes: true }).filter((entry) => !entry.name.startsWith("__MACOSX"));
    if (entries.length === 1 && entries[0].isDirectory()) {
      root = path.join(root, entries[0].name);
    }
  } catch (_err) {
    return "";
  }
  return root;
};

const validateClientBundleRoot = (bundleRoot) => {
  const root = String(bundleRoot || "").trim();
  if (!root || !fs.existsSync(root)) return { ok: false, error: "bundle_root_missing" };
  if (!fs.existsSync(path.join(root, "resources", "app.asar"))) return { ok: false, error: "bundle_app_missing" };
  const executableRel = findClientBundleExecutable(root);
  if (!executableRel) return { ok: false, error: "bundle_executable_missing" };
  return { ok: true, executableRel };
};

const quoteShSingle = (value) => `'${String(value || "").replace(/'/g, "'\\''")}'`;

const runDetachedBundleSwapAndQuit = async ({ bundleRoot, installDir, executableRel, statePath }) => {
  const src = String(bundleRoot || "").trim();
  const dst = String(installDir || "").trim();
  const exeRel = String(executableRel || "").trim();
  const pendingState = String(statePath || "").trim();
  if (!src || !dst || !exeRel) return { ok: false, error: "missing_bundle_swap_args" };
  if (!fs.existsSync(src)) return { ok: false, error: "bundle_root_missing" };
  if (!fs.existsSync(dst)) return { ok: false, error: "install_dir_missing" };
  if (pendingState && !fs.existsSync(pendingState)) return { ok: false, error: "install_state_missing" };
  if (!isWritablePath(dst)) return { ok: false, error: "install_dir_not_writable" };

  if (process.platform === "win32") {
    const esc = (value) => String(value || "").replace(/'/g, "''");
    const script = [
      `$src='${esc(src)}'`,
      `$dst='${esc(dst)}'`,
      `$exeRel='${esc(exeRel)}'`,
      `$state='${esc(pendingState)}'`,
      "Start-Sleep -Milliseconds 900",
      "for ($i = 0; $i -lt 120; $i++) {",
      "  try {",
      "    Get-ChildItem -LiteralPath $dst -Force | Where-Object { $_.Name -ne '.sekailink' } | Remove-Item -Recurse -Force -ErrorAction Stop",
      "    Copy-Item -Path (Join-Path $src '*') -Destination $dst -Recurse -Force -ErrorAction Stop",
      "    if ($state -and (Test-Path -LiteralPath $state)) {",
      "      $stateDir = Join-Path $dst '.sekailink'",
      "      New-Item -ItemType Directory -Path $stateDir -Force | Out-Null",
      "      Copy-Item -LiteralPath $state -Destination (Join-Path $stateDir 'install-state.json') -Force -ErrorAction Stop",
      "    }",
      "    $exe = Join-Path $dst $exeRel",
      "    if (-not (Test-Path -LiteralPath $exe)) { throw 'updated_executable_missing' }",
      "    Start-Process -FilePath $exe | Out-Null",
      "    exit 0",
      "  } catch {",
      "    Start-Sleep -Milliseconds 250",
      "  }",
      "}",
      "exit 1",
    ].join("; ");
    const proc = spawn("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script], {
      detached: true,
      stdio: "ignore",
      windowsHide: true,
    });
    proc.unref();
    setTimeout(() => app.quit(), 50);
    return { ok: true, method: "client-bundle", installDir: dst };
  }

  if (process.platform === "linux") {
    const script = [
      `src=${quoteShSingle(src)}`,
      `dst=${quoteShSingle(dst)}`,
      `exe_rel=${quoteShSingle(exeRel)}`,
      `state=${quoteShSingle(pendingState)}`,
      "sleep 0.9",
      "for i in $(seq 1 120); do",
      "  if find \"$dst\" -mindepth 1 -maxdepth 1 ! -name .sekailink -exec rm -rf {} + 2>/dev/null && cp -a \"$src\"/. \"$dst\"/ 2>/dev/null; then",
      "    if [ -n \"$state\" ] && [ -f \"$state\" ]; then mkdir -p \"$dst/.sekailink\" && cp -f \"$state\" \"$dst/.sekailink/install-state.json\" 2>/dev/null || exit 1; fi",
      "    chmod +x \"$dst/$exe_rel\" 2>/dev/null || true",
      "    nohup \"$dst/$exe_rel\" >/dev/null 2>&1 &",
      "    exit 0",
      "  fi",
      "  sleep 0.25",
      "done",
      "exit 1",
    ].join("\n");
    const proc = spawn("/bin/bash", ["-lc", script], {
      detached: true,
      stdio: "ignore",
    });
    proc.unref();
    setTimeout(() => app.quit(), 50);
    return { ok: true, method: "client-bundle", installDir: dst };
  }

  return { ok: false, error: "platform_not_supported" };
};

const applyClientBundleUpdate = async (zipPath, options = {}) => {
  const p = String(zipPath || "").trim();
  if (!p || !fs.existsSync(p)) return { ok: false, error: "file_missing" };
  const installDir = getClientBundleInstallDir();
  if (!installDir) return { ok: false, error: "bundle_install_dir_unavailable" };
  const versionHint = sanitizeFilename(String(options?.version || options?.latest || Date.now()).trim()) || String(Date.now());
  const stagingRoot = path.join(getClientUpdateStagingDir(), `client-bundle-${versionHint}-${Date.now()}`);
  const extractDir = path.join(stagingRoot, "extract");
  try {
    emitUpdaterEvent({ event: "install-started", artifactType: "client-bundle", status: "Installing update..." });
    extractZip(p, extractDir);
    const bundleRoot = resolveClientBundleRoot(extractDir);
    const validated = validateClientBundleRoot(bundleRoot);
    if (!validated.ok) return validated;
    const installStatePath = path.join(stagingRoot, "install-state.json");
    const nextVersion = String(options?.version || options?.latest || "").trim();
    const nextChannel = String(options?.channel || "").trim().toLowerCase();
    const nextBuild = String(options?.build || "").trim().toLowerCase();
    fs.writeFileSync(installStatePath, JSON.stringify({
      version: nextVersion,
      manifestVersion: nextVersion,
      channel: nextChannel,
      build: nextBuild,
      platform: process.platform,
      arch: process.arch,
      artifactType: "client-bundle",
      installDir,
      updatedAt: nowIso(),
    }, null, 2));
    const applied = await runDetachedBundleSwapAndQuit({
      bundleRoot,
      installDir,
      executableRel: validated.executableRel,
      statePath: installStatePath,
    });
    if (applied?.ok) {
      emitUpdaterEvent({ event: "update-restarting", artifactType: "client-bundle", status: "Restarting..." });
      writeLogLine("info", "updater", `client bundle update scheduled installDir=${installDir}`);
      return { ok: true, ...applied };
    }
    return { ok: false, error: String(applied?.error || "bundle_apply_failed") };
  } catch (err) {
    writeLogLine("warn", "updater", `client bundle update failed: ${String(err || "")}`);
    return { ok: false, error: String(err || "bundle_apply_failed") };
  }
};

const openDownloadedUpdaterFile = async (targetPath, options = {}) => {
  const p = String(targetPath || "").trim();
  if (!p) return { ok: false, error: "missing_path" };
  if (!fs.existsSync(p)) return { ok: false, error: "file_missing" };
  const fileNameLower = path.basename(p).toLowerCase();
  const ext = path.extname(p).toLowerCase();
  const artifactType = normalizeArtifactType(options?.artifactType || options?.artifact_type || "");
  const isClientBundleZip = ext === ".zip" && artifactType === "client-bundle";
  const isLinuxAppImage = process.platform === "linux" && (ext === ".appimage" || fileNameLower.includes(".appimage"));
  const isWindowsExe = process.platform === "win32" && ext === ".exe";

  if (isClientBundleZip) {
    return applyClientBundleUpdate(p, options);
  }

  if (isLinuxAppImage || isWindowsExe) {
    try {
      if (isLinuxAppImage) {
        // Downloads are often non-executable by default; AppImage must be executable.
        const stat = fs.statSync(p);
        const nextMode = stat.mode | 0o111;
        if ((stat.mode & 0o111) !== 0o111) fs.chmodSync(p, nextMode);
      }
      const runningBinary = getRunningClientBinaryPath();
      if (!runningBinary) return { ok: false, error: "current_binary_missing" };
      const applied = await runDetachedSelfUpdateAndQuit({
        sourcePath: p,
        targetPath: runningBinary,
      });
      if (applied?.ok) {
        writeLogLine("info", "updater", `self-update scheduled target=${runningBinary}`);
        return { ok: true, ...applied };
      }
      writeLogLine("warn", "updater", `self-update failed: ${String(applied?.error || "")}`);
      return { ok: false, error: String(applied?.error || "self_update_failed") };
    } catch (err) {
      writeLogLine("warn", "updater", `self-update prep failed: ${String(err || "")}`);
      return { ok: false, error: String(err || "self_update_failed") };
    }
  }
  try {
    const err = await shell.openPath(p);
    if (err) return { ok: false, error: err };
    return { ok: true };
  } catch (error) {
    return { ok: false, error: String(error) };
  }
};

const downloadAndApplyClientUpdate = async (options = {}) => {
  // Safety guard: if the UI ends up with a stale embedded version string,
  // it might try to "update" to the version we are already running, causing a loop.
  try {
    const targetVersion = String(options?.version || "").trim();
    const runningVersion = app.getVersion ? String(app.getVersion() || "").trim() : "";
    if (targetVersion && runningVersion && targetVersion === runningVersion) {
      writeLogLine("info", "updater", `skip apply: already running version=${runningVersion}`);
      return { ok: false, error: "already_latest", version: runningVersion };
    }
  } catch (_err) {
    // ignore
  }

  const runOne = async (updateOptions = {}) => {
    const started = await startClientUpdaterDownload(updateOptions || {});
    if (!started?.ok) return started;
    const timeoutMs = Number(updateOptions?.timeoutMs || 25 * 60 * 1000);
    const completed = await waitForUpdaterDownload(started.downloadId, Number.isFinite(timeoutMs) ? timeoutMs : 0);
    if (!completed?.ok) {
      return { ok: false, error: completed?.error || "download_failed", downloadId: started.downloadId };
    }
    const apply = await openDownloadedUpdaterFile(completed.path, updateOptions || {});
    if (!apply?.ok) {
      return { ok: false, error: apply?.error || "apply_failed", downloadId: started.downloadId, path: completed.path };
    }
    return { ok: true, downloadId: started.downloadId, path: completed.path, ...apply };
  };

  const primary = await runOne(options || {});
  if (primary?.ok) return primary;

  const fallbackDownloadUrl = String(options?.fallbackDownloadUrl || options?.fallback_download_url || "").trim();
  const fallbackSha256 = normalizeSha256(options?.fallbackSha256 || options?.fallback_sha256 || "");
  if (!fallbackDownloadUrl || !fallbackSha256) return primary;

  emitUpdaterEvent({
    event: "download-fallback",
    error: String(primary?.error || "primary_update_failed"),
    status: "Trying fallback update...",
  });

  const fallbackOptions = {
    ...options,
    downloadUrl: fallbackDownloadUrl,
    url: fallbackDownloadUrl,
    sha256: fallbackSha256,
    signature: options?.fallbackSignature || options?.fallback_signature || "",
    publicKey: options?.fallbackPublicKey || options?.fallback_public_key || options?.publicKey || "",
    artifactType: options?.fallbackArtifactType || options?.fallback_artifact_type || "",
    artifact_type: options?.fallbackArtifactType || options?.fallback_artifact_type || "",
  };
  return runOne(fallbackOptions);
};

const normalizeSyncStateKey = (value) => {
  const raw = String(value || "").trim().toLowerCase();
  if (!raw) return "client";
  const safe = raw.replace(/[^a-z0-9._-]+/g, "-").replace(/^-+|-+$/g, "");
  return safe || "client";
};

const getIncrementalSyncStatePath = (stateKey = "client") => {
  const key = normalizeSyncStateKey(stateKey);
  if (key === "client") return path.join(getRuntimeOverlayRoot(), "client-sync-state.json");
  return path.join(getRuntimeOverlayRoot(), `client-sync-state-${key}.json`);
};

const loadIncrementalSyncState = (stateKey = "client") => {
  const statePath = getIncrementalSyncStatePath(stateKey);
  try {
    if (!fs.existsSync(statePath)) return { files: {} };
    const parsed = JSON.parse(fs.readFileSync(statePath, "utf-8"));
    if (parsed && typeof parsed === "object" && parsed.files && typeof parsed.files === "object") {
      return parsed;
    }
  } catch (_err) {
    // ignore malformed state
  }
  return { files: {} };
};

const saveIncrementalSyncState = (state, stateKey = "client") => {
  const statePath = getIncrementalSyncStatePath(stateKey);
  ensureDir(path.dirname(statePath));
  fs.writeFileSync(statePath, JSON.stringify(state, null, 2), "utf-8");
};

const normalizeClientRelativePath = (value) => {
  const raw = String(value || "").replace(/\\/g, "/").trim();
  if (!raw) return "";
  const noLead = raw.replace(/^\/+/, "");
  if (!noLead) return "";
  if (noLead.includes("..")) return "";
  if (noLead.startsWith("./")) return "";
  return noLead;
};

const joinUrlPath = (baseUrl, relativePath) => {
  try {
    return new URL(relativePath, baseUrl).toString();
  } catch (_err) {
    return "";
  }
};

const isIncrementalSyncPathAllowed = (relativePath) => {
  const rel = normalizeClientRelativePath(relativePath);
  if (!rel) return false;
  return rel.startsWith("runtime/") || rel.startsWith("ap/");
};

const getClientServerBaseUrl = () => {
  const state = getBootstrapInstallState() || {};
  const fromState = String(state?.serverUrl || "").trim();
  if (fromState) return fromState;
  const fromEnv = String(process.env.SEKAILINK_SERVER_URL || "").trim();
  if (fromEnv) return fromEnv;
  return "https://sekailink.com";
};

const getClientPlatformKey = () => {
  if (process.platform === "win32") return "windows";
  if (process.platform === "darwin") return "macos";
  return "linux";
};

const getClientReleaseTrack = () => {
  const state = getBootstrapInstallState() || {};
  const channel = String(state?.channel || "stable").trim().toLowerCase() || "stable";
  const build = String(state?.build || state?.buildType || "release").trim().toLowerCase() || "release";
  return { channel, build };
};

const getRuntimePackApiUrl = (params = {}) => {
  const base = getClientServerBaseUrl();
  const url = new URL("/api/client/runtime-pack-manifest", base);
  url.searchParams.set("platform", getClientPlatformKey());
  const track = getClientReleaseTrack();
  url.searchParams.set("channel", track.channel);
  url.searchParams.set("build", track.build);
  if (params.moduleId) url.searchParams.set("module_id", String(params.moduleId));
  if (params.gameId) url.searchParams.set("game_id", String(params.gameId));
  return url.toString();
};

const getRuntimePackStaticIndexUrl = () => {
  return new URL("/runtime-packs/index.json", getClientServerBaseUrl()).toString();
};

const runtimePackMatchesModule = (pack, moduleId, gameId = "") => {
  const safeModuleId = normalizeGameId(moduleId || "");
  const safeGameId = normalizeGameId(gameId || "");
  const packModuleId = normalizeGameId(pack?.moduleId || pack?.module_id || "");
  const packGameId = normalizeGameId(pack?.gameId || pack?.game_id || "");
  if (packModuleId && packModuleId !== safeModuleId) return false;
  if (safeGameId && packGameId && packGameId !== safeGameId) return false;
  return true;
};

const normalizeRuntimePackEntry = (raw, apiBaseUrl = "") => {
  const row = raw && typeof raw === "object" ? raw : {};
  const id = normalizeGameId(row.id || row.pack_id || row.key || "");
  const required = row.required !== false;
  const stateKey = normalizeSyncStateKey(row.state_key || row.stateKey || id || "pack");
  let manifestUrl = String(row.manifest_url || row.manifestUrl || row.url || "").trim();
  if (manifestUrl && !/^https?:\/\//i.test(manifestUrl)) {
    manifestUrl = joinUrlPath(apiBaseUrl || getClientServerBaseUrl(), manifestUrl);
  }
  if (!id || !manifestUrl) return null;
  return {
    id,
    kind: normalizeGameId(row.kind || row.type || "runtime-pack") || "runtime-pack",
    moduleId: normalizeGameId(row.module_id || row.moduleId || ""),
    gameId: normalizeGameId(row.game_id || row.gameId || ""),
    required,
    manifestUrl,
    stateKey,
    name: String(row.name || row.label || id),
  };
};

const parseRuntimePackPayload = (payload, baseUrl, moduleId, gameId = "") => {
  const packsRaw = Array.isArray(payload?.packs) ? payload.packs : [];
  const packs = [];
  for (const raw of packsRaw) {
    if (!runtimePackMatchesModule(raw, moduleId, gameId)) continue;
    const normalized = normalizeRuntimePackEntry(raw, baseUrl);
    if (normalized) packs.push(normalized);
  }
  return packs;
};

const fetchRuntimePacksFromStaticIndex = async (moduleId, gameId = "") => {
  const indexUrl = getRuntimePackStaticIndexUrl();
  const payload = await httpGetJson(indexUrl, { accept: "application/json" });
  const packs = parseRuntimePackPayload(payload, indexUrl, moduleId, gameId);
  return { apiUrl: indexUrl, packs, source: "static-index" };
};

const fetchRuntimePacksForModule = async (moduleId, gameId = "") => {
  const apiUrl = getRuntimePackApiUrl({ moduleId, gameId });
  try {
    const payload = await httpGetJson(apiUrl, { accept: "application/json" });
    const packs = parseRuntimePackPayload(payload, apiUrl, moduleId, gameId);
    return { apiUrl, packs, source: "client-api" };
  } catch (err) {
    const detail = String(err || "");
    if (!/HTTP\s+404\b/.test(detail)) throw err;
    writeLogLine("info", "runtime-pack", `client API pack manifest missing; trying static index module=${normalizeGameId(moduleId || "")}`);
    return fetchRuntimePacksFromStaticIndex(moduleId, gameId);
  }
};

const ensureRuntimePacksForModule = async ({ moduleId, gameId = "" } = {}) => {
  const safeModuleId = normalizeGameId(moduleId || "");
  if (!safeModuleId) return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };
  let packs = [];
  try {
    const fetched = await fetchRuntimePacksForModule(safeModuleId, gameId);
    packs = Array.isArray(fetched?.packs) ? fetched.packs : [];
  } catch (err) {
    const detail = String(err || "");
    if (/HTTP\s+404\b/.test(detail)) {
      writeLogLine("info", "runtime-pack", `no remote runtime packs module=${safeModuleId}`);
      return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };
    }
    writeLogLine("warn", "runtime-pack", `manifest fetch failed module=${safeModuleId} detail=${detail}`);
    // Non-blocking if no pack metadata is available.
    return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0, warning: "pack_manifest_unavailable" };
  }
  if (!packs.length) return { ok: true, packs: [], applied: 0, skipped: 0, failed: 0 };

  let applied = 0;
  let skipped = 0;
  let failed = 0;

  for (const pack of packs) {
    const isTrackerPack = String(pack.kind || "").includes("tracker");
    const checkingStatus = isTrackerPack
      ? `Checking tracker pack: ${pack.name}...`
      : `Checking runtime pack: ${pack.name}...`;
    const downloadingStatus = isTrackerPack
      ? `Downloading tracker pack: ${pack.name}...`
      : `Downloading runtime pack: ${pack.name}...`;
    emitSessionEvent({
      event: "status",
      status: checkingStatus,
      moduleId: safeModuleId,
      packId: pack.id,
    });
    const stateKey = pack.stateKey || `pack-${pack.id}`;
    const syncRes = await syncIncrementalClientFiles({
      manifestUrl: pack.manifestUrl,
      stateKey,
      pruneMissing: true,
      downloadStatus: downloadingStatus,
      downloadStatusContext: {
        moduleId: safeModuleId,
        packId: pack.id,
        packKind: pack.kind,
      },
    });
    if (syncRes?.ok) {
      applied += 1;
      continue;
    }
    failed += 1;
    writeLogLine(
      pack.required ? "error" : "warn",
      "runtime-pack",
      `pack sync failed module=${safeModuleId} pack=${pack.id} required=${pack.required} error=${String(syncRes?.error || "")}`
    );
    if (pack.required) {
      return {
        ok: false,
        error: "runtime_pack_sync_failed",
        moduleId: safeModuleId,
        packId: pack.id,
        detail: String(syncRes?.error || ""),
        packs,
        applied,
        skipped,
        failed,
      };
    }
    skipped += 1;
  }

  return { ok: true, packs, applied, skipped, failed };
};

const syncIncrementalClientFiles = async (options) => {
  const manifestUrl = String(options?.manifestUrl || options?.url || "").trim();
  if (!manifestUrl) return { ok: false, error: "missing_manifest_url" };
  let parsedManifestUrl = null;
  try {
    parsedManifestUrl = new URL(manifestUrl);
  } catch (_err) {
    return { ok: false, error: "invalid_manifest_url" };
  }
  if (!/^https?:$/i.test(parsedManifestUrl.protocol)) {
    return { ok: false, error: "unsupported_manifest_protocol" };
  }
  const stateKey = normalizeSyncStateKey(options?.stateKey || "client");
  const syncKey = `incremental-sync:${stateKey}`;
  if (activeUpdaterDownloads.has(syncKey)) {
    return { ok: false, error: "sync_in_progress" };
  }

  const authToken = String(options?.authToken || "").trim();
  const manifestHeaders = {
    "User-Agent": "SekaiLink-IncrementalUpdater/1.0",
    "Accept": "application/json",
  };
  const fileHeaders = {
    "User-Agent": "SekaiLink-IncrementalUpdater/1.0",
    "Accept": "application/octet-stream,*/*",
  };
  if (authToken) {
    manifestHeaders.Authorization = `Bearer ${authToken}`;
    fileHeaders.Authorization = `Bearer ${authToken}`;
  }

  activeUpdaterDownloads.set(syncKey, { startedAt: Date.now() });
  emitUpdaterEvent({
    event: "incremental-sync-started",
    manifestUrl,
    stateKey,
  });

  try {
    const manifest = await httpGetJson(manifestUrl, { headers: manifestHeaders, accept: "application/json" });
    const files = Array.isArray(manifest?.files) ? manifest.files : [];
    const baseUrl = String(manifest?.base_url || "").trim() || manifestUrl;
    const manifestVersion = String(manifest?.version || "").trim();
    const overlayRoot = getRuntimeOverlayRoot();
    ensureDir(overlayRoot);

    const prevState = loadIncrementalSyncState(stateKey);
    const nextState = {
      version: manifestVersion,
      updatedAt: nowIso(),
      files: {},
    };
    const pruneMissing = options?.pruneMissing !== false && manifest?.prune !== false;

    let processed = 0;
    let changed = 0;
    let deleted = 0;
    let downloadedBytes = 0;
    let downloadStatusEmitted = false;
    const total = files.length;

    for (const rawEntry of files) {
      const entry = rawEntry && typeof rawEntry === "object" ? rawEntry : {};
      const relPath = normalizeClientRelativePath(entry.path || entry.relative_path || "");
      const sha256 = normalizeSha256(entry.sha256);
      if (!relPath || !sha256 || !isIncrementalSyncPathAllowed(relPath)) {
        continue;
      }
      processed += 1;
      const targetPath = path.join(overlayRoot, relPath);
      const itemUrl = String(entry.url || "").trim() || joinUrlPath(baseUrl, relPath);
      if (!itemUrl) continue;

      let needsDownload = true;
      const priorHash = String(prevState?.files?.[relPath] || "").toLowerCase();
      if (priorHash === sha256 && fs.existsSync(targetPath)) {
        needsDownload = false;
      } else if (fs.existsSync(targetPath)) {
        try {
          const existingHash = String(await hashFile(targetPath, "sha256")).toLowerCase();
          if (existingHash === sha256) needsDownload = false;
        } catch (_err) {
          needsDownload = true;
        }
      }

      if (needsDownload) {
        if (!downloadStatusEmitted && options?.downloadStatus) {
          downloadStatusEmitted = true;
          emitSessionEvent({
            event: "status",
            status: String(options.downloadStatus),
            ...(options.downloadStatusContext && typeof options.downloadStatusContext === "object"
              ? options.downloadStatusContext
              : {}),
          });
        }
        emitUpdaterEvent({
          event: "incremental-sync-file",
          status: "downloading",
          path: relPath,
          processed,
          total,
        });
        const tmpDir = path.join(getRuntimeDownloadsDir(), "client-sync");
        const downloaded = await downloadToDirWithProgress(itemUrl, tmpDir, {
          headers: fileHeaders,
          defaultBasename: path.basename(relPath) || "sync.bin",
        });
        const downloadedHash = String(await hashFile(downloaded.path, "sha256")).toLowerCase();
        if (downloadedHash !== sha256) {
          try {
            fs.rmSync(downloaded.path, { force: true });
          } catch (_err) {
            // ignore
          }
          throw new Error(`incremental_checksum_mismatch:${relPath}`);
        }
        ensureDir(path.dirname(targetPath));
        fs.copyFileSync(downloaded.path, targetPath);
        try {
          fs.rmSync(downloaded.path, { force: true });
        } catch (_err) {
          // ignore
        }
        changed += 1;
        downloadedBytes += Number(downloaded.bytes || 0);
      }

      nextState.files[relPath] = sha256;
      emitUpdaterEvent({
        event: "incremental-sync-file",
        status: needsDownload ? "updated" : "unchanged",
        path: relPath,
        processed,
        total,
      });
    }

    if (pruneMissing) {
      const prevFiles = prevState?.files && typeof prevState.files === "object" ? prevState.files : {};
      for (const relPath of Object.keys(prevFiles)) {
        if (nextState.files[relPath]) continue;
        const targetPath = path.join(overlayRoot, normalizeClientRelativePath(relPath));
        try {
          if (targetPath && fs.existsSync(targetPath)) {
            fs.rmSync(targetPath, { force: true });
            deleted += 1;
            emitUpdaterEvent({
              event: "incremental-sync-file",
              status: "deleted",
              path: relPath,
              processed,
              total,
              stateKey,
            });
          }
        } catch (_err) {
          // ignore cleanup errors
        }
      }
    }

    saveIncrementalSyncState(nextState, stateKey);
    _romIndexCache = null;
    _patchIndexCache = null;
    // Incremental sync can update tool runtimes under runtime/. Clear any process-lifetime caches
    // so subsequent launches restage from the updated overlay without requiring a client restart.
    _popTrackerExeCache = "";
    _popTrackerExeSource = "";
    _pythonBootstrapCache = null;
    emitUpdaterEvent({
      event: "incremental-sync-complete",
      changed,
      deleted,
      processed,
      total,
      downloadedBytes,
      stateKey,
    });
    return { ok: true, changed, deleted, processed, total, downloadedBytes };
  } catch (err) {
    const message = String(err || "incremental_sync_failed");
    emitUpdaterEvent({
      event: "incremental-sync-error",
      error: message,
      stateKey,
    });
    return { ok: false, error: message };
  } finally {
    activeUpdaterDownloads.delete(syncKey);
  }
};

const findPathByBasename = (rootDir, wantedLower, maxDepth = 6) => {
  const wanted = String(wantedLower || "").toLowerCase();
  if (!wanted || !rootDir || !fs.existsSync(rootDir)) return "";

  const queue = [{ dir: rootDir, depth: 0 }];
  while (queue.length) {
    const cur = queue.shift();
    if (!cur) break;
    if (cur.depth > maxDepth) continue;

    let entries = [];
    try {
      entries = fs.readdirSync(cur.dir, { withFileTypes: true });
    } catch (_err) {
      continue;
    }

    for (const entry of entries) {
      const full = path.join(cur.dir, entry.name);
      if (entry.isFile()) {
        if (entry.name.toLowerCase() === wanted) return full;
      } else if (entry.isDirectory()) {
        queue.push({ dir: full, depth: cur.depth + 1 });
      }
    }
  }
  return "";
};

const httpGetJson = (url, options = {}) => {
  return new Promise((resolve, reject) => {
    const client = url.startsWith("https://") ? https : http;
    const headers = {
      "User-Agent": "SekaiLink",
      "Accept": "application/vnd.github+json",
      ...sanitizeHeaders(options.headers || {}),
    };
    if (options.accept) {
      headers.Accept = String(options.accept);
    }
    const req = client.get(url, {
      headers
    }, (res) => {
      if (res.statusCode && res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        const redirected = resolveRedirectUrl(url, res.headers.location);
        res.resume();
        return resolve(httpGetJson(redirected, options));
      }
      let data = "";
      res.on("data", (chunk) => {
        data += String(chunk);
      });
      res.on("end", () => {
        if (res.statusCode && res.statusCode >= 400) {
          return reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
        }
        try {
          resolve(JSON.parse(data));
        } catch (err) {
          reject(err);
        }
      });
    });
    req.on("error", reject);
  });
};

const ensureGithubReleaseZipInstalled = async (toolId, repo, assetRegex) => {
  const id = String(toolId || "").trim();
  if (!id) throw new Error("missing_tool_id");
  const baseDir = path.join(getRuntimeToolsDir(), id);
  ensureDir(baseDir);

  const markerPath = path.join(baseDir, "install.json");
  if (fs.existsSync(markerPath)) {
    try {
      const marker = JSON.parse(fs.readFileSync(markerPath, "utf-8"));
      const rootDir = String(marker?.rootDir || "");
      if (rootDir && fs.existsSync(rootDir)) return rootDir;
    } catch (_err) {
      // ignore marker parse errors
    }
  }

  const resolvedRepo = resolveGithubRepo(repo) || repo;
  if (!resolvedRepo) throw new Error("invalid_repo");
  const assetInfo = await resolveGithubAssetInfo(resolvedRepo, assetRegex || "\\.zip$");
  if (!assetInfo?.url) throw new Error("missing_release_asset");
  if (!assetInfo.expectedSha256) throw new Error("missing_release_asset_hash");
  const downloadUrl = assetInfo.url;

  const zipName = path.basename(new URL(downloadUrl).pathname || `${id}.zip`);
  const zipPath = path.join(baseDir, zipName);
  await downloadToFile(downloadUrl, zipPath, {
    expectedSha256: assetInfo.expectedSha256,
    requireHash: true,
  });

  const extractDir = path.join(baseDir, "extract");
  if (fs.existsSync(extractDir)) fs.rmSync(extractDir, { recursive: true, force: true });
  extractZip(zipPath, extractDir);
  const rootDir = resolvePackRoot(extractDir);

  fs.writeFileSync(markerPath, JSON.stringify({
    toolId: id,
    repo: resolvedRepo,
    downloadUrl,
    sha256: assetInfo.expectedSha256,
    installed_at: new Date().toISOString(),
    rootDir,
  }, null, 2));

  return rootDir;
};

const backupAndReplaceFile = (srcPath, destPath) => {
  ensureDir(path.dirname(destPath));
  if (fs.existsSync(destPath)) {
    const ts = new Date().toISOString().replace(/[:.]/g, "-");
    const bak = `${destPath}.bak.${ts}`;
    try {
      fs.renameSync(destPath, bak);
    } catch (_err) {
      // If rename fails (cross-device, perms), try a copy + unlink.
      try {
        fs.copyFileSync(destPath, bak);
        fs.unlinkSync(destPath);
      } catch (_err2) {
        // ignore
      }
    }
  }
  fs.copyFileSync(srcPath, destPath);
};

const getFactorioModsDir = () => {
  const cfg = readConfig();
  const overrides = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
  const factorio = overrides.factorio && typeof overrides.factorio === "object" ? overrides.factorio : {};
  if (typeof factorio.mods_dir === "string" && factorio.mods_dir.trim()) {
    return factorio.mods_dir.trim();
  }

  // Default per-platform locations.
  if (process.platform === "win32") {
    return path.join(app.getPath("appData"), "Factorio", "mods");
  }
  // Linux/macOS default: ~/.factorio/mods
  return path.join(app.getPath("home"), ".factorio", "mods");
};

const tryLaunchFactorio = () => {
  const cfg = readConfig();
  const overrides = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
  const factorio = overrides.factorio && typeof overrides.factorio === "object" ? overrides.factorio : {};

  const exeOverride = typeof factorio.exe_path === "string" ? factorio.exe_path.trim() : "";
  const exePath = exeOverride || which("factorio") || "";
  if (exePath && fs.existsSync(exePath)) {
    try {
      const wrapped = spawnMaybeGamescope(exePath, [], { stdio: "ignore" });
      if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
      nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
      wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
      return { ok: true, pid: wrapped.proc.pid, method: "exe" };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  }

  const steam = which("steam");
  if (steam) {
    try {
      const proc = spawn(steam, ["-applaunch", "427520"], { stdio: "ignore" });
      nativeGameProcs.set(proc.pid, proc);
      proc.on("exit", () => nativeGameProcs.delete(proc.pid));
      return { ok: true, pid: proc.pid, method: "steam" };
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  }

  return { ok: false, error: "factorio_not_found" };
};

const getGzDoomSettings = () => {
  const cfg = readConfig();
  const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
  const gzdoom = games.gzdoom && typeof games.gzdoom === "object" ? games.gzdoom : {};
  return {
    exe_path: typeof gzdoom.exe_path === "string" ? gzdoom.exe_path.trim() : "",
    iwad_path: typeof gzdoom.iwad_path === "string" ? gzdoom.iwad_path.trim() : "",
    gzap_pk3_path: typeof gzdoom.gzap_pk3_path === "string" ? gzdoom.gzap_pk3_path.trim() : "",
    args: Array.isArray(gzdoom.args) ? gzdoom.args.map((v) => String(v)) : [],
  };
};

const getSm64ExSettings = () => {
  const cfg = readConfig();
  const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
  const sm64ex = games.sm64ex && typeof games.sm64ex === "object" ? games.sm64ex : {};
  return {
    // Path to the compiled game binary (recommended).
    exe_path: typeof sm64ex.exe_path === "string" ? sm64ex.exe_path.trim() : "",
    // Optional folder to search for a compiled build (fallback).
    root_dir: typeof sm64ex.root_dir === "string" ? sm64ex.root_dir.trim() : "",
    // Extra args for the binary (one per string).
    args: Array.isArray(sm64ex.args) ? sm64ex.args.map((v) => String(v)) : [],
  };
};

const getSohSettings = () => {
  const cfg = readConfig();
  const games = cfg.games && typeof cfg.games === "object" ? cfg.games : {};
  const soh = games.oot_soh && typeof games.oot_soh === "object" ? games.oot_soh : {};
  return {
    // Path to the Ship of Harkinian executable (recommended).
    exe_path: typeof soh.exe_path === "string" ? soh.exe_path.trim() : "",
    // Optional folder to search for a build/download (fallback).
    root_dir: typeof soh.root_dir === "string" ? soh.root_dir.trim() : "",
    // Auto-download latest AP-integrated release if executable is missing.
    auto_install: typeof soh.auto_install === "boolean" ? soh.auto_install : true,
    // Extra args for the binary (one per string).
    args: Array.isArray(soh.args) ? soh.args.map((v) => String(v)) : [],
  };
};

const findSohExecutableInDir = (rootDir) => {
  if (!rootDir || !fs.existsSync(rootDir)) return "";
  const exactCandidates = [
    "soh",
    "soh.exe",
    "soh.appimage",
    "ship.of.harkinian",
    "ship.of.harkinian.exe",
    "ship.of.harkinian.appimage",
    "ship of harkinian",
    "ship of harkinian.exe",
    "ship of harkinian.appimage",
  ];
  for (const candidate of exactCandidates) {
    const hit = findPathByBasename(rootDir, candidate, 8);
    if (hit) return hit;
  }

  const queue = [{ dir: rootDir, depth: 0 }];
  while (queue.length) {
    const cur = queue.shift();
    if (!cur) break;
    if (cur.depth > 8) continue;
    let entries = [];
    try {
      entries = fs.readdirSync(cur.dir, { withFileTypes: true });
    } catch (_err) {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(cur.dir, entry.name);
      if (entry.isDirectory()) {
        queue.push({ dir: full, depth: cur.depth + 1 });
        continue;
      }
      if (!entry.isFile()) continue;
      const lower = entry.name.toLowerCase();
      const maybeExec =
        lower.endsWith(".appimage") ||
        lower.endsWith(".exe") ||
        (!lower.includes(".") && lower.length > 0);
      if (!maybeExec) continue;
      if (lower.includes("soh") || lower.includes("harkinian")) {
        return full;
      }
    }
  }
  return "";
};

const tryLaunchSoh = async (options = {}) => {
  const settings = getSohSettings();
  const manifest = getModuleManifest("oot_soh") || {};
  const launcherRepo =
    resolveGithubRepo(String(manifest.launcher_repo || "").trim()) || "HarbourMasters/Archipelago-SoH";
  const launcherAssetRegex =
    String(manifest.launcher_asset_regex || "").trim() ||
    "Ship\\.of\\.Harkinian\\.Archipelago\\..*\\.linux\\.zip$";
  const exeOverride = settings.exe_path;
  let rootDir = options.rootDir || settings.root_dir;
  let exePath = "";

  if (exeOverride && fs.existsSync(exeOverride)) {
    exePath = exeOverride;
  } else {
    if (rootDir && fs.existsSync(rootDir)) {
      exePath = findSohExecutableInDir(rootDir);
    }
    if (!exePath && settings.auto_install) {
      try {
        rootDir = await ensureGithubReleaseZipInstalled(
          "oot_soh",
          launcherRepo,
          launcherAssetRegex
        );
        exePath = findSohExecutableInDir(rootDir);
      } catch (err) {
        return { ok: false, error: "soh_install_failed", detail: String(err || "") };
      }
    }
  }

  if (!exePath || !fs.existsSync(exePath)) {
    return { ok: false, error: "soh_not_found" };
  }

  try {
    fs.chmodSync(exePath, 0o755);
  } catch (_err) {
    // ignore chmod failures
  }

  try {
    const wrapped = spawnMaybeGamescope(exePath, settings.args || [], { stdio: "ignore" });
    if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
    nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
    wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
    return { ok: true, pid: wrapped.proc.pid, method: "exe", exePath };
  } catch (err) {
    return { ok: false, error: "soh_launch_failed", detail: String(err || "") };
  }
};

const getSekaiemuSettings = () => {
  const config = readConfig();
  const games = config.games && typeof config.games === "object" ? config.games : {};
  const sekaiemu = games.sekaiemu && typeof games.sekaiemu === "object" ? games.sekaiemu : {};
  return {
    exe_path: String(sekaiemu.exe_path || process.env.SEKAILINK_SEKAIEMU_PATH || "").trim(),
    root_dir: String(sekaiemu.root_dir || process.env.SEKAILINK_SEKAIEMU_ROOT || "").trim(),
    core_dirs: Array.isArray(sekaiemu.core_dirs) ? sekaiemu.core_dirs.map((entry) => String(entry || "").trim()).filter(Boolean) : [],
    system_dir: String(sekaiemu.system_dir || "").trim(),
    save_dir: String(sekaiemu.save_dir || "").trim(),
    log_dir: String(sekaiemu.log_dir || "").trim(),
    args: Array.isArray(sekaiemu.args) ? sekaiemu.args.map((entry) => String(entry || "").trim()).filter(Boolean) : [],
  };
};

const fileExists = (filePath) => {
  const safe = String(filePath || "").trim();
  if (!safe) return false;
  try {
    return fs.existsSync(safe) && fs.statSync(safe).isFile();
  } catch (_err) {
    return false;
  }
};

const dirExists = (dirPath) => {
  const safe = String(dirPath || "").trim();
  if (!safe) return false;
  try {
    return fs.existsSync(safe) && fs.statSync(safe).isDirectory();
  } catch (_err) {
    return false;
  }
};

const pathExists = (targetPath) => {
  const safe = String(targetPath || "").trim();
  if (!safe) return false;
  try {
    return fs.existsSync(safe);
  } catch (_err) {
    return false;
  }
};

const findExecutableByNamesInDir = (rootDir, names = [], maxDepth = 6) => {
  const root = String(rootDir || "").trim();
  if (!root) return "";
  try {
    if (!fs.existsSync(root) || !fs.statSync(root).isDirectory()) return "";
  } catch (_err) {
    return "";
  }
  const wanted = new Set(names.map((name) => String(name || "").toLowerCase()).filter(Boolean));
  const queue = [{ dir: root, depth: 0 }];
  while (queue.length) {
    const cur = queue.shift();
    if (!cur || cur.depth > maxDepth) continue;
    let entries = [];
    try {
      entries = fs.readdirSync(cur.dir, { withFileTypes: true });
    } catch (_err) {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(cur.dir, entry.name);
      if (entry.isDirectory()) {
        queue.push({ dir: full, depth: cur.depth + 1 });
      } else if (entry.isFile() && wanted.has(entry.name.toLowerCase())) {
        return full;
      }
    }
  }
  return "";
};

const resolveSekaiemuExecutable = () => {
  const settings = getSekaiemuSettings();
  const exeNames = process.platform === "win32"
    ? ["sekaiemu_libretro_spike.exe", "sekaiemu.exe"]
    : ["sekaiemu_libretro_spike", "sekaiemu"];
  const home = app.getPath("home");
  const runtimeBinCandidates = exeNames.flatMap((exeName) => [
    getRuntimePlatformPath("bin", exeName),
    path.join(getBundledRuntimeDir(), "bin", exeName),
    path.join(getBundledRuntimeDir(), "sekaiemu", exeName),
  ]);
  const devCandidates = app.isPackaged ? [] : [
    path.join("/tmp", "sekaiemu-libretro-spike-beta3-build", exeNames[0]),
    path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike", "build", exeNames[0]),
    path.join(home, "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike", "build", exeNames[0]),
  ];
  const candidates = [
    settings.exe_path,
    ...runtimeBinCandidates,
    ...devCandidates,
  ];
  for (const candidate of candidates) {
    if (fileExists(candidate)) return candidate;
  }
  const roots = [
    settings.root_dir,
    getRuntimePlatformPath("sekaiemu"),
    path.join(getBundledRuntimeDir(), "sekaiemu"),
    ...(app.isPackaged ? [] : [
      path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike"),
    ]),
  ].filter(Boolean);
  for (const root of roots) {
    const found = findExecutableByNamesInDir(root, exeNames, 5);
    if (found) return found;
  }
  return "";
};

const resolvePathFromRoots = (candidate, roots = [], existsPredicate = fileExists) => {
  const safe = String(candidate || "").trim();
  if (!safe) return "";
  if (path.isAbsolute(safe) && existsPredicate(safe)) return safe;
  for (const root of roots) {
    const base = String(root || "").trim();
    if (!base) continue;
    const resolved = path.join(base, safe);
    if (existsPredicate(resolved)) return resolved;
  }
  return "";
};

const resolveSekaiemuCorePath = (manifest = {}) => {
  const settings = getSekaiemuSettings();
  const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
  const corePath = String(sekaiemu.core_path || manifest.libretro_core_path || "").trim();
  const coreId = String(sekaiemu.core_id || manifest.libretro_core_id || manifest?.driver?.core_id || "").trim();
  const home = app.getPath("home");
  const roots = [
    ...settings.core_dirs,
    getRuntimePlatformPath("cores"),
    path.join(getBundledRuntimeDir(), "cores"),
    path.join(getBundledRuntimeDir(), "libretro"),
    ...(app.isPackaged ? [] : [
      path.join(home, "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "src"),
    ]),
  ];
  const direct = resolvePathFromRoots(corePath, roots);
  if (direct) return direct;

  const ext = process.platform === "win32" ? ".dll" : ".so";
  const names = (...baseNames) => baseNames.map((name) => `${name}${ext}`);
  const aliases = {
    bsnes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "bsnes_libretro", "snes9x_libretro"),
    snes: names("bsnes_mercury_performance_libretro", "bsnes_mercury_balanced_libretro", "snes9x_libretro"),
    gba: names("mgba_libretro"),
    mgba: names("mgba_libretro"),
    nes: names("fceumm_libretro", "nestopia_libretro"),
    fceumm: names("fceumm_libretro"),
  };
  const wanted = aliases[coreId.toLowerCase()] || (coreId ? [coreId] : []);
  if (!wanted.length) return "";
  for (const root of roots) {
    const found = findExecutableByNamesInDir(root, wanted, 5);
    if (found) return found;
  }
  return "";
};

const parseArchipelagoAddress = (address) => {
  const raw = String(address || "").trim();
  if (!raw) return null;
  try {
    const url = new URL(raw.includes("://") ? raw : `ws://${raw}`);
    const port = Number(url.port || (url.protocol === "wss:" ? 443 : 0));
    return {
      host: url.hostname,
      port: Number.isFinite(port) ? port : 0,
      path: url.pathname && url.pathname !== "" ? url.pathname : "/",
    };
  } catch (_err) {
    const [host, portText] = raw.split(":");
    const port = Number(portText || 0);
    if (!host || !Number.isFinite(port)) return null;
    return { host, port, path: "/" };
  }
};

const resolveSklmiRuntimeForSekaiemu = () => {
  const home = app.getPath("home");
  const runtimeDir = getBundledRuntimeDir();
  const exeName = process.platform === "win32" ? "sekailink_sklmi_runtime.exe" : "sekailink_sklmi_runtime";
  const devCandidates = app.isPackaged ? [] : [
    path.join(home, "DevSSD", "sekailink-beta-3-final", "clean-room", "repos", "sklmi", "build", exeName),
    path.join(home, "DevSSD", "sekailink-beta-3-final", "sklmi", "build", exeName),
  ];
  const candidates = [
    process.env.SEKAILINK_SKLMI_RUNTIME,
    getRuntimePlatformPath("bin", exeName),
    path.join(runtimeDir, "bin", exeName),
    path.join(runtimeDir, "sklmi", exeName),
    path.join(runtimeDir, exeName),
    ...devCandidates,
  ];
  for (const candidate of candidates) {
    if (fileExists(candidate)) return candidate;
  }
  return "";
};

const resolveSklmiManifestDirForSekaiemu = () => {
  const home = app.getPath("home");
  const runtimeDir = getBundledRuntimeDir();
  const devCandidates = app.isPackaged ? [] : [
    path.join(home, "DevSSD", "sekailink-beta-3-final", "clean-room", "repos", "sklmi", "manifests"),
    path.join(home, "DevSSD", "sekailink-beta-3-final", "sklmi", "manifests"),
  ];
  const candidates = [
    process.env.SEKAILINK_SKLMI_MANIFEST_DIR,
    getRuntimePlatformPath("sklmi", "manifests"),
    path.join(runtimeDir, "sklmi", "manifests"),
    path.join(runtimeDir, "manifests", "sklmi"),
    path.join(runtimeDir, "manifests"),
    ...devCandidates,
  ];
  for (const candidate of candidates) {
    try {
      if (candidate && fs.existsSync(candidate) && fs.statSync(candidate).isDirectory()) return candidate;
    } catch (_err) {
      // keep scanning
    }
  }
  return "";
};

const resolveSekaiemuTrackerPackPath = (manifest = {}, roots = []) => {
  const gameId = String(manifest.game_id || manifest.gameId || "").trim();
  const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
  if (gameId) {
    const installed = getInstalledTrackerPack(gameId);
    if (installed?.path && pathExists(installed.path)) return String(installed.path);
  }
  const declared = String(
    sekaiemu.tracker_pack ||
      sekaiemu.tracker_pack_path ||
      manifest.tracker_pack_path ||
      ""
  ).trim();
  if (declared && !path.isAbsolute(declared)) {
    const runtimePath = getRuntimeFilePath(declared);
    if (pathExists(runtimePath)) return runtimePath;
    if (declared.toLowerCase().endsWith(".zip")) {
      const unpackedRuntimePath = getRuntimeFilePath(declared.slice(0, -4));
      if (pathExists(unpackedRuntimePath)) return unpackedRuntimePath;
    }
  }
  const resolved = resolvePathFromRoots(declared, roots, pathExists);
  if (resolved) return resolved;
  if (declared.toLowerCase().endsWith(".zip")) {
    return resolvePathFromRoots(declared.slice(0, -4), roots, pathExists);
  }
  return "";
};

const normalizeChatBridgeApiBase = (value) => {
  const raw = String(
    value ||
      process.env.SEKAILINK_API_BASE_URL ||
      process.env.VITE_API_BASE_URL ||
      getClientServerBaseUrl()
  ).trim();
  if (!raw || raw === "same") return "";
  return raw.replace(/\/+$/, "");
};

const chatBridgeApiUrl = (apiBaseUrl, apiPath) => {
  const cleanPath = String(apiPath || "").startsWith("/") ? String(apiPath || "") : `/${apiPath || ""}`;
  return `${apiBaseUrl}${cleanPath}`;
};

const appendChatBridgeLine = (filePath, payload) => {
  ensureDir(path.dirname(filePath));
  fs.appendFileSync(filePath, `${JSON.stringify(payload)}\n`, "utf8");
};

const readNewChatBridgeLines = (filePath, state) => {
  try {
    if (!filePath || !fs.existsSync(filePath)) return [];
    const stat = fs.statSync(filePath);
    if (stat.size < state.offset) state.offset = 0;
    if (stat.size === state.offset) return [];
    const fd = fs.openSync(filePath, "r");
    try {
      const length = stat.size - state.offset;
      const buffer = Buffer.alloc(Math.min(length, 1024 * 1024));
      const bytesRead = fs.readSync(fd, buffer, 0, buffer.length, state.offset);
      state.offset += bytesRead;
      return buffer.toString("utf8", 0, bytesRead).split(/\r?\n/).filter((line) => line.trim());
    } finally {
      fs.closeSync(fd);
    }
  } catch (err) {
    writeLogLine("warn", "sekaiemu-chat", `read failed: ${String(err || "")}`);
    return [];
  }
};

const normalizeChatBridgeMessage = (message, fallbackChannel) => {
  const author = String(message?.author || message?.display_name || message?.username || message?.name || "Room");
  const text = String(message?.content || message?.text || message?.body || "");
  const createdAt = String(message?.created_at || message?.timestamp || nowIso());
  const id = String(
    message?.id ||
      message?.message_id ||
      `${fallbackChannel}:${createdAt}:${author}:${text}`
  );
  return {
    id,
    author,
    text,
    created_at: createdAt,
    kind: String(message?.kind || message?.type || "user"),
  };
};

const rememberChatBridgeMessageId = (bridge, id) => {
  if (!id) return true;
  if (bridge.seenServerIds.has(id)) return false;
  bridge.seenServerIds.add(id);
  bridge.seenServerOrder.push(id);
  while (bridge.seenServerOrder.length > 512) {
    bridge.seenServerIds.delete(bridge.seenServerOrder.shift());
  }
  return true;
};

const fetchChatBridgeJson = async (bridge, apiPath, init = {}) => {
  const headers = {
    ...(init.headers || {}),
    "X-SekaiLink-Client": "desktop",
  };
  if (bridge.authToken && !headers.Authorization) {
    headers.Authorization = `Bearer ${bridge.authToken}`;
  }
  if (bridge.deviceId && !headers["X-SekaiLink-Device-Id"]) {
    headers["X-SekaiLink-Device-Id"] = bridge.deviceId;
  }
  if (init.body && !headers["Content-Type"]) {
    headers["Content-Type"] = "application/json";
  }
  const response = await fetch(chatBridgeApiUrl(bridge.apiBaseUrl, apiPath), {
    ...init,
    headers,
    credentials: "omit",
  });
  const text = await response.text().catch(() => "");
  let data = {};
  if (text) {
    try {
      data = JSON.parse(text);
    } catch (_err) {
      data = { raw: text };
    }
  }
  return { ok: response.ok, status: response.status, data };
};

const listChatBridgeMessages = async (bridge) => {
  const channelPath = `/api/chat/channels/${encodeURIComponent(bridge.channelId)}/messages`;
  const primary = await fetchChatBridgeJson(bridge, channelPath);
  if (primary.ok) {
    return Array.isArray(primary.data?.messages) ? primary.data.messages : [];
  }
  if (!bridge.lobbyId) {
    throw new Error(`chat_list_failed:${primary.status}`);
  }
  const fallback = await fetchChatBridgeJson(
    bridge,
    `/api/lobbies/${encodeURIComponent(bridge.lobbyId)}/messages`
  );
  if (!fallback.ok) {
    throw new Error(`chat_list_failed:${fallback.status}`);
  }
  return Array.isArray(fallback.data?.messages) ? fallback.data.messages : [];
};

const sendChatBridgeMessage = async (bridge, text) => {
  const body = JSON.stringify({ content: String(text || "").trim() });
  const channelPath = `/api/chat/channels/${encodeURIComponent(bridge.channelId)}/messages`;
  const primary = await fetchChatBridgeJson(bridge, channelPath, { method: "POST", body });
  if (primary.ok) return true;
  if (!bridge.lobbyId) {
    throw new Error(`chat_send_failed:${primary.status}`);
  }
  const fallback = await fetchChatBridgeJson(
    bridge,
    `/api/lobbies/${encodeURIComponent(bridge.lobbyId)}/messages`,
    { method: "POST", body }
  );
  if (!fallback.ok) {
    throw new Error(`chat_send_failed:${fallback.status}`);
  }
  return true;
};

const logChatBridgeErrorOnce = (bridge, step, err) => {
  const key = `${step}:${String(err?.message || err || "")}`;
  if (bridge.lastErrorKey === key) return;
  bridge.lastErrorKey = key;
  writeLogLine("warn", "sekaiemu-chat", key);
};

const startSekaiemuChatBridge = (options = {}) => {
  const lobbyId = String(options.lobbyId || "").trim();
  const channelId = String(options.channelId || (lobbyId ? `lobby:${lobbyId}` : "")).trim();
  const apiBaseUrl = normalizeChatBridgeApiBase(options.apiBaseUrl);
  if (!channelId || !apiBaseUrl || typeof fetch !== "function") {
    return null;
  }
  const moduleId = String(options.moduleId || "game").replace(/[^a-z0-9_.-]+/gi, "_") || "game";
  const bridgeId = `${Date.now()}-${crypto.randomBytes(4).toString("hex")}`;
  const root = path.join(app.getPath("userData"), "runtime", "sekaiemu-chat", moduleId, bridgeId);
  ensureDir(root);
  const bridge = {
    id: bridgeId,
    channelId,
    lobbyId,
    apiBaseUrl,
    authToken: String(options.authToken || "").trim(),
    deviceId: String(options.deviceId || "").trim(),
    inboxPath: path.join(root, "from-core.jsonl"),
    outboxPath: path.join(root, "to-core.jsonl"),
    outboxRead: { offset: 0 },
    seenServerIds: new Set(),
    seenServerOrder: [],
    lastErrorKey: "",
    stopped: false,
    timers: [],
  };
  fs.writeFileSync(bridge.inboxPath, "", "utf8");
  fs.writeFileSync(bridge.outboxPath, "", "utf8");

  const pollOutbox = async () => {
    if (bridge.stopped) return;
    const lines = readNewChatBridgeLines(bridge.outboxPath, bridge.outboxRead);
    for (const line of lines) {
      try {
        const parsed = JSON.parse(line);
        const text = String(parsed?.text || parsed?.content || "").trim();
        if (text) await sendChatBridgeMessage(bridge, text);
      } catch (err) {
        logChatBridgeErrorOnce(bridge, "send", err);
      }
    }
  };

  const pollServer = async () => {
    if (bridge.stopped) return;
    try {
      const messages = await listChatBridgeMessages(bridge);
      for (const rawMessage of messages) {
        const message = normalizeChatBridgeMessage(rawMessage, bridge.channelId);
        if (!message.text || !rememberChatBridgeMessageId(bridge, message.id)) continue;
        appendChatBridgeLine(bridge.inboxPath, message);
      }
    } catch (err) {
      logChatBridgeErrorOnce(bridge, "list", err);
    }
  };

  bridge.timers.push(setInterval(() => { void pollOutbox(); }, 250));
  bridge.timers.push(setInterval(() => { void pollServer(); }, 1500));
  setTimeout(() => { void pollServer(); }, 100);
  return {
    inboxPath: bridge.inboxPath,
    outboxPath: bridge.outboxPath,
    stop: () => {
      bridge.stopped = true;
      for (const timer of bridge.timers) clearInterval(timer);
    },
  };
};

const tryLaunchSekaiemu = async (options = {}) => {
  const moduleId = String(options.moduleId || "").trim();
  const manifest = options.manifest || getModuleManifest(moduleId) || {};
  const romPath = String(options.romPath || "").trim();
  const settings = getSekaiemuSettings();
  const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
  const exePath = resolveSekaiemuExecutable();
  if (!exePath) return { ok: false, error: "sekaiemu_not_found" };
  const corePath = resolveSekaiemuCorePath(manifest);
  if (!corePath) return { ok: false, error: "sekaiemu_core_missing" };
  if (!romPath || !fileExists(romPath)) return { ok: false, error: "rom_missing", detail: "patched_rom_not_found" };

  const safeModuleId = moduleId || String(manifest.game_id || "game").trim() || "game";
  const runtimeRoot = path.join(app.getPath("userData"), "sekaiemu", safeModuleId);
  const systemDir = settings.system_dir || path.join(runtimeRoot, "system");
  const saveDir = settings.save_dir || path.join(runtimeRoot, "saves");
  const logDir = settings.log_dir || path.join(app.getPath("userData"), "logs", "sekaiemu");
  ensureDir(systemDir);
  ensureDir(saveDir);
  ensureDir(logDir);

  const args = ["--core", corePath, "--game", romPath, "--system-dir", systemDir, "--save-dir", saveDir, "--log-dir", logDir];
  const runtimeDir = getBundledRuntimeDir();
  const overlayRuntimeDir = getOverlayRuntimeDir();
  const roots = [
    overlayRuntimeDir,
    runtimeDir,
    path.dirname(exePath),
    path.join(path.dirname(exePath), ".."),
    path.join(app.getPath("home"), "Projects", "Sekaiemu-Libretro-Spike-Codex", "workspace", "sekaiemu-libretro-spike"),
  ];
  const profilePath = resolvePathFromRoots(String(sekaiemu.profile_path || sekaiemu.profile || "").trim(), roots);
  if (profilePath) args.push("--profile", profilePath);
  const sklmiRuntime = resolveSklmiRuntimeForSekaiemu();
  const sklmiManifestDir = resolveSklmiManifestDirForSekaiemu();
  if (sklmiRuntime) args.push("--sklmi-runtime", sklmiRuntime);
  if (sklmiManifestDir) args.push("--sklmi-manifest-dir", sklmiManifestDir);

  const apAddress = parseArchipelagoAddress(options.serverAddress);
  const apGame = String(sekaiemu.ap_game || manifest.ap_game || manifest.display_name || "").trim();
  const apSlot = String(options.slot || "").trim();
  const playerAlias = String(options.playerAlias || "").trim();
  const trackerRequired = Boolean(sklmiRuntime && sklmiManifestDir && profilePath);
  if (trackerRequired) args.push("--tracker-required");
  if (apAddress?.host && apAddress.port && apGame && apSlot) {
    args.push(
      "--ap-host", apAddress.host,
      "--ap-port", String(apAddress.port),
      "--ap-path", apAddress.path || "/",
      "--ap-game", apGame,
      "--ap-slot-name", apSlot,
      "--ap-uuid", `sekailink-${safeModuleId}`
    );
    if (options.password) args.push("--ap-password", String(options.password));
    if (playerAlias) args.push("--player-alias", playerAlias);
    args.push("--ap-tags", "AP,SekaiLink,SKLMI");
  }
  const trackerPack = resolveSekaiemuTrackerPackPath(manifest, roots);
  if (trackerPack) {
    args.push("--tracker-pack", trackerPack);
    if (dirExists(trackerPack)) args.push("--tracker-assets-root", trackerPack);
    const trackerVariant = getTrackerVariant(String(manifest.game_id || "").trim());
    if (trackerVariant) args.push("--tracker-variant", trackerVariant);
  }
  const trackerBundle = String(sekaiemu.tracker_bundle || sekaiemu.tracker_bundle_path || "").trim();
  if (trackerBundle && !trackerRequired) {
    const resolvedTracker = resolvePathFromRoots(trackerBundle, roots, pathExists);
    if (resolvedTracker) args.push("--tracker-bundle", resolvedTracker);
  } else if (trackerBundle && trackerRequired && !trackerPack) {
    writeLogLine(
      "warn",
      "sekaiemu",
      `legacy tracker bundle ignored for BETA-3 runtime: module=${safeModuleId} bundle=${trackerBundle}`
    );
  }
  if (Array.isArray(sekaiemu.args)) {
    for (const entry of sekaiemu.args) {
      const value = String(entry || "").trim();
      if (value) args.push(value);
    }
  }
  const chatBridge = startSekaiemuChatBridge({
    moduleId: safeModuleId,
    ...(isPlainObject(options.chatBridge) ? options.chatBridge : {}),
    apiBaseUrl: options.apiBaseUrl,
    authToken: options.authToken,
    deviceId: options.deviceId,
  });
  if (chatBridge) {
    args.push("--chat-inbox", chatBridge.inboxPath, "--chat-outbox", chatBridge.outboxPath);
  }
  args.push(...settings.args);

  try {
    fs.chmodSync(exePath, 0o755);
  } catch (_err) {
    // ignore chmod failures
  }

  try {
    const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore" });
    if (!wrapped.ok) {
      chatBridge?.stop?.();
      return { ok: false, error: wrapped.error || "sekaiemu_launch_failed", detail: wrapped.detail || "" };
    }
    nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
    if (chatBridge) sekaiemuChatBridges.set(wrapped.proc.pid, chatBridge);
    wrapped.proc.on("exit", () => {
      nativeGameProcs.delete(wrapped.proc.pid);
      const activeBridge = sekaiemuChatBridges.get(wrapped.proc.pid);
      activeBridge?.stop?.();
      sekaiemuChatBridges.delete(wrapped.proc.pid);
    });
    return { ok: true, pid: wrapped.proc.pid, method: "libretro-spike", exePath, corePath };
  } catch (err) {
    chatBridge?.stop?.();
    return { ok: false, error: "sekaiemu_launch_failed", detail: String(err || "") };
  }
};

const moduleHasExternalTracker = (manifest = {}) => {
  const emu = String(manifest?.emu || "").trim().toLowerCase();
  if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
    return false;
  }
  const trackerPack = String(manifest?.tracker_pack_uid || "").trim();
  const trackerWeb = String(manifest?.tracker_web_url || "").trim();
  return Boolean(trackerPack || trackerWeb);
};

const launchGameRuntimeForModule = async (options = {}) => {
  const moduleId = String(options.moduleId || "").trim();
  const manifest = options.manifest || getModuleManifest(moduleId) || {};
  const emu = String(manifest.emu || "bizhawk").trim().toLowerCase();
  const romPath = String(options.romPath || "").trim();

  emitSessionEvent({ event: "status", status: "Launching game runtime...", moduleId });
  writeLogLine(
    "info",
    "autolaunch",
    `launching runtime: emu=${emu} romPath=${romPath} moduleId=${moduleId} reused=${options.reusedPatchedRom ? "true" : "false"}`
  );

  let launchRes = null;
  if (emu === "bizhawk") {
    launchRes = await launchBizHawk({ romPath, moduleId });
  } else if (emu === "soh") {
    launchRes = await tryLaunchSoh();
  } else if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
    launchRes = await tryLaunchSekaiemu({
      romPath,
      moduleId,
      manifest,
      serverAddress: options.serverAddress,
      slot: options.slot,
      playerAlias: options.playerAlias,
      password: options.password,
      chatBridge: options.chatBridge,
      apiBaseUrl: options.apiBaseUrl,
      authToken: options.authToken,
      deviceId: options.deviceId,
    });
  } else {
    launchRes = { ok: false, error: "unsupported_emulator", emu };
  }

  writeLogLine(
    "info",
    "autolaunch",
    `runtime result: ok=${launchRes?.ok} pid=${launchRes?.pid || ""} error=${launchRes?.error || ""}`
  );

  if (!launchRes?.ok) {
    emitSessionEvent({ event: "error", step: "emu", error: launchRes?.error || "emu_failed", moduleId, emu });
    return { ok: false, error: launchRes?.error || "emu_failed", detail: launchRes?.detail || "", emu };
  }

  return {
    ok: true,
    emu,
    pid: launchRes.pid,
    detail: launchRes?.detail || "",
  };
};

const connectRuntimeBridgeForModule = async (options = {}) => {
  const moduleId = String(options.moduleId || "").trim();
  const manifest = options.manifest || getModuleManifest(moduleId) || {};
  const emu = String(manifest.emu || "bizhawk").trim().toLowerCase();
  const serverAddress = String(options.serverAddress || "").trim();
  const slot = String(options.slot || "").trim();
  const password = options.password;
  const clientMode = String(manifest.client_mode || ((emu === "sekaiemu" || emu === "sekaiemu-libretro") ? "embedded" : "")).trim().toLowerCase();

  emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId });
  writeLogLine("info", "autolaunch", `starting bridge: emu=${emu} serverAddress=${serverAddress} slot=${slot}`);

  if (emu === "bizhawk") {
    const apClient = String(manifest.ap_client || "").trim().toLowerCase();
    if (apClient === "sni") {
      emitSessionEvent({ event: "status", status: "Preparing SNES connection...", moduleId });
      const luaPort = await waitForAnyTcpPort("127.0.0.1", [43055, 43056, 43057, 43058, 43059, 43060], 9000);
      writeLogLine("info", "autolaunch", `lua connector port detected: ${luaPort || "none"}`);
      await stopSniBridge();
      const bridgeRes = await startSniBridge({
        host: "127.0.0.1",
        port: 23074,
        luaHost: "127.0.0.1",
        luaPortStart: 43055,
        luaPortEnd: 43060,
      });
      if (!bridgeRes?.ok) {
        emitSessionEvent({ event: "warning", step: "sni-bridge", error: bridgeRes?.error || "sni_bridge_failed", moduleId });
      }
      const clientRes = await startBizHawkClient({ clientKind: "sni" });
      writeLogLine("info", "autolaunch", `sniclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
      await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
      return { ok: true, mode: "bizhawk-sni" };
    }
    const clientRes = await startBizHawkClient({ address: serverAddress, slot, clientKind: "bizhawk" });
    writeLogLine("info", "autolaunch", `bizhawkclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
    await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    return { ok: true, mode: "bizhawk" };
  }

  if ((emu === "sekaiemu" || emu === "sekaiemu-libretro") && clientMode === "sklmi") {
    return { ok: false, error: "sklmi_bridge_pending" };
  }

  if (clientMode !== "embedded") {
    const clientRes = await startCommonClient({ address: serverAddress, slot, password });
    writeLogLine("info", "autolaunch", `commonclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
    await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    return { ok: true, mode: "commonclient" };
  }

  return { ok: true, mode: "embedded" };
};

const launchTrackerForModuleSession = async (options = {}) => {
  const moduleId = String(options.moduleId || "").trim();
  const manifest = options.manifest || getModuleManifest(moduleId) || {};
  const serverAddress = String(options.serverAddress || "").trim();
  const slot = String(options.slot || "").trim();
  const password = options.password;
  const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;

  if (!moduleHasExternalTracker(manifest)) {
    return { ok: true, skipped: true, pid: null };
  }

  emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId });
  writeLogLine("info", "autolaunch", `launching tracker: moduleId=${moduleId} serverAddress=${serverAddress} slot=${slot}`);
  const trackerRes = await launchPopTracker({
    moduleId,
    apHost: serverAddress,
    apSlot: slot,
    apPass: password,
    apAutoconnect: true,
    forceTrackerVariantPrompt,
  });
  writeLogLine("info", "autolaunch", `tracker result: ok=${trackerRes?.ok} pid=${trackerRes?.pid || ""} error=${trackerRes?.error || ""}`);

  if (trackerRes?.error === "tracker_variant_cancelled") {
    emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId });
    return { ok: false, error: "tracker_variant_cancelled", pid: null };
  }

  if (!trackerRes?.ok) {
    emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId });
    return { ok: true, warning: trackerRes?.error || "tracker_failed", pid: null };
  }

  return { ok: true, pid: trackerRes?.pid || null };
};

const applyRuntimeLayoutForSession = async (options = {}) => {
  const moduleId = String(options.moduleId || "").trim();
  const gamePid = Number(options.gamePid || 0);
  const trackerPid = Number(options.trackerPid || 0) || null;

  emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId });
  writeLogLine("info", "autolaunch", `applying layout: gamePid=${gamePid} trackerPid=${trackerPid || ""}`);
  const layoutRes = await applyLayoutBestEffort({ gamePid, trackerPid });
  if (!layoutRes?.ok) {
    emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId });
  }
  return layoutRes;
};

const tryLaunchSm64Ex = (options = {}) => {
  const settings = getSm64ExSettings();
  const exeOverride = settings.exe_path;
  const rootDir = options.rootDir || settings.root_dir;

  let exePath = "";
  if (exeOverride && fs.existsSync(exeOverride)) {
    exePath = exeOverride;
  } else if (rootDir && fs.existsSync(rootDir)) {
    // Common Linux build outputs:
    // - sm64ex/build/us_pc/sm64.us.f3dex2e
    // - sm64ex/build/jp_pc/sm64.jp.f3dex2e
    exePath =
      findPathByBasename(rootDir, "sm64.us.f3dex2e", 8) ||
      findPathByBasename(rootDir, "sm64.us.f3dex2e.exe", 8) ||
      findPathByBasename(rootDir, "sm64.jp.f3dex2e", 8) ||
      findPathByBasename(rootDir, "sm64.jp.f3dex2e.exe", 8) ||
      "";
  }

  if (!exePath || !fs.existsSync(exePath)) {
    return { ok: false, error: "sm64ex_not_found" };
  }

  const fileArg = options.filePath;
  if (!fileArg || !fs.existsSync(fileArg)) {
    return { ok: false, error: "missing_slot_file" };
  }

  const args = ["--sm64ap_file", fileArg, ...settings.args];
  try {
    const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore" });
    if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
    nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
    wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
    return { ok: true, pid: wrapped.proc.pid, method: "exe" };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const tryLaunchGzDoom = (options = {}) => {
  const settings = getGzDoomSettings();
  const exeOverride = settings.exe_path;
  const exePath = exeOverride || which("gzdoom") || "";
  if (!exePath || !fs.existsSync(exePath)) {
    return { ok: false, error: "gzdoom_not_found" };
  }

  const iwad = options.iwadPath || settings.iwad_path;
  if (!iwad || !fs.existsSync(iwad)) {
    return { ok: false, error: "iwad_missing" };
  }

  const gzap = settings.gzap_pk3_path;
  if (!gzap || !fs.existsSync(gzap)) {
    return { ok: false, error: "gzap_pk3_missing" };
  }

  const fileArg = options.filePath;
  if (!fileArg || !fs.existsSync(fileArg)) {
    return { ok: false, error: "missing_mod_file" };
  }

  // Load order matters: gzArchipelago first, then the generated seed pk3.
  const args = ["-iwad", iwad, "-file", gzap, fileArg, ...settings.args];
  try {
    const wrapped = spawnMaybeGamescope(exePath, args, { stdio: "ignore" });
    if (!wrapped.ok) return { ok: false, error: wrapped.error || "spawn_failed" };
    nativeGameProcs.set(wrapped.proc.pid, wrapped.proc);
    wrapped.proc.on("exit", () => nativeGameProcs.delete(wrapped.proc.pid));
    return { ok: true, pid: wrapped.proc.pid, method: "exe" };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const tryHandleDownloadedArtifact = async (ctx = {}) => {
  const apGameName = String(ctx.apGameName || "").trim().toLowerCase();
  const downloadedPath = String(ctx.downloadedPath || "").trim();
  const ext = String(ctx.ext || "").trim().toLowerCase();

  if (!downloadedPath || !fs.existsSync(downloadedPath)) return { ok: false, error: "missing_download" };

  // Super Mario 64 EX (PC port) uses a slot file (.apsm64ex) and the game binary consumes it via --sm64ap_file.
  // Note: the world currently only generates output for single-player seeds.
  if ((apGameName === "super mario 64" || ext === ".apsm64ex") && ext === ".apsm64ex") {
    const launch = tryLaunchSm64Ex({ filePath: downloadedPath });
    if (launch.ok) {
      emitSessionEvent({ event: "status", status: "Launching SM64EX...", pid: launch.pid, method: launch.method });
      return { ok: true, handled: true, installedPath: downloadedPath, gamePid: launch.pid, launchMethod: launch.method };
    }
    return {
      ok: true,
      handled: true,
      installedPath: downloadedPath,
      gamePid: null,
      error: launch.error,
      note:
        "SM64EX build not found. Compile via SM64AP-Launcher or a manual sm64ex build, then set Settings -> Games (Automation) -> SM64EX executable path.",
    };
  }

  // Factorio slot files are served as a mod zip.
  if (apGameName.startsWith("factorio") && ext === ".zip") {
    const modsDir = getFactorioModsDir();
    try {
      ensureDir(modsDir);
    } catch (err) {
      return { ok: false, error: "mods_dir_create_failed", detail: String(err), modsDir };
    }

    const destPath = path.join(modsDir, path.basename(downloadedPath));
    try {
      backupAndReplaceFile(downloadedPath, destPath);
    } catch (err) {
      return { ok: false, error: "mod_install_failed", detail: String(err), destPath };
    }

    emitSessionEvent({
      event: "status",
      status: "Installed Factorio mod.",
      modsDir,
      installedPath: destPath,
    });

    const launch = tryLaunchFactorio();
    if (launch.ok) {
      emitSessionEvent({ event: "status", status: "Launching Factorio...", pid: launch.pid, method: launch.method });
      return { ok: true, handled: true, installedPath: destPath, gamePid: launch.pid, launchMethod: launch.method };
    }

    // Still a partial automation win: mod installed.
    return { ok: true, handled: true, installedPath: destPath, gamePid: null, error: launch.error };
  }

  // gzDoom slot files are served as a .pk3 to load via -file, but requires an IWAD.
  if (apGameName === "gzdoom" && ext === ".pk3") {
    const launch = tryLaunchGzDoom({ filePath: downloadedPath });
    if (launch.ok) {
      emitSessionEvent({ event: "status", status: "Launching gzDoom...", pid: launch.pid, method: launch.method });
      return { ok: true, handled: true, installedPath: downloadedPath, gamePid: launch.pid, launchMethod: launch.method };
    }
    if (launch.error === "iwad_missing") {
      return {
        ok: true,
        handled: true,
        installedPath: downloadedPath,
        gamePid: null,
        error: "iwad_missing",
        note: "Missing IWAD. Set gzDoom IWAD path in Settings -> Games (Automation).",
      };
    }
    if (launch.error === "gzap_pk3_missing") {
      return {
        ok: true,
        handled: true,
        installedPath: downloadedPath,
        gamePid: null,
        error: "gzap_pk3_missing",
        note: "Missing gzArchipelago.pk3. Set it in Settings -> Games (Automation).",
      };
    }
    return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: launch.error };
  }

  // Dark Souls III slot file is a JSON blob used by the external DS3 randomizer package.
  // We can at least install the package and stage the JSON file for the user.
  if (apGameName === "dark souls iii" && ext === ".json") {
    emitSessionEvent({ event: "status", status: "Installing DS3 randomizer package...", apGameName });
    let rootDir = "";
    try {
      rootDir = await ensureGithubReleaseZipInstalled(
        "ds3",
        "nex3/Dark-Souls-III-Archipelago-client",
        "DS3\\.Archipelago.*\\.zip$"
      );
    } catch (err) {
      return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: "ds3_install_failed", note: String(err) };
    }

    const slotDir = path.join(rootDir, "slot_files");
    try {
      ensureDir(slotDir);
      const destPath = path.join(slotDir, path.basename(downloadedPath));
      backupAndReplaceFile(downloadedPath, destPath);

      const randomizerExe = findPathByBasename(rootDir, "ds3randomizer.exe", 7);
      const launcherBat = findPathByBasename(rootDir, "launchmod_darksouls3.bat", 7);
      const noteParts = [];
      noteParts.push("DS3 package installed; slot file staged.");
      if (randomizerExe) noteParts.push("Run DS3Randomizer.exe for one-time setup.");
      if (launcherBat) noteParts.push("Then run launchmod_darksouls3.bat to start the game.");

      return { ok: true, handled: true, installedPath: destPath, gamePid: null, note: noteParts.join(" ") };
    } catch (err) {
      return { ok: true, handled: true, installedPath: downloadedPath, gamePid: null, error: "ds3_stage_failed", note: String(err) };
    }
  }

  return { ok: false, error: "no_handler" };
};

const emitCommonClientEvent = (payload) => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  writeLogJson("commonclient", payload);
  mainWindow.webContents.send(COMMONCLIENT_EVENT_CHANNEL, payload);
};

const emitBizHawkClientEvent = (payload) => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  writeLogJson("bizhawkclient", payload);
  mainWindow.webContents.send(BIZHAWKCLIENT_EVENT_CHANNEL, payload);
};

const emitTrackerClientLog = (level, message) => {
  const lvl = String(level || "info").toLowerCase();
  const msg = String(message || "").trim();
  if (!msg) return;
  emitBizHawkClientEvent({ event: "log", level: lvl, logger: "Tracker", message: msg });
};

const emitSessionEvent = (payload) => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  writeLogJson("session", payload);
  mainWindow.webContents.send(SESSION_EVENT_CHANNEL, payload);
};

const emitUpdaterEvent = (payload) => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  writeLogJson("updater", payload);
  mainWindow.webContents.send(UPDATER_EVENT_CHANNEL, payload);
};

const sendCommonClientCommand = async (command) => {
  if (!commonClientProc || !command) return { ok: false, error: "not_running" };
  try {
    commonClientProc.stdin.write(JSON.stringify(command) + "\n");
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const stopCommonClient = async () => {
  if (!commonClientProc) return;
  const proc = commonClientProc;
  try {
    proc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
  } catch (_err) {
    // ignore
  }
  writeLogLine("info", "commonclient", `stopping pid=${proc.pid || ""}`);
  await terminateChildProcess(proc, "commonclient", { graceMs: 1400 });
  commonClientProc = null;
  if (commonClientRl) {
    commonClientRl.close();
    commonClientRl = null;
  }
};

const sendBizHawkClientCommand = async (command) => {
  if (!bizhawkClientProc || !command) return { ok: false, error: "not_running" };
  try {
    bizhawkClientProc.stdin.write(JSON.stringify(command) + "\n");
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const startSniBridge = async (options = {}) => {
  if (sniBridgeProc) return { ok: true, alreadyRunning: true };
  const python = await ensurePythonRuntime();
  const bridgePath = getSniBridgePath();
  if (!bridgePath || !fs.existsSync(bridgePath)) {
    return { ok: false, error: "sni_bridge_missing", detail: `missing: ${bridgePath}` };
  }

  const host = String(options.host || "127.0.0.1");
  const port = Number(options.port || 23074);
  const luaHost = String(options.luaHost || "127.0.0.1");
  const luaPortStart = Number(options.luaPortStart || 43055);
  const luaPortEnd = Number(options.luaPortEnd || 43060);
  const cleanup = await purgeStaleSniBridgePortHolders(port, 0);
  if (!cleanup.ok) {
    return {
      ok: false,
      error: "sni_bridge_port_in_use",
      detail: `port ${port} in use by non-SekaiLink process`,
    };
  }
  const args = [
    bridgePath,
    "--host", host,
    "--port", `${port}`,
    "--lua-host", luaHost,
    "--lua-port-start", `${luaPortStart}`,
    "--lua-port-end", `${luaPortEnd}`,
  ];

  sniBridgeProc = spawn(python, args, {
    stdio: ["ignore", "pipe", "pipe"],
    env: withApPythonEnv(process.env),
    windowsHide: true,
  });
  writeLogLine("info", "sni-bridge", `spawned pid=${sniBridgeProc.pid || ""} python=${python} script=${bridgePath}`);

  sniBridgeProc.stdout.on("data", (chunk) => {
    const text = String(chunk || "").trim();
    if (!text) return;
    for (const line of text.split(/\r?\n/).filter(Boolean).slice(-4)) {
      writeLogLine("info", "sni-bridge", line);
    }
  });
  sniBridgeProc.stderr.on("data", (chunk) => {
    const text = String(chunk || "").trim();
    if (!text) return;
    for (const line of text.split(/\r?\n/).filter(Boolean).slice(-4)) {
      writeLogLine("warn", "sni-bridge", line);
    }
  });
  sniBridgeProc.on("exit", (code, signal) => {
    writeLogLine("warn", "sni-bridge", `exited code=${code ?? "null"} signal=${signal || "none"}`);
    sniBridgeProc = null;
  });

  const ok = await waitForTcpPort(host, port, 6000);
  if (!ok) {
    const spawned = sniBridgeProc;
    if (spawned) await terminateChildProcess(spawned, "sni-bridge", { graceMs: 400 });
    sniBridgeProc = null;
    return { ok: false, error: "sni_bridge_start_timeout" };
  }
  const spawnedPid = Number(sniBridgeProc?.pid || 0);
  if (!(spawnedPid > 0 && isPidAlive(spawnedPid))) {
    sniBridgeProc = null;
    return { ok: false, error: "sni_bridge_spawn_exited" };
  }
  const probe = probeSniBridge(python, host, port, 2500);
  if (!probe.ok) {
    const spawned = sniBridgeProc;
    if (spawned) await terminateChildProcess(spawned, "sni-bridge", { graceMs: 400 });
    sniBridgeProc = null;
    return { ok: false, error: probe.error || "sni_bridge_probe_failed", detail: probe.detail || "" };
  }
  return { ok: true, host, port };
};

const stopSniBridge = async () => {
  if (!sniBridgeProc) return;
  const proc = sniBridgeProc;
  try {
    await terminateChildProcess(proc, "sni-bridge", { graceMs: 1000 });
  } catch (_err) {
    // ignore
  }
  sniBridgeProc = null;
  await purgeStaleSniBridgePortHolders(23074, 0);
};

const stopBizHawkClient = async () => {
  if (!bizhawkClientProc) return;
  const proc = bizhawkClientProc;
  try {
    proc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
  } catch (_err) {
    // ignore
  }
  writeLogLine("info", "bizhawkclient", `stopping pid=${proc.pid || ""}`);
  await terminateChildProcess(proc, "bizhawkclient", { graceMs: 1400 });
  bizhawkClientProc = null;
  bizhawkClientKind = "bizhawk";
  await stopSniBridge();
  if (bizhawkClientRl) {
    bizhawkClientRl.close();
    bizhawkClientRl = null;
  }
};

const getBizHawkBaseDir = () => {
  const dirName = process.platform === "win32" ? "BizHawk-2.10-win-x64" : "BizHawk-2.10-linux-x64";
  return path.join(getBundledThirdPartyDir(), "emulators", dirName);
};

const getBizHawkInstalledDir = () => {
  // Install BizHawk into a writable location at first launch. AppImage resources are mounted read-only.
  const dirName = process.platform === "win32" ? "BizHawk-2.10-win-x64" : "BizHawk-2.10-linux-x64";
  return path.join(getRuntimeToolsDir(), "bizhawk", dirName);
};

const BIZHAWK_REQUIRED_ASSEMBLY_VERSION = "2.10.0.0";

const readBizHawkAssemblyVersion = (baseDir) => {
  try {
    const dllPath = path.join(String(baseDir || ""), "dll", "BizHawk.Common.dll");
    if (!fs.existsSync(dllPath)) return null;
    const blob = fs.readFileSync(dllPath);
    const text = blob.toString("latin1");
    const matches = text.match(/\b2\.\d+\.\d+\.\d+\b/g) || [];
    if (!matches.length) return null;
    if (matches.includes(BIZHAWK_REQUIRED_ASSEMBLY_VERSION)) return BIZHAWK_REQUIRED_ASSEMBLY_VERSION;
    return String(matches[0]);
  } catch (_err) {
    return null;
  }
};

const verifyBizHawkVersion = (baseDir, sourceLabel = "runtime") => {
  const found = readBizHawkAssemblyVersion(baseDir);
  if (found === BIZHAWK_REQUIRED_ASSEMBLY_VERSION) return { ok: true, version: found };
  const detail = `expected ${BIZHAWK_REQUIRED_ASSEMBLY_VERSION}, found ${found || "unknown"} (${sourceLabel})`;
  writeLogLine("error", "bizhawk", `version mismatch: ${detail}`);
  return { ok: false, error: "bizhawk_version_mismatch", detail };
};

const stageBizHawkToDir = (sourceDir, destDir) => {
  try {
    if (!fs.existsSync(sourceDir)) {
      return { ok: false, error: "bizhawk_missing", detail: `source_missing: ${sourceDir}` };
    }
    fs.rmSync(destDir, { recursive: true, force: true });
    fs.mkdirSync(destDir, { recursive: true });
    // Node 18+ supports cpSync; Electron 28 ships a modern Node.
    fs.cpSync(sourceDir, destDir, { recursive: true });
    if (process.platform !== "win32") {
      try {
        fs.chmodSync(path.join(destDir, "EmuHawkMono.sh"), 0o755);
      } catch (_err) {
        // ignore
      }
    }
    return { ok: true, baseDir: destDir };
  } catch (err) {
    const detail = String(err || "");
    writeLogLine("error", "bizhawk", `staging failed: ${detail}`);
    return { ok: false, error: "bizhawk_stage_failed", detail };
  }
};

const ensureBizHawkInstalled = async () => {
  const dest = getBizHawkInstalledDir();
  // If the user overrides BizHawk, prefer it when writable; otherwise stage it to a writable location.
  const override = String(process.env.SEKAILINK_BIZHAWK || "").trim();
  if (override) {
    const overridePath = path.resolve(override);
    const overrideDir = path.dirname(overridePath);
    if (!fs.existsSync(overridePath)) {
      return { ok: false, error: "bizhawk_missing", detail: `override_missing: ${overridePath}` };
    }
    if (isWritablePath(overrideDir)) {
      const verify = verifyBizHawkVersion(overrideDir, "override");
      if (!verify.ok) return verify;
      return { ok: true, baseDir: overrideDir, emuPath: overridePath };
    }
    emitSessionEvent({ event: "status", status: "Preparing BizHawk runtime (read-only override detected)...", tool: "bizhawk" });
    writeLogLine("warn", "bizhawk", `override is read-only; staging runtime to writable dir: ${overrideDir} -> ${dest}`);
    const staged = stageBizHawkToDir(overrideDir, dest);
    if (!staged.ok) return staged;
    const verify = verifyBizHawkVersion(dest, "override-staged");
    if (!verify.ok) return verify;
    return { ok: true, baseDir: dest, emuPath: path.join(dest, path.basename(overridePath)) };
  }

  const bundled = getBizHawkBaseDir();
  const bundledVerify = verifyBizHawkVersion(bundled, "bundled");
  if (!bundledVerify.ok) return bundledVerify;
  if (!app.isPackaged) return { ok: true, baseDir: bundled };

  const marker = path.join(dest, process.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh");
  const stampPath = path.join(dest, ".sekailink-bizhawk-stamp.json");
  const bundledMarker = path.join(bundled, process.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh");
  const bundledMtime = fs.existsSync(bundledMarker) ? Number(fs.statSync(bundledMarker).mtimeMs || 0) : 0;
  const runtimeVersion = app.getVersion();
  if (fs.existsSync(marker) && fs.existsSync(stampPath)) {
    try {
      const stamp = JSON.parse(fs.readFileSync(stampPath, "utf8"));
      if (String(stamp?.version || "") === runtimeVersion && Number(stamp?.bundledMtime || 0) === bundledMtime) {
        const verify = verifyBizHawkVersion(dest, "staged");
        if (!verify.ok) return verify;
        return { ok: true, baseDir: dest };
      }
      writeLogLine("info", "bizhawk", "runtime stamp changed, refreshing staged BizHawk files");
    } catch (_err) {
      writeLogLine("warn", "bizhawk", "invalid BizHawk runtime stamp, restaging");
    }
  } else if (fs.existsSync(marker)) {
    writeLogLine("info", "bizhawk", "staged BizHawk exists without stamp, refreshing once");
  }

  emitSessionEvent({ event: "status", status: "Installing BizHawk runtime (one-time)...", tool: "bizhawk" });
  writeLogLine("info", "bizhawk", `staging runtime to writable dir: ${dest}`);
  const staged = stageBizHawkToDir(bundled, dest);
  if (!staged.ok) return staged;
  const verify = verifyBizHawkVersion(dest, "staged");
  if (!verify.ok) return verify;
  try {
    fs.writeFileSync(stampPath, JSON.stringify({ version: runtimeVersion, bundledMtime }, null, 2), "utf8");
  } catch (_err) {
    // non-fatal
  }
  return staged;
};

const getGamescopePath = () => {
  if (_gamescopePathCache !== undefined) return _gamescopePathCache;
  const fromEnv = process.env.SEKAILINK_GAMESCOPE;
  if (fromEnv && fs.existsSync(fromEnv)) {
    _gamescopePathCache = fromEnv;
    return _gamescopePathCache;
  }
  try {
    const res = spawnSync("which", ["gamescope"], { stdio: ["ignore", "pipe", "ignore"] });
    if (res.status === 0) {
      const found = String(res.stdout || "").trim();
      _gamescopePathCache = found || null;
      return _gamescopePathCache;
    }
  } catch (_err) {
    // ignore
  }
  _gamescopePathCache = null;
  return _gamescopePathCache;
};

const spawnMaybeGamescope = (cmd, cmdArgs, spawnOpts = {}) => {
  const config = readConfig();
  const windowing = config.windowing || {};
  const gamescopeCfg = windowing.gamescope || {};

  if (!gamescopeCfg?.enabled) {
    return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
  }

  const gamescopePath = getGamescopePath();
  const mode = gamescopeCfg?.mode || "prefer"; // "prefer" | "require"
  if (!gamescopePath) {
    if (mode === "require") return { ok: false, error: "gamescope_missing" };
    return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
  }

  const gsArgs = [];
  if (gamescopeCfg?.fullscreen) gsArgs.push("-f");
  if (Number.isFinite(gamescopeCfg?.width)) gsArgs.push("-W", String(gamescopeCfg.width));
  if (Number.isFinite(gamescopeCfg?.height)) gsArgs.push("-H", String(gamescopeCfg.height));
  if (Array.isArray(gamescopeCfg?.args)) {
    for (const a of gamescopeCfg.args) {
      const s = String(a || "").trim();
      if (s) gsArgs.push(s);
    }
  }
  gsArgs.push("--", cmd, ...cmdArgs);

  try {
    return { ok: true, proc: spawn(gamescopePath, gsArgs, spawnOpts), wrapped: true };
  } catch (err) {
    if (mode === "require") return { ok: false, error: "gamescope_spawn_failed", detail: String(err) };
    return { ok: true, proc: spawn(cmd, cmdArgs, spawnOpts), wrapped: false };
  }
};

const getWmctrlPath = () => {
  if (_wmctrlPathCache !== undefined) return _wmctrlPathCache;
  try {
    const res = spawnSync("which", ["wmctrl"], { stdio: ["ignore", "pipe", "ignore"] });
    if (res.status === 0) {
      const found = String(res.stdout || "").trim();
      _wmctrlPathCache = found || null;
      return _wmctrlPathCache;
    }
  } catch (_err) {
    // ignore
  }
  _wmctrlPathCache = null;
  return _wmctrlPathCache;
};

const which = (cmd) => {
  try {
    const res = spawnSync("which", [cmd], { stdio: ["ignore", "pipe", "ignore"] });
    if (res.status === 0) return String(res.stdout || "").trim();
  } catch (_err) {
    // ignore
  }
  return "";
};

const wmctrlFindWindowIdByPid = async (pid, timeoutMs = 5000) => {
  const wmctrl = getWmctrlPath();
  if (!wmctrl) return null;
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    try {
      const res = spawnSync(wmctrl, ["-lp"], { stdio: ["ignore", "pipe", "ignore"] });
      if (res.status === 0) {
        const out = String(res.stdout || "");
        const lines = out.split(/\r?\n/);
        for (const line of lines) {
          // Example: 0x03c00007  0  12345 host Window Title
          const m = line.match(/^(0x[0-9a-fA-F]+)\s+\S+\s+(\d+)\s+/);
          if (!m) continue;
          const winId = m[1];
          const winPid = Number(m[2]);
          if (winPid === Number(pid)) return winId;
        }
      }
    } catch (_err) {
      // ignore
    }
    await sleep(250);
  }
  return null;
};

const wmctrlMoveResize = (winId, bounds) => {
  const wmctrl = getWmctrlPath();
  if (!wmctrl) return { ok: false, error: "wmctrl_missing" };
  const x = Math.max(0, Math.floor(bounds?.x ?? 0));
  const y = Math.max(0, Math.floor(bounds?.y ?? 0));
  const w = Math.max(1, Math.floor(bounds?.width ?? 1));
  const h = Math.max(1, Math.floor(bounds?.height ?? 1));
  const spec = `0,${x},${y},${w},${h}`;
  try {
    const res = spawnSync(wmctrl, ["-ir", winId, "-e", spec], { stdio: "ignore" });
    return res.status === 0 ? { ok: true } : { ok: false, error: "wmctrl_failed" };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const getLayoutConfig = () => {
  const config = readConfig();
  const layout = config.layout && typeof config.layout === "object" ? config.layout : {};
  const preset = typeof layout.preset === "string" ? layout.preset : "handheld";
  const mode = layout.mode === "side_by_side" || layout.mode === "separate_displays" ? layout.mode : "";
  const gameDisplay = Number.isFinite(layout.game_display) ? Number(layout.game_display) : 0;
  const trackerDisplay = Number.isFinite(layout.tracker_display) ? Number(layout.tracker_display) : 1;
  const split = Number.isFinite(layout.split) ? Math.min(0.9, Math.max(0.1, Number(layout.split))) : 0.7;
  return { preset, mode, gameDisplay, trackerDisplay, split };
};

const applyLayoutBestEffort = async (options = {}) => {
  const gamePid = options.gamePid;
  const trackerPid = options.trackerPid;
  const { preset, mode, gameDisplay, trackerDisplay, split } = getLayoutConfig();
  const displays = screen.getAllDisplays();
  if (!displays?.length) return { ok: true };

  // Only implemented for X11/XWayland via wmctrl. Wayland-native environments may not allow this.
  if (!getWmctrlPath()) return { ok: false, error: "wmctrl_missing" };

  const pickDisplay = (idx) => {
    const i = Math.max(0, Math.min(displays.length - 1, Number(idx) || 0));
    return displays[i];
  };

  const dGame = pickDisplay(gameDisplay);
  const dTracker = pickDisplay(trackerDisplay);
  const bGame = dGame?.workArea || dGame?.bounds;
  const bTracker = dTracker?.workArea || dTracker?.bounds;

  const effectiveMode =
    mode ||
    (preset === "desktop_dual" || preset === "streamer_dual" ? "separate_displays" : "side_by_side");

  const gameWinId = gamePid ? await wmctrlFindWindowIdByPid(gamePid, 6000) : null;
  const trackerWinId = trackerPid ? await wmctrlFindWindowIdByPid(trackerPid, 6000) : null;

  if (effectiveMode === "separate_displays") {
    if (gameWinId && bGame) wmctrlMoveResize(gameWinId, bGame);
    if (trackerWinId && bTracker) wmctrlMoveResize(trackerWinId, bTracker);
    return { ok: true };
  }

  // side_by_side on game display
  if (bGame) {
    const left = { x: bGame.x, y: bGame.y, width: Math.floor(bGame.width * split), height: bGame.height };
    const right = {
      x: bGame.x + left.width,
      y: bGame.y,
      width: Math.max(1, bGame.width - left.width),
      height: bGame.height
    };
    if (gameWinId) wmctrlMoveResize(gameWinId, left);
    if (trackerWinId) wmctrlMoveResize(trackerWinId, right);
  }
  return { ok: true };
};

const ensureBizHawkConfig = (baseDir) => {
  try {
    const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
    const configPath = path.join(dir, "config.ini");
    let config = {};
    if (fs.existsSync(configPath)) {
      try {
        config = JSON.parse(fs.readFileSync(configPath, "utf-8"));
      } catch (_err) {
        config = {};
      }
    }

    config.RunInBackground = true;
    config.AutosaveSaveRAM = true;
    config.FlushSaveRamFrames = 300;
    config.BackupSaveram = true;
    config.LastWrittenFrom = "2.10";
    if (!config.LastWrittenFromDetailed) config.LastWrittenFromDetailed = "2.10";

    // NES autotracking handlers (MM2/MM3/TLoZ/Zelda2/etc) expect memory domains like "PRG ROM",
    // which are not exposed by the ultra-fast "quickerNES" core.
    // Force NesHawk to avoid "No handler was found" + RequestFailedError loops.
    config.PreferredCores = config.PreferredCores && typeof config.PreferredCores === "object" ? config.PreferredCores : {};
    config.PreferredCores.NES = "NesHawk";

    fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const ensureBizHawkLuaCompat = (baseDir) => {
  try {
    const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
    const luaDir = path.join(dir, "Lua");
    const apRoot = getBundledApRootDir();
    const apLuaDir = path.join(apRoot, "data", "lua");
    const requiredLuaFiles = [
      "lua_5_3_compat.lua",
      "base64.lua",
      "json.lua",
      "socket.lua",
    ];
    const missing = [];

    for (const file of requiredLuaFiles) {
      const destPath = path.join(luaDir, file);
      if (fs.existsSync(destPath)) continue;

      const candidates = [
        path.join(apLuaDir, file),
        getRuntimeModulePath("bizhawk_base", "lua", file),
        path.join(getBizHawkBaseDir(), "Lua", file),
      ];

      let copied = false;
      for (const src of candidates) {
        if (!fs.existsSync(src)) continue;
        ensureDir(luaDir);
        fs.copyFileSync(src, destPath);
        copied = true;
        break;
      }
      if (!copied) missing.push(file);
    }

    // Archipelago's socket.lua loads native module from "./x64/socket-<os>-<lua>.{so|dll}".
    // Depending on BizHawk runtime behavior, "." can resolve to either BizHawk root or Lua dir.
    const rootX64Dir = path.join(dir, "x64");
    const luaX64Dir = path.join(luaDir, "x64");
    const srcX64Dir = path.join(apLuaDir, "x64");
    const neededNative =
      process.platform === "win32"
        ? ["socket-windows-5-4.dll", "socket-windows-5-1.dll"]
        : ["socket-linux-5-4.so", "socket-linux-5-1.so"];
    for (const file of neededNative) {
      const src = path.join(srcX64Dir, file);
      if (!fs.existsSync(src)) {
        missing.push(`x64/${file}`);
        continue;
      }
      for (const targetDir of [rootX64Dir, luaX64Dir]) {
        const destPath = path.join(targetDir, file);
        if (fs.existsSync(destPath)) continue;
        ensureDir(targetDir);
        fs.copyFileSync(src, destPath);
        try {
          fs.chmodSync(destPath, 0o755);
        } catch (_err) {
          // ignore chmod failures
        }
      }
    }

    if (missing.length) {
      return {
        ok: false,
        error: "lua_compat_missing",
        detail: `missing lua runtime files for ${dir}: ${missing.join(", ")}`,
      };
    }
    return { ok: true };
  } catch (err) {
    return { ok: false, error: "lua_compat_copy_failed", detail: String(err || "") };
  }
};

const stageBizHawkConnectorLua = (sourceLuaPath, baseDir, moduleId = "") => {
  try {
    const src = String(sourceLuaPath || "").trim();
    if (!src || !fs.existsSync(src)) {
      return { ok: false, error: "lua_not_found", detail: `source connector missing: ${src}` };
    }
    const dir = String(baseDir || "").trim() || getBizHawkBaseDir();
    const luaDir = path.join(dir, "Lua");
    ensureDir(luaDir);
    const safeId = String(moduleId || "module").replace(/[^a-zA-Z0-9_-]/g, "_") || "module";
    const stagedPath = path.join(luaDir, `sekailink_connector_${safeId}.lua`);
    fs.copyFileSync(src, stagedPath);
    return { ok: true, luaPath: stagedPath };
  } catch (err) {
    return { ok: false, error: "lua_stage_failed", detail: String(err || "") };
  }
};

const getBundledBizHawkLibsDir = () => {
  // Optional native libs we bundle specifically to make BizHawk+Mono work on distros
  // that do not ship libgdiplus by default.
  return path.join(getBundledRuntimeDir(), "_bundled_libs", "bizhawk");
};

const launchBizHawk = async (options = {}) => {
  const modulesBase = path.join(__dirname, "..", "..", "runtime", "modules");
  const install = await ensureBizHawkInstalled();
  if (!install.ok) return { ok: false, error: install.error || "bizhawk_stage_failed", detail: install.detail || "" };

  const baseDir = String(options.baseDir || "").trim() || install.baseDir;
  const defaultEmuName = process.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh";
  const emuPath = options.emuPath || install.emuPath || path.join(baseDir, defaultEmuName);
  const romPath = options.romPath;
  let luaPath = options.luaPath;
  if (!luaPath && options.moduleId) {
    const manifest = getModuleManifest(options.moduleId);
    const rel = options.luaRelPath || manifest?.lua_connector || path.join("lua", "connector.lua");
    luaPath = path.join(modulesBase, options.moduleId, rel);
  }

  if (!romPath) return { ok: false, error: "missing_rom" };
  if (!luaPath) return { ok: false, error: "missing_lua" };
  if (!fs.existsSync(luaPath)) return { ok: false, error: "lua_not_found" };

  if (!fs.existsSync(emuPath)) return { ok: false, error: "bizhawk_missing" };
  let monoBin = "";
  if (process.platform !== "win32") {
    try {
      monoBin = await ensureMonoRuntime();
    } catch (err) {
      const detail = String(err || "");
      writeLogLine("error", "bizhawk", `mono ensure failed: ${detail}`);
      return { ok: false, error: "mono_missing", detail };
    }
    try {
      fs.chmodSync(emuPath, 0o755);
    } catch (_err) {
      // ignore chmod failures
    }
  }

  const configResult = ensureBizHawkConfig(baseDir);
  if (!configResult.ok) return configResult;
  const luaCompatResult = ensureBizHawkLuaCompat(baseDir);
  if (!luaCompatResult.ok) return luaCompatResult;
  const stagedLuaResult = stageBizHawkConnectorLua(luaPath, baseDir, options.moduleId || "manual");
  if (!stagedLuaResult.ok) return stagedLuaResult;
  luaPath = stagedLuaResult.luaPath;

  const args = [`--lua=${luaPath}`, romPath];
  const spawnEnv = { ...process.env };
  try {
    const logsDir = path.join(app.getPath("userData"), "logs");
    ensureDir(logsDir);
    spawnEnv.SEKAILINK_LUA_LOG_PATH = path.join(logsDir, "bizhawk-lua-connector.log");
  } catch (_err) {
    // best-effort only
  }

  if (process.platform !== "win32") {
    // If mono is a downloaded portable runtime, help it find its config/GAC/native libs.
    // Layout assumed: <monoHome>/{bin/mono, lib/mono, etc/mono, lib/*.so}
    try {
      const monoHome = path.resolve(path.dirname(monoBin), "..");
      const portableCfgDir = path.join(monoHome, "etc");
      if (fs.existsSync(path.join(portableCfgDir, "mono"))) {
        spawnEnv.MONO_CFG_DIR = portableCfgDir;
      }
      const gacA = path.join(monoHome, "lib", "mono", "gac");
      const gacB = path.join(monoHome, "lib64", "mono", "gac");
      if (fs.existsSync(gacA) || fs.existsSync(gacB)) {
        spawnEnv.MONO_GAC_PREFIX = monoHome;
      }
      const monoLibDirs = [path.join(monoHome, "lib"), path.join(monoHome, "lib64")].filter((d) => fs.existsSync(d));
      if (monoLibDirs.length) {
        const prior = spawnEnv.LD_LIBRARY_PATH || "";
        spawnEnv.LD_LIBRARY_PATH = prior ? `${monoLibDirs.join(":")}:${prior}` : monoLibDirs.join(":");
      }
    } catch (_err) {
      // best-effort
    }
  
    // Fedora-like systems often place mono helper libs in /usr/lib64.
    // Keep these system paths as a last-resort fallback even when using bundled mono.
    if (process.platform === "linux") {
      const fallbackLibDirs = ["/usr/lib64", "/usr/lib"].filter((d) => fs.existsSync(d));
      if (fallbackLibDirs.length) {
        const prior = spawnEnv.LD_LIBRARY_PATH || "";
        const missing = fallbackLibDirs.filter((d) => !prior.split(":").includes(d));
        if (missing.length) {
          spawnEnv.LD_LIBRARY_PATH = prior ? `${prior}:${missing.join(":")}` : missing.join(":");
        }
      }
    }
  
    // If we bundled libgdiplus (or other native deps) for BizHawk, prepend them for the emulator process.
    // EmuHawkMono.sh must preserve LD_LIBRARY_PATH for this to work (we patch it in third_party).
    const extraLibDir = getBundledBizHawkLibsDir();
    if (fs.existsSync(extraLibDir)) {
      const prior = spawnEnv.LD_LIBRARY_PATH || "";
      spawnEnv.LD_LIBRARY_PATH = prior ? `${extraLibDir}:${prior}` : extraLibDir;
    }
    // Allow overriding mono (e.g. portable install) without relying on PATH.
    spawnEnv.SEKAILINK_MONO = monoBin;
  }

  const wrapped = spawnMaybeGamescope(emuPath, args, { stdio: "ignore", env: spawnEnv });
  if (!wrapped.ok) return wrapped;
  const proc = wrapped.proc;
  bizhawkProcs.set(proc.pid, proc);
  proc.on("exit", (code, signal) => {
    const exitedPid = proc.pid;
    bizhawkProcs.delete(exitedPid);
    triggerCoupledRuntimeTeardown("bizhawk", exitedPid, { code, signal });
  });
  return { ok: true, pid: proc.pid, gamescope: Boolean(wrapped.wrapped) };
};

const stopBizHawk = async (pid) => {
  if (pid && bizhawkProcs.has(pid)) {
    const proc = bizhawkProcs.get(pid);
    await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
    bizhawkProcs.delete(pid);
    return { ok: true };
  }
  return { ok: false, error: "not_running" };
};

const getModuleManifest = (moduleId) => {
  if (!moduleId) return null;
  for (const modulesDir of getRuntimeModulesDirs()) {
    const manifestPath = path.join(modulesDir, moduleId, "manifest.json");
    if (!fs.existsSync(manifestPath)) continue;
    try {
      return JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
    } catch (_err) {
      return null;
    }
  }
  return null;
};

const listRuntimeModules = () => {
  const moduleIds = new Set();
  const results = [];
  for (const modulesDir of getRuntimeModulesDirs()) {
    if (!fs.existsSync(modulesDir)) continue;
    const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isDirectory()) continue;
      if (moduleIds.has(entry.name)) continue;
      moduleIds.add(entry.name);
      const manifest = getModuleManifest(entry.name);
      if (!manifest) continue;
      results.push({ moduleId: entry.name, manifest });
    }
  }
  results.sort((a, b) => String(a.moduleId).localeCompare(String(b.moduleId)));
  return results;
};

const isUrl = (value) => /^https?:\/\//i.test(String(value || ""));

const getInstalledTrackerPack = (gameId) => {
  const config = readConfig();
  const packs = config.trackerPacks || {};
  return packs[gameId] || null;
};

const getTrackerVariant = (gameId) => {
  const config = readConfig();
  const variants = config.trackerVariants || {};
  const value = variants[gameId];
  return typeof value === "string" ? value : "";
};

const setTrackerVariant = (gameId, variant) => {
  const config = readConfig();
  if (!config.trackerVariants) config.trackerVariants = {};
  let v = typeof variant === "string" ? variant : "";
  if (v === "default") v = "";
  config.trackerVariants[gameId] = v;
  writeConfig(config);
  return { ok: true };
};

const resolveGithubRepo = (source) => {
  if (!source) return null;
  if (/^[^/]+\/[^/]+$/.test(source)) return source;
  const match = String(source).match(/github\.com\/([^/]+\/[^/]+)/i);
  return match ? match[1] : null;
};

const resolveGithubAssetInfo = async (repo, assetRegex) => {
  const data = await httpGetJson(`https://api.github.com/repos/${repo}/releases/latest`);
  const assets = Array.isArray(data?.assets) ? data.assets : [];
  if (!assets.length) return null;
  const regex = assetRegex ? new RegExp(assetRegex, "i") : null;
  const candidates = regex ? assets.filter((asset) => regex.test(asset.name)) : assets;
  const target = candidates.find((asset) => asset.browser_download_url) || assets[0];
  if (!target?.browser_download_url) return null;
  const digest = String(target?.digest || "").trim().toLowerCase();
  const expectedSha256 = digest.startsWith("sha256:") ? digest.slice("sha256:".length) : "";
  return {
    url: target.browser_download_url,
    expectedSha256: normalizeSha256(expectedSha256),
    assetName: String(target?.name || ""),
  };
};

const extractZip = (zipPath, destDir) => {
  ensureDir(destDir);
  if (process.platform === "win32") {
    // Secure extraction with path traversal checks.
    const esc = (value) => String(value || "").replace(/'/g, "''");
    const script = `
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath='${esc(zipPath)}'
$destDir='${esc(destDir)}'
[System.IO.Directory]::CreateDirectory($destDir) | Out-Null
$destRoot = [System.IO.Path]::GetFullPath($destDir + [System.IO.Path]::DirectorySeparatorChar)
$zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
  foreach ($entry in $zip.Entries) {
    $name = ($entry.FullName -replace '\\\\','/').Trim()
    if ([string]::IsNullOrWhiteSpace($name)) { continue }
    if ($name.StartsWith('/') -or $name.StartsWith('\\\\') -or ($name -match '^[A-Za-z]:')) { throw 'zip_path_traversal' }
    $parts = $name.Split('/')
    if ($parts -contains '..') { throw 'zip_path_traversal' }
    $target = [System.IO.Path]::GetFullPath((Join-Path $destDir $name))
    if (-not $target.StartsWith($destRoot, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'zip_path_traversal' }
    if ($name.EndsWith('/')) {
      [System.IO.Directory]::CreateDirectory($target) | Out-Null
      continue
    }
    $parent = [System.IO.Path]::GetDirectoryName($target)
    if ($parent) { [System.IO.Directory]::CreateDirectory($parent) | Out-Null }
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
  }
} finally {
  $zip.Dispose()
}
`.trim();
    const ps = spawnSync(
      "powershell",
      ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script],
      { stdio: "ignore" }
    );
    if (ps.status === 0) return;
  }

  // POSIX fallback: safe extraction with explicit checks and symlink blocking.
  const python = getPythonCommand();
  const code = `
import os, stat, sys, zipfile
zip_path = sys.argv[1]
dest_dir = os.path.abspath(sys.argv[2])
os.makedirs(dest_dir, exist_ok=True)
dest_root = dest_dir + os.sep
with zipfile.ZipFile(zip_path) as z:
  for info in z.infolist():
    name = (info.filename or '').replace('\\\\\\\\', '/').strip()
    if not name:
      continue
    if name.startswith('/') or name.startswith('\\\\\\\\') or ':' in name.split('/')[0]:
      raise RuntimeError('zip_path_traversal')
    parts = [p for p in name.split('/') if p]
    if '..' in parts:
      raise RuntimeError('zip_path_traversal')
    mode_type = (info.external_attr >> 16) & 0o170000
    if mode_type == stat.S_IFLNK:
      raise RuntimeError('zip_symlink_forbidden')
    out_path = os.path.abspath(os.path.join(dest_dir, name))
    if not (out_path == dest_dir or out_path.startswith(dest_root)):
      raise RuntimeError('zip_path_traversal')
    if info.is_dir() or name.endswith('/'):
      os.makedirs(out_path, exist_ok=True)
      continue
    parent = os.path.dirname(out_path)
    os.makedirs(parent, exist_ok=True)
    with z.open(info) as src, open(out_path, 'wb') as dst:
      dst.write(src.read())
    mode = (info.external_attr >> 16) & 0o777
    if mode:
      try:
        os.chmod(out_path, mode)
      except OSError:
        pass
`.trim();
  const res = spawnSync(python, ["-c", code, zipPath, destDir], { stdio: "ignore" });
  if (res.status !== 0) throw new Error("zip_extract_failed");
};

const resolvePackRoot = (destDir) => {
  const entries = fs.readdirSync(destDir, { withFileTypes: true });
  const dirs = entries.filter((entry) => entry.isDirectory());
  if (dirs.length === 1 && entries.length === 1) {
    return path.join(destDir, dirs[0].name);
  }
  return destDir;
};

const installTrackerPack = async (options = {}) => {
  const gameId = options.gameId;
  if (!gameId) return { ok: false, error: "missing_game_id" };

  let downloadUrl = options.packUrl;
  let expectedSha256 = normalizeSha256(options.sha256 || options.packSha256);
  if (!downloadUrl && options.packRepo) {
    const repo = resolveGithubRepo(options.packRepo);
    if (!repo) return { ok: false, error: "invalid_repo" };
    const assetInfo = await resolveGithubAssetInfo(repo, options.assetRegex || "\\.zip$");
    if (assetInfo?.url) {
      downloadUrl = assetInfo.url;
      if (!expectedSha256) expectedSha256 = assetInfo.expectedSha256;
    }
  }
  if (!downloadUrl) return { ok: false, error: "missing_pack_url" };
  if (!expectedSha256) return { ok: false, error: "missing_pack_hash" };

  const baseDir = path.join(getPopTrackerPacksDir(), gameId);
  ensureDir(baseDir);

  const fileName = path.basename(new URL(downloadUrl).pathname || "pack.zip");
  const zipPath = path.join(baseDir, fileName);
  await downloadToFile(downloadUrl, zipPath, {
    expectedSha256,
    requireHash: true,
  });

  const extractDir = path.join(baseDir, "pack");
  if (fs.existsSync(extractDir)) {
    fs.rmSync(extractDir, { recursive: true, force: true });
  }
  extractZip(zipPath, extractDir);
  const packRoot = resolvePackRoot(extractDir);
  const validation = validatePackDir(packRoot);
  if (!validation.ok) {
    return { ok: false, error: validation.error || "invalid_pack" };
  }

  const config = readConfig();
  if (!config.trackerPacks) config.trackerPacks = {};
  config.trackerPacks[gameId] = {
    path: packRoot,
    source: downloadUrl,
    sha256: expectedSha256,
    installed_at: new Date().toISOString()
  };
  writeConfig(config);

  return { ok: true, path: packRoot };
};

const validatePackDir = (dirPath) => {
  if (!dirPath || !fs.existsSync(dirPath)) return { ok: false, error: "pack_missing" };
  const manifestPath = path.join(dirPath, "manifest.json");
  if (!fs.existsSync(manifestPath)) return { ok: false, error: "pack_manifest_missing" };
  try {
    // Some packs ship a UTF-8 BOM; strip it so JSON.parse doesn't fail.
    const raw = fs.readFileSync(manifestPath, "utf-8").replace(/^\ufeff/, "");
    const manifest = JSON.parse(raw);
    if (!manifest) return { ok: false, error: "pack_manifest_invalid" };
    if (!manifest.name && !manifest.package_uid) return { ok: false, error: "pack_manifest_invalid" };
  } catch (_err) {
    return { ok: false, error: "pack_manifest_invalid" };
  }
  return { ok: true };
};

const getPopTrackerStatus = () => {
  const exePath = getPopTrackerExePath();
  const source = _popTrackerExeSource || (exePath.includes(`${path.sep}third_party${path.sep}`) ? "third_party" : "runtime");
  return { ok: true, exists: fs.existsSync(exePath), path: exePath, source };
};

const listPopTrackerPackVariants = (uidOrPath, packVersion = "") => {
  const baseDir = getPopTrackerDir();
  const exePath = getPopTrackerExePath();
  if (!fs.existsSync(exePath)) return { ok: false, error: "poptracker_missing" };
  if (!uidOrPath) return { ok: false, error: "missing_pack" };

  const args = ["--list-pack-variants", uidOrPath];
  if (packVersion) args.push("--pack-version", packVersion);
  const res = spawnSync(exePath, args, { stdio: ["ignore", "pipe", "pipe"], cwd: baseDir, env: process.env });
  const stdout = String(res.stdout || "");
  const stderr = String(res.stderr || "");
  if (res.status !== 0) {
    return { ok: false, error: "variant_list_failed", stdout, stderr };
  }

  const variants = [];
  const lines = stdout.split(/\r?\n/);
  let inVariants = false;
  for (const rawLine of lines) {
    const line = String(rawLine || "").trimEnd();
    if (!line) continue;
    if (line.trim() === "Variants:") {
      inVariants = true;
      continue;
    }
    if (!inVariants) continue;
    const m = line.match(/^\s*([^\s:]+)\s*:\s*(.+)\s*$/);
    if (!m) continue;
    const id = m[1];
    const name = m[2];
    variants.push({ id, name });
  }
  return { ok: true, variants, stdout };
};

const chooseTrackerPackVariantForLaunch = async (options = {}) => {
  const gameId = String(options.gameId || "").trim();
  const gameLabel = String(options.gameLabel || gameId || "this game").trim();
  const currentVariant = String(options.currentVariant || "").trim();
  const uidOrPath = String(options.uidOrPath || "").trim();
  const forcePrompt = options.forcePrompt === true;
  if (!gameId) return { ok: true, variant: currentVariant };
  if (!uidOrPath) return { ok: true, variant: currentVariant };
  if (currentVariant && !forcePrompt) return { ok: true, variant: currentVariant };

  const variantsRes = listPopTrackerPackVariants(uidOrPath);
  if (!variantsRes?.ok || !Array.isArray(variantsRes.variants)) return { ok: true, variant: "" };

  const variants = variantsRes.variants
    .map((v) => ({
      id: String(v?.id || "").trim(),
      name: String(v?.name || v?.id || "").trim(),
    }))
    .filter((v) => v.id && v.name);

  if (!variants.length) return { ok: true, variant: "" };
  if (variants.length === 1 && !forcePrompt) {
    const only = variants[0].id;
    setTrackerVariant(gameId, only);
    return { ok: true, variant: only };
  }

  const choices = [{ id: "", name: "Default (Recommended)" }, ...variants].slice(0, 9);
  const detail = choices.map((c, idx) => `${idx + 1}. ${c.name} [${c.id || "default"}]`).join("\n");

  if (!mainWindow || mainWindow.isDestroyed()) {
    const fallback = choices[0] || { id: "", name: "Default" };
    setTrackerVariant(gameId, fallback.id || "");
    return { ok: true, variant: fallback.id || "" };
  }

  const requestId = `${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
  const picked = await new Promise((resolve) => {
    const timer = setTimeout(() => {
      pendingTrackerVariantRequests.delete(requestId);
      resolve({ cancel: true, variant: "" });
    }, 120000);
    pendingTrackerVariantRequests.set(requestId, {
      resolve: (payload) => {
        clearTimeout(timer);
        resolve(payload);
      },
    });
    emitSessionEvent({
      event: "tracker_variant_request",
      requestId,
      gameId,
      gameLabel,
      title: "Choose PopTracker Layout",
      message: `Select a tracker layout for ${gameLabel}.`,
      detail,
      choices,
      defaultId: 0,
    });
  });

  if (!picked || picked.cancel) return { ok: false, error: "tracker_variant_cancelled" };
  const wanted = String(picked.variant || "");
  const selected = choices.find((c) => c.id === wanted) || choices[0];
  setTrackerVariant(gameId, selected.id || "");
  return { ok: true, variant: selected.id || "" };
};

const buildWebTrackerLaunchUrl = (baseUrl, options = {}) => {
  if (!isUrl(baseUrl)) return null;
  try {
    const url = new URL(baseUrl);
    const apHost = String(options.apHost || "").trim();
    const apSlot = String(options.apSlot || "").trim();
    if (apHost) {
      if (!url.searchParams.has("ap_host")) url.searchParams.set("ap_host", apHost);
      if (!url.searchParams.has("host")) url.searchParams.set("host", apHost);
    }
    if (apSlot) {
      if (!url.searchParams.has("ap_slot")) url.searchParams.set("ap_slot", apSlot);
      if (!url.searchParams.has("slot")) url.searchParams.set("slot", apSlot);
    }
    if (options.apAutoconnect && !url.searchParams.has("autoconnect")) {
      url.searchParams.set("autoconnect", "1");
    }
    return url.toString();
  } catch (_err) {
    return null;
  }
};

const launchWebTrackerWindow = (options = {}) => {
  const finalUrl = buildWebTrackerLaunchUrl(options.url, options);
  if (!finalUrl) {
    emitTrackerClientLog("error", `Invalid web tracker URL: ${String(options.url || "")}`);
    return { ok: false, error: "invalid_web_tracker_url" };
  }

  const title = String(options.title || "Web Tracker").trim() || "Web Tracker";
  const win = new BrowserWindow({
    width: 1280,
    height: 860,
    minWidth: 980,
    minHeight: 640,
    backgroundColor: "#05070A",
    show: false,
    title,
    icon: resolveWindowIconPath(),
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  win.once("ready-to-show", () => {
    if (!win.isDestroyed()) win.show();
  });
  win.on("closed", () => {
    trackerWebWindows.delete(win);
  });
  win.webContents.setWindowOpenHandler(({ url }) => {
    void openExternalSafely(url, "window-open");
    return { action: "deny" };
  });

  trackerWebWindows.add(win);
  win.loadURL(finalUrl).catch((err) => {
    emitTrackerClientLog("error", `Web tracker failed to load: ${String(err)}`);
  });

  emitTrackerClientLog("info", `Launching web tracker: ${finalUrl}`);
  let pid = null;
  try {
    const osPid = Number(win.webContents.getOSProcessId());
    if (Number.isFinite(osPid) && osPid > 0) pid = osPid;
  } catch (_err) {
    pid = null;
  }
  return { ok: true, pid, mode: "web", url: finalUrl };
};

const launchPopTracker = async (options = {}) => {
  const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;

  let packUid = options.packUid;
  let packPath = options.packPath;
  let packVariant = options.packVariant;
  let gameId = String(options.gameId || "").trim();
  let gameLabel = gameId;
  let manifest = null;
  let trackerWebUrl = String(options.trackerWebUrl || "").trim();
  let trackerWebTitle = "";

  if (!packUid && !packPath && options.moduleId) {
    manifest = getModuleManifest(options.moduleId);
    gameId = String(manifest?.game_id || gameId || "").trim();
    gameLabel = String(manifest?.display_name || manifest?.game_name || gameId || "").trim();
    trackerWebUrl = trackerWebUrl || String(manifest?.tracker_web_url || "").trim();
    trackerWebTitle = String(manifest?.tracker_web_title || gameLabel || "Web Tracker").trim();
    if (gameId) {
      const installed = getInstalledTrackerPack(gameId);
      if (installed?.path) packPath = installed.path;
      if (!packVariant) {
        const savedVariant = forceTrackerVariantPrompt ? "" : getTrackerVariant(gameId);
        if (savedVariant) packVariant = savedVariant;
      }
    }
    // If the module declares a tracker pack source, prefer installing to a stable local path
    // and launch via packPath (more deterministic than UIDs/remote sources).
    if (!packPath && (manifest?.tracker_pack_uid || manifest?.tracker_pack_url)) {
      const trackerPackSource = String(manifest?.tracker_pack_uid || "").trim();
      const trackerPackUrl = String(manifest?.tracker_pack_url || "").trim();
      const trackerPackSha = String(manifest?.tracker_pack_sha256 || "").trim();
      const trackerAssetRegex = String(manifest?.tracker_asset_regex || "\\.zip$").trim();
      if (gameId && trackerPackUrl) {
        const installed = await installTrackerPack({
          gameId,
          packUrl: trackerPackUrl,
          packSha256: trackerPackSha,
        });
        if (installed?.ok && installed.path) packPath = installed.path;
      } else {
        const repo = resolveGithubRepo(trackerPackSource);
        if (repo && gameId) {
          const installed = await installTrackerPack({
            gameId,
            packRepo: repo,
            packSha256: trackerPackSha,
            assetRegex: trackerAssetRegex,
          });
          if (installed?.ok && installed.path) packPath = installed.path;
        } else if (gameId && isUrl(trackerPackSource)) {
          const installed = await installTrackerPack({
            gameId,
            packUrl: trackerPackSource,
            packSha256: trackerPackSha,
          });
          if (installed?.ok && installed.path) packPath = installed.path;
        } else if (!packUid && !isUrl(trackerPackSource)) {
          // Legacy: allow loading by UID when it's not a URL.
          packUid = trackerPackSource;
        }
      }
    }
  }

  if (packUid && isUrl(packUid)) {
    packUid = null;
  }

  if (packPath && fs.existsSync(packPath)) {
    const check = validatePackDir(packPath);
    if (!check.ok) {
      emitTrackerClientLog("warn", `Installed tracker pack is invalid (${check.error || "invalid_pack"}). Reinstalling...`);
      packPath = null;
      if (manifest && gameId && (manifest?.tracker_pack_uid || manifest?.tracker_pack_url)) {
        const trackerPackSource = String(manifest?.tracker_pack_uid || "").trim();
        const trackerPackUrl = String(manifest?.tracker_pack_url || "").trim();
        const trackerPackSha = String(manifest?.tracker_pack_sha256 || "").trim();
        const trackerAssetRegex = String(manifest?.tracker_asset_regex || "\\.zip$").trim();
        if (trackerPackUrl) {
          const reinstalled = await installTrackerPack({
            gameId,
            packUrl: trackerPackUrl,
            packSha256: trackerPackSha,
          });
          if (reinstalled?.ok && reinstalled.path) packPath = reinstalled.path;
        } else {
          const repo = resolveGithubRepo(trackerPackSource);
          if (repo) {
            const reinstalled = await installTrackerPack({
              gameId,
              packRepo: repo,
              packSha256: trackerPackSha,
              assetRegex: trackerAssetRegex,
            });
            if (reinstalled?.ok && reinstalled.path) packPath = reinstalled.path;
          } else if (isUrl(trackerPackSource)) {
            const reinstalled = await installTrackerPack({
              gameId,
              packUrl: trackerPackSource,
              packSha256: trackerPackSha,
            });
            if (reinstalled?.ok && reinstalled.path) packPath = reinstalled.path;
          }
        }
      }
    }
  }

  if (!packUid && !packPath && trackerWebUrl) {
    return launchWebTrackerWindow({
      url: trackerWebUrl,
      title: trackerWebTitle,
      apHost: options.apHost,
      apSlot: options.apSlot,
      apAutoconnect: options.apAutoconnect,
    });
  }

  const exePath = getPopTrackerExePath();
  const baseDir = path.dirname(exePath);
  ensurePopTrackerPortableConfig(baseDir);
  // FF4 pack can retain stale character states across reconnect/pause cycles; start clean per launch.
  if (packPath && gameId === "ff4fe") clearPopTrackerAutosaveForPack(baseDir, packPath);
  if (!fs.existsSync(exePath)) {
    if (trackerWebUrl) {
      return launchWebTrackerWindow({
        url: trackerWebUrl,
        title: trackerWebTitle,
        apHost: options.apHost,
        apSlot: options.apSlot,
        apAutoconnect: options.apAutoconnect,
      });
    }
    emitTrackerClientLog("error", "PopTracker binary not found.");
    return { ok: false, error: "poptracker_missing" };
  }

  if (!packUid && !packPath) {
    emitTrackerClientLog("error", "No PopTracker pack configured for this game.");
    return { ok: false, error: "no_pack" };
  }
  if (packPath && !fs.existsSync(packPath)) {
    emitTrackerClientLog("error", `PopTracker pack path missing: ${packPath}`);
    return { ok: false, error: "pack_missing" };
  }
  if (gameId && (!packVariant || forceTrackerVariantPrompt)) {
    const picked = await chooseTrackerPackVariantForLaunch({
      gameId,
      gameLabel,
      currentVariant: packVariant,
      uidOrPath: packPath || packUid || "",
      forcePrompt: forceTrackerVariantPrompt,
    });
    if (!picked?.ok) {
      emitTrackerClientLog("warn", "PopTracker layout selection cancelled.");
      return { ok: false, error: picked?.error || "tracker_variant_select_failed" };
    }
    packVariant = picked.variant || "";
  }

  try {
    fs.chmodSync(exePath, 0o755);
  } catch (_err) {
    // ignore chmod failures
  }

  const args = [];
  // PopTracker CLI expects action args first, then the action/pack path last.
  // If path/action is passed first, it prints usage and exits with code 1.
  if (packVariant && packVariant !== "default") args.push("--pack-variant", packVariant);

  if (options.apHost) {
    const apUri = String(options.apHost || "").trim();
    // PopTracker expects an AP websocket URI. If we pass host:port without a scheme,
    // it may flip between ws:// and wss:// on error and end up with TLS handshake failures.
    args.push("--ap-host", apUri.includes("://") ? apUri : `ws://${apUri}`);
  }
  if (options.apSlot) args.push("--ap-slot", options.apSlot);
  // Security: keep password out of argv. PopTracker supports --ap-pass-env.
  const spawnEnv = { ...process.env };
  const bundledLibDir = getBundledPopTrackerLibsDir();
  if (fs.existsSync(bundledLibDir)) {
    const prior = spawnEnv.LD_LIBRARY_PATH || "";
    spawnEnv.LD_LIBRARY_PATH = prior ? `${bundledLibDir}:${prior}` : bundledLibDir;
  }
  if (typeof options.apPass === "string" && options.apPass.length) {
    spawnEnv.SEKAILINK_AP_PASS = options.apPass;
    args.push("--ap-pass-env", "SEKAILINK_AP_PASS");
  }
  if (options.apAutoconnect) args.push("--ap-autoconnect");
  if (packUid) {
    args.push("--load-pack", packUid);
  } else if (packPath) {
    args.push(packPath);
  }

  emitTrackerClientLog("info", `Launching PopTracker (${_popTrackerExeSource || "runtime"}) with ${packVariant || "default"} layout.`);

  const proc = spawn(exePath, args, { stdio: ["ignore", "pipe", "pipe"], cwd: baseDir, env: spawnEnv });
  let stdoutTail = "";
  let stdoutRemainder = "";
  proc.stdout.on("data", (chunk) => {
    const text = String(chunk || "");
    if (!text) return;
    stdoutTail = (stdoutTail + text).slice(-4000);
    stdoutRemainder += text;
    const parts = stdoutRemainder.split(/\r?\n/);
    stdoutRemainder = parts.pop() || "";
    for (const rawLine of parts) {
      const line = rawLine.trim();
      if (!line) continue;
      emitTrackerClientLog("info", `PopTracker stdout: ${line}`);
    }
  });
  let stderrTail = "";
  let stderrRemainder = "";
  proc.stderr.on("data", (chunk) => {
    const text = String(chunk || "");
    if (!text) return;
    stderrTail = (stderrTail + text).slice(-4000);
    stderrRemainder += text;
    const parts = stderrRemainder.split(/\r?\n/);
    stderrRemainder = parts.pop() || "";
    for (const rawLine of parts) {
      const line = rawLine.trim();
      if (!line) continue;
      emitTrackerClientLog("warn", `PopTracker stderr: ${line}`);
    }
  });
  proc.on("error", (err) => {
    emitTrackerClientLog("error", `PopTracker process error: ${String(err)}`);
  });

  trackerProcs.set(proc.pid, proc);
  proc.on("exit", (code, signal) => {
    const exitedPid = proc.pid;
    const lastStdout = stdoutRemainder.trim();
    if (lastStdout) {
      emitTrackerClientLog("info", `PopTracker stdout: ${lastStdout}`);
    }
    const lastStderr = stderrRemainder.trim();
    if (lastStderr) {
      emitTrackerClientLog("warn", `PopTracker stderr: ${lastStderr}`);
    }
    emitTrackerClientLog("warn", `PopTracker exited (code=${code ?? "null"}, signal=${signal || "none"}).`);
    trackerProcs.delete(exitedPid);
    triggerCoupledRuntimeTeardown("tracker", exitedPid, { code, signal });
  });

  const earlyExit = await new Promise((resolve) => {
    let done = false;
    const timer = setTimeout(() => {
      if (done) return;
      done = true;
      resolve({ exited: false });
    }, 1200);
    proc.once("exit", (code, signal) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      resolve({ exited: true, code, signal });
    });
    proc.once("error", (err) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      resolve({ exited: true, code: null, signal: null, error: String(err) });
    });
  });

  if (earlyExit && earlyExit.exited) {
    trackerProcs.delete(proc.pid);
    const detail = String(earlyExit.error || `code=${earlyExit.code ?? "null"} signal=${earlyExit.signal || "none"}`).trim();
    const errTail = String(stderrTail || "").trim();
    const outTail = String(stdoutTail || "").trim();
    const tailLine =
      (errTail ? errTail.split(/\r?\n/).filter(Boolean).slice(-1)[0] : "") ||
      (outTail ? outTail.split(/\r?\n/).filter(Boolean).slice(-1)[0] : "");
    emitTrackerClientLog("error", `PopTracker exited immediately (${detail})${tailLine ? `: ${tailLine}` : ""}`);
    return { ok: false, error: "tracker_exited_early", detail: tailLine || detail };
  }

  return { ok: true, pid: proc.pid };
};

const stopPopTracker = async (pid) => {
  if (pid && trackerProcs.has(pid)) {
    const proc = trackerProcs.get(pid);
    await terminateChildProcess(proc, "tracker", { graceMs: 900 });
    trackerProcs.delete(pid);
    return { ok: true };
  }
  return { ok: false, error: "not_running" };
};

const triggerCoupledRuntimeTeardown = (origin = "unknown", pid = null, extra = {}) => {
  if (runtimeShutdownPromise || coupledRuntimeTeardownPromise) return;
  coupledRuntimeTeardownPromise = (async () => {
    try {
      const code = extra && Object.prototype.hasOwnProperty.call(extra, "code") ? extra.code : null;
      const signal = extra && Object.prototype.hasOwnProperty.call(extra, "signal") ? extra.signal : null;
      writeLogLine(
        "warn",
        "runtime-coupling",
        `peer exited origin=${origin} pid=${pid || ""} code=${code ?? "null"} signal=${signal || "none"} -> stopping emulator/tracker/SNI`
      );

      for (const [otherPid, proc] of Array.from(trackerProcs.entries())) {
        await terminateChildProcess(proc, "tracker", { graceMs: 900 });
        trackerProcs.delete(otherPid);
      }
      for (const [otherPid, proc] of Array.from(bizhawkProcs.entries())) {
        await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
        bizhawkProcs.delete(otherPid);
      }
      await stopBizHawkClient();
    } catch (err) {
      writeLogLine("warn", "runtime-coupling", `teardown failed: ${String(err || "")}`);
    } finally {
      coupledRuntimeTeardownPromise = null;
    }
  })();
};

let runtimeShutdownPromise = null;
const shutdownRuntimeProcesses = async (reason = "shutdown") => {
  if (runtimeShutdownPromise) return runtimeShutdownPromise;
  runtimeShutdownPromise = (async () => {
    writeLogLine("info", "runtime-shutdown", `begin reason=${reason}`);
    await stopCommonClient();
    await stopBizHawkClient();

    for (const [pid, proc] of Array.from(trackerProcs.entries())) {
      await terminateChildProcess(proc, "tracker", { graceMs: 900 });
      trackerProcs.delete(pid);
    }

    for (const [pid, proc] of Array.from(bizhawkProcs.entries())) {
      await terminateChildProcess(proc, "bizhawk", { graceMs: 1100 });
      bizhawkProcs.delete(pid);
    }

    for (const [pid, proc] of Array.from(nativeGameProcs.entries())) {
      await terminateChildProcess(proc, "native", { graceMs: 900 });
      nativeGameProcs.delete(pid);
    }

    for (const [pid, session] of Array.from(linkedWorldProcs.entries())) {
      if (session?.server) await terminateChildProcess(session.server, "linkedworld-server", { graceMs: 900 });
      linkedWorldProcs.delete(pid);
    }

    await purgeStaleSniBridgePortHolders(23074, 0);
    writeLogLine("info", "runtime-shutdown", `complete reason=${reason}`);
  })();
  try {
    return await runtimeShutdownPromise;
  } finally {
    runtimeShutdownPromise = null;
  }
};

const startCommonClient = async (options = {}) => {
  if (commonClientProc) return { ok: true, alreadyRunning: true };

  const python = await ensurePythonRuntime();
  const wrapperPath = getCommonClientWrapperPath();
  // Security: avoid putting secrets in argv. We send connection details via stdin JSON commands.
  // Also avoid passing address/slot to keep the startup argv minimal and consistent.
  const args = [wrapperPath];

  commonClientProc = spawn(python, args, {
    stdio: ["pipe", "pipe", "pipe"],
    env: withApPythonEnv(process.env)
  });
  writeLogLine("info", "commonclient", `spawned pid=${commonClientProc.pid || ""} python=${python} wrapper=${wrapperPath}`);

  commonClientRl = readline.createInterface({ input: commonClientProc.stdout });
  commonClientRl.on("line", (line) => {
    const trimmed = line.trim();
    if (!trimmed) return;
    try {
      const data = JSON.parse(trimmed);
      emitCommonClientEvent(data);
    } catch (_err) {
      emitCommonClientEvent({ event: "error", message: "Invalid JSON from CommonClient" });
    }
  });

  commonClientProc.stderr.on("data", (chunk) => {
    emitCommonClientEvent({ event: "log", level: "error", logger: "CommonClient", message: String(chunk).trim() });
  });

  commonClientProc.on("exit", (code, signal) => {
    emitCommonClientEvent({ event: "exit", code, signal });
    commonClientProc = null;
    if (commonClientRl) {
      commonClientRl.close();
      commonClientRl = null;
    }
  });

  return { ok: true };
};

const startBizHawkClient = async (options = {}) => {
  const requestedKind = String(options.clientKind || "").trim().toLowerCase();
  const kind = requestedKind === "sni" ? "sni" : "bizhawk";
  if (bizhawkClientProc) {
    // If the running client wrapper kind doesn't match the requested one,
    // restart cleanly to avoid running SNI logic during non-SNES launches
    // (causes CARTROM/CARTRAM domain spam in BizHawk logs on NES/GBA/etc).
    if (bizhawkClientKind !== kind) {
      await stopBizHawkClient();
    } else {
      return { ok: true, alreadyRunning: true };
    }
  }

  const python = await ensurePythonRuntime();
  const wrapperPath = kind === "sni" ? getSniClientWrapperPath() : getBizHawkClientWrapperPath();
  const args = [wrapperPath];

  bizhawkClientProc = spawn(python, args, {
    stdio: ["pipe", "pipe", "pipe"],
    env: withApPythonEnv(process.env)
  });
  bizhawkClientKind = kind;
  writeLogLine("info", "bizhawkclient", `spawned pid=${bizhawkClientProc.pid || ""} python=${python} wrapper=${wrapperPath}`);

  bizhawkClientRl = readline.createInterface({ input: bizhawkClientProc.stdout });
  bizhawkClientRl.on("line", (line) => {
    const trimmed = String(line || "").trim();
    if (!trimmed) return;
    try {
      const data = JSON.parse(trimmed);
      emitBizHawkClientEvent(data);
    } catch (_err) {
      writeLogLine("warn", "bizhawkclient", `non-json stdout: ${trimmed}`);
      emitBizHawkClientEvent({ event: "log", level: "warn", logger: "BizHawkClient", message: trimmed });
    }
  });

  bizhawkClientProc.stderr.on("data", (chunk) => {
    emitBizHawkClientEvent({ event: "log", level: "error", logger: "BizHawkClient", message: String(chunk).trim() });
  });

  bizhawkClientProc.on("exit", (code, signal) => {
    emitBizHawkClientEvent({ event: "exit", code, signal });
    bizhawkClientProc = null;
    if (bizhawkClientRl) {
      bizhawkClientRl.close();
      bizhawkClientRl = null;
    }
  });

  // For now, the caller should send a "connect" JSON command after spawning.
  return { ok: true };
};

const runPatcher = async (options = {}) => {
  const python = await ensurePythonRuntime();
  const wrapperPath = getPatcherWrapperPath();
  if (!fs.existsSync(wrapperPath)) return { ok: false, error: "patcher_wrapper_missing" };
  const moduleId = normalizeIpcString(options?.moduleId, 200);
  const manifest = isPlainObject(options?.manifest)
    ? options.manifest
    : moduleId
      ? getModuleManifest(moduleId)
      : null;
  const patchWorldModule = normalizeIpcString(
    options?.worldModule || manifest?.patch_world_module || manifest?.patcher_world_module || "",
    300
  );
  const worldProbe = await verifyPatcherPythonWorld(python, patchWorldModule);
  if (!worldProbe.ok) return { ok: false, error: `python_world_probe_failed: ${worldProbe.error || "unknown"}` };

  const userDataDir = app.getPath("userData");
  const patchDir = path.join(userDataDir, "runtime", "patches");
  const romDir = path.join(userDataDir, "runtime", "roms");
  const configPath =
    options.configPath ||
    path.join(app.getPath("home"), ".sekailink", "config.json");

  let patchPath = options.patchPath;
  if (!patchPath && options.patchUrl) {
    ensureDir(patchDir);
    const urlObj = new URL(options.patchUrl);
    const fileName = path.basename(urlObj.pathname || "patch.ap");
    patchPath = path.join(patchDir, fileName);
    try {
      await downloadToFile(options.patchUrl, patchPath, {
        expectedSha256: normalizeSha256(options.patchSha256 || options.sha256),
      });
    } catch (err) {
      return { ok: false, error: String(err) };
    }
  }
  if (!patchPath) return { ok: false, error: "missing_patch" };

  const args = [wrapperPath, "--patch", patchPath];
  if (configPath) args.push("--config", configPath);
  if (patchWorldModule) args.push("--world-module", patchWorldModule);
  args.push("--out-dir", options.outDir || romDir);

  return new Promise((resolve) => {
    const proc = spawn(python, args, {
      stdio: ["ignore", "pipe", "pipe"],
      env: withApPythonEnv(process.env)
    });
    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (chunk) => {
      stdout += String(chunk);
    });
    proc.stderr.on("data", (chunk) => {
      stderr += String(chunk);
    });
    proc.on("exit", () => {
      const line = stdout.trim().split("\n").filter(Boolean).pop();
      if (!line) {
        resolve({ ok: false, error: stderr || "no_output" });
        return;
      }
      try {
        const data = JSON.parse(line);
        resolve(data);
      } catch (err) {
        resolve({ ok: false, error: String(err) });
      }
    });
  });
};

const hashFile = (filePath, algo = "md5", options = {}) => {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash(algo);
    const stream = fs.createReadStream(filePath, {
      start: Number.isFinite(options?.start) ? Math.max(0, Number(options.start)) : undefined,
    });
    stream.on("error", reject);
    stream.on("data", (chunk) => hash.update(chunk));
    stream.on("end", () => resolve(hash.digest("hex")));
  });
};

const readFilePrefix = (filePath, length = 32) => {
  try {
    const fd = fs.openSync(filePath, "r");
    try {
      const buf = Buffer.alloc(Math.max(0, Math.min(4096, Number(length) || 0)));
      const n = fs.readSync(fd, buf, 0, buf.length, 0);
      return buf.slice(0, Math.max(0, n));
    } finally {
      fs.closeSync(fd);
    }
  } catch (_err) {
    return Buffer.alloc(0);
  }
};

const getRomHashOffsets = (filePath) => {
  const offsets = new Set([0]);
  const ext = path.extname(String(filePath || "")).toLowerCase();
  try {
    const size = fs.statSync(filePath).size || 0;
    // SNES copier header: 512 bytes. Archipelago strips it before hashing.
    if ((ext === ".sfc" || ext === ".smc") && size > 512 && (size % 0x8000) === 512) {
      offsets.add(512);
    }
  } catch (_err) {
    // ignore
  }
  // NES iNES header: 16 bytes when magic is present.
  if (ext === ".nes") {
    const prefix = readFilePrefix(filePath, 4);
    if (prefix.length >= 4 && prefix[0] === 0x4e && prefix[1] === 0x45 && prefix[2] === 0x53 && prefix[3] === 0x1a) {
      offsets.add(16);
    }
  }
  return Array.from(offsets.values()).sort((a, b) => a - b);
};

const findRomMatch = async (filePath, romIndex) => {
  const offsets = getRomHashOffsets(filePath);

  // Try md5 first (fast and used by most worlds), then sha1.
  for (const start of offsets) {
    const md5 = await hashFile(filePath, "md5", { start });
    const match = romIndex.get(String(md5 || "").toLowerCase());
    if (match) return { ...match, hash: md5, algo: "md5", start };
  }
  for (const start of offsets) {
    const sha1 = await hashFile(filePath, "sha1", { start });
    const match = romIndex.get(String(sha1 || "").toLowerCase());
    if (match) return { ...match, hash: sha1, algo: "sha1", start };
  }
  return null;
};

const getDefaultPatcherConfigPath = () => {
  return path.join(app.getPath("home"), ".sekailink", "config.json");
};

const getPatchedRomCachePath = () => {
  return path.join(app.getPath("userData"), "runtime", "patched-rom-cache.json");
};

const patchedRomCache = createPatchedRomCacheStore({
  fs,
  path,
  crypto,
  ensureDir,
  nowIso,
  hashFile,
  cachePath: getPatchedRomCachePath(),
});

const resolvePatchedRomPlan = async (options = {}) => {
  const moduleId = normalizeIpcString(options?.moduleId, 200);
  const patchPath = normalizeIpcPath(options?.patchPath);
  if (!moduleId || !patchPath) {
    return { ok: false, error: "missing_args" };
  }

  const cached = await patchedRomCache.resolveByPatch({ moduleId, patchPath });
  if (cached.ok && cached.outputPath) {
    const plan = {
      ok: true,
      moduleId,
      patchPath,
      patchHash: cached.patchHash || "",
      action: "reuse",
      shouldReuse: true,
      cached: true,
      outputPath: cached.outputPath,
      cacheHit: cached.cacheHit || "exact",
    };
    writeLogJson("patch-plan", plan);
    return plan;
  }

  const plan = {
    ok: true,
    moduleId,
    patchPath,
    patchHash: cached.patchHash || "",
    action: "patch",
    shouldReuse: false,
    cached: false,
    cacheHit: cached.cacheHit || "",
  };
  writeLogJson("patch-plan", plan);
  return plan;
};

let _romIndexCache = null;
let _patchIndexCache = null;

const normalizeStringList = (value) => {
  if (Array.isArray(value)) return value.map((v) => String(v));
  if (typeof value === "string") {
    const s = value.trim();
    return s ? [s] : [];
  }
  return [];
};

const loadRomIndex = () => {
  if (_romIndexCache) return _romIndexCache;
  const index = new Map();
  const seenModuleIds = new Set();
  for (const modulesDir of getRuntimeModulesDirs()) {
    if (!fs.existsSync(modulesDir)) continue;
    const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isDirectory()) continue;
      if (seenModuleIds.has(entry.name)) continue;
      seenModuleIds.add(entry.name);
      const manifestPath = path.join(modulesDir, entry.name, "manifest.json");
      if (!fs.existsSync(manifestPath)) continue;
      try {
        const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
        const req = manifest.rom_requirements || {};
        const md5s = normalizeStringList(req.md5);
        const sha1s = normalizeStringList(req.sha1);
        for (const md5 of md5s) {
          index.set(md5.toLowerCase(), { gameId: manifest.game_id, moduleId: entry.name, algo: "md5" });
        }
        for (const sha1 of sha1s) {
          index.set(sha1.toLowerCase(), { gameId: manifest.game_id, moduleId: entry.name, algo: "sha1" });
        }
      } catch (_err) {
        // ignore malformed manifest
      }
    }
  }
  _romIndexCache = index;
  return index;
};

const loadPatchIndex = () => {
  if (_patchIndexCache) return _patchIndexCache;
  const index = new Map();
  const collisions = [];
  const seenModuleIds = new Set();
  for (const modulesDir of getRuntimeModulesDirs()) {
    if (!fs.existsSync(modulesDir)) continue;
    const entries = fs.readdirSync(modulesDir, { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isDirectory()) continue;
      if (seenModuleIds.has(entry.name)) continue;
      seenModuleIds.add(entry.name);
      const manifestPath = path.join(modulesDir, entry.name, "manifest.json");
      if (!fs.existsSync(manifestPath)) continue;
      try {
        const manifest = JSON.parse(fs.readFileSync(manifestPath, "utf-8"));
        const exts = normalizeStringList(manifest.patch_extensions);
        for (const ext of exts) {
          const normalized = String(ext || "").trim().toLowerCase();
          if (!normalized || !normalized.startsWith(".")) continue;
          const existing = index.get(normalized);
          if (existing) {
            collisions.push({
              ext: normalized,
              first: existing,
              second: { gameId: manifest.game_id, moduleId: entry.name },
            });
            continue;
          }
          index.set(normalized, { gameId: manifest.game_id, moduleId: entry.name });
        }
      } catch (_err) {
        // ignore malformed manifest
      }
    }
  }
  if (collisions.length) {
    console.warn("[runtime] Duplicate patch_extensions detected; resolution is ambiguous:", collisions);
  }
  _patchIndexCache = index;
  return index;
};

const resolveModuleForDownload = (downloadUrl) => {
  if (!downloadUrl) return { ok: false, error: "missing_url" };
  let pathname = "";
  try {
    pathname = new URL(downloadUrl).pathname || "";
  } catch (_err) {
    pathname = String(downloadUrl);
  }
  const ext = path.extname(pathname).toLowerCase();
  if (!ext) return { ok: false, error: "unknown_extension" };

  const patchIndex = loadPatchIndex();
  const found = patchIndex.get(ext);
  if (!found) return { ok: false, error: "unsupported_patch_type", ext };
  return { ok: true, moduleId: found.moduleId, gameId: found.gameId, ext };
};

const findModuleByApGameName = (apGameName) => {
  const target = String(apGameName || "").trim().toLowerCase();
  if (!target) return null;
  for (const info of listRuntimeModules()) {
    const manifest = info.manifest || {};
    const name = String(manifest.display_name || manifest.game_name || "").trim().toLowerCase();
    if (name && name === target) return info.moduleId;
  }
  return null;
};

const validateSetupForModule = async (moduleId) => {
  const manifest = getModuleManifest(moduleId);
  if (!manifest) return { ok: false, error: "manifest_missing" };

  // Install per-module system/runtime dependencies during the setup phase,
  // so we fail early with actionable errors (instead of failing mid-launch).
  if (process.platform === "linux" && String(manifest.emu || "").toLowerCase() === "bizhawk") {
    try {
      await ensureMonoRuntime();
    } catch (err) {
      return { ok: false, error: "mono_missing", detail: String(err || ""), setupArea: "paths" };
    }
  }

  const emu = String(manifest.emu || "").trim().toLowerCase();
  if (emu === "sekaiemu" || emu === "sekaiemu-libretro") {
    if (!resolveSekaiemuExecutable()) {
      return { ok: false, error: "sekaiemu_not_found", setupArea: "paths" };
    }
    if (!resolveSekaiemuCorePath(manifest)) {
      return { ok: false, error: "sekaiemu_core_missing", setupArea: "paths" };
    }
    if (!resolveSklmiRuntimeForSekaiemu()) {
      return { ok: false, error: "sklmi_runtime_missing", setupArea: "paths" };
    }
    if (!resolveSklmiManifestDirForSekaiemu()) {
      return { ok: false, error: "sklmi_manifest_missing", setupArea: "paths" };
    }
    const sekaiemu = manifest.sekaiemu && typeof manifest.sekaiemu === "object" ? manifest.sekaiemu : {};
    const declaredTrackerPack = String(
      sekaiemu.tracker_pack ||
        sekaiemu.tracker_pack_path ||
        manifest.tracker_pack_path ||
        ""
    ).trim();
    if (declaredTrackerPack) {
      const trackerPack = resolveSekaiemuTrackerPackPath(manifest, [
        getBundledRuntimeDir(),
        path.join(getBundledRuntimeDir(), "tracker-bundles"),
      ]);
      if (!trackerPack) {
        return { ok: false, error: "sekaiemu_tracker_pack_missing", setupArea: "paths" };
      }
    }
  }

  const config = readConfig();
  const roms = config.roms || {};
  const requiredRomIds = Array.isArray(manifest.required_roms)
    ? manifest.required_roms
    : manifest.game_id
      ? [manifest.game_id]
      : [];
  for (const gameId of requiredRomIds) {
    const romPath = String(roms[gameId] || "").trim();
    if (!romPath) return { ok: false, error: "rom_missing", gameId, setupArea: "roms" };
    if (!fs.existsSync(romPath)) {
      return { ok: false, error: "rom_missing", gameId, detail: "rom_path_not_found", setupArea: "roms" };
    }
    try {
      const stat = fs.statSync(romPath);
      if (!stat.isFile()) {
        return { ok: false, error: "rom_missing", gameId, detail: "rom_path_not_file", setupArea: "roms" };
      }
    } catch (_err) {
      return { ok: false, error: "rom_missing", gameId, detail: "rom_path_unreadable", setupArea: "roms" };
    }
  }

  // Separate PopTracker is only for legacy/non-Sekaiemu launch flows. Sekaiemu passes packs to SKLMI.
  if (manifest.tracker_pack_uid && emu !== "sekaiemu" && emu !== "sekaiemu-libretro") {
    const trackerStatus = getPopTrackerStatus();
    if (!trackerStatus.exists) return { ok: false, error: "poptracker_missing", setupArea: "paths" };
  }

  return { ok: true, manifest };
};

const planSessionAutoLaunch = async (options = {}) => {
  const downloadUrl = String(options.downloadUrl || "").trim();
  const apGameName = String(options.apGameName || "").trim();

  if (!downloadUrl) {
    const moduleId = findModuleByApGameName(apGameName);
    if (!moduleId) return { ok: false, error: "missing_patch_url" };
    const setup = await validateSetupForModule(moduleId);
    if (!setup.ok) return { ...setup, moduleId };
    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    if (String(manifest.patcher || "").toLowerCase() !== "none") {
      return { ok: false, error: "missing_patch_url", moduleId };
    }
    return {
      ok: true,
      moduleId,
      gameId: String(manifest.game_id || "").trim(),
      emu: String(manifest.emu || "").trim().toLowerCase(),
      launchKind: "native_patchless",
      canAutoPatch: false,
    };
  }

  let pathname = "";
  try {
    pathname = new URL(downloadUrl).pathname || "";
  } catch (_err) {
    pathname = String(downloadUrl || "");
  }
  const ext = path.extname(pathname).toLowerCase();
  const extResolved = ext ? resolveModuleForDownload(downloadUrl) : { ok: false, error: "unknown_extension" };
  const byName = !extResolved.ok ? findModuleByApGameName(apGameName) : null;
  const resolved = extResolved.ok
    ? extResolved
    : byName
      ? { ok: true, moduleId: byName, gameId: "", ext }
      : extResolved;

  if (!resolved.ok || !resolved.moduleId) {
    return {
      ok: true,
      ext,
      launchKind: "manual_unknown_artifact",
      canAutoPatch: false,
    };
  }

  const moduleId = String(resolved.moduleId || "").trim();
  const canAutoPatch = Boolean(extResolved.ok);
  if (!canAutoPatch) {
    return {
      ok: true,
      moduleId,
      gameId: String(resolved.gameId || "").trim(),
      ext,
      launchKind: "manual_known_artifact",
      canAutoPatch: false,
    };
  }

  const setup = await validateSetupForModule(moduleId);
  if (!setup.ok) {
    return {
      ...setup,
      moduleId,
      ext,
      launchKind: "auto_patch",
      canAutoPatch: true,
    };
  }

  const manifest = setup.manifest || getModuleManifest(moduleId) || {};
  return {
    ok: true,
    moduleId,
    gameId: String(resolved.gameId || manifest.game_id || "").trim(),
    ext,
    emu: String(manifest.emu || "").trim().toLowerCase(),
    launchKind: "auto_patch",
    canAutoPatch: true,
  };
};

const autoLaunchFromPatchUrl = async (options = {}) => {
  const downloadUrl = String(options.downloadUrl || "").trim();
  const serverAddress = options.serverAddress;
  const slot = options.slot;
  const playerAlias = options.playerAlias;
  const password = options.password;
  const apGameName = options.apGameName;
  const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;
  const chatBridge = isPlainObject(options.chatBridge) ? options.chatBridge : null;
  const apiBaseUrl = options.apiBaseUrl;
  const authToken = options.authToken;
  const deviceId = options.deviceId;

  if (!serverAddress) return { ok: false, error: "missing_server_address" };
  if (!slot) return { ok: false, error: "missing_slot" };

  // Patchless flow for native AP-integrated games (e.g. Ship of Harkinian).
  if (!downloadUrl) {
    const moduleId = findModuleByApGameName(apGameName);
    if (!moduleId) return { ok: false, error: "missing_patch_url" };

    emitSessionEvent({ event: "status", status: "Validating setup...", moduleId });
    const setup = await validateSetupForModule(moduleId);
    if (!setup.ok) {
      emitSessionEvent({
        event: "error",
        step: "validate",
        error: setup.error,
        detail: setup.detail,
        setupArea: setup.setupArea,
        moduleId,
        gameId: setup.gameId,
      });
      return setup;
    }

    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    if (String(manifest.patcher || "").toLowerCase() !== "none") {
      return { ok: false, error: "missing_patch_url" };
    }

    const runtimeRes = await launchGameRuntimeForModule({
      moduleId,
      manifest,
      serverAddress,
      slot,
      playerAlias,
      password,
      chatBridge,
      apiBaseUrl,
      authToken,
      deviceId,
    });
    if (!runtimeRes?.ok) {
      return { ok: false, error: runtimeRes?.error || "emu_failed", detail: runtimeRes?.detail || "" };
    }

    await applyRuntimeLayoutForSession({ moduleId, gamePid: runtimeRes.pid, trackerPid: null });

    const note =
      "Ship of Harkinian started. Connect from in-game menu: ESC > Network > Archipelago.";
    emitSessionEvent({ event: "status", status: note, moduleId });
    emitSessionEvent({
      event: "ready",
      moduleId,
      emuPid: runtimeRes.pid,
      trackerPid: null,
      note,
    });
    return {
      ok: true,
      moduleId,
      emuPid: runtimeRes.pid,
      trackerPid: null,
      note,
      noPatch: true,
    };
  }

  emitSessionEvent({ event: "status", status: "Downloading...", downloadUrl });
  let dl = null;
  try {
    const downloadHeaders = {
      "Accept": "application/octet-stream,*/*",
    };
    if (authToken) downloadHeaders.Authorization = `Bearer ${authToken}`;
    if (deviceId) downloadHeaders["X-SekaiLink-Device-Id"] = String(deviceId);
    dl = await downloadToDirWithProgress(downloadUrl, getRuntimeDownloadsDir(), {
      defaultBasename: "ap-download",
      headers: downloadHeaders,
      onProgress: (p) => {
        // Drive a real progress bar in the renderer (Lobby launch modal).
        // Keep the status message stable to avoid log/DOM spam.
        emitSessionEvent({
          event: "download-progress",
          downloadUrl,
          receivedBytes: p?.receivedBytes ?? 0,
          totalBytes: p?.totalBytes ?? 0,
          percent: p?.percent ?? null,
        });
      },
    });
  } catch (err) {
    const detail = String(err?.message || err || "download_failed");
    emitSessionEvent({ event: "error", step: "download", error: "download_failed", detail, downloadUrl });
    return { ok: false, error: "download_failed", detail, downloadUrl };
  }
  const downloadedPath = dl?.path;
  const ext = downloadedPath ? path.extname(downloadedPath).toLowerCase() : "";
  emitSessionEvent({ event: "status", status: "Resolving game type...", downloadUrl, ext });

  // Before falling back to manual, try dedicated handlers for non-AP patch artifacts (slot files).
  const artifact = await tryHandleDownloadedArtifact({ downloadedPath, ext, apGameName });
  if (artifact?.ok && artifact?.handled) {
    const artifactModuleId =
      (ext === ".apsm64ex" ? "sm64ex" : "") ||
      findModuleByApGameName(apGameName) ||
      "";

    emitSessionEvent({
      event: "status",
      status: artifact.gamePid ? "Game package prepared. Connecting..." : "Game package installed. Manual action may be required.",
      downloadedPath,
      ext,
      apGameName,
      installedPath: artifact.installedPath,
      handlerError: artifact.error,
      note: artifact.note,
    });

    // Still connect CommonClient for chat/commands.
    emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId: artifactModuleId });
    await startCommonClient({ address: serverAddress, slot, password });
    await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });

    let trackerPid = null;
    const artifactManifest = artifactModuleId ? (getModuleManifest(artifactModuleId) || {}) : {};
    if (artifactModuleId && moduleHasExternalTracker(artifactManifest)) {
      emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId: artifactModuleId });
      const trackerRes = await launchPopTracker({
        moduleId: artifactModuleId,
        apHost: serverAddress,
        apSlot: slot,
        apPass: password,
        apAutoconnect: true,
        forceTrackerVariantPrompt,
      });
      if (trackerRes?.error === "tracker_variant_cancelled") {
        emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId: artifactModuleId });
        return { ok: false, error: "tracker_variant_cancelled" };
      }
      if (!trackerRes?.ok) {
        emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId: artifactModuleId });
      } else if (trackerRes?.pid) {
        trackerPid = trackerRes.pid;
      }
    }

    if (artifact.gamePid) {
      emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId: artifactModuleId });
      const layoutRes = await applyLayoutBestEffort({ gamePid: artifact.gamePid, trackerPid });
      if (!layoutRes?.ok) {
        emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId: artifactModuleId });
      }
      emitSessionEvent({
        event: "ready",
        moduleId: artifactModuleId,
        downloadedPath,
        emuPid: artifact.gamePid,
        trackerPid,
        installedPath: artifact.installedPath,
      });
      return {
        ok: true,
        handled: true,
        moduleId: artifactModuleId,
        downloadedPath,
        ext,
        gamePid: artifact.gamePid,
        trackerPid,
        installedPath: artifact.installedPath,
      };
    }

    emitSessionEvent({
      event: "manual",
      downloadedPath,
      ext,
      apGameName,
      moduleId: artifactModuleId,
      trackerPid,
      installedPath: artifact.installedPath,
      note: artifact.note || "Installed successfully. Launch the game to continue.",
    });
    return {
      ok: true,
      manual: true,
      moduleId: artifactModuleId,
      downloadedPath,
      ext,
      trackerPid,
      installedPath: artifact.installedPath,
    };
  }

  // Prefer extension-based routing when available (Archipelago patch files).
  const extResolved = ext ? resolveModuleForDownload(downloadedPath) : { ok: false, error: "unknown_extension" };
  const byName = !extResolved.ok ? findModuleByApGameName(apGameName) : null;
  const resolved = extResolved.ok ? extResolved : byName ? { ok: true, moduleId: byName, gameId: "", ext } : extResolved;
  const canAutoPatch = Boolean(extResolved.ok);

  const moduleId = resolved.moduleId;
  if (!resolved.ok || !moduleId) {
    // Unknown file type: keep it simple for now. Download succeeded, user may need manual steps.
    emitSessionEvent({
      event: "status",
      status: "Download complete. Manual action may be required for this game.",
      downloadedPath,
      ext,
      apGameName
    });
    // Still connect CommonClient for chat/commands if possible.
    emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId: "" });
    await startCommonClient({ address: serverAddress, slot, password });
    await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    emitSessionEvent({
      event: "status",
      status: "Manual action required: file downloaded and client connected.",
      downloadedPath,
      ext
    });
    emitSessionEvent({ event: "manual", downloadedPath, ext, apGameName });
    return { ok: true, manual: true, downloadedPath, ext };
  }

  const packCheck = await ensureRuntimePacksForModule({
    moduleId,
    gameId: String(resolved.gameId || "").trim(),
  });
  if (!packCheck?.ok) {
    emitSessionEvent({
      event: "error",
      step: "packs",
      error: packCheck?.error || "runtime_pack_sync_failed",
      moduleId,
      packId: packCheck?.packId || "",
    });
    return {
      ok: false,
      error: packCheck?.error || "runtime_pack_sync_failed",
      moduleId,
      packId: packCheck?.packId || "",
      detail: packCheck?.detail || "",
    };
  }

  if (!canAutoPatch) {
    // We recognized the game by name, but not the file type. Keep the download and connect/tracker best-effort.
    emitSessionEvent({
      event: "status",
      status: "Download complete. Automatic game preparation is not supported for this file type yet.",
      downloadedPath,
      ext,
      moduleId
    });
    emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId });
    await startCommonClient({ address: serverAddress, slot, password });
    await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    let trackerRes = null;
    const manifest = getModuleManifest(moduleId) || {};
    if (moduleHasExternalTracker(manifest)) {
      emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId });
      trackerRes = await launchPopTracker({
        moduleId,
        apHost: serverAddress,
        apSlot: slot,
        apPass: password,
        apAutoconnect: true,
        forceTrackerVariantPrompt,
      });
      if (trackerRes?.error === "tracker_variant_cancelled") {
        emitSessionEvent({ event: "error", step: "tracker", error: "tracker_variant_cancelled", moduleId });
        return { ok: false, error: "tracker_variant_cancelled" };
      }
      if (!trackerRes?.ok) {
        emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId });
      }
    }
    emitSessionEvent({
      event: "status",
      status: "Manual action required: file downloaded and client connected.",
      downloadedPath,
      ext,
      moduleId
    });
    emitSessionEvent({ event: "manual", downloadedPath, ext, moduleId, apGameName, trackerPid: trackerRes?.pid });
    return { ok: true, manual: true, moduleId, downloadedPath, ext, trackerPid: trackerRes?.pid };
  }

  emitSessionEvent({ event: "status", status: "Validating setup...", moduleId });
  const setup = await validateSetupForModule(moduleId);
  if (!setup.ok) {
    emitSessionEvent({
      event: "error",
      step: "validate",
      error: setup.error,
      detail: setup.detail,
      setupArea: setup.setupArea,
      moduleId,
      gameId: setup.gameId,
    });
    return setup;
  }

  const configPath = getDefaultPatcherConfigPath();
  let patchOutput = "";
  let patchCacheKey = "";
  let patchHash = "";
  let reusedPatchedRom = false;

  try {
    const cached = await patchedRomCache.resolveByPatch({ moduleId, patchPath: downloadedPath });
    patchCacheKey = cached.key || "";
    patchHash = cached.patchHash || "";
    if (cached.ok && cached.outputPath) {
      patchOutput = cached.outputPath;
      reusedPatchedRom = true;
      emitSessionEvent({ event: "status", status: "Reusing prepared game image...", moduleId });
      writeLogLine("info", "autolaunch", `reusing patched rom: moduleId=${moduleId} patchHash=${patchHash} output=${patchOutput}`);
      writeLogJson("patch-decision", {
        moduleId,
        action: "reuse",
        patchPath: downloadedPath,
        patchHash,
        outputPath: patchOutput,
        cacheHit: cached.cacheHit || "exact",
      });
    }
    // Do not reuse by URL only. URL reuse can occur with updated patch content and
    // cause slot/ROM auth mismatches ("Invalid ROM detected"). Hash-based reuse
    // above remains enabled.
  } catch (err) {
    writeLogLine("warn", "autolaunch", `cache lookup failed; patching normally: ${String(err)}`);
  }

  if (!patchOutput) {
    emitSessionEvent({ event: "status", status: "Preparing game image...", moduleId });
    writeLogLine("info", "autolaunch", `patching: patchPath=${downloadedPath} moduleId=${moduleId}`);
    writeLogJson("patch-decision", {
      moduleId,
      action: "patch",
      patchPath: downloadedPath,
      patchHash,
      reason: "cache_miss",
    });
    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    const patchRes = await runPatcher({ patchPath: downloadedPath, patchUrl: downloadUrl, configPath, moduleId, manifest });
    writeLogLine("info", "autolaunch", `patch result: ok=${patchRes?.ok} output=${patchRes?.output || ""} error=${patchRes?.error || ""}`);
    if (!patchRes?.ok || !patchRes.output) {
      emitSessionEvent({ event: "error", step: "patch", error: patchRes?.error || "patch_failed", moduleId });
      return { ok: false, error: patchRes?.error || "patch_failed" };
    }
    patchOutput = patchRes.output;
    patchedRomCache.remember({
      key: patchCacheKey,
      moduleId,
      patchPath: downloadedPath,
      patchHash,
      outputPath: patchOutput,
      downloadUrl,
    });
    writeLogJson("patch-result", {
      moduleId,
      ok: true,
      action: "patch",
      patchPath: downloadedPath,
      patchHash,
      outputPath: patchOutput,
      remembered: true,
    });
  }

  const manifest = setup.manifest || getModuleManifest(moduleId) || {};
  const runtimeRes = await launchGameRuntimeForModule({
    moduleId,
    manifest,
    romPath: patchOutput,
    reusedPatchedRom,
    serverAddress,
    slot,
    playerAlias,
    password,
    chatBridge,
    apiBaseUrl,
    authToken,
    deviceId,
  });
  if (!runtimeRes?.ok) {
    return { ok: false, error: runtimeRes?.error || "emu_failed", detail: runtimeRes?.detail || "" };
  }

  await connectRuntimeBridgeForModule({
    moduleId,
    manifest,
    serverAddress,
    slot,
    playerAlias: options.playerAlias,
    password,
  });

  const trackerRes = await launchTrackerForModuleSession({
    moduleId,
    manifest,
    serverAddress,
    slot,
    password,
    forceTrackerVariantPrompt,
  });
  if (!trackerRes?.ok && trackerRes?.error === "tracker_variant_cancelled") {
    return { ok: false, error: "tracker_variant_cancelled" };
  }

  await applyRuntimeLayoutForSession({
    moduleId,
    gamePid: runtimeRes.pid,
    trackerPid: trackerRes?.pid || null,
  });

  emitSessionEvent({
    event: "ready",
    moduleId,
    patchOutput,
    patchReused: reusedPatchedRom,
    emuPid: runtimeRes.pid,
    trackerPid: trackerRes?.pid,
    downloadedPath,
  });

  return {
    ok: true,
    moduleId,
    patchOutput,
    patchReused: reusedPatchedRom,
    emuPid: runtimeRes.pid,
    trackerPid: trackerRes?.pid,
    downloadedPath,
  };
};

const scanRomFolder = async (folderPath) => {
  const romIndex = loadRomIndex();
  const results = [];
  if (!folderPath || !fs.existsSync(folderPath)) return results;
  const walk = (dirPath) => {
    const files = [];
    const entries = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dirPath, entry.name);
      if (entry.isDirectory()) {
        files.push(...walk(fullPath));
      } else if (entry.isFile()) {
        files.push(fullPath);
      }
    }
    return files;
  };

  const files = walk(folderPath);
  for (const srcPath of files) {
    const ext = path.extname(srcPath).toLowerCase();
    if (!ext || ext.length > 6) continue;
    try {
      const found = await findRomMatch(srcPath, romIndex);
      if (!found) continue;

      const gameId = found.gameId;
      const destDir = getRomsDir();
      ensureDir(destDir);
      const destPath = path.join(destDir, `${gameId}${ext}`);
      fs.copyFileSync(srcPath, destPath);

      const config = readConfig();
      if (!config.roms) config.roms = {};
      config.roms[gameId] = destPath;
      writeConfig(config);

      results.push({ gameId, file: path.basename(srcPath) });
    } catch (_err) {
      // ignore hashing errors per file
    }
  }
  return results;
};

const importRomFile = async (filePath) => {
  const romIndex = loadRomIndex();
  if (!filePath || !fs.existsSync(filePath)) return { ok: false, error: "missing_file" };
  const ext = path.extname(filePath).toLowerCase();
  if (!ext || ext.length > 6) return { ok: false, error: "invalid_extension" };

  try {
    const found = await findRomMatch(filePath, romIndex);
    if (!found) return { ok: false, error: "hash_mismatch" };

    const gameId = found.gameId;
    const destDir = getRomsDir();
    ensureDir(destDir);
    const destPath = path.join(destDir, `${gameId}${ext}`);
    fs.copyFileSync(filePath, destPath);

    const config = readConfig();
    if (!config.roms) config.roms = {};
    config.roms[gameId] = destPath;
    writeConfig(config);

    return { ok: true, gameId, path: destPath };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
};

const createDashboardWindow = (targetUrl) => {
  const win = new BrowserWindow({
    width: 1200,
    height: 820,
    minWidth: 1000,
    minHeight: 700,
    backgroundColor: "#05070A",
    show: false,
    icon: resolveWindowIconPath(),
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  const loadingPath = path.join(__dirname, "loading.html");
  let loaded = false;

  win.once("ready-to-show", () => {
    win.show();
  });

  win.on("closed", () => {
    dashboardWindows.delete(win);
  });

  win.webContents.setWindowOpenHandler(({ url }) => {
    void openExternalSafely(url, "dashboard-window-open");
    return { action: "deny" };
  });

  win.webContents.on("did-finish-load", () => {
    if (!loaded && targetUrl) {
      loaded = true;
      // Give the loading screen a moment to paint.
      setTimeout(() => {
        if (!win.isDestroyed()) win.loadURL(targetUrl);
      }, 150);
    }
  });

  win.loadFile(loadingPath);
  dashboardWindows.add(win);
  return win;
};

const createWindow = () => {
  const width = 1600;
  const height = 900;
  const bounds = readMainWindowBounds(width, height);

  mainWindow = new BrowserWindow({
    width: bounds.width,
    height: bounds.height,
    x: bounds.x,
    y: bounds.y,
    minWidth: 1280,
    minHeight: 720,
    frame: false,
    titleBarStyle: "hidden",
    backgroundColor: "#05070A",
    show: false,
    icon: resolveWindowIconPath(),
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  mainWindow.on("moved", saveMainWindowBounds);
  mainWindow.on("resized", saveMainWindowBounds);
  mainWindow.on("close", saveMainWindowBounds);

  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
  });

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    void openExternalSafely(url, "main-window-open");
    return { action: "deny" };
  });

  // Capture renderer console output into the main log file (best-effort).
  mainWindow.webContents.on("console-message", (_event, level, message, line, sourceId) => {
    const lvl = level >= 2 ? "warn" : "info";
    writeLogLine(lvl, "renderer-console", `${message} (${sourceId || "unknown"}:${line || 0})`);
  });

  mainWindow.webContents.on("render-process-gone", (_event, details) => {
    writeLogJson("renderer-crash", details || {});
  });

  const loadingPath = path.join(__dirname, "loading.html");
  let mainTargetLoadStarted = false;
  let mainTargetLoaded = false;
  const loadMainTarget = () => {
    if (mainWindow.isDestroyed()) return;
    if (isDev) {
      mainWindow.loadURL(devServerUrl);
    } else {
      mainWindow.loadFile(path.join(__dirname, "../dist/index.html"));
    }
  };
  const deliverPendingAuthUrl = () => {
    if (!mainTargetLoaded || !pendingAuthUrl || mainWindow.isDestroyed()) return;
    mainWindow.webContents.send("auth:callback", pendingAuthUrl);
    pendingAuthUrl = null;
  };

  mainWindow.webContents.once("did-finish-load", () => {
    if (!mainTargetLoadStarted) {
      mainTargetLoadStarted = true;
      setTimeout(loadMainTarget, 80);
    }
  });
  mainWindow.webContents.on("did-finish-load", () => {
    if (!mainTargetLoadStarted) return;
    if (mainTargetLoaded) return;
    const currentUrl = mainWindow.webContents.getURL();
    if (currentUrl.startsWith("file:") && currentUrl.endsWith("loading.html")) return;
    mainTargetLoaded = true;
    if (shouldOpenDevTools) mainWindow.webContents.openDevTools({ mode: "detach" });
    deliverPendingAuthUrl();
  });
  mainWindow.loadFile(loadingPath);
};

app.whenReady().then(() => {
  startFileLogging();
  writeLogJson("env", {
    app_version: app.getVersion ? app.getVersion() : "",
    platform: process.platform,
    arch: process.arch,
    electron: process.versions?.electron,
    chrome: process.versions?.chrome,
    node: process.versions?.node
  });

  const bootstrapCheck = validateBootstrapLaunchToken();
  if (!bootstrapCheck.ok) {
    writeLogLine("error", "bootstrap", `launch rejected: ${String(bootstrapCheck.error || "unknown")}`);
    dialog.showErrorBox(
      "SekaiLink launch blocked",
      "This client must be launched via the SekaiLink bootstrapper/updater."
    );
    app.exit(1);
    return;
  }

  app.setAsDefaultProtocolClient("sekailink");
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

let quitCleanupInProgress = false;
app.on("before-quit", (event) => {
  if (quitCleanupInProgress) return;
  event.preventDefault();
  quitCleanupInProgress = true;
  void (async () => {
    await shutdownRuntimeProcesses("before-quit");
    for (const win of Array.from(trackerWebWindows)) {
      try {
        if (!win.isDestroyed()) win.close();
      } catch (_err) {
        // ignore
      }
    }
    trackerWebWindows.clear();
    logger.stop();
    app.exit(0);
  })();
});

app.on("will-quit", () => {
  void shutdownRuntimeProcesses("will-quit");
});

const installSignalShutdownHandlers = () => {
  let inSignalShutdown = false;
  const onSignal = (signalName) => {
    if (inSignalShutdown) return;
    inSignalShutdown = true;
    void (async () => {
      await shutdownRuntimeProcesses(`signal:${signalName}`);
      logger.stop();
      process.exit(0);
    })();
  };
  process.on("SIGINT", () => onSignal("SIGINT"));
  process.on("SIGTERM", () => onSignal("SIGTERM"));
};
installSignalShutdownHandlers();

app.on("quit", () => {
  for (const win of Array.from(trackerWebWindows)) {
    try {
      if (!win.isDestroyed()) win.close();
    } catch (_err) {
      // ignore
    }
  }
  trackerWebWindows.clear();
});

app.on("open-url", (event, url) => {
  event.preventDefault();
  if (mainWindow) {
    mainWindow.webContents.send("auth:callback", url);
  } else {
    pendingAuthUrl = url;
  }
});

const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  try {
    app.exit(0);
  } catch (_err) {
    // ignore
  }
  setTimeout(() => process.exit(0), 25).unref();
} else {
  app.on("second-instance", (_event, argv) => {
    const urlArg = argv.find((arg) => typeof arg === "string" && arg.startsWith("sekailink://"));
    if (urlArg) {
      if (mainWindow) {
        mainWindow.webContents.send("auth:callback", urlArg);
      } else {
        pendingAuthUrl = urlArg;
      }
    }
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.focus();
    }
  });
}

// Crash + error handlers (best-effort; always log and keep the process alive when possible)
process.on("uncaughtException", (err) => {
  const msg = err && err.stack ? err.stack : String(err);
  writeLogLine("error", "uncaughtException", msg);
  if (crashReportingOptIn) {
    void submitDiagnosticsReport({
      uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
      meta: { event: "uncaughtException", message: msg },
    });
  }
});

process.on("unhandledRejection", (reason) => {
  const msg = reason && reason.stack ? reason.stack : logger.safeStringify(reason);
  writeLogLine("error", "unhandledRejection", msg);
  if (crashReportingOptIn) {
    void submitDiagnosticsReport({
      uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
      meta: { event: "unhandledRejection", message: msg },
    });
  }
});

app.on("render-process-gone", (_event, webContents, details) => {
  const payload = {
    url: webContents?.getURL ? webContents.getURL() : "",
    ...details
  };
  writeLogJson("app.render-process-gone", payload);
  if (crashReportingOptIn) {
    void submitDiagnosticsReport({
      uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
      meta: { event: "render-process-gone", details: payload },
    });
  }
});

app.on("child-process-gone", (_event, details) => {
  writeLogJson("app.child-process-gone", details || {});
  if (crashReportingOptIn) {
    void submitDiagnosticsReport({
      uploadUrl: process.env.SEKAILINK_CRASH_REPORT_URL || "",
      meta: { event: "child-process-gone", details: details || {} },
    });
  }
});


secureIpcHandle("app:openExternal", async (_event, url) => {
  return openExternalSafely(url, "ipc.app:openExternal");
});

secureIpcHandle("app:getEnv", async () => {
  const installState = getBootstrapInstallState() || {};
  const clientUpdateInstallDir = getClientBundleInstallDir();
  const bootstrapCheck = validateBootstrapLaunchToken();
  const bootstrapperPath = resolveBootstrapperExecutable();
  return {
    app_version: app.getVersion ? String(app.getVersion() || "") : "",
    bootstrap_release_version: String(installState.version || installState.manifestVersion || "").trim(),
    bootstrap_channel: String(installState.channel || "").trim().toLowerCase(),
    bootstrap_build: String(installState.build || "").trim().toLowerCase(),
    bootstrap_install_dir: String(installState.installDir || process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim(),
    bootstrapper_path: bootstrapperPath,
    bootstrapper_available: Boolean(bootstrapperPath),
    bootstrapper_download_url: getBootstrapperDownloadUrl(),
    bootstrap_launch_enforced: Boolean(bootstrapCheck.enforced),
    bootstrap_launch_token_present: Boolean(bootstrapCheck.token_present),
    bootstrap_launch_token_valid: Boolean(bootstrapCheck.token_valid),
    bootstrap_launch_error: String(bootstrapCheck.error || ""),
    client_update_install_dir: clientUpdateInstallDir,
    client_update_bundle_supported: app.isPackaged && Boolean(clientUpdateInstallDir),
    app_is_packaged: app.isPackaged,
    platform: process.platform,
    arch: process.arch,
    electron: process.versions?.electron,
    chrome: process.versions?.chrome,
    node: process.versions?.node
  };
});

secureIpcHandle("app:setCrashReportingOptIn", async (_event, enabled) => {
  crashReportingOptIn = Boolean(enabled);
  return { ok: true, enabled: crashReportingOptIn };
});

secureIpcHandle("app:collectDiagnostics", async (_event, options) => {
  const report = await collectDiagnosticsReport(isPlainObject(options) ? options : {});
  return { ok: true, report };
});

secureIpcHandle("app:submitDiagnostics", async (_event, options) => {
  return submitDiagnosticsReport(isPlainObject(options) ? options : {});
});

secureIpcHandle("app:collectBugReportArtifacts", async (_event, options) => {
  const artifacts = await collectBugReportArtifacts(isPlainObject(options) ? options : {});
  return { ok: true, artifacts };
});

secureIpcHandle("app:copyText", async (_event, text) => {
  const value = normalizeIpcString(text, 1024 * 1024);
  if (!value) return { ok: false, error: "empty_text" };
  clipboard.writeText(value);
  return { ok: true };
});

secureIpcHandle("app:showItemInFolder", async (_event, targetPath) => {
  try {
    const p = normalizeIpcPath(targetPath);
    if (!p) return { ok: false, error: "invalid_path" };
    shell.showItemInFolder(p);
    return { ok: true };
  } catch (err) {
    return { ok: false, error: String(err) };
  }
});

secureIpcHandle("app:openDashboard", async (_event, url) => {
  const targetUrl = normalizeIpcString(url, 2048);
  if (!targetUrl) return { ok: false, error: "invalid_url" };
  if (!isSafeExternalUrl(targetUrl)) return { ok: false, error: "unsafe_url" };
  createDashboardWindow(targetUrl);
  return { ok: true };
});

secureIpcHandle("app:window:minimize", async () => {
  const win = BrowserWindow.getFocusedWindow() || mainWindow;
  if (!win || win.isDestroyed()) return { ok: false, error: "no_window" };
  win.minimize();
  return { ok: true };
});

secureIpcHandle("app:window:toggleMaximize", async () => {
  const win = BrowserWindow.getFocusedWindow() || mainWindow;
  if (!win || win.isDestroyed()) return { ok: false, error: "no_window" };
  if (win.isMaximized()) win.unmaximize();
  else win.maximize();
  return { ok: true, maximized: win.isMaximized() };
});

secureIpcHandle("app:window:close", async () => {
  const win = BrowserWindow.getFocusedWindow() || mainWindow;
  if (!win || win.isDestroyed()) return { ok: false, error: "no_window" };
  win.close();
  return { ok: true };
});

secureIpcHandle("updater:download", async (_event, options) => {
  return startClientUpdaterDownload(isPlainObject(options) ? options : {});
});

secureIpcHandle("updater:downloadAndApply", async (_event, options) => {
  return downloadAndApplyClientUpdate(isPlainObject(options) ? options : {});
});

secureIpcHandle("updater:openDownloaded", async (_event, targetPath) => {
  return openDownloadedUpdaterFile(targetPath);
});

secureIpcHandle("updater:syncIncremental", async (_event, options) => {
  return syncIncrementalClientFiles(isPlainObject(options) ? options : {});
});

secureIpcHandle("updater:launchBootstrapperAndQuit", async () => {
  return launchBootstrapperAndQuit();
});

secureIpcHandle("app:pickFile", async (_event, options) => {
  const normalizedOptions = isPlainObject(options) ? options : {};
  const result = await dialog.showOpenDialog(mainWindow || undefined, {
    title: normalizeIpcString(normalizedOptions.title, 128) || "Select File",
    properties: ["openFile"],
    filters: Array.isArray(normalizedOptions.filters) ? normalizedOptions.filters : [{ name: "All Files", extensions: ["*"] }]
  });
  if (result.canceled || !result.filePaths?.length) return { canceled: true };
  return { canceled: false, path: result.filePaths[0] };
});

secureIpcHandle("app:pickFolder", async (_event, options) => {
  const normalizedOptions = isPlainObject(options) ? options : {};
  const result = await dialog.showOpenDialog(mainWindow || undefined, {
    title: normalizeIpcString(normalizedOptions.title, 128) || "Select Folder",
    properties: ["openDirectory"]
  });
  if (result.canceled || !result.filePaths?.length) return { canceled: true };
  return { canceled: false, path: result.filePaths[0] };
});

secureIpcHandle("config:get", async () => {
  return readConfig();
});

secureIpcHandle("config:setRom", async (_event, gameId, romPath) => {
  const safeGameId = normalizeGameId(gameId);
  const safeRomPath = normalizeIpcPath(romPath);
  if (!safeGameId || !safeRomPath) return { ok: false, error: "missing_args" };
  const config = readConfig();
  if (!config.roms) config.roms = {};
  config.roms[safeGameId] = safeRomPath;
  writeConfig(config);
  return { ok: true };
});

secureIpcHandle("config:deleteRom", async (_event, gameId) => {
  const safeGameId = normalizeGameId(gameId);
  if (!safeGameId) return { ok: false, error: "missing_game_id" };
  const config = readConfig();
  if (!config.roms || typeof config.roms !== "object") return { ok: true };
  if (Object.prototype.hasOwnProperty.call(config.roms, safeGameId)) {
    delete config.roms[safeGameId];
    writeConfig(config);
  }
  return { ok: true };
});

secureIpcHandle("config:setGame", async (_event, gameId, patch) => {
  const safeGameId = normalizeGameId(gameId);
  if (!safeGameId) return { ok: false, error: "missing_game_id" };
  const nextPatch = isPlainObject(patch) ? patch : {};

  const config = readConfig();
  config.games = config.games && typeof config.games === "object" ? config.games : {};
  const current = config.games[safeGameId] && typeof config.games[safeGameId] === "object" ? config.games[safeGameId] : {};

  // Shallow merge; game configs are small and explicit.
  config.games[safeGameId] = { ...current, ...nextPatch };
  writeConfig(config);
  return { ok: true };
});

secureIpcHandle("config:setWindowing", async (_event, windowing) => {
  const config = readConfig();
  const next = isPlainObject(windowing) ? windowing : {};
  const gamescope = isPlainObject(next.gamescope) ? next.gamescope : {};

  const enabled = Boolean(gamescope.enabled);
  const mode = gamescope.mode === "require" ? "require" : "prefer";
  const fullscreen = typeof gamescope.fullscreen === "boolean" ? gamescope.fullscreen : true;
  const width = Number.isFinite(gamescope.width) ? Math.max(1, Number(gamescope.width)) : undefined;
  const height = Number.isFinite(gamescope.height) ? Math.max(1, Number(gamescope.height)) : undefined;
  const args = Array.isArray(gamescope.args) ? gamescope.args.map((v) => String(v)) : [];

  config.windowing = config.windowing && typeof config.windowing === "object" ? config.windowing : {};
  config.windowing.gamescope = { enabled, mode, fullscreen, width, height, args };
  writeConfig(config);
  return { ok: true };
});

secureIpcHandle("config:setLayout", async (_event, layout) => {
  const config = readConfig();
  const next = isPlainObject(layout) ? layout : {};
  const preset = typeof next.preset === "string" ? next.preset : "handheld";
  const mode =
    next.mode === "side_by_side" || next.mode === "separate_displays" ? next.mode : undefined;
  const gameDisplay = Number.isFinite(next.game_display) ? Math.max(0, Number(next.game_display)) : 0;
  const trackerDisplay = Number.isFinite(next.tracker_display) ? Math.max(0, Number(next.tracker_display)) : 1;
  const split = Number.isFinite(next.split) ? Math.min(0.9, Math.max(0.1, Number(next.split))) : 0.7;

  config.layout = { preset, mode, game_display: gameDisplay, tracker_display: trackerDisplay, split };
  writeConfig(config);
  return { ok: true };
});

secureIpcHandle("roms:scan", async (_event, folderPath) => {
  const safeFolderPath = normalizeIpcPath(folderPath);
  if (!safeFolderPath) return { ok: false, error: "invalid_folder_path" };
  const results = await scanRomFolder(safeFolderPath);
  return { ok: true, results };
});

secureIpcHandle("roms:import", async (_event, filePath) => {
  const safeFilePath = normalizeIpcPath(filePath);
  if (!safeFilePath) return { ok: false, error: "invalid_file_path" };
  return importRomFile(safeFilePath);
});

secureIpcHandle("commonclient:start", async (_event, options) => {
  return startCommonClient(isPlainObject(options) ? options : {});
});

secureIpcHandle("commonclient:send", async (_event, command) => {
  const safeCommand = normalizeIpcString(command, 512);
  if (!safeCommand) return { ok: false, error: "invalid_command" };
  return sendCommonClientCommand(safeCommand);
});

secureIpcHandle("commonclient:stop", async () => {
  await stopCommonClient();
  return { ok: true };
});

secureIpcHandle("bizhawkclient:start", async (_event, options) => {
  return startBizHawkClient(isPlainObject(options) ? options : {});
});

secureIpcHandle("bizhawkclient:send", async (_event, command) => {
  const safeCommand = normalizeIpcString(command, 512);
  if (!safeCommand) return { ok: false, error: "invalid_command" };
  return sendBizHawkClientCommand(safeCommand);
});

secureIpcHandle("bizhawkclient:stop", async () => {
  await stopBizHawkClient();
  return { ok: true };
});

secureIpcHandle("patcher:apply", async (_event, options) => {
  return runPatcher(isPlainObject(options) ? options : {});
});

secureIpcHandle("patcher:resolveCachedRom", async (_event, options) => {
  return resolvePatchedRomPlan(isPlainObject(options) ? options : {});
});

secureIpcHandle("patcher:listCachedRoms", async () => {
  return { ok: true, entries: patchedRomCache.listEntries() };
});

secureIpcHandle("bizhawk:launch", async (_event, options) => {
  return launchBizHawk(isPlainObject(options) ? options : {});
});

secureIpcHandle("bizhawk:stop", async (_event, pid) => {
  const safePid = Number(pid);
  if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
  return stopBizHawk(safePid);
});

secureIpcHandle("tracker:launch", async (_event, options) => {
  return launchPopTracker(isPlainObject(options) ? options : {});
});

secureIpcHandle("tracker:stop", async (_event, pid) => {
  const safePid = Number(pid);
  if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
  return stopPopTracker(safePid);
});

secureIpcHandle("tracker:installPack", async (_event, options) => {
  return installTrackerPack(isPlainObject(options) ? options : {});
});

secureIpcHandle("tracker:status", async () => {
  return getPopTrackerStatus();
});

secureIpcHandle("tracker:validatePack", async (_event, gameId) => {
  const safeGameId = normalizeGameId(gameId);
  if (!safeGameId) return { ok: false, error: "missing_game_id" };
  const installed = getInstalledTrackerPack(safeGameId);
  if (!installed?.path) return { ok: true, valid: false, error: "pack_missing" };
  const validation = validatePackDir(installed.path);
  return { ok: true, valid: validation.ok, error: validation.error };
});

secureIpcHandle("tracker:listPackVariants", async (_event, options) => {
  const safeOptions = isPlainObject(options) ? options : {};
  const gameId = normalizeGameId(safeOptions.gameId);
  const packPath = normalizeIpcPath(safeOptions.packPath);
  const uidOrPath = packPath || (gameId ? getInstalledTrackerPack(gameId)?.path : "");
  return listPopTrackerPackVariants(uidOrPath || "");
});

secureIpcHandle("tracker:setVariant", async (_event, gameId, variant) => {
  const safeGameId = normalizeGameId(gameId);
  if (!safeGameId) return { ok: false, error: "missing_game_id" };
  return setTrackerVariant(safeGameId, normalizeIpcString(variant, 128));
});

secureIpcHandle("session:trackerVariantResponse", async (_event, payload) => {
  const safe = isPlainObject(payload) ? payload : {};
  const requestId = normalizeIpcString(safe.requestId, 128);
  if (!requestId) return { ok: false, error: "missing_request_id" };
  const pending = pendingTrackerVariantRequests.get(requestId);
  if (!pending) return { ok: false, error: "request_not_found" };
  pendingTrackerVariantRequests.delete(requestId);
  pending.resolve({
    cancel: safe.cancel === true,
    variant: normalizeIpcString(safe.variant, 128) || "",
  });
  return { ok: true };
});

secureIpcHandle("log:renderer", async (_event, payload) => {
  if (!isPlainObject(payload)) return { ok: false, error: "invalid_payload" };
  writeLogJson("renderer", payload);
  return { ok: true };
});

secureIpcHandle("runtime:resolveModuleForDownload", async (_event, downloadUrl) => {
  const safeUrl = normalizeIpcString(downloadUrl, 2048);
  if (!safeUrl || !isSafeExternalUrl(safeUrl)) return { ok: false, error: "invalid_download_url" };
  return resolveModuleForDownload(safeUrl);
});

secureIpcHandle("runtime:planSessionAutoLaunch", async (_event, options) => {
  const safeOptions = isPlainObject(options) ? options : {};
  const safeUrl = normalizeIpcString(safeOptions.downloadUrl, 2048);
  const safeGame = normalizeIpcString(safeOptions.apGameName, 256);
  if (safeUrl && !isSafeExternalUrl(safeUrl)) return { ok: false, error: "invalid_download_url" };
  return planSessionAutoLaunch({
    downloadUrl: safeUrl,
    apGameName: safeGame,
  });
});

secureIpcHandle("runtime:getModuleManifest", async (_event, moduleId) => {
  const safeModuleId = normalizeGameId(moduleId);
  if (!safeModuleId) return { ok: false, error: "invalid_module_id" };
  const manifest = getModuleManifest(safeModuleId);
  if (!manifest) return { ok: false, error: "manifest_missing" };
  return { ok: true, manifest };
});

secureIpcHandle("runtime:validateSetupForModule", async (_event, moduleId) => {
  const safeModuleId = normalizeGameId(moduleId);
  if (!safeModuleId) return { ok: false, error: "invalid_module_id" };
  return validateSetupForModule(safeModuleId);
});

secureIpcHandle("runtime:listModules", async () => {
  return { ok: true, modules: listRuntimeModules() };
});

const getLinkedWorldInstallRoot = () => path.join(app.getPath("userData"), "linkedworlds");

const getLinkedWorldRuntimeRoot = (worldId) => {
  const safeId = normalizeGameId(worldId);
  return path.join(app.getPath("home"), ".local", "share", "sekailink", "linkedworlds", safeId || "wind-waker");
};

const resolveBundledLinkedWorldArchive = (worldId, version = "") => {
  const safeId = normalizeGameId(worldId);
  const safeVersion = normalizeIpcString(version, 64) || "";
  if (!safeId) return "";
  const archiveName = safeVersion ? `${safeId}-${safeVersion}.linkedworld` : `${safeId}.linkedworld`;
  const envKey = `SEKAILINK_${safeId.replace(/[^a-z0-9]/gi, "_").toUpperCase()}_LINKEDWORLD`;
  const candidates = [
    normalizeIpcPath(process.env[envKey] || ""),
    normalizeIpcPath(process.env.SEKAILINK_LINKEDWORLD_ARCHIVE || ""),
    path.join(process.resourcesPath || "", "linkedworlds", archiveName),
    path.join(__dirname, "..", "..", "linkedworlds", archiveName),
    path.join(app.getPath("home"), "sekailink-emu-repo", "dist", "linkedworlds", archiveName),
    path.join(app.getPath("home"), "sekailink", "dist", "linkedworlds", archiveName),
  ].filter(Boolean);
  for (const candidate of candidates) {
    try {
      if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
    } catch (_err) {
      // keep looking
    }
  }
  return "";
};

const resolveBundledWindWakerLinkedWorldArchive = () => {
  const candidates = [
    normalizeIpcPath(process.env.SEKAILINK_WIND_WAKER_LINKEDWORLD || ""),
    resolveBundledLinkedWorldArchive("wind-waker", "0.1.0-dev"),
    path.join(process.resourcesPath || "", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
    path.join(__dirname, "..", "..", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
    path.join(app.getPath("home"), "sekailink-emu-repo", "dist", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
    path.join(app.getPath("home"), "sekailink", "dist", "linkedworlds", "wind-waker-0.1.0-dev.linkedworld"),
  ].filter(Boolean);
  for (const candidate of candidates) {
    try {
      if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
    } catch (_err) {
      // keep looking
    }
  }
  return "";
};

const readLinkedWorldManifest = (worldRoot) => {
  try {
    const manifestPath = path.join(worldRoot, "linkedworld.json");
    if (!fs.existsSync(manifestPath)) return null;
    return JSON.parse(fs.readFileSync(manifestPath, "utf8"));
  } catch (_err) {
    return null;
  }
};

const chmodExecutableIfPresent = (targetPath) => {
  try {
    if (!targetPath || !fs.existsSync(targetPath)) return;
    const stat = fs.statSync(targetPath);
    fs.chmodSync(targetPath, stat.mode | 0o111);
  } catch (_err) {
    // best-effort on non-POSIX targets
  }
};

const describeWindWakerLinkedWorld = () => {
  const archivePath = resolveBundledWindWakerLinkedWorldArchive();
  const root = path.join(getLinkedWorldInstallRoot(), "wind-waker");
  const manifest = readLinkedWorldManifest(root);
  const runtimeRoot = getLinkedWorldRuntimeRoot("wind-waker");
  const dolphinPath = path.join(root, "runtime", process.platform === "win32" ? "windows" : "linux", process.platform === "win32" ? "dolphin-emu-nogui.exe" : "dolphin-emu-nogui");
  const serverPath = path.join(root, "runtime", process.platform === "win32" ? "windows" : "linux", process.platform === "win32" ? "sekailink-wind-waker-ap-server.exe" : "sekailink-wind-waker-ap-server");
  const aptwwPath = path.join(root, "seeds", "Jade-WW.aptww");
  const trackerPath = path.join(root, "tracker", "ww-poptracker-1.3.0.zip");
  const randomizedIsoPath = path.join(runtimeRoot, "seeds", "Jade-WW.GZLE99.iso");
  const errors = [];
  if (!manifest) errors.push("manifest_missing");
  if (!fs.existsSync(dolphinPath)) errors.push("dolphin_missing");
  if (!fs.existsSync(serverPath)) errors.push("local_server_missing");
  if (!fs.existsSync(aptwwPath)) errors.push("seed_missing");
  if (!fs.existsSync(trackerPath)) errors.push("tracker_pack_missing");
  if (!fs.existsSync(randomizedIsoPath)) errors.push("randomized_iso_missing");
  return {
    id: "wind-waker",
    installed: Boolean(manifest),
    installPath: root,
    manifest: manifest || undefined,
    ready: errors.length === 0,
    errors,
    archiveAvailable: Boolean(archivePath),
    archivePath,
    runtimeRoot,
    paths: {
      dolphinPath,
      serverPath,
      aptwwPath,
      trackerPath,
      randomizedIsoPath,
      userDir: path.join(runtimeRoot, "dolphin-user"),
      logDir: path.join(runtimeRoot, "logs"),
    },
  };
};

const installWindWakerLinkedWorldTest = async () => {
  const archivePath = resolveBundledWindWakerLinkedWorldArchive();
  if (!archivePath) return { ok: false, error: "linkedworld_archive_missing" };
  const installRoot = getLinkedWorldInstallRoot();
  const targetRoot = path.join(installRoot, "wind-waker");
  fs.mkdirSync(installRoot, { recursive: true });
  fs.rmSync(targetRoot, { recursive: true, force: true });
  const res = spawnSync("tar", ["--zstd", "-xf", archivePath, "-C", installRoot], {
    encoding: "utf8",
    timeout: 120000,
  });
  if (res.status !== 0) {
    return {
      ok: false,
      error: "linkedworld_extract_failed",
      detail: String(res.stderr || res.stdout || `tar exited ${res.status}`),
    };
  }
  const world = describeWindWakerLinkedWorld();
  chmodExecutableIfPresent(world.paths?.dolphinPath);
  chmodExecutableIfPresent(world.paths?.serverPath);
  writeLogJson("linkedworld.install", {
    id: "wind-waker",
    archivePath,
    installPath: targetRoot,
    ready: world.ready,
    errors: world.errors,
  });
  return { ok: true, world };
};

const installLinkedWorld = async (options = {}) => {
  const safe = isPlainObject(options) ? options : {};
  const id = normalizeGameId(safe.id || safe.worldId || "");
  if (!id) return { ok: false, error: "invalid_linkedworld_id" };
  if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
  return installWindWakerLinkedWorldTest();
};

const validateLinkedWorld = async (options = {}) => {
  const safe = isPlainObject(options) ? options : {};
  const id = normalizeGameId(safe.id || safe.worldId || "");
  if (!id) return { ok: false, error: "invalid_linkedworld_id" };
  if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
  return { ok: true, world: describeWindWakerLinkedWorld() };
};

const launchLinkedWorld = async (options = {}) => {
  const safe = isPlainObject(options) ? options : {};
  const id = normalizeGameId(safe.id || safe.worldId || "wind-waker");
  const mode = normalizeIpcString(safe.mode || "solo", 32) || "solo";
  if (id !== "wind-waker") return { ok: false, error: "linkedworld_unsupported" };
  if (mode !== "solo" && mode !== "offline") return { ok: false, error: "linkedworld_mode_unsupported" };
  return launchWindWakerLinkedWorldSolo(safe);
};

const findFreeLocalPort = (preferred = 38281) => new Promise((resolve) => {
  const server = net.createServer();
  server.once("error", () => {
    const fallback = net.createServer();
    fallback.listen(0, "127.0.0.1", () => {
      const address = fallback.address();
      const port = typeof address === "object" && address ? address.port : 0;
      fallback.close(() => resolve(port || preferred));
    });
  });
  server.listen(preferred, "127.0.0.1", () => {
    const address = server.address();
    const port = typeof address === "object" && address ? address.port : preferred;
    server.close(() => resolve(port));
  });
});

const attachLinkedWorldProcessLog = (worldId, name, proc) => {
  const prefix = `linkedworld:${worldId}:${name}`;
  const attach = (stream, level) => {
    if (!stream) return;
    const rl = readline.createInterface({ input: stream });
    rl.on("line", (line) => {
      const text = String(line || "").trim();
      if (text) writeLogLine(level, prefix, text);
    });
    proc.once("close", () => rl.close());
  };
  attach(proc.stdout, "info");
  attach(proc.stderr, "warn");
};

const launchWindWakerLinkedWorldSolo = async (options = {}) => {
  let world = describeWindWakerLinkedWorld();
  if (!world.installed || options.reinstall === true) {
    const installed = await installWindWakerLinkedWorldTest();
    if (!installed.ok) return installed;
    world = describeWindWakerLinkedWorld();
  }
  if (!world.ready) {
    return { ok: false, error: "linkedworld_not_ready", detail: world.errors.join(","), world };
  }
  const port = Number.isFinite(Number(options.port)) && Number(options.port) > 0
    ? Math.round(Number(options.port))
    : await findFreeLocalPort(38281);
  fs.mkdirSync(world.paths.logDir, { recursive: true });
  fs.mkdirSync(world.paths.userDir, { recursive: true });

  const serverArgs = [
    `--port=${port}`,
    "--seconds=0",
    `--linkedworld-root=${world.installPath}`,
    `--aptww=${world.paths.aptwwPath}`,
    "--slot=Jade-WW",
  ];
  const serverProc = spawn(world.paths.serverPath, serverArgs, {
    cwd: world.installPath,
    stdio: ["ignore", "pipe", "pipe"],
    env: { ...process.env },
  });
  attachLinkedWorldProcessLog("wind-waker", "server", serverProc);

  await new Promise((resolve) => setTimeout(resolve, 550));
  if (serverProc.exitCode !== null) {
    return { ok: false, error: "linkedworld_server_exited", detail: `exit=${serverProc.exitCode}` };
  }

  const dolphinArgs = [
    "--platform=x11",
    `--user=${world.paths.userDir}`,
    `--sekailink-manifest=${path.join(world.installPath, "linkedworld.json")}`,
    "--sekailink-mode=offline",
    `--sekailink-log-dir=${world.paths.logDir}`,
    `--sekailink-tracker-pack=${world.paths.trackerPath}`,
    "--sekailink-slot=Jade-WW",
    "--sekailink-server=127.0.0.1",
    `--sekailink-port=${port}`,
    `--exec=${world.paths.randomizedIsoPath}`,
  ];
  const dolphinProc = spawn(world.paths.dolphinPath, dolphinArgs, {
    cwd: world.installPath,
    stdio: ["ignore", "pipe", "pipe"],
    env: { ...process.env },
  });
  attachLinkedWorldProcessLog("wind-waker", "dolphin", dolphinProc);
  linkedWorldProcs.set(dolphinProc.pid, { worldId: "wind-waker", game: dolphinProc, server: serverProc, port });
  nativeGameProcs.set(dolphinProc.pid, dolphinProc);

  emitSessionEvent({
    event: "status",
    status: "Wind Waker LinkedWorld launched.",
    moduleId: "wind-waker",
    gamePid: dolphinProc.pid,
    serverPid: serverProc.pid,
    port,
  });
  writeLogJson("linkedworld.launch", {
    id: "wind-waker",
    gamePid: dolphinProc.pid,
    serverPid: serverProc.pid,
    port,
    dolphinArgs,
    serverArgs,
  });

  dolphinProc.once("exit", (code, signal) => {
    writeLogJson("linkedworld.exit", { id: "wind-waker", gamePid: dolphinProc.pid, code, signal });
    linkedWorldProcs.delete(dolphinProc.pid);
    nativeGameProcs.delete(dolphinProc.pid);
    try {
      if (!serverProc.killed) serverProc.kill("SIGTERM");
    } catch (_err) {
      // ignore shutdown race
    }
  });
  serverProc.once("exit", (code, signal) => {
    writeLogJson("linkedworld.server.exit", { id: "wind-waker", serverPid: serverProc.pid, code, signal });
  });

  return { ok: true, gamePid: dolphinProc.pid, serverPid: serverProc.pid, port, world };
};

secureIpcHandle("linkedworld:list", async () => {
  return { ok: true, worlds: [describeWindWakerLinkedWorld()] };
});

secureIpcHandle("linkedworld:validate", async (_event, options) => {
  return validateLinkedWorld(options);
});

secureIpcHandle("linkedworld:install", async (_event, options) => {
  return installLinkedWorld(options);
});

secureIpcHandle("linkedworld:launch", async (_event, options) => {
  return launchLinkedWorld(options);
});

secureIpcHandle("linkedworld:installWindWakerTest", async () => {
  return installWindWakerLinkedWorldTest();
});

secureIpcHandle("linkedworld:launchSolo", async (_event, options) => {
  return launchLinkedWorld({ ...(isPlainObject(options) ? options : {}), mode: "solo" });
});

secureIpcHandle("runtime:gamescopeStatus", async () => {
  const p = getGamescopePath();
  return { ok: true, exists: Boolean(p), path: p || "" };
});

secureIpcHandle("runtime:wmctrlStatus", async () => {
  const p = getWmctrlPath();
  return { ok: true, exists: Boolean(p), path: p || "" };
});

secureIpcHandle("runtime:getDisplays", async () => {
  const displays = screen.getAllDisplays().map((d, idx) => ({
    index: idx,
    id: d.id,
    bounds: d.bounds,
    workArea: d.workArea,
    scaleFactor: d.scaleFactor,
    internal: Boolean(d.internal),
    rotation: d.rotation
  }));
  return { ok: true, displays };
});

secureIpcHandle("session:autoLaunch", async (_event, options) => {
  try {
    return await autoLaunchFromPatchUrl(isPlainObject(options) ? options : {});
  } catch (err) {
    const message = String(err?.message || err || "").trim();
    const errorCode = (() => {
      const m = message.match(/^([a-z0-9_]+)\s*:/i);
      return m && m[1] ? String(m[1]).toLowerCase() : "autolaunch_failed";
    })();
    emitSessionEvent({ event: "error", step: "autolaunch", error: errorCode, detail: message || "Unexpected launch error." });
    return {
      ok: false,
      error: errorCode,
      detail: message || "Unexpected launch error.",
    };
  }
});
