const { app, BrowserWindow, dialog, ipcMain, shell } = require("electron");
const crypto = require("node:crypto");
const fs = require("node:fs");
const http = require("node:http");
const https = require("node:https");
const os = require("node:os");
const path = require("node:path");
const { spawn } = require("node:child_process");
const extractZipPackage = require("extract-zip");

const DEFAULT_API_BASE_URL = process.env.SEKAILINK_API_BASE_URL || "https://sekailink.com";
const DEFAULT_CHANNEL = process.env.SEKAILINK_CHANNEL || "test";
const DEFAULT_BUILD = process.env.SEKAILINK_BUILD || "release";

let mainWindow = null;
let installedExecutable = "";
let logFilePath = "";

const scrub = (value) => String(value || "")
  .replace(/\b(token|secret|password|authorization)=([^\s,;]+)/gi, "$1=[REDACTED]")
  .replace(/(Authorization:\s*Bearer\s+)[^\s,;]+/gi, "$1[REDACTED]");

const logLine = (level, scope, message) => {
  const line = `[${new Date().toISOString()}] [${level}] ${scope}: ${scrub(message)}`;
  try {
    if (!logFilePath) {
      const logsDir = path.join(app.getPath("userData"), "logs");
      fs.mkdirSync(logsDir, { recursive: true });
      const stamp = new Date().toISOString().replace(/[:.]/g, "-");
      logFilePath = path.join(logsDir, `bootloader-${stamp}.log`);
    }
    fs.appendFileSync(logFilePath, `${line}\n`, "utf8");
  } catch (_err) {
    // Bootloader logging must never block repair/update.
  }
  try {
    if (level === "error") console.error(line);
    else if (level === "warn") console.warn(line);
    else console.log(line);
  } catch (_err) {
    // ignore invalid console handles
  }
};

const logJson = (scope, payload) => {
  try {
    logLine("info", scope, JSON.stringify(payload));
  } catch (_err) {
    logLine("info", scope, String(payload || ""));
  }
};

const reportError = (scope, err) => {
  const message = err && err.stack ? err.stack : String(err || "unknown_error");
  logLine("error", scope, message);
  send("error", { scope, message, logFilePath });
  try {
    dialog.showErrorBox("SekaiLink Bootloader Error", `${scope}\n\n${String(err?.message || err || "Unknown error")}\n\nLog:\n${logFilePath || "(log unavailable)"}`);
  } catch (_dialogErr) {
    // ignore
  }
};

const platformToken = () => {
  if (process.platform === "win32") return "win32-x64";
  if (process.platform === "darwin") return process.arch === "arm64" ? "darwin-arm64" : "darwin-x64";
  return process.arch === "arm64" ? "linux-arm64" : "linux-x64";
};

const defaultInstallDir = () => {
  if (process.platform === "win32") {
    return path.join(process.env.LOCALAPPDATA || path.join(os.homedir(), "AppData", "Local"), "Programs", "sekailink-client");
  }
  return path.join(process.env.XDG_DATA_HOME || path.join(os.homedir(), ".local", "share"), "sekailink-client");
};

const send = (event, payload = {}) => {
  if (!mainWindow || mainWindow.isDestroyed()) return;
  mainWindow.webContents.send("bootloader:event", { event, logFilePath, ...payload });
};

const sha256File = (filePath) =>
  new Promise((resolve, reject) => {
    const hash = crypto.createHash("sha256");
    const input = fs.createReadStream(filePath);
    input.on("error", reject);
    input.on("data", (chunk) => hash.update(chunk));
    input.on("end", () => resolve(hash.digest("hex")));
  });

const httpGetJson = (url, depth = 0) =>
  new Promise((resolve, reject) => {
    if (depth > 5) return reject(new Error("too_many_redirects"));
    const client = url.startsWith("https://") ? https : http;
    const req = client.get(url, {
      headers: {
        "User-Agent": "SekaiLink-Bootloader/0.1 Electron",
        "Accept": "application/json",
        "Cache-Control": "no-cache",
      },
    }, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        const next = new URL(res.headers.location, url).toString();
        res.resume();
        return resolve(httpGetJson(next, depth + 1));
      }
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error(`http_${res.statusCode || "unknown"}`));
      }
      let body = "";
      res.setEncoding("utf8");
      res.on("data", (chunk) => { body += chunk; });
      res.on("end", () => {
        try {
          resolve(JSON.parse(body));
        } catch (err) {
          reject(err);
        }
      });
    });
    req.setTimeout(30000, () => {
      req.destroy(new Error("request_timeout"));
    });
    req.on("error", reject);
  });

const downloadFile = (url, destPath, depth = 0) =>
  new Promise((resolve, reject) => {
    if (depth > 5) return reject(new Error("too_many_redirects"));
    fs.mkdirSync(path.dirname(destPath), { recursive: true });
    const client = url.startsWith("https://") ? https : http;
    const req = client.get(url, {
      headers: {
        "User-Agent": "SekaiLink-Bootloader/0.1 Electron",
        "Accept": "application/zip,application/octet-stream,*/*",
        "Cache-Control": "no-cache",
      },
    }, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        res.resume();
        const next = new URL(res.headers.location, url).toString();
        return resolve(downloadFile(next, destPath, depth + 1));
      }
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error(`download_http_${res.statusCode || "unknown"}`));
      }
      const total = Number(res.headers["content-length"] || 0);
      let received = 0;
      const out = fs.createWriteStream(destPath);
      res.on("data", (chunk) => {
        received += chunk.length;
        send("progress", { phase: "download", received, total, percent: total ? Math.round((received / total) * 100) : 0 });
      });
      res.pipe(out);
      out.on("finish", () => out.close(() => resolve({ path: destPath, received, total })));
      out.on("error", reject);
    });
    req.setTimeout(60000, () => {
      req.destroy(new Error("download_timeout"));
    });
    req.on("error", reject);
  });

const run = (command, args, options = {}) =>
  new Promise((resolve, reject) => {
    logJson("spawn", { command, args });
    const child = spawn(command, args, { windowsHide: true, ...options });
    let stderr = "";
    child.stderr?.on("data", (chunk) => { stderr += String(chunk); });
    child.on("error", reject);
    child.on("exit", (code) => {
      if (code === 0) resolve();
      else reject(new Error(stderr.trim() || `${command} exited with ${code}`));
    });
  });

const runWithOutput = (command, args, options = {}) =>
  new Promise((resolve, reject) => {
    logJson("spawn", { command, args });
    const child = spawn(command, args, { windowsHide: false, ...options });
    let stdout = "";
    let stderr = "";
    child.stdout?.on("data", (chunk) => {
      const text = String(chunk);
      stdout += text;
      for (const line of text.split(/\r?\n/).map((item) => item.trim()).filter(Boolean)) {
        logLine("info", "spawn.stdout", line);
        send("status", { text: line.replace(/^\[SekaiLink bootstrapper\]\s*/i, "") });
      }
    });
    child.stderr?.on("data", (chunk) => {
      const text = String(chunk);
      stderr += text;
      for (const line of text.split(/\r?\n/).map((item) => item.trim()).filter(Boolean)) {
        logLine("warn", "spawn.stderr", line);
      }
    });
    child.on("error", reject);
    child.on("exit", (code) => {
      if (code === 0) resolve({ stdout, stderr });
      else reject(new Error((stderr || stdout).trim() || `${command} exited with ${code}`));
    });
  });

const quotePowerShellSingle = (value) => String(value || "").replace(/'/g, "''");

const cleanupOldExtractDirs = (tempRoot) => {
  try {
    if (!fs.existsSync(tempRoot)) return;
    for (const entry of fs.readdirSync(tempRoot, { withFileTypes: true })) {
      if (!entry.isDirectory() || !entry.name.startsWith("extract")) continue;
      const full = path.join(tempRoot, entry.name);
      try {
        fs.rmSync(full, { recursive: true, force: true, maxRetries: 2, retryDelay: 250 });
      } catch (err) {
        logLine("warn", "extract.cleanup_skipped", `${full}: ${err?.message || err}`);
      }
    }
  } catch (err) {
    logLine("warn", "extract.cleanup_scan_failed", err?.message || err);
  }
};

const resetDirectory = (dir) => {
  try {
    fs.rmSync(dir, { recursive: true, force: true, maxRetries: 3, retryDelay: 300 });
  } catch (err) {
    logLine("warn", "directory.reset_cleanup_failed", `${dir}: ${err?.message || err}`);
  }
  fs.mkdirSync(dir, { recursive: true });
};

const extractedAppAsarSize = (dir) => {
  try {
    return fs.statSync(path.join(dir, "resources", "app.asar")).size;
  } catch (_err) {
    return 0;
  }
};

const assertExtractedAppAsarLooksValid = (dir, method) => {
  const appAsar = path.join(dir, "resources", "app.asar");
  const size = extractedAppAsarSize(dir);
  logJson("extract.validation", { method, appAsar, size, exists: fs.existsSync(appAsar) });
  if (size < 1024 * 1024) throw new Error(`extract_${method}_app_asar_invalid:${size}`);
};

const extractZip = async (zipPath, targetDir) => {
  const archive = String(zipPath || "").trim();
  const destination = String(targetDir || "").trim();
  if (!archive) throw new Error("extract_zip_path_missing");
  if (!fs.existsSync(archive)) throw new Error(`extract_zip_missing:${archive}`);
  if (!destination) throw new Error("extract_destination_missing");
  resetDirectory(destination);
  send("progress", { phase: "extract", percent: 0 });
  logJson("extract.start", { archive, destination });
  try {
    await extractZipPackage(archive, { dir: destination });
    assertExtractedAppAsarLooksValid(destination, "extract_zip_package");
    send("progress", { phase: "extract", percent: 100 });
    return;
  } catch (jsErr) {
    logLine("warn", "extract.package_failed", jsErr?.message || jsErr);
    resetDirectory(destination);
  }
  if (process.platform === "win32") {
    try {
      await run("tar.exe", ["-xf", archive, "-C", destination]);
      assertExtractedAppAsarLooksValid(destination, "tar");
      send("progress", { phase: "extract", percent: 100 });
      return;
    } catch (tarErr) {
      logLine("warn", "extract.tar_failed", tarErr?.message || tarErr);
      resetDirectory(destination);
    }
    const script = [
      `$zip='${quotePowerShellSingle(archive)}'`,
      `$dest='${quotePowerShellSingle(destination)}'`,
      "if ([string]::IsNullOrWhiteSpace($zip)) { throw 'extract_zip_path_missing' }",
      "if (-not (Test-Path -LiteralPath $zip)) { throw ('extract_zip_missing:' + $zip) }",
      "New-Item -ItemType Directory -Path $dest -Force | Out-Null",
      "Expand-Archive -LiteralPath $zip -DestinationPath $dest -Force",
    ].join("; ");
    await run("powershell.exe", [
      "-NoProfile",
      "-NonInteractive",
      "-ExecutionPolicy",
      "Bypass",
      "-Command",
      script,
    ]);
    assertExtractedAppAsarLooksValid(destination, "powershell");
  } else {
    await run("python3", ["-c", [
      "import sys, zipfile",
      "zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])",
    ].join("; "), archive, destination]);
    assertExtractedAppAsarLooksValid(destination, "python");
  }
  send("progress", { phase: "extract", percent: 100 });
};

const resolveBundleRoot = (extractDir) => {
  if (fs.existsSync(path.join(extractDir, "resources", "app.asar"))) return extractDir;
  const entries = fs.readdirSync(extractDir, { withFileTypes: true });
  if (entries.length === 1 && entries[0].isDirectory()) {
    return path.join(extractDir, entries[0].name);
  }
  return extractDir;
};

const findClientExecutable = (bundleRoot) => {
  const candidates = process.platform === "win32"
    ? ["SekaiLink Client.exe", "SekaiLink-client.exe", "sekailink-client.exe"]
    : ["sekailink-client", "SekaiLink Client", "SekaiLink-client"];
  for (const name of candidates) {
    const full = path.join(bundleRoot, name);
    if (fs.existsSync(full)) return full;
  }
  const entries = fs.readdirSync(bundleRoot, { withFileTypes: true });
  for (const entry of entries) {
    if (!entry.isFile()) continue;
    const lower = entry.name.toLowerCase();
    if (lower.includes("sekailink") && (process.platform !== "win32" || lower.endsWith(".exe"))) {
      return path.join(bundleRoot, entry.name);
    }
  }
  return "";
};

const bundleDiagnostics = (bundleRoot) => {
  const executable = findClientExecutable(bundleRoot);
  const appAsar = path.join(bundleRoot, "resources", "app.asar");
  const resourcesPak = path.join(bundleRoot, "resources.pak");
  let appAsarSize = 0;
  try { appAsarSize = fs.statSync(appAsar).size; } catch {}
  return {
    bundleRoot,
    executable,
    executableExists: Boolean(executable && fs.existsSync(executable)),
    appAsar,
    appAsarExists: fs.existsSync(appAsar),
    appAsarSize,
    resourcesPakExists: fs.existsSync(resourcesPak),
  };
};

const assertValidClientBundle = (bundleRoot, scope) => {
  const info = bundleDiagnostics(bundleRoot);
  logJson(`${scope}.bundle_validation`, info);
  if (!info.executableExists) throw new Error(`${scope}_client_executable_missing:${bundleRoot}`);
  if (!info.appAsarExists) throw new Error(`${scope}_app_asar_missing:${info.appAsar}`);
  if (info.appAsarSize < 1024 * 1024) throw new Error(`${scope}_app_asar_too_small:${info.appAsarSize}`);
  if (!info.resourcesPakExists) throw new Error(`${scope}_resources_pak_missing:${bundleRoot}`);
  return info;
};

const findLatestInstalledExecutable = () => {
  const preferredDir = defaultInstallDir();
  const candidates = [];
  const addCandidate = (dir) => {
    let info;
    try {
      info = assertValidClientBundle(dir, "installed_candidate");
    } catch (err) {
      logLine("warn", "installed_candidate.skipped", err?.message || err);
      return;
    }
    const executable = info.executable;
    let updatedAt = 0;
    try {
      const statePath = path.join(dir, ".sekailink", "install-state.json");
      const state = JSON.parse(fs.readFileSync(statePath, "utf8"));
      updatedAt = Date.parse(state.updatedAt || "") || 0;
    } catch (_err) {
      try { updatedAt = fs.statSync(executable).mtimeMs; } catch {}
    }
    candidates.push({ executable, updatedAt });
  };

  try {
    if (fs.existsSync(preferredDir)) addCandidate(preferredDir);
    const parentDir = path.dirname(preferredDir);
    const baseName = path.basename(preferredDir);
    for (const entry of fs.readdirSync(parentDir, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue;
      if (entry.name === baseName || entry.name.startsWith(`${baseName}-`)) {
        addCandidate(path.join(parentDir, entry.name));
      }
    }
  } catch (_err) {
    // Fall through to the preferred path lookup below.
  }

  candidates.sort((a, b) => b.updatedAt - a.updatedAt);
  return candidates[0]?.executable || "";
};

const copyBundleToInstallDir = async (bundleRoot, preferredInstallDir, version) => {
  const parentDir = path.dirname(preferredInstallDir);
  const backupDir = `${preferredInstallDir}.backup-${Date.now()}`;
  const fallbackInstallDir = path.join(parentDir, "sekailink-client-next");
  fs.mkdirSync(parentDir, { recursive: true });
  assertValidClientBundle(bundleRoot, "extracted");

  const copyInto = (targetDir) => {
    try {
      fs.rmSync(targetDir, { recursive: true, force: true, maxRetries: 3, retryDelay: 300 });
    } catch (err) {
      logLine("warn", "install.target_cleanup_failed", `${targetDir}: ${err?.message || err}`);
    }
    fs.mkdirSync(path.dirname(targetDir), { recursive: true });
    fs.cpSync(bundleRoot, targetDir, { recursive: true });
    assertValidClientBundle(targetDir, "installed");
    return targetDir;
  };

  if (!fs.existsSync(preferredInstallDir)) {
    return copyInto(preferredInstallDir);
  }

  try {
    fs.rmSync(backupDir, { recursive: true, force: true });
    fs.renameSync(preferredInstallDir, backupDir);
    const installedDir = copyInto(preferredInstallDir);
    try { fs.rmSync(backupDir, { recursive: true, force: true }); } catch (cleanupErr) {
      logLine("warn", "install.backup_cleanup", cleanupErr?.message || cleanupErr);
    }
    return installedDir;
  } catch (renameErr) {
    logLine("warn", "install.rename_replace_failed", renameErr?.message || renameErr);
    try {
      fs.rmSync(preferredInstallDir, { recursive: true, force: true, maxRetries: 5, retryDelay: 400 });
      return copyInto(preferredInstallDir);
    } catch (removeErr) {
      logLine("warn", "install.remove_replace_failed", removeErr?.message || removeErr);
      send("status", { text: "Previous install is locked; installing a fresh copy beside it..." });
      return copyInto(fallbackInstallDir);
    }
  }
};

const releaseUrl = (options) => {
  const base = String(options.apiBaseUrl || DEFAULT_API_BASE_URL).replace(/\/+$/, "");
  const url = new URL("/api/client/release-latest", base);
  url.searchParams.set("channel", String(options.channel || DEFAULT_CHANNEL));
  url.searchParams.set("platform", String(options.platform || platformToken()));
  url.searchParams.set("build", String(options.build || DEFAULT_BUILD));
  return url.toString();
};

const changelogText = (release) => {
  const raw = release?.changelog || release?.release_notes || release?.notes || release?.changes;
  if (Array.isArray(raw)) return raw.map((entry) => `- ${String(entry)}`).join("\n");
  if (raw && typeof raw === "object") return JSON.stringify(raw, null, 2);
  return String(raw || "No changelog was provided by the release server.");
};

const ensureLauncherSecret = (stateDir) => {
  fs.mkdirSync(stateDir, { recursive: true });
  const secretPath = path.join(stateDir, "launcher-secret.key");
  try {
    if (fs.existsSync(secretPath)) {
      const existing = String(fs.readFileSync(secretPath, "utf8") || "").trim();
      if (existing.length >= 32) return existing;
    }
  } catch (_err) {
    // replace unreadable partial secrets
  }
  const secret = crypto.randomBytes(32).toString("base64");
  fs.writeFileSync(secretPath, secret, "ascii");
  return secret;
};

const base64Url = (value) => Buffer.from(String(value), "utf8")
  .toString("base64")
  .replace(/=+$/g, "")
  .replace(/\+/g, "-")
  .replace(/\//g, "_");

const newLaunchToken = (secret) => {
  const now = Date.now();
  const body = base64Url(JSON.stringify({
    iss: "sekailink-bootstrapper",
    aud: "sekailink-client",
    iat: now,
    exp: now + 5 * 60 * 1000,
  }));
  const sig = crypto.createHmac("sha256", secret).update(body).digest("hex");
  return `${body}.${sig}`;
};

const bundledLegacyPowerShellPath = () => {
  const candidates = [
    path.join(__dirname, "SekaiLink-bootstrapper.ps1"),
    path.join(__dirname, "..", "SekaiLink-bootstrapper.ps1"),
  ];
  for (const candidate of candidates) {
    try {
      if (fs.existsSync(candidate)) return candidate;
    } catch (_err) {
      // asar path probing can fail on unusual installs
    }
  }
  return candidates[0];
};

const bundledSevenZipPath = () => {
  if (process.platform !== "win32") return "";
  const candidates = [
    path.join(process.resourcesPath || "", "bin", "7za.exe"),
    path.join(__dirname, "resources", "bin", "7za.exe"),
    path.join(__dirname, "..", "bin", "7za.exe"),
  ];
  for (const candidate of candidates) {
    try {
      if (candidate && fs.existsSync(candidate)) return candidate;
    } catch (_err) {
      // ignore inaccessible candidates
    }
  }
  return "";
};

const runLegacyWindowsBootstrapper = async (options = {}) => {
  const installDir = path.resolve(String(options.installDir || defaultInstallDir()));
  const scriptSource = bundledLegacyPowerShellPath();
  const sevenZipPath = bundledSevenZipPath();
  const scriptsDir = path.join(app.getPath("userData"), "scripts");
  fs.mkdirSync(scriptsDir, { recursive: true });
  const scriptTarget = path.join(scriptsDir, "SekaiLink-bootstrapper.ps1");
  fs.copyFileSync(scriptSource, scriptTarget);
  logJson("legacy_ps1.start", { scriptSource, scriptTarget, installDir, sevenZipPath });
  send("status", { text: "Running Windows bootstrapper..." });
  send("progress", { phase: "install", percent: 0 });
  const args = [
    "-NoProfile",
    "-NonInteractive",
    "-ExecutionPolicy",
    "Bypass",
    "-File",
    scriptTarget,
    "-NoLaunch",
  ];
  if (options.force) args.push("-Force");
  await runWithOutput("powershell.exe", args, {
    cwd: scriptsDir,
    env: {
      ...process.env,
      SEKAILINK_CHANNEL: String(options.channel || DEFAULT_CHANNEL),
      SEKAILINK_BUILD: String(options.build || DEFAULT_BUILD),
      SEKAILINK_PLATFORM: String(options.platform || platformToken()),
      SEKAILINK_API_BASE_URL: String(options.apiBaseUrl || DEFAULT_API_BASE_URL),
      SEKAILINK_INSTALL_DIR: installDir,
      SEKAILINK_SEVENZIP_PATH: sevenZipPath,
    },
  });
  installedExecutable = findLatestInstalledExecutable();
  if (!installedExecutable || !fs.existsSync(installedExecutable)) {
    throw new Error("legacy_ps1_install_completed_but_client_missing");
  }
  const actualInstallDir = path.dirname(installedExecutable);
  logJson("legacy_ps1.done", { installDir: actualInstallDir, installedExecutable });
  send("progress", { phase: "install", percent: 100 });
  send("done", { executable: installedExecutable, installDir: actualInstallDir });
  if (options.autoLaunch !== false) {
    await launchClient();
  }
  return { ok: true, executable: installedExecutable, installDir: actualInstallDir, method: "legacy-powershell" };
};

const runUpdate = async (options = {}) => {
  installedExecutable = "";
  if (process.platform === "win32" && options.useNativeElectronInstaller !== true) {
    return runLegacyWindowsBootstrapper(options);
  }
  const installDir = path.resolve(String(options.installDir || defaultInstallDir()));
  const tempRoot = path.join(app.getPath("userData"), "downloads");
  const releaseInfoUrl = releaseUrl(options);
  logJson("update.start", { installDir, releaseInfoUrl, options: { ...options, apiBaseUrl: options.apiBaseUrl || DEFAULT_API_BASE_URL } });
  send("status", { text: "Checking latest SekaiLink release...", releaseInfoUrl });
  const release = await httpGetJson(releaseInfoUrl);
  logJson("update.release", { version: release.version || release.latest || "", download_url: release.download_url || "" });
  send("release", {
    version: release.version || release.latest || "",
    changelog: changelogText(release),
    release,
  });

  const artifacts = Array.isArray(release.artifacts) ? release.artifacts : [];
  const artifact = artifacts.find((item) => String(item.artifact_type || item.type || "").includes("client-bundle")) ||
    artifacts.find((item) => String(item.url || item.download_url || "").endsWith(".zip")) ||
    release;
  const downloadUrl = String(artifact.download_url || artifact.url || release.download_url || release.url || "").trim();
  if (!downloadUrl) throw new Error("release_missing_download_url");
  const expectedSha256 = String(artifact.sha256 || release.sha256 || "").trim().toLowerCase();
  const fileName = path.basename(new URL(downloadUrl).pathname) || "sekailink-client.zip";
  const downloadPath = path.join(tempRoot, fileName);
  send("status", { text: `Downloading ${fileName}...` });
  logJson("update.download", { downloadUrl, downloadPath });
  const downloaded = await downloadFile(downloadUrl, downloadPath);
  const archivePath = String(downloaded?.path || downloadPath || "").trim();
  logJson("update.download.complete", { archivePath, received: downloaded?.received || 0, total: downloaded?.total || 0 });
  if (!archivePath || !fs.existsSync(archivePath)) throw new Error(`downloaded_archive_missing:${archivePath || "(empty)"}`);
  if (expectedSha256) {
    send("status", { text: "Verifying download..." });
    const actual = String(await sha256File(archivePath)).toLowerCase();
    logJson("update.hash", { expectedSha256, actual });
    if (actual !== expectedSha256) throw new Error("checksum_mismatch");
  }
  cleanupOldExtractDirs(tempRoot);
  const extractDir = path.join(tempRoot, `extract-${Date.now()}-${crypto.randomBytes(4).toString("hex")}`);
  send("status", { text: "Installing SekaiLink..." });
  await extractZip(archivePath, extractDir);
  const bundleRoot = resolveBundleRoot(extractDir);
  const version = release.version || release.latest || "";
  const actualInstallDir = await copyBundleToInstallDir(bundleRoot, installDir, version);
  installedExecutable = findClientExecutable(actualInstallDir);
  if (!installedExecutable) throw new Error("client_executable_not_found_after_install");
  if (installedExecutable && process.platform !== "win32") {
    try { fs.chmodSync(installedExecutable, 0o755); } catch {}
  }
  const stateDir = path.join(actualInstallDir, ".sekailink");
  fs.mkdirSync(stateDir, { recursive: true });
  ensureLauncherSecret(stateDir);
  fs.writeFileSync(path.join(stateDir, "install-state.json"), JSON.stringify({
    version,
    manifestVersion: version,
    channel: options.channel || DEFAULT_CHANNEL,
    build: options.build || DEFAULT_BUILD,
    platform: options.platform || platformToken(),
    artifactType: "client-bundle",
    installDir: actualInstallDir,
    preferredInstallDir: installDir,
    executable: installedExecutable,
    updatedAt: new Date().toISOString(),
  }, null, 2));
  logJson("update.done", { installDir: actualInstallDir, preferredInstallDir: installDir, installedExecutable, version });
  send("done", { executable: installedExecutable, installDir: actualInstallDir });
  if (options.autoLaunch !== false && installedExecutable) {
    await launchClient();
  }
  return { ok: true, executable: installedExecutable, installDir: actualInstallDir };
};

const launchClient = async () => {
  if (!installedExecutable || !fs.existsSync(installedExecutable)) {
    installedExecutable = findLatestInstalledExecutable();
  }
  if (!installedExecutable || !fs.existsSync(installedExecutable)) {
    throw new Error("client_executable_not_found");
  }
  const installDir = path.dirname(installedExecutable);
  const stateDir = path.join(installDir, ".sekailink");
  const secret = ensureLauncherSecret(stateDir);
  const launchToken = newLaunchToken(secret);
  logJson("launch", { installedExecutable, installDir, stateDir });
  const child = spawn(installedExecutable, [], {
    cwd: installDir,
    detached: true,
    stdio: "ignore",
    windowsHide: false,
    env: {
      ...process.env,
      SEKAILINK_BOOTSTRAP_INSTALL_DIR: installDir,
      SEKAILINK_BOOTSTRAP_STATE_DIR: stateDir,
      SEKAILINK_BOOTSTRAP_TOKEN: launchToken,
    },
  });
  child.unref();
  setTimeout(() => app.quit(), 300);
};

const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 760,
    height: 560,
    minWidth: 640,
    minHeight: 480,
    backgroundColor: "#0b0f14",
    title: "SekaiLink Bootloader",
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
    },
  });
  mainWindow.removeMenu();
  mainWindow.loadFile(path.join(__dirname, "index.html"));
};

const handleWithDiagnostics = (scope, fn) => async (...args) => {
  try {
    return await fn(...args);
  } catch (err) {
    reportError(scope, err);
    throw err;
  }
};

ipcMain.handle("bootloader:start", handleWithDiagnostics("update", (_event, options) => runUpdate(options)));
ipcMain.handle("bootloader:launch", handleWithDiagnostics("launch", () => launchClient()));
ipcMain.handle("bootloader:open-folder", (_event, dir) => shell.openPath(String(dir || defaultInstallDir())));
ipcMain.handle("bootloader:open-log", () => {
  if (!logFilePath) return shell.openPath(path.join(app.getPath("userData"), "logs"));
  return shell.showItemInFolder(logFilePath);
});
ipcMain.handle("bootloader:defaults", () => ({
  apiBaseUrl: DEFAULT_API_BASE_URL,
  channel: DEFAULT_CHANNEL,
  build: DEFAULT_BUILD,
  platform: platformToken(),
  installDir: defaultInstallDir(),
  logFilePath,
}));

process.on("uncaughtException", (err) => reportError("uncaughtException", err));
process.on("unhandledRejection", (err) => reportError("unhandledRejection", err));

app.whenReady().then(() => {
  logJson("env", {
    version: app.getVersion(),
    platform: process.platform,
    arch: process.arch,
    electron: process.versions.electron,
    logFilePath,
  });
  createWindow();
});
app.on("window-all-closed", () => app.quit());
