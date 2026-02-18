const { app, BrowserWindow, ipcMain, shell, screen, dialog, clipboard } = require("electron");
const { spawn, spawnSync } = require("child_process");
const readline = require("readline");
const fs = require("fs");
const crypto = require("crypto");
const https = require("https");
const http = require("http");
const net = require("net");
const path = require("path");
const { createLogger } = require("./lib/logger.cjs");
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

const getBundledRuntimeDir = () => {
  // In packaged apps, runtime is shipped via electron-builder extraResources to process.resourcesPath/runtime.
  // In dev, runtime lives in repo at client/runtime relative to this file (client/app/electron).
  if (app.isPackaged) return path.join(process.resourcesPath, "runtime");
  return path.join(__dirname, "..", "..", "runtime");
};

const getBundledThirdPartyDir = () => {
  // In packaged apps, third_party is shipped via extraResources to process.resourcesPath/third_party.
  // In dev, third_party lives at repo root (../../../third_party relative to client/app/electron).
  if (app.isPackaged) return path.join(process.resourcesPath, "third_party");
  return path.join(__dirname, "..", "..", "..", "third_party");
};

const getBundledApRootDir = () => {
  // In packaged apps, the curated AP python sources are shipped via extraResources to process.resourcesPath/ap.
  // In dev, the AP python sources live at repo root.
  if (app.isPackaged) return path.join(process.resourcesPath, "ap");
  return path.join(__dirname, "..", "..", "..");
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
  if (fs.existsSync(overlay)) dirs.push(overlay);
  dirs.push(getBundledApRootDir());
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
const activeUpdaterDownloads = new Map();
const updaterDownloadResults = new Map();
const updaterDownloadWaiters = new Map();

const COMMONCLIENT_EVENT_CHANNEL = "commonclient:event";
const BIZHAWKCLIENT_EVENT_CHANNEL = "bizhawkclient:event";
const SESSION_EVENT_CHANNEL = "session:event";
const UPDATER_EVENT_CHANNEL = "updater:event";
const LOG_EVENT_CHANNEL = "log:event";

let _gamescopePathCache = undefined;
let _wmctrlPathCache = undefined;
let crashReportingOptIn = false;

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
  if (existingPortable.length) return existingPortable[0];

  const root = getWindowsPortablePythonRoot();

  emitSessionEvent({ event: "status", status: "Installing portable Python runtime (Windows, one-time)..." });
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
  if (process.platform === "win32") {
    for (const portable of listExistingWindowsPortablePythonExes()) {
      candidates.push({ cmd: portable, args: [], label: "portable-win-runtime" });
    }
  }
  if (process.platform === "win32") {
    candidates.push(
      { cmd: "py", args: ["-3"], label: "py3" },
      { cmd: "py", args: [], label: "py" },
      { cmd: "python3", args: [], label: "python3" },
      { cmd: "python", args: [], label: "python" }
    );
  } else {
    candidates.push({ cmd: "python3", args: [], label: "python3" }, { cmd: "python", args: [], label: "python" });
  }
  return candidates;
};

const probePythonCandidate = async (candidate) => {
  const cmd = String(candidate?.cmd || "").trim();
  const args = Array.isArray(candidate?.args) ? candidate.args : [];
  if (!cmd) return false;
  const probe = await runProcess(cmd, [...args, "-c", "import sys; print(sys.executable)"], {
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
  if (process.platform === "win32") {
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
          "import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, bsdiff4, websockets, certifi, orjson; print('ok')",
        ],
        {
          env: { ...process.env, PYTHONNOUSERSITE: "1" },
        }
      );
      if (check.code === 0) return venvPython;
      writeLogLine("warn", "python-runtime", `venv present but missing deps; will reinstall. stderr=${String(check.stderr || "").trim()}`);
    }

    emitSessionEvent({ event: "status", status: "Installing Python runtime dependencies (one-time)..." });

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

    // Upgrade pip and install minimal deps required by the bundled AP python sources.
    const pipEnv = {
      ...process.env,
      PYTHONNOUSERSITE: "1",
      PIP_DISABLE_PIP_VERSION_CHECK: "1",
      PIP_NO_INPUT: "1",
    };
    // Pin setuptools<70 to ensure pkg_resources is included (removed in setuptools 70+ on Python 3.12+)
    const upgraded = await runProcess(venvPython, ["-m", "pip", "install", "--upgrade", "pip", "setuptools<70", "wheel"], { env: pipEnv });
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
    ]);

    const installOne = async (spec) => {
      const s = String(spec || "").trim();
      if (!s) return { ok: true };
      const res = await runProcess(venvPython, ["-m", "pip", "install", s], { env: pipEnv });
      if (res.code !== 0) {
        const msg = String(res.stderr || res.stdout || "pip_failed").trim();
        writeLogLine("error", "python-runtime", `pip install failed spec=${s}: ${msg}`);
        return { ok: false, error: msg };
      }
      return { ok: true };
    };

    // Install base deps, then auto-heal any missing imports we discover.
    const base = await runProcess(venvPython, ["-m", "pip", "install", ...Array.from(deps)], { env: pipEnv });
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
      emitSessionEvent({ event: "status", status: `Installing missing Python module: ${missing}` });
      const one = await installOne(spec);
      if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
    }

    // Probe imports of the bundled AP sources and the worlds we ship for autorun (MVP set).
    // This catches missing deps that don't show up in the "core" module list above.
    const apRoot = getBundledApRootDir();
    const worldImports = [
      "worlds.tmc",
      "worlds.tloz",
      "worlds.zelda2",
      "worlds.mzm",
      "worlds.metroidfusion",
      "worlds.wl4",
      "worlds.wl",
      "worlds.marioland2",
      "worlds.pokemon_rb",
      "worlds.pokemon_crystal",
      "worlds.pokemon_frlg",
      "worlds.pokemon_emerald",
      "worlds.sm",
      "worlds.smw",
      "worlds.dkc",
      "worlds.dkc2",
      "worlds.dkc3",
      "worlds.earthbound",
      "worlds.kdl3",
      "worlds.lufia2ac",
      "worlds.mm2",
      "worlds.mm3",
      "worlds.oot",
      "worlds.smz3",
      "worlds.alttp",
      "worlds.tloz_oos",
    ];
    const apProbeCode = `
import importlib, os, sys
ap_root = os.environ.get("SEKAILINK_AP_ROOT","")
if ap_root and ap_root not in sys.path:
  sys.path.insert(0, ap_root)
mods = ["Patch", "CommonClient", "NetUtils", "Utils"] + ${JSON.stringify(worldImports)}
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
      emitSessionEvent({ event: "status", status: `Installing missing Python module: ${missing}` });
      const one = await installOne(spec);
      if (!one.ok) throw new Error(`python_deps_install_failed: ${one.error || "pip_failed"}`);
    }

    // Verify.
    const verify = await runProcess(
      venvPython,
      [
        "-c",
        "import pkg_resources, yaml, pathspec, platformdirs, schema, jinja2, colorama, typing_extensions, jellyfish, bsdiff4, websockets, certifi, orjson; print('ok')",
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

    emitSessionEvent({ event: "status", status: "Python runtime ready." });
    return venvPython;
  })().catch((err) => {
    // Allow retry after transient bootstrap failures (download/network/corrupt partial install).
    _pythonRuntimePromise = null;
    throw err;
  });
  return _pythonRuntimePromise;
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
  return getRuntimeFilePath("poptracker");
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
  return getRuntimeFilePath(path.join("_bundled_libs", "poptracker"));
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
        return resolve(downloadToFile(res.headers.location, destPath));
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
    "Accept": "application/octet-stream,application/x-msdownload,application/x-iso9660-image,*/*",
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

const openDownloadedUpdaterFile = async (targetPath) => {
  const p = String(targetPath || "").trim();
  if (!p) return { ok: false, error: "missing_path" };
  if (!fs.existsSync(p)) return { ok: false, error: "file_missing" };
  const fileNameLower = path.basename(p).toLowerCase();
  const ext = path.extname(p).toLowerCase();
  const isLinuxAppImage = process.platform === "linux" && (ext === ".appimage" || fileNameLower.includes(".appimage"));
  const isWindowsExe = process.platform === "win32" && ext === ".exe";

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
  const started = await startClientUpdaterDownload(options || {});
  if (!started?.ok) return started;
  const timeoutMs = Number(options?.timeoutMs || 25 * 60 * 1000);
  const completed = await waitForUpdaterDownload(started.downloadId, Number.isFinite(timeoutMs) ? timeoutMs : 0);
  if (!completed?.ok) {
    return { ok: false, error: completed?.error || "download_failed", downloadId: started.downloadId };
  }
  const apply = await openDownloadedUpdaterFile(completed.path);
  if (!apply?.ok) {
    return { ok: false, error: apply?.error || "apply_failed", downloadId: started.downloadId, path: completed.path };
  }
  return { ok: true, downloadId: started.downloadId, path: completed.path, ...apply };
};

const getIncrementalSyncStatePath = () => {
  return path.join(getRuntimeOverlayRoot(), "client-sync-state.json");
};

const loadIncrementalSyncState = () => {
  const statePath = getIncrementalSyncStatePath();
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

const saveIncrementalSyncState = (state) => {
  const statePath = getIncrementalSyncStatePath();
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
  if (activeUpdaterDownloads.has("incremental-sync")) {
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

  activeUpdaterDownloads.set("incremental-sync", { startedAt: Date.now() });
  emitUpdaterEvent({
    event: "incremental-sync-started",
    manifestUrl,
  });

  try {
    const manifest = await httpGetJson(manifestUrl, { headers: manifestHeaders, accept: "application/json" });
    const files = Array.isArray(manifest?.files) ? manifest.files : [];
    const baseUrl = String(manifest?.base_url || "").trim() || manifestUrl;
    const manifestVersion = String(manifest?.version || "").trim();
    const overlayRoot = getRuntimeOverlayRoot();
    ensureDir(overlayRoot);

    const prevState = loadIncrementalSyncState();
    const nextState = {
      version: manifestVersion,
      updatedAt: nowIso(),
      files: {},
    };

    let processed = 0;
    let changed = 0;
    let deleted = 0;
    let downloadedBytes = 0;
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
          });
        }
      } catch (_err) {
        // ignore cleanup errors
      }
    }

    saveIncrementalSyncState(nextState);
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
    });
    return { ok: true, changed, deleted, processed, total, downloadedBytes };
  } catch (err) {
    const message = String(err || "incremental_sync_failed");
    emitUpdaterEvent({
      event: "incremental-sync-error",
      error: message,
    });
    return { ok: false, error: message };
  } finally {
    activeUpdaterDownloads.delete("incremental-sync");
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

const moduleHasExternalTracker = (manifest = {}) => {
  const trackerPack = String(manifest?.tracker_pack_uid || "").trim();
  const trackerWeb = String(manifest?.tracker_web_url || "").trim();
  return Boolean(trackerPack || trackerWeb);
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
  try {
    commonClientProc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
  } catch (_err) {
    // ignore
  }
  writeLogLine("info", "commonclient", `stopping pid=${commonClientProc.pid || ""}`);
  commonClientProc.kill();
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
    return { ok: false, error: "sni_bridge_start_timeout" };
  }
  return { ok: true, host, port };
};

const stopSniBridge = async () => {
  if (!sniBridgeProc) return;
  try {
    sniBridgeProc.kill();
  } catch (_err) {
    // ignore
  }
  sniBridgeProc = null;
};

const stopBizHawkClient = async () => {
  if (!bizhawkClientProc) return;
  try {
    bizhawkClientProc.stdin.write(JSON.stringify({ cmd: "shutdown" }) + "\n");
  } catch (_err) {
    // ignore
  }
  writeLogLine("info", "bizhawkclient", `stopping pid=${bizhawkClientProc.pid || ""}`);
  bizhawkClientProc.kill();
  bizhawkClientProc = null;
  bizhawkClientKind = "bizhawk";
  void stopSniBridge();
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
      return { ok: true, baseDir: overrideDir, emuPath: overridePath };
    }
    emitSessionEvent({ event: "status", status: "Preparing BizHawk runtime (read-only override detected)...", tool: "bizhawk" });
    writeLogLine("warn", "bizhawk", `override is read-only; staging runtime to writable dir: ${overrideDir} -> ${dest}`);
    const staged = stageBizHawkToDir(overrideDir, dest);
    if (!staged.ok) return staged;
    return { ok: true, baseDir: dest, emuPath: path.join(dest, path.basename(overridePath)) };
  }

  const bundled = getBizHawkBaseDir();
  if (!app.isPackaged) return { ok: true, baseDir: bundled };

  const marker = path.join(dest, process.platform === "win32" ? "EmuHawk.exe" : "EmuHawkMono.sh");
  if (fs.existsSync(marker)) return { ok: true, baseDir: dest };

  emitSessionEvent({ event: "status", status: "Installing BizHawk runtime (one-time)...", tool: "bizhawk" });
  writeLogLine("info", "bizhawk", `staging runtime to writable dir: ${dest}`);
  return stageBizHawkToDir(bundled, dest);
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

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

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
  proc.on("exit", () => {
    bizhawkProcs.delete(proc.pid);
  });
  return { ok: true, pid: proc.pid, gamescope: Boolean(wrapped.wrapped) };
};

const stopBizHawk = async (pid) => {
  if (pid && bizhawkProcs.has(pid)) {
    const proc = bizhawkProcs.get(pid);
    proc.kill();
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
  const buttons = choices.map((c) => c.name.slice(0, 64));
  buttons.push("Cancel launch");
  const detail = choices.map((c, idx) => `${idx + 1}. ${c.name} [${c.id || "default"}]`).join("\n");

  const result = await dialog.showMessageBox(mainWindow || undefined, {
    type: "question",
    buttons,
    defaultId: 0,
    cancelId: buttons.length - 1,
    noLink: true,
    title: "Choose PopTracker Layout",
    message: `Select a tracker layout for ${gameLabel}.`,
    detail,
  });
  if (result.response === buttons.length - 1) return { ok: false, error: "tracker_variant_cancelled" };

  const picked = choices[result.response] || choices[0];
  setTrackerVariant(gameId, picked.id || "");
  return { ok: true, variant: picked.id || "" };
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
    if (!packPath && manifest?.tracker_pack_uid) {
      const repo = resolveGithubRepo(manifest.tracker_pack_uid);
      if (repo && gameId) {
        const installed = await installTrackerPack({
          gameId,
          packRepo: repo,
          assetRegex: manifest?.tracker_asset_regex || "\\.zip$"
        });
        if (installed?.ok && installed.path) packPath = installed.path;
      } else if (!packUid && !isUrl(manifest.tracker_pack_uid)) {
        // Legacy: allow loading by UID when it's not a URL.
        packUid = manifest.tracker_pack_uid;
      }
    }
  }

  if (packUid && isUrl(packUid)) {
    packUid = null;
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
  proc.stdout.on("data", (chunk) => {
    const text = String(chunk || "");
    if (!text.trim()) return;
    stdoutTail = (stdoutTail + text).slice(-4000);
    const lines = text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
    for (const line of lines.slice(-4)) {
      emitTrackerClientLog("info", `PopTracker stdout: ${line}`);
    }
  });
  let stderrTail = "";
  proc.stderr.on("data", (chunk) => {
    const text = String(chunk || "");
    if (!text.trim()) return;
    stderrTail = (stderrTail + text).slice(-4000);
    const lines = text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
    for (const line of lines.slice(-4)) {
      emitTrackerClientLog("warn", `PopTracker stderr: ${line}`);
    }
  });
  proc.on("error", (err) => {
    emitTrackerClientLog("error", `PopTracker process error: ${String(err)}`);
  });

  trackerProcs.set(proc.pid, proc);
  proc.on("exit", (code, signal) => {
    emitTrackerClientLog("warn", `PopTracker exited (code=${code ?? "null"}, signal=${signal || "none"}).`);
    trackerProcs.delete(proc.pid);
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
    proc.kill();
    trackerProcs.delete(pid);
    return { ok: true };
  }
  return { ok: false, error: "not_running" };
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
  if (bizhawkClientProc) return { ok: true, alreadyRunning: true };

  const python = await ensurePythonRuntime();
  const requestedKind = String(options.clientKind || "").trim().toLowerCase();
  const kind = requestedKind === "sni" ? "sni" : "bizhawk";
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

let _patchedRomCacheData = null;

const getDefaultPatcherConfigPath = () => {
  return path.join(app.getPath("home"), ".sekailink", "config.json");
};

const getPatchedRomCachePath = () => {
  return path.join(app.getPath("userData"), "runtime", "patched-rom-cache.json");
};

const loadPatchedRomCache = () => {
  if (_patchedRomCacheData) return _patchedRomCacheData;
  const p = getPatchedRomCachePath();
  try {
    if (fs.existsSync(p)) {
      const parsed = JSON.parse(fs.readFileSync(p, "utf-8"));
      if (parsed && typeof parsed === "object" && parsed.entries && typeof parsed.entries === "object") {
        _patchedRomCacheData = parsed;
        return _patchedRomCacheData;
      }
    }
  } catch (_err) {
    // ignore malformed cache and recreate
  }
  _patchedRomCacheData = { entries: {} };
  return _patchedRomCacheData;
};

const savePatchedRomCache = () => {
  try {
    const cachePath = getPatchedRomCachePath();
    ensureDir(path.dirname(cachePath));
    fs.writeFileSync(cachePath, JSON.stringify(loadPatchedRomCache(), null, 2), "utf-8");
  } catch (_err) {
    // best-effort cache
  }
};

const getConfigStampForPatchCache = (configPath) => {
  try {
    const stat = fs.statSync(configPath);
    return `${Math.trunc(stat.mtimeMs)}`;
  } catch (_err) {
    return "0";
  }
};

const makePatchedRomCacheKey = ({ moduleId, patchPath, configPath, patchHash }) => {
  const ext = path.extname(String(patchPath || "")).toLowerCase();
  const cfgStamp = getConfigStampForPatchCache(configPath);
  return `${String(moduleId || "")}|${ext}|${String(patchHash || "")}|cfg:${cfgStamp}`;
};

const tryFindCachedPatchedRom = async ({ moduleId, patchPath, configPath }) => {
  if (!moduleId || !patchPath || !fs.existsSync(patchPath)) {
    return { ok: false, error: "missing_args" };
  }
  const hash = await hashFile(patchPath, "md5");
  const key = makePatchedRomCacheKey({ moduleId, patchPath, configPath, patchHash: hash });
  const cache = loadPatchedRomCache();
  const entry = cache.entries[key];
  if (entry && entry.outputPath && fs.existsSync(entry.outputPath)) {
    return { ok: true, key, patchHash: hash, outputPath: entry.outputPath };
  }
  if (entry) {
    delete cache.entries[key];
    savePatchedRomCache();
  }
  return { ok: false, key, patchHash: hash, error: "cache_miss" };
};

const rememberPatchedRom = ({ key, moduleId, patchPath, patchHash, outputPath }) => {
  if (!key || !outputPath) return;
  const cache = loadPatchedRomCache();
  cache.entries[key] = {
    moduleId: String(moduleId || ""),
    patchName: path.basename(String(patchPath || "")),
    patchHash: String(patchHash || ""),
    outputPath: String(outputPath),
    updatedAt: nowIso(),
  };

  const rows = Object.entries(cache.entries);
  if (rows.length > 400) {
    rows
      .sort((a, b) => String(a[1]?.updatedAt || "").localeCompare(String(b[1]?.updatedAt || "")))
      .slice(0, rows.length - 400)
      .forEach(([k]) => delete cache.entries[k]);
  }
  savePatchedRomCache();
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
      return { ok: false, error: "mono_missing", detail: String(err || "") };
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
    if (!roms[gameId]) return { ok: false, error: "rom_missing", gameId };
  }

  // PopTracker is optional per module, but if a module declares a pack source, then PopTracker must exist.
  if (manifest.tracker_pack_uid) {
    const trackerStatus = getPopTrackerStatus();
    if (!trackerStatus.exists) return { ok: false, error: "poptracker_missing" };
  }

  return { ok: true, manifest };
};

const autoLaunchFromPatchUrl = async (options = {}) => {
  const downloadUrl = String(options.downloadUrl || "").trim();
  const serverAddress = options.serverAddress;
  const slot = options.slot;
  const password = options.password;
  const apGameName = options.apGameName;
  const forceTrackerVariantPrompt = options.forceTrackerVariantPrompt === true;

  if (!serverAddress) return { ok: false, error: "missing_server_address" };
  if (!slot) return { ok: false, error: "missing_slot" };

  // Patchless flow for native AP-integrated games (e.g. Ship of Harkinian).
  if (!downloadUrl) {
    const moduleId = findModuleByApGameName(apGameName);
    if (!moduleId) return { ok: false, error: "missing_patch_url" };

    emitSessionEvent({ event: "status", status: "Validating setup...", moduleId });
    const setup = await validateSetupForModule(moduleId);
    if (!setup.ok) {
      emitSessionEvent({ event: "error", step: "validate", error: setup.error, moduleId, gameId: setup.gameId });
      return setup;
    }

    const manifest = setup.manifest || getModuleManifest(moduleId) || {};
    if (String(manifest.patcher || "").toLowerCase() !== "none") {
      return { ok: false, error: "missing_patch_url" };
    }

    const emu = String(manifest.emu || "").toLowerCase();
    emitSessionEvent({ event: "status", status: `Launching ${emu || "game"}...`, moduleId });

    let emuRes = null;
    if (emu === "soh") {
      emuRes = await tryLaunchSoh();
    } else {
      emuRes = { ok: false, error: "unsupported_emulator", emu };
    }

    if (!emuRes?.ok) {
      emitSessionEvent({ event: "error", step: "emu", error: emuRes?.error || "emu_failed", moduleId, emu });
      return { ok: false, error: emuRes?.error || "emu_failed", detail: emuRes?.detail || "" };
    }

    emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId });
    const layoutRes = await applyLayoutBestEffort({ gamePid: emuRes.pid, trackerPid: null });
    if (!layoutRes?.ok) {
      emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId });
    }

    const note =
      "Ship of Harkinian started. Connect from in-game menu: ESC > Network > Archipelago.";
    emitSessionEvent({ event: "status", status: note, moduleId });
    emitSessionEvent({
      event: "ready",
      moduleId,
      emuPid: emuRes.pid,
      trackerPid: null,
      note,
    });
    return {
      ok: true,
      moduleId,
      emuPid: emuRes.pid,
      trackerPid: null,
      note,
      noPatch: true,
    };
  }

  emitSessionEvent({ event: "status", status: "Downloading...", downloadUrl });
  let dl = null;
  try {
    dl = await downloadToDirWithProgress(downloadUrl, getRuntimeDownloadsDir(), {
      defaultBasename: "ap-download",
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
    emitSessionEvent({ event: "error", step: "download", error: String(err) });
    return { ok: false, error: "download_failed" };
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
      status: artifact.gamePid ? "Artifact handled. Connecting..." : "Artifact installed. Manual launch may be required.",
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
      status: "Downloaded. Manual setup may be required for this game.",
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
      status: "Manual mode: file downloaded and client connected.",
      downloadedPath,
      ext
    });
    emitSessionEvent({ event: "manual", downloadedPath, ext, apGameName });
    return { ok: true, manual: true, downloadedPath, ext };
  }

  if (!canAutoPatch) {
    // We recognized the game by name, but not the file type. Keep the download and connect/tracker best-effort.
    emitSessionEvent({
      event: "status",
      status: "Downloaded. Auto-patching is not supported for this file type yet.",
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
      status: "Manual mode: file downloaded and client connected.",
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
    emitSessionEvent({ event: "error", step: "validate", error: setup.error, moduleId, gameId: setup.gameId });
    return setup;
  }

  const configPath = getDefaultPatcherConfigPath();
  let patchOutput = "";
  let patchCacheKey = "";
  let patchHash = "";
  let reusedPatchedRom = false;

  try {
    const cached = await tryFindCachedPatchedRom({ moduleId, patchPath: downloadedPath, configPath });
    patchCacheKey = cached.key || "";
    patchHash = cached.patchHash || "";
    if (cached.ok && cached.outputPath) {
      patchOutput = cached.outputPath;
      reusedPatchedRom = true;
      emitSessionEvent({ event: "status", status: "Patched ROM already exists. Reusing it...", moduleId });
      writeLogLine("info", "autolaunch", `reusing patched rom: moduleId=${moduleId} patchHash=${patchHash} output=${patchOutput}`);
    }
  } catch (err) {
    writeLogLine("warn", "autolaunch", `cache lookup failed; patching normally: ${String(err)}`);
  }

  if (!patchOutput) {
    emitSessionEvent({ event: "status", status: "Patching game...", moduleId });
    writeLogLine("info", "autolaunch", `patching: patchPath=${downloadedPath} moduleId=${moduleId}`);
    const patchRes = await runPatcher({ patchPath: downloadedPath, patchUrl: downloadUrl, configPath });
    writeLogLine("info", "autolaunch", `patch result: ok=${patchRes?.ok} output=${patchRes?.output || ""} error=${patchRes?.error || ""}`);
    if (!patchRes?.ok || !patchRes.output) {
      emitSessionEvent({ event: "error", step: "patch", error: patchRes?.error || "patch_failed", moduleId });
      return { ok: false, error: patchRes?.error || "patch_failed" };
    }
    patchOutput = patchRes.output;
    rememberPatchedRom({
      key: patchCacheKey,
      moduleId,
      patchPath: downloadedPath,
      patchHash,
      outputPath: patchOutput,
    });
  }

  const manifest = setup.manifest || getModuleManifest(moduleId) || {};
  const emu = manifest.emu || "bizhawk";
  emitSessionEvent({ event: "status", status: `Launching ${emu}...`, moduleId });
  writeLogLine("info", "autolaunch", `launching emulator: emu=${emu} romPath=${patchOutput} moduleId=${moduleId} reused=${reusedPatchedRom}`);

  let emuRes = null;
  if (emu === "bizhawk") {
    emuRes = await launchBizHawk({ romPath: patchOutput, moduleId });
  } else if (emu === "soh") {
    emuRes = await tryLaunchSoh();
  } else {
    emuRes = { ok: false, error: "unsupported_emulator", emu };
  }
  writeLogLine("info", "autolaunch", `emu result: ok=${emuRes?.ok} pid=${emuRes?.pid || ""} error=${emuRes?.error || ""}`);
  if (!emuRes?.ok) {
    emitSessionEvent({ event: "error", step: "emu", error: emuRes?.error || "emu_failed", moduleId, emu });
    return { ok: false, error: emuRes?.error || "emu_failed" };
  }

  emitSessionEvent({ event: "status", status: "Connecting to server...", moduleId });
  writeLogLine("info", "autolaunch", `starting client: emu=${emu} serverAddress=${serverAddress} slot=${slot}`);
  const clientMode = String(manifest.client_mode || "").trim().toLowerCase();
  if (emu === "bizhawk") {
    const apClient = String(manifest.ap_client || "").trim().toLowerCase();
    if (apClient === "sni") {
      emitSessionEvent({ event: "status", status: "Starting SNES bridge...", moduleId });
      // Wait for the BizHawk Lua TCP connector to come up, then start the SNI websocket bridge.
      const luaPort = await waitForAnyTcpPort("127.0.0.1", [43055, 43056, 43057, 43058, 43059, 43060], 9000);
      writeLogLine("info", "autolaunch", `lua connector port detected: ${luaPort || "none"}`);
      const bridgeRes = await startSniBridge({ host: "127.0.0.1", port: 23074, luaHost: "127.0.0.1", luaPortStart: 43055, luaPortEnd: 43060 });
      if (!bridgeRes?.ok) {
        emitSessionEvent({ event: "warning", step: "sni-bridge", error: bridgeRes?.error || "sni_bridge_failed", moduleId });
      }
      const clientRes = await startBizHawkClient({ clientKind: "sni" });
      writeLogLine("info", "autolaunch", `sniclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
      await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    } else {
      const clientRes = await startBizHawkClient({ address: serverAddress, slot, clientKind: "bizhawk" });
      writeLogLine("info", "autolaunch", `bizhawkclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
      await sendBizHawkClientCommand({ cmd: "connect", address: serverAddress, slot, password });
    }
  } else if (clientMode !== "embedded") {
    const clientRes = await startCommonClient({ address: serverAddress, slot, password });
    writeLogLine("info", "autolaunch", `commonclient started: ok=${clientRes?.ok} alreadyRunning=${clientRes?.alreadyRunning || false}`);
    await sendCommonClientCommand({ cmd: "connect", address: serverAddress, slot, password });
  }

  let trackerRes = null;
  if (moduleHasExternalTracker(manifest)) {
    emitSessionEvent({ event: "status", status: "Launching tracker...", moduleId });
    writeLogLine("info", "autolaunch", `launching tracker: moduleId=${moduleId} serverAddress=${serverAddress} slot=${slot}`);
    trackerRes = await launchPopTracker({
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
      return { ok: false, error: "tracker_variant_cancelled" };
    }

    // Tracker is best-effort; session still considered OK even if tracker fails.
    if (!trackerRes?.ok) {
      emitSessionEvent({ event: "warning", step: "tracker", error: trackerRes?.error || "tracker_failed", moduleId });
    }
  }

  emitSessionEvent({ event: "status", status: "Applying window layout...", moduleId });
  writeLogLine("info", "autolaunch", `applying layout: gamePid=${emuRes.pid} trackerPid=${trackerRes?.pid || ""}`);

  const layoutRes = await applyLayoutBestEffort({ gamePid: emuRes.pid, trackerPid: trackerRes?.pid });
  if (!layoutRes?.ok) {
    emitSessionEvent({ event: "warning", step: "layout", error: layoutRes?.error || "layout_failed", moduleId });
  }

  emitSessionEvent({
    event: "ready",
    moduleId,
    patchOutput,
    patchReused: reusedPatchedRom,
    emuPid: emuRes.pid,
    trackerPid: trackerRes?.pid,
    downloadedPath,
  });

  return {
    ok: true,
    moduleId,
    patchOutput,
    patchReused: reusedPatchedRom,
    emuPid: emuRes.pid,
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
  const baseWidth = 1280;
  const baseHeight = 820;
  const width = baseWidth + 250;
  const height = baseHeight + 100;
  const workArea = screen.getPrimaryDisplay().workArea;
  const x = Math.round(workArea.x + (workArea.width - width) / 2);
  const y = Math.round(workArea.y + (workArea.height - height) / 2);

  mainWindow = new BrowserWindow({
    width,
    height,
    x,
    y,
    minWidth: 1100,
    minHeight: 720,
    frame: false,
    titleBarStyle: "hidden",
    backgroundColor: "#05070A",
    show: false,
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
    if (pendingAuthUrl) {
      mainWindow.webContents.send("auth:callback", pendingAuthUrl);
      pendingAuthUrl = null;
    }
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

  if (isDev) {
    mainWindow.loadURL(devServerUrl);
    mainWindow.webContents.openDevTools({ mode: "detach" });
  } else {
    mainWindow.loadFile(path.join(__dirname, "../dist/index.html"));
  }
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

  app.setAsDefaultProtocolClient("sekailink");
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

app.on("before-quit", async () => {
  await stopCommonClient();
  await stopBizHawkClient();
  for (const [pid] of trackerProcs) {
    await stopPopTracker(pid);
  }
  for (const win of Array.from(trackerWebWindows)) {
    try {
      if (!win.isDestroyed()) win.close();
    } catch (_err) {
      // ignore
    }
  }
  trackerWebWindows.clear();
  logger.stop();
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
  app.quit();
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
  return {
    app_version: app.getVersion ? String(app.getVersion() || "") : "",
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

secureIpcHandle("runtime:getModuleManifest", async (_event, moduleId) => {
  const safeModuleId = normalizeGameId(moduleId);
  if (!safeModuleId) return { ok: false, error: "invalid_module_id" };
  const manifest = getModuleManifest(safeModuleId);
  if (!manifest) return { ok: false, error: "manifest_missing" };
  return { ok: true, manifest };
});

secureIpcHandle("runtime:listModules", async () => {
  return { ok: true, modules: listRuntimeModules() };
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
  return autoLaunchFromPatchUrl(isPlainObject(options) ? options : {});
});
