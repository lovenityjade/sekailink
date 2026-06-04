export type CommonClientCommand = {
  cmd: string;
  [key: string]: unknown;
};

export type PatcherOptions = {
  moduleId?: string;
  patchPath?: string;
  patchUrl?: string;
  configPath?: string;
  outDir?: string;
};

export type PatchedRomPlan = {
  ok: boolean;
  moduleId?: string;
  patchPath?: string;
  patchHash?: string;
  action?: "reuse" | "patch";
  shouldReuse?: boolean;
  cached?: boolean;
  outputPath?: string;
  cacheHit?: string;
  error?: string;
};

export type PatchedRomCacheEntry = {
  key: string;
  moduleId: string;
  patchName: string;
  patchHash: string;
  outputPath: string;
  exists: boolean;
  updatedAt: string;
};

export type BizHawkLaunchOptions = {
  emuPath?: string;
  romPath: string;
  luaPath?: string;
  moduleId?: string;
  luaRelPath?: string;
};

export type SekaiEmuAudioOptions = {
  pid: number;
  maxSamples?: number;
};

export type TrackerLaunchOptions = {
  moduleId?: string;
  packUid?: string;
  packPath?: string;
  packVariant?: string;
  forceTrackerVariantPrompt?: boolean;
  apHost?: string;
  apSlot?: string;
  apPass?: string;
  apAutoconnect?: boolean;
};

export type TrackerInstallOptions = {
  gameId: string;
  packUrl?: string;
  packRepo?: string;
  packSha256?: string;
  assetRegex?: string;
};

export type TrackerListVariantsOptions = {
  gameId?: string;
  packPath?: string;
};

export type SessionAutoLaunchOptions = {
  downloadUrl: string;
  serverAddress: string;
  slot: string;
  playerAlias?: string;
  password?: string;
  apGameName?: string;
  forceTrackerVariantPrompt?: boolean;
  chatBridge?: {
    channelId?: string;
    lobbyId?: string;
  };
  apiBaseUrl?: string;
  authToken?: string | null;
  deviceId?: string;
};

export type ClientUpdaterDownloadOptions = {
  downloadUrl?: string;
  url?: string;
  version?: string;
  latest?: string;
  sha256?: string;
  signature?: string;
  publicKey?: string;
  authToken?: string;
  timeoutMs?: number;
};

export type ClientUpdaterSyncOptions = {
  manifestUrl?: string;
  url?: string;
  authToken?: string;
};

export type RuntimeModuleInfo = {
  moduleId: string;
  manifest: Record<string, any>;
};

export type LaunchSetupValidationResult = {
  ok: boolean;
  moduleId?: string;
  gameId?: string;
  manifest?: Record<string, any>;
  error?: string;
  detail?: string;
  setupArea?: string;
};

export type SessionLaunchPlanResult = {
  ok: boolean;
  moduleId?: string;
  gameId?: string;
  ext?: string;
  emu?: string;
  launchKind?: "native_patchless" | "auto_patch" | "manual_known_artifact" | "manual_unknown_artifact";
  canAutoPatch?: boolean;
  error?: string;
  detail?: string;
  setupArea?: string;
};

export type LinkedWorldInfo = {
  id: string;
  installed: boolean;
  installPath?: string;
  manifest?: Record<string, any>;
  ready?: boolean;
  errors?: string[];
  archiveAvailable?: boolean;
  archivePath?: string;
};

export type NativeBizHawkHandlerInfo = {
  moduleId: string;
  gameId: string;
  handler: string;
  apGameName: string;
  system: string;
  nativeCoreAvailable: boolean;
  nativeHandlerAvailable: boolean;
  itemsHandling: number;
  wantSlotData: boolean;
  authAddress: number;
  authSize: number;
};

export type SniRequestOptions = {
  uri?: string;
  kinds?: string[];
  address?: number;
  size?: number;
  domain?: string;
  data?: number[];
};

export type Smz3RequestOptions = SniRequestOptions & {
  romDomain?: string;
  wramDomain?: string;
  sramDomain?: string;
  playerId?: number;
  itemId?: number;
};

export type PickFileOptions = {
  title?: string;
  filters?: { name: string; extensions: string[] }[];
};

export type DiagnosticsReport = {
  reportId: string;
  createdAt: string;
  app: Record<string, unknown>;
  log: { path?: string; tail?: string };
  meta?: Record<string, unknown>;
};

export type BugReportArtifacts = {
  screenshotBase64?: string;
  logsText?: string;
  systemInfo?: Record<string, unknown>;
  appInfo?: Record<string, unknown>;
};

export const runtime = {
  pickFile: (options?: PickFileOptions) => window.sekailink?.pickFile?.(options),
  pickFolder: (options?: { title?: string }) => window.sekailink?.pickFolder?.(options),
  openExternal: (url: string) => (window.sekailink as any)?.openExternal?.(url),
  setCrashReportingOptIn: (enabled: boolean) => (window.sekailink as any)?.setCrashReportingOptIn?.(enabled),
  collectDiagnostics: (options?: Record<string, unknown>) =>
    (window.sekailink as any)?.collectDiagnostics?.(options) as Promise<{ ok: boolean; report?: DiagnosticsReport; error?: string }>,
  submitDiagnostics: (options: { uploadUrl: string; authToken?: string; report?: DiagnosticsReport }) =>
    (window.sekailink as any)?.submitDiagnostics?.(options) as Promise<{ ok: boolean; status?: number; error?: string }>,
  collectBugReportArtifacts: (options?: Record<string, unknown>) =>
    (window.sekailink as any)?.collectBugReportArtifacts?.(options) as Promise<{ ok: boolean; artifacts?: BugReportArtifacts; error?: string }>,
  copyText: (text: string) => (window.sekailink as any)?.copyText?.(text),
  showItemInFolder: (targetPath: string) => (window.sekailink as any)?.showItemInFolder?.(targetPath),
  updaterDownload: (options: ClientUpdaterDownloadOptions) => (window.sekailink as any)?.updaterDownload?.(options),
  updaterDownloadAndApply: (options: ClientUpdaterDownloadOptions) => (window.sekailink as any)?.updaterDownloadAndApply?.(options),
  updaterOpenDownloaded: (targetPath: string) => (window.sekailink as any)?.updaterOpenDownloaded?.(targetPath),
  updaterSyncIncremental: (options: ClientUpdaterSyncOptions) => (window.sekailink as any)?.updaterSyncIncremental?.(options),
  configGet: () => window.sekailink?.configGet?.(),
  configSetRom: (gameId: string, romPath: string) =>
    window.sekailink?.configSetRom?.(gameId, romPath),
  configDeleteRom: (gameId: string) =>
    (window.sekailink as any)?.configDeleteRom?.(gameId),
  configSetGame: (gameId: string, patch: Record<string, any>) =>
    (window.sekailink as any)?.configSetGame?.(gameId, patch),
  configSetWindowing: (windowing: Record<string, any>) => window.sekailink?.configSetWindowing?.(windowing),
  configSetLayout: (layout: Record<string, any>) => window.sekailink?.configSetLayout?.(layout),
  romsScan: (folderPath: string) => window.sekailink?.romsScan?.(folderPath),
  romsImport: (filePath: string) => window.sekailink?.romsImport?.(filePath),
  commonClientStart: (options: Record<string, unknown>) =>
    window.sekailink?.commonClientStart?.(options),
  commonClientSend: (command: CommonClientCommand) =>
    window.sekailink?.commonClientSend?.(command),
  commonClientStop: () => window.sekailink?.commonClientStop?.(),
  onCommonClientEvent: (handler: (data: unknown) => void) =>
    window.sekailink?.onCommonClientEvent?.(handler),

  bizhawkClientStart: (options: Record<string, unknown>) =>
    (window.sekailink as any)?.bizhawkClientStart?.(options),
  bizhawkClientSend: (command: CommonClientCommand) =>
    (window.sekailink as any)?.bizhawkClientSend?.(command),
  bizhawkClientStop: () => (window.sekailink as any)?.bizhawkClientStop?.(),
  onBizHawkClientEvent: (handler: (data: unknown) => void) =>
    (window.sekailink as any)?.onBizHawkClientEvent?.(handler),

  patcherApply: (options: PatcherOptions) => window.sekailink?.patcherApply?.(options),
  patcherResolveCachedRom: (options: PatcherOptions) =>
    (window.sekailink as any)?.patcherResolveCachedRom?.(options) as Promise<PatchedRomPlan>,
  patcherListCachedRoms: () =>
    (window.sekailink as any)?.patcherListCachedRoms?.() as Promise<{ ok: boolean; entries?: PatchedRomCacheEntry[]; error?: string }>,
  bizhawkLaunch: (options: BizHawkLaunchOptions) => window.sekailink?.bizhawkLaunch?.(options),
  bizhawkStop: (pid: number) => window.sekailink?.bizhawkStop?.(pid),
  sekaiEmuGetSession: (pid: number) => (window.sekailink as any)?.sekaiEmuGetSession?.(pid),
  sekaiEmuSetInputState: (pid: number, keys: number) => (window.sekailink as any)?.sekaiEmuSetInputState?.(pid, keys),
  sekaiEmuSetPaused: (pid: number, paused: boolean) => (window.sekailink as any)?.sekaiEmuSetPaused?.(pid, paused),
  sekaiEmuReset: (pid: number) => (window.sekailink as any)?.sekaiEmuReset?.(pid),
  sekaiEmuReadAudio: (options: SekaiEmuAudioOptions) => (window.sekailink as any)?.sekaiEmuReadAudio?.(options),
  trackerLaunch: (options: TrackerLaunchOptions) => window.sekailink?.trackerLaunch?.(options),
  trackerStop: (pid: number) => window.sekailink?.trackerStop?.(pid),
  trackerGetSession: (pid: number) => (window.sekailink as any)?.trackerGetSession?.(pid),
  trackerListSessions: () => (window.sekailink as any)?.trackerListSessions?.(),
  trackerInstallPack: (options: TrackerInstallOptions) => window.sekailink?.trackerInstallPack?.(options),
  trackerStatus: () => window.sekailink?.trackerStatus?.(),
  trackerValidatePack: (gameId: string) => window.sekailink?.trackerValidatePack?.(gameId),
  trackerListPackVariants: (options: TrackerListVariantsOptions) => window.sekailink?.trackerListPackVariants?.(options),
  trackerSetVariant: (gameId: string, variant: string) => window.sekailink?.trackerSetVariant?.(gameId, variant),
  sniStatus: () => (window.sekailink as any)?.sniStatus?.(),
  sniListDevices: (options?: SniRequestOptions) => (window.sekailink as any)?.sniListDevices?.(options),
  sniReadMemory: (options: SniRequestOptions) => (window.sekailink as any)?.sniReadMemory?.(options),
  sniWriteMemory: (options: SniRequestOptions) => (window.sekailink as any)?.sniWriteMemory?.(options),
  sniResetSystem: (options: SniRequestOptions) => (window.sekailink as any)?.sniResetSystem?.(options),
  sniResetToMenu: (options: SniRequestOptions) => (window.sekailink as any)?.sniResetToMenu?.(options),
  sniReadDirectory: (options: SniRequestOptions) => (window.sekailink as any)?.sniReadDirectory?.(options),
  sniMakeDirectory: (options: SniRequestOptions) => (window.sekailink as any)?.sniMakeDirectory?.(options),
  sniRemoveFile: (options: SniRequestOptions) => (window.sekailink as any)?.sniRemoveFile?.(options),
  sniRenameFile: (options: SniRequestOptions & { newFilename?: string }) => (window.sekailink as any)?.sniRenameFile?.(options),
  sniPutFile: (options: SniRequestOptions & { sourcePath?: string; path?: string }) => (window.sekailink as any)?.sniPutFile?.(options),
  sniGetFile: (options: SniRequestOptions & { outPath?: string; path?: string }) => (window.sekailink as any)?.sniGetFile?.(options),
  sniBootFile: (options: SniRequestOptions & { path?: string }) => (window.sekailink as any)?.sniBootFile?.(options),
  smz3ValidateRom: (options: Smz3RequestOptions) => (window.sekailink as any)?.smz3ValidateRom?.(options),
  smz3Poll: (options: Smz3RequestOptions) => (window.sekailink as any)?.smz3Poll?.(options),
  smz3DeliverItem: (options: Smz3RequestOptions) => (window.sekailink as any)?.smz3DeliverItem?.(options),
  resolveModuleForDownload: (downloadUrl: string) => window.sekailink?.resolveModuleForDownload?.(downloadUrl),
  planSessionAutoLaunch: (options: { downloadUrl?: string; apGameName?: string }) =>
    (window.sekailink as any)?.planSessionAutoLaunch?.(options) as Promise<SessionLaunchPlanResult>,
  getModuleManifest: (moduleId: string) => window.sekailink?.getModuleManifest?.(moduleId),
  validateSetupForModule: (moduleId: string) =>
    (window.sekailink as any)?.validateSetupForModule?.(moduleId) as Promise<LaunchSetupValidationResult>,
  listRuntimeModules: () => window.sekailink?.listRuntimeModules?.(),
  linkedWorldList: () =>
    (window.sekailink as any)?.linkedWorldList?.() as Promise<{ ok: boolean; worlds?: LinkedWorldInfo[]; error?: string }>,
  linkedWorldValidate: (options: Record<string, unknown>) =>
    (window.sekailink as any)?.linkedWorldValidate?.(options) as Promise<{ ok: boolean; world?: LinkedWorldInfo; error?: string; detail?: string }>,
  linkedWorldInstall: (options: Record<string, unknown>) =>
    (window.sekailink as any)?.linkedWorldInstall?.(options) as Promise<{ ok: boolean; world?: LinkedWorldInfo; error?: string; detail?: string }>,
  linkedWorldLaunch: (options: Record<string, unknown>) =>
    (window.sekailink as any)?.linkedWorldLaunch?.(options) as Promise<{ ok: boolean; error?: string; detail?: string; gamePid?: number; serverPid?: number; port?: number }>,
  linkedWorldInstallWindWakerTest: () =>
    (window.sekailink as any)?.linkedWorldInstallWindWakerTest?.() as Promise<{ ok: boolean; world?: LinkedWorldInfo; error?: string; detail?: string }>,
  linkedWorldLaunchSolo: (options: Record<string, unknown>) =>
    (window.sekailink as any)?.linkedWorldLaunchSolo?.(options) as Promise<{ ok: boolean; error?: string; detail?: string; gamePid?: number; serverPid?: number; port?: number }>,
  listNativeBizHawkHandlers: () =>
    (window.sekailink as any)?.listNativeBizHawkHandlers?.() as Promise<{ ok: boolean; handlers?: NativeBizHawkHandlerInfo[]; error?: string }>,
  gamescopeStatus: () => window.sekailink?.gamescopeStatus?.(),
  wmctrlStatus: () => window.sekailink?.wmctrlStatus?.(),
  getDisplays: () => window.sekailink?.getDisplays?.(),
  sessionAutoLaunch: (options: SessionAutoLaunchOptions) => window.sekailink?.sessionAutoLaunch?.(options),
  sessionTrackerVariantResponse: (payload: { requestId: string; variant?: string; cancel?: boolean }) =>
    (window.sekailink as any)?.sessionTrackerVariantResponse?.(payload),
  onSessionEvent: (handler: (data: unknown) => void) => window.sekailink?.onSessionEvent?.(handler),
  onUpdaterEvent: (handler: (data: unknown) => void) => window.sekailink?.onUpdaterEvent?.(handler),

  logToMain: (payload: Record<string, unknown>) => (window.sekailink as any)?.logToMain?.(payload),

  // assets are handled internally by main process (patch download)
};
