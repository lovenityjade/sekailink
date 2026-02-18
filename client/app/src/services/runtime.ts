export type CommonClientCommand = {
  cmd: string;
  [key: string]: unknown;
};

export type PatcherOptions = {
  patchPath?: string;
  patchUrl?: string;
  configPath?: string;
  outDir?: string;
};

export type BizHawkLaunchOptions = {
  emuPath?: string;
  romPath: string;
  luaPath?: string;
  moduleId?: string;
  luaRelPath?: string;
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
  password?: string;
  apGameName?: string;
  forceTrackerVariantPrompt?: boolean;
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

export const runtime = {
  pickFile: (options?: PickFileOptions) => window.sekailink?.pickFile?.(options),
  pickFolder: (options?: { title?: string }) => window.sekailink?.pickFolder?.(options),
  openExternal: (url: string) => (window.sekailink as any)?.openExternal?.(url),
  setCrashReportingOptIn: (enabled: boolean) => (window.sekailink as any)?.setCrashReportingOptIn?.(enabled),
  collectDiagnostics: (options?: Record<string, unknown>) =>
    (window.sekailink as any)?.collectDiagnostics?.(options) as Promise<{ ok: boolean; report?: DiagnosticsReport; error?: string }>,
  submitDiagnostics: (options: { uploadUrl: string; authToken?: string; report?: DiagnosticsReport }) =>
    (window.sekailink as any)?.submitDiagnostics?.(options) as Promise<{ ok: boolean; status?: number; error?: string }>,
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
  bizhawkLaunch: (options: BizHawkLaunchOptions) => window.sekailink?.bizhawkLaunch?.(options),
  bizhawkStop: (pid: number) => window.sekailink?.bizhawkStop?.(pid),
  trackerLaunch: (options: TrackerLaunchOptions) => window.sekailink?.trackerLaunch?.(options),
  trackerStop: (pid: number) => window.sekailink?.trackerStop?.(pid),
  trackerInstallPack: (options: TrackerInstallOptions) => window.sekailink?.trackerInstallPack?.(options),
  trackerStatus: () => window.sekailink?.trackerStatus?.(),
  trackerValidatePack: (gameId: string) => window.sekailink?.trackerValidatePack?.(gameId),
  trackerListPackVariants: (options: TrackerListVariantsOptions) => window.sekailink?.trackerListPackVariants?.(options),
  trackerSetVariant: (gameId: string, variant: string) => window.sekailink?.trackerSetVariant?.(gameId, variant),
  resolveModuleForDownload: (downloadUrl: string) => window.sekailink?.resolveModuleForDownload?.(downloadUrl),
  getModuleManifest: (moduleId: string) => window.sekailink?.getModuleManifest?.(moduleId),
  listRuntimeModules: () => window.sekailink?.listRuntimeModules?.(),
  gamescopeStatus: () => window.sekailink?.gamescopeStatus?.(),
  wmctrlStatus: () => window.sekailink?.wmctrlStatus?.(),
  getDisplays: () => window.sekailink?.getDisplays?.(),
  sessionAutoLaunch: (options: SessionAutoLaunchOptions) => window.sekailink?.sessionAutoLaunch?.(options),
  onSessionEvent: (handler: (data: unknown) => void) => window.sekailink?.onSessionEvent?.(handler),
  onUpdaterEvent: (handler: (data: unknown) => void) => window.sekailink?.onUpdaterEvent?.(handler),

  logToMain: (payload: Record<string, unknown>) => (window.sekailink as any)?.logToMain?.(payload),

  // assets are handled internally by main process (patch download)
};
