"use strict";

const { hardenBrowserWindow } = require("./browser-window-hardening.cjs");

const registerMainIpcHandlers = (deps = {}) => {
  const {
    app,
    BrowserWindow,
    shell,
    dialog,
    clipboard,
    screen,
    desktopCapturer,
    processRef = process,
    secureIpcHandle,
    isPlainObject,
    isSafeExternalUrl,
    normalizeIpcString,
    normalizeIpcPath,
    normalizeGameId,
    openExternalSafely,
    getMainWindow,
    createDashboardWindow,
    getBootstrapInstallState,
    getClientBundleInstallDir,
    validateBootstrapLaunchToken,
    resolveBootstrapperExecutable,
    getPreferredReleaseChannel,
    setPreferredReleaseChannel,
    fetchBootstrapperLatest,
    checkAndApplyBootstrapperUpdate,
    getBootstrapperDownloadUrl,
    launchBootstrapperAndQuit,
    readConfig,
    writeConfig,
    getGamescopePath,
    getWmctrlPath,
    startClientUpdaterDownload,
    downloadAndApplyClientUpdate,
    openDownloadedUpdaterFile,
    syncIncrementalClientFiles,
    collectDiagnosticsReport,
    submitDiagnosticsReport,
    collectBugReportArtifacts,
    setCrashReportingOptIn,
    scanRomFolder,
    importRomFile,
    startCommonClient,
    sendCommonClientCommand,
    stopCommonClient,
    startBizHawkClient,
    sendBizHawkClientCommand,
    stopBizHawkClient,
    readArchipelagoClientRegistry,
    startArchipelagoClient,
    sendArchipelagoClientCommand,
    stopArchipelagoClient,
    runPatcher,
    resolvePatchedRomPlan,
    patchedRomCache,
    clearRuntimeCaches = () => {},
    launchBizHawk,
    stopBizHawk,
    sekaiemuLab,
    sekaiemuRuntimeSocial,
    launchPopTracker,
    stopPopTracker,
    readPopTrackerRuntimeStatus,
    sendPopTrackerRuntimeCommand,
    openPopTrackerBroadcast,
    installTrackerPack,
    getPopTrackerStatus,
    getInstalledTrackerPack,
    validatePackDir,
    listPopTrackerPackVariants,
    setTrackerVariant,
    handleTrackerVariantResponse,
    writeLogJson,
    resolveModuleForDownload,
    planSessionAutoLaunch,
    getModuleManifest,
    validateRomForModule,
    validateSetupForModule,
    listRuntimeModules,
    pcPackageRuntime,
    createLinkedWorldRuntime,
    fs,
    path,
    spawn,
    spawnSync,
    readline,
    dirname,
    isDev = false,
    devServerUrl = "http://localhost:5173",
    findFreeLocalPort,
    writeLogLine,
    emitSessionEvent,
    nativeGameProcs,
    linkedWorldProcs,
    autoLaunchFromPatchUrl,
    listMultiGameSession,
    switchMultiGame,
  } = deps;
  const process = processRef;
  const mainWindow = () => getMainWindow?.() || null;
  const hostConsoleWindows = new Map();
  const desktopAuthSessionPath = () => path.join(app.getPath("userData"), "desktop-auth-session.json");
  const normalizeDesktopAuthSession = (payload) => {
    if (!isPlainObject(payload)) return null;
    const token = normalizeIpcString(payload.token, 16 * 1024);
    if (!token) return null;
    const session = {
      token,
      updated_at: normalizeIpcString(payload.updated_at, 64) || new Date().toISOString(),
    };
    if (isPlainObject(payload.user)) session.user = payload.user;
    if (isPlainObject(payload.session)) session.session = payload.session;
    return session;
  };
  const readDesktopAuthSession = () => {
    try {
      const filePath = desktopAuthSessionPath();
      if (!fs.existsSync(filePath)) return { ok: true, session: null };
      const parsed = JSON.parse(fs.readFileSync(filePath, "utf8"));
      const session = normalizeDesktopAuthSession(parsed);
      if (!session) return { ok: true, session: null };
      return { ok: true, session };
    } catch (err) {
      return { ok: false, error: String(err && err.message ? err.message : err) };
    }
  };
  const writeDesktopAuthSession = (payload) => {
    try {
      const filePath = desktopAuthSessionPath();
      fs.mkdirSync(path.dirname(filePath), { recursive: true });
      const session = normalizeDesktopAuthSession(payload);
      if (!session) {
        try {
          fs.rmSync(filePath, { force: true });
        } catch (_err) {
          // ignore missing auth store
        }
        return { ok: true, session: null };
      }
      const tmpPath = `${filePath}.${process.pid}.tmp`;
      fs.writeFileSync(tmpPath, JSON.stringify(session, null, 2), { mode: 0o600 });
      fs.renameSync(tmpPath, filePath);
      return { ok: true };
    } catch (err) {
      return { ok: false, error: String(err && err.message ? err.message : err) };
    }
  };

  const rmPathIfExists = (targetPath) => {
    const clean = String(targetPath || "");
    if (!clean || !fs.existsSync(clean)) return { removed: false };
    fs.rmSync(clean, { recursive: true, force: true });
    return { removed: true };
  };

  const clearSeedCache = () => {
    const userDataRuntimeDir = path.join(app.getPath("userData"), "runtime");
    const targets = [
      path.join(userDataRuntimeDir, "downloads"),
      path.join(userDataRuntimeDir, "patches"),
      path.join(userDataRuntimeDir, "roms"),
    ];
    const result = {
      ok: true,
      activeRuntimeCount: nativeGameProcs?.size || 0,
      cleared: [],
      failed: [],
      patchedRomCache: { removedFiles: 0, failedFiles: 0 },
    };
    try {
      result.patchedRomCache = patchedRomCache?.clear?.({ removeFiles: true }) || result.patchedRomCache;
    } catch (err) {
      result.failed.push({ target: "patched-rom-cache", error: String(err?.message || err || "") });
    }
    for (const target of targets) {
      try {
        const removed = rmPathIfExists(target);
        if (removed.removed) result.cleared.push(target);
      } catch (err) {
        result.failed.push({ target, error: String(err?.message || err || "") });
      }
    }
    try {
      clearRuntimeCaches();
    } catch (err) {
      result.failed.push({ target: "runtime-module-caches", error: String(err?.message || err || "") });
    }
    result.ok = result.failed.length === 0;
    writeLogJson("maintenance", {
      event: "seed_cache_cleared",
      activeRuntimeCount: result.activeRuntimeCount,
      clearedCount: result.cleared.length,
      failedCount: result.failed.length,
      patchedRomRemovedFiles: result.patchedRomCache.removedFiles,
      at: new Date().toISOString(),
    });
    return result;
  };

  secureIpcHandle("app:openExternal", async (_event, url) => {
    return openExternalSafely(url, "ipc.app:openExternal");
  });

  secureIpcHandle("auth:getDesktopSession", async () => {
    return readDesktopAuthSession();
  });

  secureIpcHandle("auth:setDesktopSession", async (_event, payload) => {
    return writeDesktopAuthSession(isPlainObject(payload) ? payload : null);
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
      bootstrap_preferred_channel: typeof getPreferredReleaseChannel === "function" ? getPreferredReleaseChannel() : String(installState.channel || "").trim().toLowerCase(),
      bootstrap_build: String(installState.build || "").trim().toLowerCase(),
      bootstrap_install_dir: String(installState.installDir || process.env.SEKAILINK_BOOTSTRAP_INSTALL_DIR || "").trim(),
      bootstrapper_path: bootstrapperPath,
      bootstrapper_available: Boolean(bootstrapperPath),
      bootstrapper_download_url: getBootstrapperDownloadUrl(),
      bootstrap_launch_enforced: Boolean(bootstrapCheck.enforced),
      bootstrap_launch_token_present: Boolean(bootstrapCheck.token_present),
      bootstrap_launch_token_valid: Boolean(bootstrapCheck.token_valid),
      bootstrap_launch_error: String(bootstrapCheck.error || ""),
      runtime_lab_direct: Boolean(bootstrapCheck.runtime_lab_direct),
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
    const nextEnabled = setCrashReportingOptIn ? setCrashReportingOptIn(enabled) : Boolean(enabled);
    return { ok: true, enabled: nextEnabled };
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

  secureIpcHandle("app:clearSeedCache", async () => {
    return clearSeedCache();
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

  secureIpcHandle("app:listCaptureSources", async () => {
    const sources = await desktopCapturer.getSources({ types: ["screen"], thumbnailSize: { width: 320, height: 180 }, fetchWindowIcons: false });
    return {
      ok: true,
      sources: sources.map((source) => ({ id: source.id, name: source.name, displayId: source.display_id, previewBase64: source.thumbnail.toPNG().toString("base64") })),
    };
  });

  secureIpcHandle("app:captureSource", async (_event, sourceId) => {
    const safeId = normalizeIpcString(sourceId, 160);
    if (!safeId) return { ok: false, error: "invalid_capture_source" };
    const displays = screen.getAllDisplays();
    const maxWidth = Math.max(1280, ...displays.map((display) => Number(display.size?.width || 0)));
    const maxHeight = Math.max(720, ...displays.map((display) => Number(display.size?.height || 0)));
    const sources = await desktopCapturer.getSources({ types: ["screen"], thumbnailSize: { width: maxWidth, height: maxHeight }, fetchWindowIcons: false });
    const source = sources.find((entry) => entry.id === safeId);
    if (!source) return { ok: false, error: "capture_source_not_found" };
    return { ok: true, screenshotBase64: source.thumbnail.toPNG().toString("base64"), sourceName: source.name };
  });

  secureIpcHandle("hostConsole:open", async (_event, lobbyId) => {
    const safeLobbyId = normalizeIpcString(lobbyId, 160).replace(/[^A-Za-z0-9_.:-]/g, "-");
    if (!safeLobbyId) return { ok: false, error: "invalid_lobby_id" };
    const existing = hostConsoleWindows.get(safeLobbyId);
    if (existing && !existing.isDestroyed()) {
      existing.show();
      existing.focus();
      return { ok: true, focused: true };
    }
    const win = new BrowserWindow({
      width: 980,
      height: 720,
      minWidth: 720,
      minHeight: 520,
      title: `Host Console - ${safeLobbyId}`,
      frame: false,
      titleBarStyle: "hidden",
      autoHideMenuBar: true,
      backgroundColor: "#05070A",
      show: false,
      webPreferences: {
        preload: path.join(dirname, "preload.cjs"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: true,
        devTools: false,
      },
    });
    hardenBrowserWindow(win);
    win.once("ready-to-show", () => {
      win.show();
      win.focus();
    });
    win.on("closed", () => hostConsoleWindows.delete(safeLobbyId));
    hostConsoleWindows.set(safeLobbyId, win);
    const hash = `#/host-console/${encodeURIComponent(safeLobbyId)}`;
    if (isDev) await win.loadURL(`${devServerUrl}${hash}`);
    else await win.loadFile(path.join(dirname, "../dist/index.html"), { hash });
    return { ok: true, opened: true };
  });
  
  secureIpcHandle("app:window:minimize", async () => {
    const win = BrowserWindow.getFocusedWindow() || mainWindow();
    if (!win || win.isDestroyed()) return { ok: false, error: "no_window" };
    win.minimize();
    return { ok: true };
  });
  
  secureIpcHandle("app:window:toggleMaximize", async () => {
    const win = BrowserWindow.getFocusedWindow() || mainWindow();
    if (!win || win.isDestroyed()) return { ok: false, error: "no_window" };
    if (win.isMaximized()) win.unmaximize();
    else win.maximize();
    return { ok: true, maximized: win.isMaximized() };
  });
  
  secureIpcHandle("app:window:close", async () => {
    const win = BrowserWindow.getFocusedWindow() || mainWindow();
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

  secureIpcHandle("bootstrapper:getReleaseChannel", async () => {
    const installState = getBootstrapInstallState() || {};
    return {
      ok: true,
      channel: typeof getPreferredReleaseChannel === "function" ? getPreferredReleaseChannel() : String(installState.channel || "canonical").trim().toLowerCase(),
      installedChannel: String(installState.channel || "").trim().toLowerCase(),
      installedVersion: String(installState.version || installState.manifestVersion || "").trim(),
    };
  });

  secureIpcHandle("bootstrapper:setReleaseChannel", async (_event, channel) => {
    if (typeof setPreferredReleaseChannel !== "function") return { ok: false, error: "release_channel_unavailable" };
    return setPreferredReleaseChannel(channel);
  });

  secureIpcHandle("bootstrapper:checkUpdate", async (_event, options) => {
    if (typeof checkAndApplyBootstrapperUpdate !== "function") return { ok: false, error: "bootstrapper_update_unavailable" };
    return checkAndApplyBootstrapperUpdate(isPlainObject(options) ? options : {});
  });

  secureIpcHandle("bootstrapper:getLatest", async (_event, channel) => {
    if (typeof fetchBootstrapperLatest !== "function") return { ok: false, error: "bootstrapper_manifest_unavailable" };
    return fetchBootstrapperLatest(channel);
  });
  
  secureIpcHandle("app:pickFile", async (_event, options) => {
    const normalizedOptions = isPlainObject(options) ? options : {};
    const result = await dialog.showOpenDialog(mainWindow() || undefined, {
      title: normalizeIpcString(normalizedOptions.title, 128) || "Select File",
      properties: ["openFile"],
      filters: Array.isArray(normalizedOptions.filters) ? normalizedOptions.filters : [{ name: "All Files", extensions: ["*"] }]
    });
    if (result.canceled || !result.filePaths?.length) return { canceled: true };
    return { canceled: false, path: result.filePaths[0] };
  });
  
  secureIpcHandle("app:pickFolder", async (_event, options) => {
    const normalizedOptions = isPlainObject(options) ? options : {};
    const result = await dialog.showOpenDialog(mainWindow() || undefined, {
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
  
    const current = config.layout && typeof config.layout === "object" ? config.layout : {};
    config.layout = { ...current, ...next, preset, mode, game_display: gameDisplay, tracker_display: trackerDisplay, split };
    const coreSettings = next.core_settings && typeof next.core_settings === "object" ? next.core_settings : null;
    const coreValues = coreSettings && coreSettings.values && typeof coreSettings.values === "object" ? coreSettings.values : null;
    if (coreValues) {
      const flattened = {};
      for (const rawOptions of Object.values(coreValues)) {
        if (!rawOptions || typeof rawOptions !== "object") continue;
        for (const [rawKey, rawValue] of Object.entries(rawOptions)) {
          const key = String(rawKey || "").trim();
          if (!/^[A-Za-z0-9._-]+$/.test(key)) continue;
          flattened[key] = String(rawValue ?? "").replace(/[\r\n]/g, " ").trim();
        }
      }
      config.games = config.games && typeof config.games === "object" ? config.games : {};
      config.games.sekaiemu = config.games.sekaiemu && typeof config.games.sekaiemu === "object" ? config.games.sekaiemu : {};
      config.games.sekaiemu.cores = { ...(config.games.sekaiemu.cores || {}), ...flattened };
    }
    writeConfig(config);
    return { ok: true };
  });
  
  secureIpcHandle("roms:scan", async (_event, folderPath) => {
    const safeFolderPath = normalizeIpcPath(folderPath);
    if (!safeFolderPath) return { ok: false, error: "invalid_folder_path" };
    const results = await scanRomFolder(safeFolderPath);
    return { ok: true, results };
  });
  
  secureIpcHandle("roms:import", async (_event, payload) => {
    const safePayload = isPlainObject(payload) ? payload : { filePath: payload };
    const safeFilePath = normalizeIpcPath(safePayload.filePath || safePayload.path || safePayload);
    if (!safeFilePath) return { ok: false, error: "invalid_file_path" };
    return importRomFile(safeFilePath, {
      gameId: safePayload.gameId || safePayload.game_id || "",
    });
  });
  
  secureIpcHandle("commonclient:start", async (_event, options) => {
    return startCommonClient(isPlainObject(options) ? options : {});
  });
  
  secureIpcHandle("commonclient:send", async (_event, command) => {
    const safeCommand = isPlainObject(command)
      ? command
      : { cmd: "command", text: normalizeIpcString(command, 512) };
    if (!safeCommand.cmd) return { ok: false, error: "invalid_command" };
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
    const safeCommand = isPlainObject(command)
      ? command
      : { cmd: "command", text: normalizeIpcString(command, 512) };
    if (!safeCommand.cmd) return { ok: false, error: "invalid_command" };
    return sendBizHawkClientCommand(safeCommand);
  });
  
  secureIpcHandle("bizhawkclient:stop", async () => {
    await stopBizHawkClient();
    return { ok: true };
  });
  
  secureIpcHandle("archipelagoclient:list", async () => {
    return readArchipelagoClientRegistry();
  });
  
  secureIpcHandle("archipelagoclient:start", async (_event, options) => {
    return startArchipelagoClient(isPlainObject(options) ? options : {});
  });
  
  secureIpcHandle("archipelagoclient:send", async (_event, clientId, command) => {
    const safeCommand = isPlainObject(command) ? command : { cmd: "command", text: normalizeIpcString(command, 512) };
    return sendArchipelagoClientCommand(clientId, safeCommand);
  });
  
  secureIpcHandle("archipelagoclient:stop", async (_event, clientId) => {
    return stopArchipelagoClient(clientId);
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
  
  sekaiemuLab.registerIpcHandlers();
  sekaiemuRuntimeSocial?.registerIpcHandlers?.();
  
  secureIpcHandle("tracker:launch", async (_event, options) => {
    return launchPopTracker(isPlainObject(options) ? options : {});
  });
  
  secureIpcHandle("tracker:stop", async (_event, pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
    return stopPopTracker(safePid);
  });
  
  secureIpcHandle("tracker:runtimeStatus", async (_event, pid) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
    return readPopTrackerRuntimeStatus(safePid);
  });
  
  secureIpcHandle("tracker:runtimeCommand", async (_event, pid, command, detail) => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
    const safeCommand = normalizeIpcString(command, 64) || "ping";
    return sendPopTrackerRuntimeCommand(safePid, safeCommand, isPlainObject(detail) ? detail : {});
  });

  secureIpcHandle("tracker:openBroadcast", async () => {
    return openPopTrackerBroadcast();
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
    return handleTrackerVariantResponse(isPlainObject(payload) ? payload : {});
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

  secureIpcHandle("runtime:validateRomForModule", async (_event, moduleId) => {
    const safeModuleId = normalizeGameId(moduleId);
    if (!safeModuleId) return { ok: false, error: "invalid_module_id", setupArea: "roms" };
    return validateRomForModule(safeModuleId);
  });
  
  secureIpcHandle("runtime:listModules", async () => {
    return { ok: true, modules: listRuntimeModules() };
  });

  secureIpcHandle("pcPackages:list", async (_event, options) => {
    return await pcPackageRuntime.getCatalog({ refresh: Boolean(options?.refresh) });
  });

  secureIpcHandle("pcPackages:install", async (_event, packageId) => {
    return await pcPackageRuntime.install(normalizeIpcString(packageId, 128));
  });

  secureIpcHandle("pcPackages:uninstall", async (_event, packageId) => {
    return await pcPackageRuntime.uninstall(normalizeIpcString(packageId, 128));
  });
  
  createLinkedWorldRuntime({
    app,
    fs,
    path,
    spawn,
    spawnSync,
    readline,
    processRef: process,
    dirname: __dirname,
    secureIpcHandle,
    isPlainObject,
    normalizeGameId,
    normalizeIpcString,
    normalizeIpcPath,
    findFreeLocalPort,
    writeLogLine,
    writeLogJson,
    emitSessionEvent,
    nativeGameProcs,
    linkedWorldProcs,
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

  secureIpcHandle("runtime:multiGameList", async () => {
    if (typeof listMultiGameSession !== "function") return { ok: false, error: "multi_game_unavailable" };
    return listMultiGameSession();
  });

  secureIpcHandle("runtime:multiGameSwitch", async (_event, entryId, options) => {
    if (typeof switchMultiGame !== "function") return { ok: false, error: "multi_game_unavailable" };
    const safeEntryId = normalizeIpcString(entryId, 128);
    const safeOptions = isPlainObject(options) ? { ...options } : {};
    let result = await switchMultiGame(safeEntryId, safeOptions);
    if (result?.error !== "save_state_decision_required") return result;

    const prompt = result.prompt && typeof result.prompt === "object" ? result.prompt : {};
    const response = await dialog.showMessageBox(mainWindow() || undefined, {
      type: "question",
      title: String(prompt.title || "Do you wish to make a save state?"),
      message: String(prompt.message || "Do you wish to make a save state?"),
      buttons: ["Yes", "No", "Cancel"],
      defaultId: 0,
      cancelId: 2,
      noLink: true,
    });
    const saveStateDecision = response.response === 0 ? "yes" : response.response === 1 ? "no" : "cancel";
    result = await switchMultiGame(safeEntryId, { ...safeOptions, saveStateDecision });
    return result;
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
};

module.exports = { registerMainIpcHandlers };
