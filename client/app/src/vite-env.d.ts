/// <reference types="vite/client" />

interface Window {
  sekailink?: {
    getEnv?: () => Promise<Record<string, unknown>> | Record<string, unknown>;
    openExternal?: (url: string) => Promise<void> | void;
    setCrashReportingOptIn?: (enabled: boolean) => Promise<{ ok: boolean; enabled?: boolean; error?: string }> | { ok: boolean; enabled?: boolean; error?: string };
    collectDiagnostics?: (options?: Record<string, unknown>) => Promise<{ ok: boolean; report?: Record<string, unknown>; error?: string }> | { ok: boolean; report?: Record<string, unknown>; error?: string };
    submitDiagnostics?: (options: Record<string, unknown>) => Promise<{ ok: boolean; status?: number; error?: string }> | { ok: boolean; status?: number; error?: string };
    copyText?: (text: string) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    showItemInFolder?: (targetPath: string) => Promise<unknown> | unknown;
    openDashboard?: (url: string) => Promise<void> | void;
    updaterDownload?: (options: { downloadUrl?: string; url?: string; version?: string; latest?: string; sha256?: string; signature?: string; publicKey?: string; authToken?: string }) => Promise<{ ok: boolean; downloadId?: string; error?: string }> | { ok: boolean; downloadId?: string; error?: string };
    updaterDownloadAndApply?: (options: { downloadUrl?: string; url?: string; version?: string; latest?: string; sha256?: string; signature?: string; publicKey?: string; authToken?: string; timeoutMs?: number }) => Promise<{ ok: boolean; error?: string; downloadId?: string; path?: string; method?: string }> | { ok: boolean; error?: string; downloadId?: string; path?: string; method?: string };
    updaterOpenDownloaded?: (targetPath: string) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    updaterSyncIncremental?: (options: { manifestUrl?: string; url?: string; authToken?: string }) => Promise<{ ok: boolean; error?: string; changed?: number; deleted?: number; processed?: number; total?: number; downloadedBytes?: number }> | { ok: boolean; error?: string; changed?: number; deleted?: number; processed?: number; total?: number; downloadedBytes?: number };
    pickFile?: (options?: { title?: string; filters?: { name: string; extensions: string[] }[] }) => Promise<{ canceled: boolean; path?: string }> | { canceled: boolean; path?: string };
    pickFolder?: (options?: { title?: string }) => Promise<{ canceled: boolean; path?: string }> | { canceled: boolean; path?: string };
    configGet?: () => Promise<Record<string, any>> | Record<string, any>;
    configSetRom?: (gameId: string, romPath: string) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    configDeleteRom?: (gameId: string) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    configSetGame?: (gameId: string, patch: Record<string, any>) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    configSetWindowing?: (windowing: Record<string, any>) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    configSetLayout?: (layout: Record<string, any>) => Promise<{ ok: boolean; error?: string }> | { ok: boolean; error?: string };
    romsScan?: (folderPath: string) => Promise<{ ok: boolean; results: Array<{ gameId: string; file: string }> }> | { ok: boolean; results: Array<{ gameId: string; file: string }> };
    romsImport?: (filePath: string) => Promise<{ ok: boolean; gameId?: string; path?: string; error?: string }> | { ok: boolean; gameId?: string; path?: string; error?: string };
    onAuthCallback?: (handler: (url: string) => void) => () => void;
    commonClientStart?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    commonClientSend?: (command: Record<string, unknown>) => Promise<unknown> | unknown;
    commonClientStop?: () => Promise<unknown> | unknown;
    onCommonClientEvent?: (handler: (data: unknown) => void) => () => void;
    bizhawkClientStart?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    bizhawkClientSend?: (command: Record<string, unknown>) => Promise<unknown> | unknown;
    bizhawkClientStop?: () => Promise<unknown> | unknown;
    onBizHawkClientEvent?: (handler: (data: unknown) => void) => () => void;
    patcherApply?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    bizhawkLaunch?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    bizhawkStop?: (pid: number) => Promise<unknown> | unknown;
    trackerLaunch?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    trackerStop?: (pid: number) => Promise<unknown> | unknown;
    trackerInstallPack?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    trackerStatus?: () => Promise<unknown> | unknown;
    trackerValidatePack?: (gameId: string) => Promise<unknown> | unknown;
    trackerListPackVariants?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    trackerSetVariant?: (gameId: string, variant: string) => Promise<unknown> | unknown;
    resolveModuleForDownload?: (downloadUrl: string) => Promise<unknown> | unknown;
    getModuleManifest?: (moduleId: string) => Promise<unknown> | unknown;
    listRuntimeModules?: () => Promise<unknown> | unknown;
    gamescopeStatus?: () => Promise<unknown> | unknown;
    wmctrlStatus?: () => Promise<unknown> | unknown;
    getDisplays?: () => Promise<unknown> | unknown;
    sessionAutoLaunch?: (options: Record<string, unknown>) => Promise<unknown> | unknown;
    onSessionEvent?: (handler: (data: unknown) => void) => () => void;
    onUpdaterEvent?: (handler: (data: unknown) => void) => () => void;
    logToMain?: (payload: Record<string, unknown>) => Promise<unknown> | unknown;
  };
}

declare const __APP_VERSION__: string;
