"use strict";

const { hardenBrowserWindow } = require("./browser-window-hardening.cjs");

const createPopTrackerRuntime = (deps = {}) => {
  const {
    app,
    BrowserWindow,
    fs,
    path,
    crypto,
    spawn,
    spawnSync,
    processRef = process,
    mainWindowRef = () => null,
    resolveWindowIconPath,
    openExternalSafely,
    firstExistingDir,
    getPlatformRuntimeDirCandidates,
    getRuntimeFilePath,
    getRuntimeToolsDir,
    getBundledThirdPartyDir,
    getPopTrackerPacksDir,
    readConfig,
    writeConfig,
    ensureDir,
    writeLogLine,
    emitSessionEvent,
    emitTrackerClientLog,
    normalizeGameId,
    normalizeIpcString,
    normalizeIpcPath,
    normalizeSha256,
    downloadToFile,
    resolveGithubRepo,
    resolveGithubAssetInfo,
    extractZip,
    resolvePackRoot,
    isUrl,
    getModuleManifest,
    trackerProcs,
    trackerRuntimeControls,
    trackerWebWindows,
    terminateChildProcess,
    triggerCoupledRuntimeTeardown,
    startArchipelagoClient,
  } = deps;
  const process = processRef;
  let _popTrackerExeCache = "";
  let _popTrackerExeSource = "";
  const pendingTrackerVariantRequests = new Map();

  const mainWindow = () => mainWindowRef?.() || null;

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
    if (!cfg.window || typeof cfg.window !== "object" || Array.isArray(cfg.window)) cfg.window = {};
    cfg.window.size = [520, 760];
    if (!Array.isArray(cfg.window.pos)) cfg.window.pos = [80, 80];
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
      const manifest = parsePackManifest(fs.readFileSync(manifestPath, "utf-8"));
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

  const parsePackManifest = (raw) => {
    const withoutBom = String(raw || "").replace(/^\ufeff/, "");
    const withoutBlockComments = withoutBom.replace(/\/\*[\s\S]*?\*\//g, "");
    const withoutLineComments = withoutBlockComments.replace(/(^|[^:])\/\/.*$/gm, "$1");
    const withoutTrailingCommas = withoutLineComments.replace(/,\s*([}\]])/g, "$1");
    return JSON.parse(withoutTrailingCommas);
  };

  const resolveManifestTrackerPackPath = (value) => {
    const raw = String(value || "").trim();
    if (!raw) return "";
    return path.isAbsolute(raw) ? raw : getRuntimeFilePath(raw);
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
        const thirdPartyExe = getThirdPartyPopTrackerExePath();
        const bundledSekailinkExe = path.join(sourceDir, "sekailink-poptracker.exe");
        const isSekailinkRuntime = fs.existsSync(bundledSekailinkExe) || /sekailink-poptracker\.exe$/i.test(String(thirdPartyExe || ""));
        const dstExe = path.join(destDir, isSekailinkRuntime ? "sekailink-poptracker.exe" : "poptracker.exe");
        const bundledExe = path.join(sourceDir, "poptracker.exe");
        const exeSrc = fs.existsSync(bundledSekailinkExe)
          ? bundledSekailinkExe
          : (thirdPartyExe && fs.existsSync(thirdPartyExe) ? thirdPartyExe : (fs.existsSync(bundledExe) ? bundledExe : ""));
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
  
      const thirdPartyExe = getThirdPartyPopTrackerExePath();
      const bundledSekailinkExe = path.join(sourceDir, "sekailink-poptracker");
      const isSekailinkRuntime = fs.existsSync(bundledSekailinkExe) || /sekailink-poptracker$/i.test(String(thirdPartyExe || ""));
      const dstExe = path.join(destDir, isSekailinkRuntime ? "sekailink-poptracker" : "poptracker");
      const bundledExe = path.join(sourceDir, "poptracker");
      const exeSrc = fs.existsSync(bundledSekailinkExe)
        ? bundledSekailinkExe
        : (thirdPartyExe && fs.existsSync(thirdPartyExe) ? thirdPartyExe : (fs.existsSync(bundledExe) ? bundledExe : ""));
      if (exeSrc && !fs.existsSync(dstExe)) fs.copyFileSync(exeSrc, dstExe);
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
    const sekailinkBuildRoot = path.join(getBundledThirdPartyDir(), "upstream", "poptracker-sekailink", "build");
    const candidates = [];
    if (process.platform === "win32") {
      candidates.push(
        path.join(sekailinkBuildRoot, "win64", "sekailink-poptracker.exe"),
        path.join(sekailinkBuildRoot, "win32", "sekailink-poptracker.exe"),
        path.join(buildRoot, "win64", "poptracker.exe"),
        path.join(buildRoot, "win32", "poptracker.exe")
      );
    } else {
      const linuxArch = process.arch === "arm64" ? "linux-arm64" : "linux-x86_64";
      candidates.push(
        path.join(sekailinkBuildRoot, linuxArch, "sekailink-poptracker"),
        path.join(sekailinkBuildRoot, "linux-x86_64", "sekailink-poptracker"),
        path.join(sekailinkBuildRoot, "linux-arm64", "sekailink-poptracker"),
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
  
  const isSekailinkPopTrackerExe = (exePath) => /(?:^|[/\\])sekailink-poptracker(?:\.exe)?$/i.test(String(exePath || ""));
  
  const createPopTrackerRuntimeFiles = () => {
    const runtimeDir = path.join(app.getPath("userData"), "runtime", "poptracker");
    ensureDir(runtimeDir);
    const runtimeId = `${Date.now()}-${crypto.randomBytes(4).toString("hex")}`;
    const statusFile = path.join(runtimeDir, `${runtimeId}.status.json`);
    const controlFile = path.join(runtimeDir, `${runtimeId}.control.json`);
    try {
      fs.writeFileSync(controlFile, JSON.stringify({
        schema: "sekailink.poptracker.control.v1",
        command: "ping",
        createdAt: new Date().toISOString(),
      }, null, 2), "utf-8");
    } catch (_err) {
      // The runtime can still launch; health commands will fail until this path is writable.
    }
    return { runtimeId, statusFile, controlFile };
  };
  
  const readPopTrackerRuntimeStatus = (pid) => {
    const runtime = trackerRuntimeControls.get(Number(pid));
    if (!runtime?.statusFile) return { ok: false, error: "runtime_status_unavailable" };
    try {
      const raw = fs.readFileSync(runtime.statusFile, "utf-8");
      return { ok: true, ...runtime, status: JSON.parse(raw) };
    } catch (err) {
      return { ok: false, error: "runtime_status_read_failed", detail: String(err?.message || err || ""), ...runtime };
    }
  };
  
  const sendPopTrackerRuntimeCommand = (pid, command, detail = {}) => {
    const runtime = trackerRuntimeControls.get(Number(pid));
    if (!runtime?.controlFile) return { ok: false, error: "runtime_control_unavailable" };
    try {
      fs.writeFileSync(runtime.controlFile, JSON.stringify({
        schema: "sekailink.poptracker.control.v1",
        command: String(command || "ping"),
        detail: isPlainObject(detail) ? detail : {},
        sentAt: new Date().toISOString(),
      }, null, 2), "utf-8");
      return { ok: true, ...runtime };
    } catch (err) {
      return { ok: false, error: "runtime_control_write_failed", detail: String(err?.message || err || ""), ...runtime };
    }
  };

  const sendPopTrackerRuntimeCommandToLatest = (command, detail = {}) => {
    let latestPid = 0;
    let latestTime = 0;
    for (const [pid, runtime] of trackerRuntimeControls.entries()) {
      const time = Date.parse(runtime?.launchedAt || "") || 0;
      if (time >= latestTime) {
        latestPid = Number(pid);
        latestTime = time;
      }
    }
    if (!latestPid) return { ok: false, error: "no_runtime_tracker" };
    return sendPopTrackerRuntimeCommand(latestPid, command, detail);
  };

  const openPopTrackerBroadcast = () => sendPopTrackerRuntimeCommandToLatest("show_broadcast");
  
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
      const manifest = parsePackManifest(fs.readFileSync(manifestPath, "utf-8"));
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

  const isUniversalTrackerRef = (value) => {
    const normalized = String(value || "").trim().toLowerCase().replace(/[-\s]+/g, "_");
    return normalized === "universal_tracker" || normalized === "universaltracker";
  };

  const moduleUsesUniversalTracker = (manifest = {}, trackerWebUrl = "") => {
    return (
      isUniversalTrackerRef(trackerWebUrl) ||
      isUniversalTrackerRef(manifest?.tracker_type) ||
      isUniversalTrackerRef(manifest?.tracker_web_url) ||
      isUniversalTrackerRef(manifest?.tracker_status)
    );
  };

  const launchUniversalTracker = async (options = {}) => {
    if (typeof startArchipelagoClient !== "function") {
      emitTrackerClientLog("error", "Universal Tracker runtime is unavailable.");
      return { ok: false, error: "universal_tracker_unavailable" };
    }

    const moduleId = String(options.moduleId || "").trim();
    const manifest = options.manifest || (moduleId ? getModuleManifest(moduleId) : null) || {};
    const gameId = String(manifest?.game_id || options.gameId || moduleId || "universal_tracker").trim();
    const gameLabel = String(manifest?.display_name || manifest?.game_name || gameId || "Universal Tracker").trim();
    const clientId = `universal-tracker-${gameId || "game"}-${Date.now()}`.replace(/[^a-z0-9_.:-]+/gi, "_");

    emitTrackerClientLog("info", `Launching Universal Tracker for ${gameLabel}.`);
    const res = await startArchipelagoClient({
      clientId,
      kind: "module",
      module: "worlds.tracker.TrackerClient",
      moduleId: moduleId || gameId,
      game: gameLabel,
      address: options.apHost || options.serverAddress || "",
      slot: options.apSlot || options.slot || "",
      password: options.apPass || options.password || "",
    });
    if (!res?.ok) {
      emitTrackerClientLog("error", `Universal Tracker launch failed: ${res?.error || "unknown"}`);
      return { ok: false, error: res?.error || "universal_tracker_failed", detail: res?.detail || "" };
    }
    return {
      ok: true,
      mode: "universal_tracker",
      clientId: res.clientId,
      pid: res.pid || null,
    };
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
      autoHideMenuBar: true,
      icon: resolveWindowIconPath(),
      webPreferences: {
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        devTools: false
      }
    });
    hardenBrowserWindow(win);
  
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
    let packVariant = options.packVariant || options.trackerVariant || options.tracker_variant;
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
      if (moduleUsesUniversalTracker(manifest, trackerWebUrl)) {
        return launchUniversalTracker({ ...options, moduleId: options.moduleId, manifest, gameId });
      }
      if (gameId) {
        const installed = getInstalledTrackerPack(gameId);
        if (installed?.path) packPath = installed.path;
        if (!packVariant) {
          const savedVariant = forceTrackerVariantPrompt ? "" : getTrackerVariant(gameId);
          if (savedVariant) packVariant = savedVariant;
        }
      }
      if (!packPath) {
        packPath = resolveManifestTrackerPackPath(
          manifest?.tracker_pack_path || manifest?.sekaiemu?.tracker_pack_path || ""
        );
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

    if (moduleUsesUniversalTracker(manifest || {}, trackerWebUrl)) {
      return launchUniversalTracker({ ...options, manifest, gameId });
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
    // PopTracker persists window geometry in pack autosaves. Start clean so a stale oversized
    // tracker window cannot cover the showcase workspace on relaunch.
    if (packPath) clearPopTrackerAutosaveForPack(baseDir, packPath);
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
    let sekailinkRuntime = null;
    if (isSekailinkPopTrackerExe(exePath)) {
      sekailinkRuntime = createPopTrackerRuntimeFiles();
      args.push(
        "--sekailink-runtime",
        "--sekailink-status-file", sekailinkRuntime.statusFile,
        "--sekailink-control-file", sekailinkRuntime.controlFile
      );
    }
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
    if (sekailinkRuntime) {
      trackerRuntimeControls.set(proc.pid, {
        ...sekailinkRuntime,
        exePath,
        packPath: packPath || "",
        packUid: packUid || "",
        packVariant: packVariant || "default",
        launchedAt: new Date().toISOString(),
      });
    }
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
      trackerRuntimeControls.delete(exitedPid);
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
      trackerRuntimeControls.delete(proc.pid);
      const detail = String(earlyExit.error || `code=${earlyExit.code ?? "null"} signal=${earlyExit.signal || "none"}`).trim();
      const errTail = String(stderrTail || "").trim();
      const outTail = String(stdoutTail || "").trim();
      const tailLine =
        (errTail ? errTail.split(/\r?\n/).filter(Boolean).slice(-1)[0] : "") ||
        (outTail ? outTail.split(/\r?\n/).filter(Boolean).slice(-1)[0] : "");
      emitTrackerClientLog("error", `PopTracker exited immediately (${detail})${tailLine ? `: ${tailLine}` : ""}`);
      return { ok: false, error: "tracker_exited_early", detail: tailLine || detail };
    }
  
    return {
      ok: true,
      pid: proc.pid,
      runtime: sekailinkRuntime ? "sekailink-poptracker" : "poptracker",
      statusFile: sekailinkRuntime?.statusFile || "",
      controlFile: sekailinkRuntime?.controlFile || "",
    };
  };
  
  const stopPopTracker = async (pid) => {
    if (pid && trackerProcs.has(pid)) {
      const proc = trackerProcs.get(pid);
      if (trackerRuntimeControls.has(pid)) {
        sendPopTrackerRuntimeCommand(pid, "quit");
        await new Promise((resolve) => setTimeout(resolve, 180));
      }
      await terminateChildProcess(proc, "tracker", { graceMs: 900 });
      trackerProcs.delete(pid);
      trackerRuntimeControls.delete(pid);
      return { ok: true };
    }
    return { ok: false, error: "not_running" };
  };

  const handleTrackerVariantResponse = (payload) => {
    const safe = payload && typeof payload === "object" ? payload : {};
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
  };

  const clearRuntimeCache = () => {
    _popTrackerExeCache = "";
    _popTrackerExeSource = "";
  };

  return {
    getInstalledTrackerPack,
    getTrackerVariant,
    setTrackerVariant,
    installTrackerPack,
    validatePackDir,
    getPopTrackerStatus,
    listPopTrackerPackVariants,
    chooseTrackerPackVariantForLaunch,
    launchPopTracker,
    stopPopTracker,
    readPopTrackerRuntimeStatus,
    sendPopTrackerRuntimeCommand,
    openPopTrackerBroadcast,
    handleTrackerVariantResponse,
    clearRuntimeCache,
  };
};

module.exports = { createPopTrackerRuntime };
