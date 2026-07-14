const { app, BrowserWindow, Menu, ipcMain, shell, screen, dialog, clipboard, desktopCapturer } = require("electron");
const { spawn, spawnSync } = require("child_process");
const readline = require("readline");
const fs = require("fs"), os = require("os"), crypto = require("crypto"), https = require("https"), http = require("http"), net = require("net"), path = require("path");
const { createLogger } = require("./lib/logger.cjs");
const { createPatchedRomCacheStore } = require("./lib/patched-rom-cache.cjs");
const { createSekaiemuLab } = require("./lib/sekaiemu-lab.cjs");
const { createRuntimePaths } = require("./lib/runtime-paths.cjs");
const { createProcessTools } = require("./lib/process-tools.cjs");
const { createBootstrapperControl } = require("./lib/bootstrapper-control.cjs");
const { createDiagnostics } = require("./lib/diagnostics.cjs");
const { createPythonRuntime } = require("./lib/python-runtime.cjs");
const { createDownloadTools } = require("./lib/download-tools.cjs");
const { createBizHawkRuntime } = require("./lib/bizhawk-runtime.cjs");
const { createClientUpdateRuntime } = require("./lib/client-update-runtime.cjs");
const { createLinkedWorldRuntime } = require("./lib/linked-world-runtime.cjs");
const { createPopTrackerRuntime } = require("./lib/poptracker-runtime.cjs");
const { createArchipelagoClientRuntime } = require("./lib/archipelago-client-runtime.cjs");
const { createWebApClientRuntime } = require("./lib/web-ap-client-runtime.cjs");
const { createSekaiemuChatBridge } = require("./lib/sekaiemu-chat-bridge.cjs");
const { createSekaiemuRuntime } = require("./lib/sekaiemu-runtime.cjs");
const { createSekaiemuRuntimeSocial } = require("./lib/sekaiemu-runtime-social.cjs");
const { createNativeGameRuntime } = require("./lib/native-game-runtime.cjs");
const { createRuntimeModuleLibrary } = require("./lib/runtime-module-library.cjs");
const { createPcPackageRuntime } = require("./lib/pc-package-runtime.cjs");
const { createAutoLaunchRuntime } = require("./lib/autolaunch-runtime.cjs");
const { registerMainIpcHandlers } = require("./lib/main-ipc-handlers.cjs");
const { createClientRuntimeConfig } = require("./lib/client-runtime-config.cjs");
const { createWindowingRuntime } = require("./lib/windowing-runtime.cjs");
const { createRuntimeAssetTools } = require("./lib/runtime-asset-tools.cjs");
const { createMonoRuntime } = require("./lib/mono-runtime.cjs");
const { createPatcherRuntime } = require("./lib/patcher-runtime.cjs");
const { createElectronAppShell } = require("./lib/electron-app-shell.cjs");
const { createRuntimeShutdown } = require("./lib/runtime-shutdown.cjs");
const { isSafeExternalUrl, isPlainObject, normalizeIpcString, normalizeIpcPath, normalizeGameId, createIpcSecurity } = require("./lib/ipc-security.cjs");

const isDev = !app.isPackaged, devServerUrl = process.env.VITE_DEV_SERVER_URL || "http://localhost:5173", shouldOpenDevTools = process.env.SEKAILINK_OPEN_DEVTOOLS === "1";

if (process.platform === "win32" && process.env.SEKAILINK_ENABLE_GPU !== "1") {
  app.disableHardwareAcceleration();
  app.commandLine.appendSwitch("disable-gpu");
  app.commandLine.appendSwitch("disable-gpu-compositing");
}

const {
  firstExistingDir, getBundledRuntimeDir, getRuntimePlatformId, getRuntimePlatformDir, getRuntimePlatformPath,
  getPlatformRuntimeDirCandidates, getBundledThirdPartyDir, isValidApRuntimeRoot, getBundledApRootDir,
  getRuntimeOverlayRoot, getOverlayApRootDir, getEffectiveApRootDirs, withApPythonEnv,
} = createRuntimePaths({ app, fs, path, dirname: __dirname, processRef: process });

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

const archipelagoClientProcs = new Map(), archipelagoClientReadlines = new Map(), archipelagoClientChatBridges = new Map();
const archipelagoClientSessions = new Map(), retroarchMemoryBridgeProcs = new Map(), bizhawkProcs = new Map();
const trackerProcs = new Map(), trackerRuntimeControls = new Map(), trackerWebWindows = new Set();
const nativeGameProcs = new Map(), linkedWorldProcs = new Map(), sekaiemuChatBridges = new Map();
let sekaiemuLab = null, sekaiemuRuntimeSocial = null, archipelagoClientRuntime = null, webApClientRuntime = null;
let clearPopTrackerRuntimeCache = () => {}, clearRuntimeModuleCaches = () => {};

const COMMONCLIENT_EVENT_CHANNEL = "commonclient:event", BIZHAWKCLIENT_EVENT_CHANNEL = "bizhawkclient:event";
const ARCHIPELAGOCLIENT_EVENT_CHANNEL = "archipelagoclient:event", SESSION_EVENT_CHANNEL = "session:event";
const UPDATER_EVENT_CHANNEL = "updater:event", LOG_EVENT_CHANNEL = "log:event";

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

const {
  getBootstrapLaunchTokenStatus,
  validateBootstrapLaunchToken,
  getBootstrapInstallState,
  getPreferredReleaseChannel,
  setPreferredReleaseChannel,
  fetchBootstrapperLatest,
  checkAndApplyBootstrapperUpdate,
  resolveBootstrapperExecutable,
  getBootstrapperDownloadUrl,
  spawnDetachedBootstrapper,
  launchBootstrapperAndQuit,
} = createBootstrapperControl({
  app,
  crypto,
  fs,
  path,
  spawn,
  spawnSync,
  processRef: process,
  httpGetJson: (...args) => httpGetJson(...args),
  downloadToDirWithProgress: (...args) => downloadToDirWithProgress(...args),
  extractZip: (...args) => extractZip(...args),
  ensureDir: (...args) => ensureDir(...args),
  writeLogLine,
});

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

const {
  sleep,
  waitForTcpPort,
  waitForAnyTcpPort,
  findFreeLocalPort,
  findFreeLocalPortInRange,
  isPidAlive,
  waitForChildExit,
  terminateChildProcess,
  getListeningPidsOnTcpPort,
  getPidCommandLine,
  killPidGracefully,
  purgeStaleSniBridgePortHolders,
  probeSniBridge,
} = createProcessTools({ net, spawnSync, processRef: process, writeLogLine, withApPythonEnv });

const {
  startFileLogging,
  getLatestLogFilePath,
  readFileTailUtf8,
  collectDiagnosticsReport,
  collectBugReportArtifacts,
  postJson,
  submitDiagnosticsReport,
} = createDiagnostics({
  app,
  fs,
  http,
  https,
  os,
  path,
  processRef: process,
  logger,
  isPlainObject,
  isSafeExternalUrl,
  normalizeIpcString,
  getBootstrapInstallState,
  getArchipelagoClientStats: () => archipelagoClientRuntime?.getArchipelagoClientStats?.() || ({
    processes: archipelagoClientProcs.size,
    traceEvents: 0,
  }),
  getArchipelagoTraceTail: () => archipelagoClientRuntime?.formatArchipelagoClientTraceTail?.() || "",
  getRuntimeLogPaths: () => sekaiemuRuntimeSocial?.getLogPaths?.() || [],
  getMainWindow: () => appShell.getMainWindow(),
  nowIso,
});

const appShell = createElectronAppShell({
  app,
  BrowserWindow,
  Menu,
  dialog,
  screen,
  fs,
  path,
  processRef: process,
  isDev,
  devServerUrl,
  shouldOpenDevTools,
  resolveWindowIconPath,
  openExternalSafely,
  dirname: __dirname,
  readJsonFileSafe,
  startFileLogging,
  getLatestLogFilePath,
  writeLogLine,
  writeLogJson,
  logger,
  validateBootstrapLaunchToken,
  shutdownRuntimeProcesses: (...args) => shutdownRuntimeProcesses(...args),
  trackerWebWindows,
  submitDiagnosticsReport,
  getCrashReportingOptIn: () => crashReportingOptIn,
});

let pythonRuntime = null;
const getPythonCommand = (...args) => pythonRuntime.getPythonCommand(...args);
const ensurePythonRuntime = (...args) => pythonRuntime.ensurePythonRuntime(...args);
const verifyPatcherPythonWorld = (...args) => pythonRuntime.verifyPatcherPythonWorld(...args);

pythonRuntime = createPythonRuntime({
  app,
  fs,
  path,
  spawn,
  spawnSync,
  https,
  processRef: process,
  getRuntimeToolsDir: (...args) => getRuntimeToolsDir(...args),
  getRuntimeFilePath: (...args) => getRuntimeFilePath(...args),
  getBundledThirdPartyDir: (...args) => getBundledThirdPartyDir(...args),
  getBundledApRootDir: (...args) => getBundledApRootDir(...args),
  withApPythonEnv: (...args) => withApPythonEnv(...args),
  emitSessionEvent: (...args) => emitSessionEvent(...args),
  downloadToFile: (...args) => downloadToFile(...args),
  extractZip: (...args) => extractZip(...args),
  ensureDir: (...args) => ensureDir(...args),
  findPathByBasename: (...args) => findPathByBasename(...args),
  writeLogLine,
});

const { ensureMonoRuntime } = createMonoRuntime({
  fs,
  path,
  spawnSync,
  processRef: process,
  getBundledThirdPartyDir: (...args) => getBundledThirdPartyDir(...args),
  findPathByBasename: (...args) => findPathByBasename(...args),
  which: (...args) => which(...args),
  writeLogLine,
});

const clientRuntimeConfig = createClientRuntimeConfig({
  app,
  fs,
  os,
  path,
  processRef: process,
  getRuntimeOverlayRoot,
  getBundledRuntimeDir,
  normalizeGameId,
});

const {
  getOverlayRuntimeDir,
  getRuntimeFilePath,
  getPythonRuntimeWheelhouseTag,
  getPythonRuntimeWheelhouseDir,
  getPythonPipInstallArgs,
  getRuntimeModulesDirs,
  getRuntimeModuleDir,
  getRuntimeModulePath,
  getCommonClientWrapperPath,
  getBizHawkClientWrapperPath,
  getSniClientWrapperPath,
  getArchipelagoClientWrapperPath,
  getRetroarchMemoryBridgePath,
  getArchipelagoClientRegistryPath,
  getSniBridgePath,
  getPatcherWrapperPath,
  getConfigPath,
  normalizeConfig,
  readConfig,
  writeConfig,
  ensureDir,
  isWritablePath,
  getRuntimeModulesDir,
  getRomsDir,
  getPopTrackerPacksDir,
  getRuntimeDownloadsDir,
  getRuntimeToolsDir,
  getClientUpdateDownloadsDir,
  getClientUpdateStagingDir,
} = clientRuntimeConfig;

const {
  computeFileHash,
  verifyDownloadedFileHash,
  downloadToFile,
  sanitizeFilename,
  parseContentDispositionFilename,
  uniquePathInDir,
  downloadToDir,
  resolveRedirectUrl,
  sanitizeHeaders,
  downloadToDirWithProgress,
  normalizeSha256,
} = createDownloadTools({ fs, path, http, https, crypto, processRef: process, ensureDir, writeLogLine });

const pcPackageRuntime = createPcPackageRuntime({
  app, fs, path, https, spawnSync, processRef: process, ensureDir, downloadToDirWithProgress, writeLogLine,
});


const runtimeAssetTools = createRuntimeAssetTools({
  fs,
  path,
  http,
  https,
  processRef: process,
  spawnSync,
  sanitizeHeaders,
  resolveRedirectUrl,
  downloadToFile,
  getRuntimeToolsDir,
  ensureDir,
});

const {
  findPathByBasename,
  httpGetJson,
  ensureGithubReleaseZipInstalled,
  backupAndReplaceFile,
  isUrl,
  resolveGithubRepo,
  resolveGithubAssetInfo,
  extractZip,
  resolvePackRoot,
} = runtimeAssetTools;

const moduleHasExternalTracker = (manifest = {}) => {
  const trackerPack = String(manifest?.tracker_pack_uid || "").trim();
  const trackerPackPath = String(manifest?.tracker_pack_path || manifest?.sekaiemu?.tracker_pack_path || "").trim();
  const trackerWeb = String(manifest?.tracker_web_url || "").trim();
  return Boolean(trackerPack || trackerPackPath || trackerWeb);
};

const moduleUsesBizHawkProtocolClient = (manifest = {}) => {
  const clientMode = String(manifest.client_mode || "").trim().toLowerCase();
  const apClient = String(manifest.ap_client || "").trim().toLowerCase();
  const clientSpec = manifest.archipelago_client && typeof manifest.archipelago_client === "object"
    ? manifest.archipelago_client
    : {};
  const wrapper = String(clientSpec.wrapper || apClient || "").trim().toLowerCase();
  if (String(manifest.client_mode || "").trim().toLowerCase() === "mmbn3") return true;
  return ["archipelago", "archipelago-client", "apclient", "wrapper"].includes(clientMode)
    && wrapper === "bizhawk";
};

const sekaiemuRuntime = createSekaiemuRuntime({
  app,
  fs,
  os,
  path,
  crypto,
  processRef: process,
  readConfig,
  ensureDir,
  writeLogLine,
  nowIso,
  isPlainObject,
  getRuntimePlatformPath,
  getBundledRuntimeDir,
  getRuntimeFilePath,
  getOverlayRuntimeDir,
  getInstalledTrackerPack: (...args) => getInstalledTrackerPack(...args),
  getTrackerVariant: (...args) => getTrackerVariant(...args),
  moduleUsesBizHawkProtocolClient,
  findFreeLocalPort,
  findFreeLocalPortInRange,
  spawnMaybeGamescope: (...args) => spawnMaybeGamescope(...args),
  startSekaiemuChatBridge: (...args) => startSekaiemuChatBridge(...args),
  stopArchipelagoClientsForSession: (...args) => stopArchipelagoClientsForSession(...args),
  triggerCoupledRuntimeTeardown: (...args) => triggerCoupledRuntimeTeardown(...args),
  nativeGameProcs,
  sekaiemuChatBridges,
  rememberSekaiemuSession: (...args) => {
    sekaiemuLab?.rememberSession?.(...args);
    sekaiemuRuntimeSocial?.rememberSession?.(...args);
  },
  forgetSekaiemuSession: (...args) => {
    sekaiemuLab?.forgetSession?.(...args);
    sekaiemuRuntimeSocial?.forgetSession?.(...args);
  },
  emitSessionEvent: (...args) => emitSessionEvent(...args),
});

const {
  getSekaiemuSettings,
  fileExists,
  dirExists,
  pathExists,
  resolveSekaiemuExecutable,
  resolveSekaiemuCorePath,
  resolveSklmiRuntimeForSekaiemu,
  resolveSklmiManifestDirForSekaiemu,
  resolveSekaiemuTrackerPackPath,
  tryLaunchSekaiemu,
  tryCaptureSekaiemuInput,
} = sekaiemuRuntime;

sekaiemuLab = createSekaiemuLab({
  app,
  fs,
  path,
  net,
  Buffer,
  getSekaiemuSettings,
  getRuntimePlatformPath,
  getBundledRuntimeDir,
  dirExists,
  fileExists,
  pathExists,
  ensureDir,
  isPlainObject,
  normalizeIpcPath,
  nativeGameProcs,
  sekaiemuChatBridges,
  terminateChildProcess,
  secureIpcHandle,
  tryLaunchSekaiemu,
  tryCaptureSekaiemuInput,
});

sekaiemuRuntimeSocial = createSekaiemuRuntimeSocial({
  app,
  BrowserWindow,
  fs,
  path,
  processRef: process,
  ensureDir,
  writeLogLine,
  nowIso,
  isDev,
  devServerUrl,
  dirname: __dirname,
  secureIpcHandle,
  readConfig,
  emitSessionEvent: (...args) => emitSessionEvent(...args),
});

const runtimeModuleLibrary = createRuntimeModuleLibrary({
  fs,
  path,
  processRef: process,
  getRuntimeModulesDirs,
  normalizeGameId,
  normalizeIpcString,
  readConfig,
  writeConfig,
  fileExists,
  hashFile: (...args) => hashFile(...args),
  getRomsDir,
  ensureDir,
  ensureMonoRuntime,
  resolveSekaiemuExecutable,
  resolveSekaiemuCorePath,
  resolveSklmiRuntimeForSekaiemu,
  resolveSklmiManifestDirForSekaiemu,
  resolveSekaiemuTrackerPackPath,
  getBundledRuntimeDir,
  getRuntimePlatformPath,
  getPopTrackerStatus: (...args) => getPopTrackerStatus(...args),
});

const {
  getModuleManifest,
  listRuntimeModules,
  getRomImportCandidatesForExtension,
  resolveRomImportTarget,
  resolveConfiguredRomForModule,
  resolveModuleForDownload,
  findModuleByApGameName,
  validateRomForModule,
  validateSetupForModule,
  scanRomFolder,
  importRomFile,
  clearCaches: clearRuntimeModuleCachesImpl,
} = runtimeModuleLibrary;
clearRuntimeModuleCaches = clearRuntimeModuleCachesImpl;

const emitSessionEvent = (payload) => {
  writeLogJson("session", payload);
  try {
    const win = appShell.getMainWindow();
    if (!win || win.isDestroyed() || !win.webContents || win.webContents.isDestroyed()) return;
    win.webContents.send(SESSION_EVENT_CHANNEL, payload);
  } catch (err) {
    writeLogLine("warn", "session", `session event dispatch skipped: ${String(err?.message || err || "")}`);
  }
};

const emitUpdaterEvent = (payload) => {
  const win = appShell.getMainWindow();
  if (!win || win.isDestroyed()) return;
  writeLogJson("updater", payload);
  win.webContents.send(UPDATER_EVENT_CHANNEL, payload);
};

const getClientServerBaseUrl = () => {
  const state = getBootstrapInstallState() || {};
  const fromState = String(state?.serverUrl || "").trim();
  if (fromState) return fromState;
  const fromEnv = String(process.env.SEKAILINK_SERVER_URL || "").trim();
  if (fromEnv) return fromEnv;
  return "https://sekailink.com";
};

const {
  appendSekaiemuSystemChat,
  startSekaiemuChatBridge,
} = createSekaiemuChatBridge({
  app,
  fs,
  path,
  crypto,
  processRef: process,
  getClientServerBaseUrl,
  ensureDir,
  writeLogLine,
  nowIso,
});

archipelagoClientRuntime = createArchipelagoClientRuntime({
  fs,
  path,
  crypto,
  readline,
  spawn,
  processRef: process,
  getMainWindow: () => appShell.getMainWindow(),
  COMMONCLIENT_EVENT_CHANNEL,
  BIZHAWKCLIENT_EVENT_CHANNEL,
  ARCHIPELAGOCLIENT_EVENT_CHANNEL,
  writeLogLine,
  writeLogJson,
  nowIso,
  isPlainObject,
  normalizeIpcString,
  normalizeIpcPath,
  withApPythonEnv,
  getEffectiveApRootDirs,
  ensurePythonRuntime,
  getCommonClientWrapperPath,
  getBizHawkClientWrapperPath,
  getSniClientWrapperPath,
  getArchipelagoClientWrapperPath,
  getRetroarchMemoryBridgePath,
  getArchipelagoClientRegistryPath,
  getSniBridgePath,
  appendSekaiemuSystemChat,
  waitForTcpPort,
  isPidAlive,
  terminateChildProcess,
  purgeStaleSniBridgePortHolders,
  probeSniBridge,
  archipelagoClientProcs,
  archipelagoClientReadlines,
  archipelagoClientChatBridges,
  archipelagoClientSessions,
  retroarchMemoryBridgeProcs,
  notifyRuntimeItem: (...args) => sekaiemuRuntimeSocial?.notifyRuntimeItem?.(...args),
});

const {
  emitTrackerClientLog,
  startCommonClient,
  sendCommonClientCommand,
  stopCommonClient,
  startBizHawkClient,
  sendBizHawkClientCommand,
  stopBizHawkClient,
  startArchipelagoClient,
  sendArchipelagoClientCommand,
  stopArchipelagoClient,
  stopArchipelagoClientsForSession,
  readArchipelagoClientRegistry,
  startSniBridge,
  stopSniBridge,
  stopRetroarchMemoryBridge,
} = archipelagoClientRuntime;

webApClientRuntime = createWebApClientRuntime({
  BrowserWindow,
  crypto,
  writeLogLine,
  writeLogJson,
  nowIso,
  normalizeIpcString,
  appendSekaiemuSystemChat,
  notifyRuntimeItem: (...args) => sekaiemuRuntimeSocial?.notifyRuntimeItem?.(...args),
  notifyRuntimeActivity: (...args) => sekaiemuRuntimeSocial?.notifyRuntimeActivity?.(...args),
});

const {
  startWebApClient,
  stopAllWebApClients,
} = webApClientRuntime;


const {
  getBizHawkBaseDir,
  getBizHawkInstalledDir,
  ensureBizHawkInstalled,
  ensureBizHawkConfig,
  ensureBizHawkLuaCompat,
  stageBizHawkConnectorLua,
  launchBizHawk,
  stopBizHawk,
} = createBizHawkRuntime({
  app,
  fs,
  path,
  dirname: __dirname,
  processRef: process,
  getBundledThirdPartyDir,
  getRuntimeToolsDir,
  getBundledRuntimeDir,
  getBundledApRootDir,
  getRuntimeModulePath,
  getModuleManifest: (...args) => getModuleManifest(...args),
  ensureMonoRuntime,
  ensureDir,
  isWritablePath,
  spawnMaybeGamescope: (...args) => spawnMaybeGamescope(...args),
  terminateChildProcess,
  triggerCoupledRuntimeTeardown: (...args) => triggerCoupledRuntimeTeardown(...args),
  emitSessionEvent,
  writeLogLine,
  bizhawkProcs,
});

const windowingRuntime = createWindowingRuntime({
  fs,
  path,
  spawn,
  spawnSync,
  screen,
  processRef: process,
  readConfig,
  writeLogLine,
});

const {
  getGamescopePath,
  spawnMaybeGamescope,
  getWmctrlPath,
  which,
  wmctrlFindWindowIdByPid,
  wmctrlMoveResize,
  getLayoutConfig,
  applyLayoutBestEffort,
} = windowingRuntime;

const {
  tryLaunchSoh,
  tryLaunch2Ship,
  tryLaunchSm64Ex,
  tryLaunchGzDoom,
  tryHandleDownloadedArtifact,
} = createNativeGameRuntime({
  app,
  fs,
  path,
  processRef: process,
  spawn,
  ensureDir,
  readConfig,
  getModuleManifest,
  resolveGithubRepo,
  which,
  pcPackageRuntime,
  writeLogLine,
  isPlainObject,
  normalizeIpcString,
  getRuntimeToolsDir,
  getBundledThirdPartyDir,
  ensureGithubReleaseZipInstalled,
  backupAndReplaceFile,
  findPathByBasename,
  spawnMaybeGamescope,
  nativeGameProcs,
});

const runtimeShutdown = createRuntimeShutdown({
  writeLogLine,
  stopCommonClient: (...args) => stopCommonClient(...args),
  stopBizHawkClient: (...args) => stopBizHawkClient(...args),
  stopArchipelagoClient: (...args) => stopArchipelagoClient(...args),
  stopRetroarchMemoryBridge: (...args) => stopRetroarchMemoryBridge(...args),
  stopAllWebApClients: (...args) => stopAllWebApClients(...args),
  stopBizHawkClientForCoupling: (...args) => stopBizHawkClient(...args),
  stopSniBridge: (...args) => stopSniBridge(...args),
  sendPopTrackerRuntimeCommand: (...args) => sendPopTrackerRuntimeCommand(...args),
  terminateChildProcess,
  purgeStaleSniBridgePortHolders,
  trackerProcs,
  trackerRuntimeControls,
  bizhawkProcs,
  nativeGameProcs,
  linkedWorldProcs,
  archipelagoClientProcs,
  retroarchMemoryBridgeProcs,
  sekaiemuChatBridges,
});

const {
  triggerCoupledRuntimeTeardown,
  shutdownRuntimeProcesses,
} = runtimeShutdown;

const popTrackerRuntime = createPopTrackerRuntime({
  app,
  BrowserWindow,
  fs,
  path,
  crypto,
  spawn,
  spawnSync,
  processRef: process,
  mainWindowRef: () => appShell.getMainWindow(),
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
});

const {
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
  clearRuntimeCache: clearPopTrackerRuntimeCacheImpl,
} = popTrackerRuntime;
clearPopTrackerRuntimeCache = clearPopTrackerRuntimeCacheImpl;

const patcherRuntime = createPatcherRuntime({
  app,
  fs,
  path,
  crypto,
  spawn,
  processRef: process,
  createPatchedRomCacheStore,
  ensurePythonRuntime,
  getPatcherWrapperPath,
  verifyPatcherPythonWorld,
  normalizeIpcString,
  normalizeIpcPath,
  isPlainObject,
  getModuleManifest,
  resolveConfiguredRomForModule,
  downloadToFile,
  normalizeSha256,
  ensureDir,
  withApPythonEnv,
  writeLogJson,
  nowIso,
});

const {
  runPatcher,
  hashFile,
  getDefaultPatcherConfigPath,
  patchedRomCache,
  resolvePatchedRomPlan,
} = patcherRuntime;

const {
  getClientBundleInstallDir,
  startClientUpdaterDownload,
  downloadAndApplyClientUpdate,
  openDownloadedUpdaterFile,
  syncIncrementalClientFiles,
  ensureRuntimePacksForModule,
} = createClientUpdateRuntime({
  app,
  fs,
  path,
  os,
  crypto,
  spawn,
  shell,
  processRef: process,
  normalizeIpcString,
  normalizeGameId,
  normalizeSha256,
  sanitizeFilename,
  downloadToDirWithProgress,
  hashFile,
  extractZip,
  httpGetJson,
  ensureDir,
  isWritablePath,
  getBootstrapInstallState,
  getClientUpdateDownloadsDir,
  getClientUpdateStagingDir,
  getRuntimeOverlayRoot,
  getRuntimeDownloadsDir,
  emitUpdaterEvent,
  emitSessionEvent,
  writeLogLine,
  nowIso,
  clearRuntimeCaches: () => {
    clearRuntimeModuleCaches();
    clearPopTrackerRuntimeCache();
  },
});

const {
  planSessionAutoLaunch,
  autoLaunchFromPatchUrl,
  listMultiGameSession,
  switchMultiGame,
} = createAutoLaunchRuntime({
  path,
  isPlainObject,
  normalizeIpcPath,
  normalizeIpcString,
  downloadToDirWithProgress,
  getRuntimeDownloadsDir,
  emitSessionEvent,
  writeLogLine,
  writeLogJson,
  findModuleByApGameName,
  validateSetupForModule,
  getModuleManifest,
  resolveModuleForDownload,
  tryHandleDownloadedArtifact,
  moduleHasExternalTracker,
  launchBizHawk,
  tryLaunchSoh,
  tryLaunch2Ship,
  tryLaunchSekaiemu,
  startCommonClient,
  sendCommonClientCommand,
  startBizHawkClient,
  sendBizHawkClientCommand,
  stopSniBridge,
  startSniBridge,
  waitForAnyTcpPort,
  startArchipelagoClient,
  startWebApClient,
  stopArchipelagoClientsForSession,
  launchPopTracker,
  stopPopTracker,
  stopGameProcess: async (pid, reason = "runtime_stop") => {
    const safePid = Number(pid);
    if (!Number.isFinite(safePid) || safePid <= 0) return { ok: false, error: "invalid_pid" };
    const proc = nativeGameProcs.get(safePid);
    if (!proc) return { ok: false, error: "not_running" };
    await terminateChildProcess(proc, String(reason || "runtime_stop"), { graceMs: 900 });
    nativeGameProcs.delete(safePid);
    return { ok: true };
  },
  applyLayoutBestEffort,
  ensureRuntimePacksForModule,
  getDefaultPatcherConfigPath,
  patchedRomCache,
  runPatcher,
});


appShell.registerAppLifecycle();

app.whenReady().then(() => {
  setTimeout(() => {
    void checkAndApplyBootstrapperUpdate()
      .then((result) => {
        if (result?.ok) {
          writeLogLine("info", "bootstrapper", `startup check ok updated=${Boolean(result.updated)} channel=${String(result.channel || "")} version=${String(result.version || "")}`);
          return;
        }
        writeLogLine("warn", "bootstrapper", `startup check skipped: ${String(result?.error || "unknown")}`);
      })
      .catch((err) => {
        writeLogLine("warn", "bootstrapper", `startup check failed: ${String(err || "")}`);
      });
  }, 2500);
});

registerMainIpcHandlers({
  app, BrowserWindow, shell, dialog, clipboard, screen, desktopCapturer, fs, path, spawn, spawnSync, readline,
  processRef: process, dirname: __dirname, isDev, devServerUrl,
  secureIpcHandle, isPlainObject, isSafeExternalUrl, normalizeIpcString, normalizeIpcPath, normalizeGameId,
  openExternalSafely, getMainWindow: () => appShell.getMainWindow(), createDashboardWindow: (...args) => appShell.createDashboardWindow(...args),
  getBootstrapInstallState, getClientBundleInstallDir, validateBootstrapLaunchToken, resolveBootstrapperExecutable,
  getPreferredReleaseChannel, setPreferredReleaseChannel, fetchBootstrapperLatest, checkAndApplyBootstrapperUpdate,
  getBootstrapperDownloadUrl, launchBootstrapperAndQuit,
  readConfig, writeConfig, getGamescopePath, getWmctrlPath,
  collectDiagnosticsReport, submitDiagnosticsReport, collectBugReportArtifacts,
  setCrashReportingOptIn: (enabled) => {
    crashReportingOptIn = Boolean(enabled);
    return crashReportingOptIn;
  },
  startClientUpdaterDownload, downloadAndApplyClientUpdate, openDownloadedUpdaterFile, syncIncrementalClientFiles,
  scanRomFolder, importRomFile, runPatcher, resolvePatchedRomPlan, patchedRomCache,
  clearRuntimeCaches: () => {
    clearRuntimeModuleCaches();
    clearPopTrackerRuntimeCache();
  },
  startCommonClient, sendCommonClientCommand, stopCommonClient,
  startBizHawkClient, sendBizHawkClientCommand, stopBizHawkClient, launchBizHawk, stopBizHawk,
  readArchipelagoClientRegistry, startArchipelagoClient, sendArchipelagoClientCommand, stopArchipelagoClient,
  sekaiemuLab, sekaiemuRuntimeSocial,
  launchPopTracker, stopPopTracker, readPopTrackerRuntimeStatus, sendPopTrackerRuntimeCommand, openPopTrackerBroadcast,
  installTrackerPack, getPopTrackerStatus, getInstalledTrackerPack, validatePackDir,
  listPopTrackerPackVariants, setTrackerVariant, handleTrackerVariantResponse,
  writeLogJson, writeLogLine, emitSessionEvent, findFreeLocalPort,
  resolveModuleForDownload, planSessionAutoLaunch, getModuleManifest, validateRomForModule, validateSetupForModule, listRuntimeModules,
  pcPackageRuntime,
  createLinkedWorldRuntime, nativeGameProcs, linkedWorldProcs, autoLaunchFromPatchUrl,
  listMultiGameSession, switchMultiGame,
});
