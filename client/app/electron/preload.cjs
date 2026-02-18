const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("sekailink", {
  getEnv: () => ipcRenderer.invoke("app:getEnv"),
  openExternal: (url) => ipcRenderer.invoke("app:openExternal", url),
  setCrashReportingOptIn: (enabled) => ipcRenderer.invoke("app:setCrashReportingOptIn", enabled),
  collectDiagnostics: (options) => ipcRenderer.invoke("app:collectDiagnostics", options),
  submitDiagnostics: (options) => ipcRenderer.invoke("app:submitDiagnostics", options),
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
  pickFile: (options) => ipcRenderer.invoke("app:pickFile", options),
  pickFolder: (options) => ipcRenderer.invoke("app:pickFolder", options),
  configGet: () => ipcRenderer.invoke("config:get"),
  configSetRom: (gameId, romPath) => ipcRenderer.invoke("config:setRom", gameId, romPath),
  configDeleteRom: (gameId) => ipcRenderer.invoke("config:deleteRom", gameId),
  configSetGame: (gameId, patch) => ipcRenderer.invoke("config:setGame", gameId, patch),
  configSetWindowing: (windowing) => ipcRenderer.invoke("config:setWindowing", windowing),
  configSetLayout: (layout) => ipcRenderer.invoke("config:setLayout", layout),
  romsScan: (folderPath) => ipcRenderer.invoke("roms:scan", folderPath),
  romsImport: (filePath) => ipcRenderer.invoke("roms:import", filePath),
  commonClientStart: (options) => ipcRenderer.invoke("commonclient:start", options),
  commonClientSend: (command) => ipcRenderer.invoke("commonclient:send", command),
  commonClientStop: () => ipcRenderer.invoke("commonclient:stop"),
  bizhawkClientStart: (options) => ipcRenderer.invoke("bizhawkclient:start", options),
  bizhawkClientSend: (command) => ipcRenderer.invoke("bizhawkclient:send", command),
  bizhawkClientStop: () => ipcRenderer.invoke("bizhawkclient:stop"),
  patcherApply: (options) => ipcRenderer.invoke("patcher:apply", options),
  bizhawkLaunch: (options) => ipcRenderer.invoke("bizhawk:launch", options),
  bizhawkStop: (pid) => ipcRenderer.invoke("bizhawk:stop", pid),
  trackerLaunch: (options) => ipcRenderer.invoke("tracker:launch", options),
  trackerStop: (pid) => ipcRenderer.invoke("tracker:stop", pid),
  trackerInstallPack: (options) => ipcRenderer.invoke("tracker:installPack", options),
  trackerStatus: () => ipcRenderer.invoke("tracker:status"),
  trackerValidatePack: (gameId) => ipcRenderer.invoke("tracker:validatePack", gameId),
  trackerListPackVariants: (options) => ipcRenderer.invoke("tracker:listPackVariants", options),
  trackerSetVariant: (gameId, variant) => ipcRenderer.invoke("tracker:setVariant", gameId, variant),
  resolveModuleForDownload: (downloadUrl) => ipcRenderer.invoke("runtime:resolveModuleForDownload", downloadUrl),
  getModuleManifest: (moduleId) => ipcRenderer.invoke("runtime:getModuleManifest", moduleId),
  listRuntimeModules: () => ipcRenderer.invoke("runtime:listModules"),
  gamescopeStatus: () => ipcRenderer.invoke("runtime:gamescopeStatus"),
  wmctrlStatus: () => ipcRenderer.invoke("runtime:wmctrlStatus"),
  getDisplays: () => ipcRenderer.invoke("runtime:getDisplays"),
  sessionAutoLaunch: (options) => ipcRenderer.invoke("session:autoLaunch", options),
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
