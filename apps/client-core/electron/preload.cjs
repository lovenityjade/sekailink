const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("sekailink", {
  getEnv: () => ipcRenderer.invoke("app:getEnv"),
  authGetDesktopSession: () => ipcRenderer.invoke("auth:getDesktopSession"),
  authSetDesktopSession: (payload) => ipcRenderer.invoke("auth:setDesktopSession", payload),
  openExternal: (url) => ipcRenderer.invoke("app:openExternal", url),
  setCrashReportingOptIn: (enabled) => ipcRenderer.invoke("app:setCrashReportingOptIn", enabled),
  collectDiagnostics: (options) => ipcRenderer.invoke("app:collectDiagnostics", options),
  submitDiagnostics: (options) => ipcRenderer.invoke("app:submitDiagnostics", options),
  collectBugReportArtifacts: (options) => ipcRenderer.invoke("app:collectBugReportArtifacts", options),
  clearSeedCache: () => ipcRenderer.invoke("app:clearSeedCache"),
  copyText: (text) => ipcRenderer.invoke("app:copyText", text),
  showItemInFolder: (targetPath) => ipcRenderer.invoke("app:showItemInFolder", targetPath),
  openDashboard: (url) => ipcRenderer.invoke("app:openDashboard", url),
  windowMinimize: () => ipcRenderer.invoke("app:window:minimize"),
  windowToggleMaximize: () => ipcRenderer.invoke("app:window:toggleMaximize"),
  windowClose: () => ipcRenderer.invoke("app:window:close"),
  updaterDownload: (options) => ipcRenderer.invoke("updater:download", options),
  updaterDownloadAndApply: (options) => ipcRenderer.invoke("updater:downloadAndApply", options),
  updaterOpenDownloaded: (targetPath) => ipcRenderer.invoke("updater:openDownloaded", targetPath),
  updaterSyncIncremental: (options) => ipcRenderer.invoke("updater:syncIncremental", options),
  updaterLaunchBootstrapperAndQuit: () => ipcRenderer.invoke("updater:launchBootstrapperAndQuit"),
  bootstrapperGetReleaseChannel: () => ipcRenderer.invoke("bootstrapper:getReleaseChannel"),
  bootstrapperSetReleaseChannel: (channel) => ipcRenderer.invoke("bootstrapper:setReleaseChannel", channel),
  bootstrapperCheckUpdate: (options) => ipcRenderer.invoke("bootstrapper:checkUpdate", options),
  bootstrapperGetLatest: (channel) => ipcRenderer.invoke("bootstrapper:getLatest", channel),
  pickFile: (options) => ipcRenderer.invoke("app:pickFile", options),
  pickFolder: (options) => ipcRenderer.invoke("app:pickFolder", options),
  configGet: () => ipcRenderer.invoke("config:get"),
  configSetRom: (gameId, romPath) => ipcRenderer.invoke("config:setRom", gameId, romPath),
  configDeleteRom: (gameId) => ipcRenderer.invoke("config:deleteRom", gameId),
  configSetGame: (gameId, patch) => ipcRenderer.invoke("config:setGame", gameId, patch),
  configSetWindowing: (windowing) => ipcRenderer.invoke("config:setWindowing", windowing),
  configSetLayout: (layout) => ipcRenderer.invoke("config:setLayout", layout),
  romsScan: (folderPath) => ipcRenderer.invoke("roms:scan", folderPath),
  romsImport: (payload) => ipcRenderer.invoke("roms:import", payload),
  commonClientStart: (options) => ipcRenderer.invoke("commonclient:start", options),
  commonClientSend: (command) => ipcRenderer.invoke("commonclient:send", command),
  commonClientStop: () => ipcRenderer.invoke("commonclient:stop"),
  bizhawkClientStart: (options) => ipcRenderer.invoke("bizhawkclient:start", options),
  bizhawkClientSend: (command) => ipcRenderer.invoke("bizhawkclient:send", command),
  bizhawkClientStop: () => ipcRenderer.invoke("bizhawkclient:stop"),
  archipelagoClientList: () => ipcRenderer.invoke("archipelagoclient:list"),
  archipelagoClientStart: (options) => ipcRenderer.invoke("archipelagoclient:start", options),
  archipelagoClientSend: (clientId, command) => ipcRenderer.invoke("archipelagoclient:send", clientId, command),
  archipelagoClientStop: (clientId) => ipcRenderer.invoke("archipelagoclient:stop", clientId),
  patcherApply: (options) => ipcRenderer.invoke("patcher:apply", options),
  patcherResolveCachedRom: (options) => ipcRenderer.invoke("patcher:resolveCachedRom", options),
  patcherListCachedRoms: () => ipcRenderer.invoke("patcher:listCachedRoms"),
  bizhawkLaunch: (options) => ipcRenderer.invoke("bizhawk:launch", options),
  bizhawkStop: (pid) => ipcRenderer.invoke("bizhawk:stop", pid),
  sekaiEmuLabListCores: () => ipcRenderer.invoke("sekaiemu:lab:listCores"),
  sekaiEmuLabLaunch: (options) => ipcRenderer.invoke("sekaiemu:lab:launch", options),
  sekaiEmuInputCapture: (options) => ipcRenderer.invoke("sekaiemu:inputCapture", options),
  sekaiEmuStop: (pid) => ipcRenderer.invoke("sekaiemu:stop", pid),
  sekaiEmuGetSession: (pid) => ipcRenderer.invoke("sekaiemu:getSession", pid),
  sekaiEmuSetInputState: (pid, keys) => ipcRenderer.invoke("sekaiemu:setInputState", pid, keys),
  sekaiEmuSetPaused: (pid, paused) => ipcRenderer.invoke("sekaiemu:setPaused", pid, paused),
  sekaiEmuReset: (pid) => ipcRenderer.invoke("sekaiemu:reset", pid),
  sekaiEmuMemoryDomains: (options) => ipcRenderer.invoke("sekaiemu:memory:domains", options),
  sekaiEmuReadMemory: (options) => ipcRenderer.invoke("sekaiemu:memory:read", options),
  sekaiEmuDumpMemory: (options) => ipcRenderer.invoke("sekaiemu:memory:dump", options),
  runtimeSocialGetState: () => ipcRenderer.invoke("runtimeSocial:getState"),
  runtimeSocialOpen: (surface) => ipcRenderer.invoke("runtimeSocial:open", surface),
  runtimeSocialSendChat: (text) => ipcRenderer.invoke("runtimeSocial:sendChat", text),
  runtimeSocialRequestHint: (item) => ipcRenderer.invoke("runtimeSocial:requestHint", item),
  trackerLaunch: (options) => ipcRenderer.invoke("tracker:launch", options),
  trackerStop: (pid) => ipcRenderer.invoke("tracker:stop", pid),
  trackerRuntimeStatus: (pid) => ipcRenderer.invoke("tracker:runtimeStatus", pid),
  trackerRuntimeCommand: (pid, command, detail) => ipcRenderer.invoke("tracker:runtimeCommand", pid, command, detail),
  trackerOpenBroadcast: () => ipcRenderer.invoke("tracker:openBroadcast"),
  trackerInstallPack: (options) => ipcRenderer.invoke("tracker:installPack", options),
  trackerStatus: () => ipcRenderer.invoke("tracker:status"),
  trackerValidatePack: (gameId) => ipcRenderer.invoke("tracker:validatePack", gameId),
  trackerListPackVariants: (options) => ipcRenderer.invoke("tracker:listPackVariants", options),
  trackerSetVariant: (gameId, variant) => ipcRenderer.invoke("tracker:setVariant", gameId, variant),
  resolveModuleForDownload: (downloadUrl) => ipcRenderer.invoke("runtime:resolveModuleForDownload", downloadUrl),
  planSessionAutoLaunch: (options) => ipcRenderer.invoke("runtime:planSessionAutoLaunch", options),
  getModuleManifest: (moduleId) => ipcRenderer.invoke("runtime:getModuleManifest", moduleId),
  validateSetupForModule: (moduleId) => ipcRenderer.invoke("runtime:validateSetupForModule", moduleId),
  listRuntimeModules: () => ipcRenderer.invoke("runtime:listModules"),
  linkedWorldList: () => ipcRenderer.invoke("linkedworld:list"),
  linkedWorldValidate: (options) => ipcRenderer.invoke("linkedworld:validate", options),
  linkedWorldInstall: (options) => ipcRenderer.invoke("linkedworld:install", options),
  linkedWorldLaunch: (options) => ipcRenderer.invoke("linkedworld:launch", options),
  linkedWorldInstallWindWakerTest: () => ipcRenderer.invoke("linkedworld:installWindWakerTest"),
  linkedWorldLaunchSolo: (options) => ipcRenderer.invoke("linkedworld:launchSolo", options),
  gamescopeStatus: () => ipcRenderer.invoke("runtime:gamescopeStatus"),
  wmctrlStatus: () => ipcRenderer.invoke("runtime:wmctrlStatus"),
  getDisplays: () => ipcRenderer.invoke("runtime:getDisplays"),
  sessionAutoLaunch: (options) => ipcRenderer.invoke("session:autoLaunch", options),
  multiGameList: () => ipcRenderer.invoke("runtime:multiGameList"),
  multiGameSwitch: (entryId, options) => ipcRenderer.invoke("runtime:multiGameSwitch", entryId, options),
  sessionTrackerVariantResponse: (payload) => ipcRenderer.invoke("session:trackerVariantResponse", payload),
  logToMain: (payload) => ipcRenderer.invoke("log:renderer", payload),
  onCommonClientEvent: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, data) => handler(data);
    ipcRenderer.on("commonclient:event", listener);
    return () => ipcRenderer.removeListener("commonclient:event", listener);
  },
  onBizHawkClientEvent: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, data) => handler(data);
    ipcRenderer.on("bizhawkclient:event", listener);
    return () => ipcRenderer.removeListener("bizhawkclient:event", listener);
  },
  onArchipelagoClientEvent: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, data) => handler(data);
    ipcRenderer.on("archipelagoclient:event", listener);
    return () => ipcRenderer.removeListener("archipelagoclient:event", listener);
  },
  onSessionEvent: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, data) => handler(data);
    ipcRenderer.on("session:event", listener);
    return () => ipcRenderer.removeListener("session:event", listener);
  },
  onUpdaterEvent: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, data) => handler(data);
    ipcRenderer.on("updater:event", listener);
    return () => ipcRenderer.removeListener("updater:event", listener);
  },
  onAuthCallback: (handler) => {
    if (typeof handler !== "function") return () => {};
    const listener = (_event, url) => handler(url);
    ipcRenderer.on("auth:callback", listener);
    return () => ipcRenderer.removeListener("auth:callback", listener);
  }
});
