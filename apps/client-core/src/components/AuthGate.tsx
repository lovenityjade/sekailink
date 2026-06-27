import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { apiFetchWithTimeout, apiUrl, API_BASE_URL, getDesktopToken, getDeviceId, setDesktopToken, setWebAuthCache } from "../services/api";
import { isRuntimeLabCredentials, setRuntimeLabSession } from "../services/runtimeLab";
import { hasSoloModeUrlFlag, setSoloModeEnabled } from "../services/soloMode";
import AnimatedBackground from "./AnimatedBackground";
import BootScreen from "./BootScreen";
import UpdateNotesModal from "./UpdateNotesModal";
import { emitUpdateAvailable } from "../services/toast";
import { useI18n } from "../i18n";
import { trace, traceError } from "../services/trace";

interface AuthGateProps {
  children: React.ReactNode;
}

type AuthStatus = "checking" | "authed" | "unauth" | "unreachable" | "noapi" | "update-required" | "launcher-repair";

const SHOW_BETA4_SOLO_ENTRY = false;

const compareVersions = (a: string, b: string) => {
  const parse = (v: string) => {
    const [base, pre] = String(v || "0").split("-");
    const nums = base.split(".").map((n) => Number(n || 0));
    return { nums, pre: pre || "" };
  };
  const av = parse(a);
  const bv = parse(b);
  const len = Math.max(av.nums.length, bv.nums.length);
  for (let i = 0; i < len; i += 1) {
    const diff = (av.nums[i] || 0) - (bv.nums[i] || 0);
    if (diff !== 0) return diff;
  }
  if (av.pre && !bv.pre) return -1;
  if (!av.pre && bv.pre) return 1;
  return av.pre.localeCompare(bv.pre);
};

const formatBytes = (value: number) => {
  if (!Number.isFinite(value) || value <= 0) return "0 B";
  const units = ["B", "KB", "MB", "GB"];
  let num = value;
  let unitIdx = 0;
  while (num >= 1024 && unitIdx < units.length - 1) {
    num /= 1024;
    unitIdx += 1;
  }
  return `${num.toFixed(unitIdx === 0 ? 0 : 1)} ${units[unitIdx]}`;
};

const hashTextSignature = (value: string) => {
  let hash = 2166136261;
  for (let i = 0; i < value.length; i += 1) {
    hash ^= value.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return (hash >>> 0).toString(16).padStart(8, "0");
};

const clientPlatformFromEnv = (env: Record<string, unknown> | null | undefined) => {
  const platform = String((env && env.platform) || "").toLowerCase();
  if (platform === "win32") return "windows";
  if (platform === "linux") return "linux";
  if (platform === "darwin") return "macos";
  const ua = String(window.navigator?.userAgent || "").toLowerCase();
  if (ua.includes("windows")) return "windows";
  if (ua.includes("linux")) return "linux";
  if (ua.includes("mac os") || ua.includes("macintosh")) return "macos";
  return "";
};

const releaseChannelFromEnv = (env: Record<string, unknown> | null | undefined, appVersion: string) => {
  const fromEnv = String((env && env.bootstrap_channel) || "").trim().toLowerCase();
  if (fromEnv === "stable" || fromEnv === "debug" || fromEnv === "test") return fromEnv;
  const v = appVersion.toLowerCase();
  if (v.includes("debug") || v.includes("nightly")) return "debug";
  return "test";
};

const releaseBuildFromEnv = (env: Record<string, unknown> | null | undefined) => {
  const fromEnv = String((env && env.bootstrap_build) || "").trim().toLowerCase();
  return fromEnv || "release";
};

interface VersionInfo {
  latest?: string;
  min_supported?: string;
  download_url?: string;
  notes?: string;
  sha256?: string;
  incremental_manifest_url?: string;
  update_notes_url?: string;
  update_notes_version?: string;
  update_notes_title?: string;
}

interface ReleaseLatestInfo {
  ok?: boolean;
  available?: boolean;
  version?: string;
  semver?: string;
  platform?: string;
  channel?: string;
  build?: string;
  artifact_type?: string;
  download_url?: string;
  sha256?: string;
  signature?: string;
  public_key?: string;
  fallback_download_url?: string;
  fallback_sha256?: string;
  fallback_signature?: string;
  fallback_public_key?: string;
  fallback_artifact_type?: string;
  requires_client_updater?: string;
  files_count?: number;
  total_bytes?: number;
  updated_at?: string;
}

interface UpdateDownloadState {
  active: boolean;
  progress: number | null;
  receivedBytes: number;
  totalBytes: number;
  path: string;
  error: string;
  downloadId: string;
}

interface IncrementalSyncState {
  active: boolean;
  changed: number;
  deleted: number;
  processed: number;
  total: number;
  downloadedBytes: number;
  error: string;
}

const AuthGate: React.FC<AuthGateProps> = ({ children }) => {
  const { t } = useI18n();
  const [status, setStatus] = useState<AuthStatus>("checking");
  const [bootLabel, setBootLabel] = useState(t("boot.status_default"));
  const [bootProgress, setBootProgress] = useState(0.1);
  const [bootDone, setBootDone] = useState(false);
  const [runtimeEnv, setRuntimeEnv] = useState<Record<string, unknown> | null>(null);
  const [versionInfo, setVersionInfo] = useState<VersionInfo | null>(null);
  const [updateAvailable, setUpdateAvailable] = useState(false);
  const [showNotes, setShowNotes] = useState(false);
  const [authError, setAuthError] = useState("");
  const [identity, setIdentity] = useState("");
  const [password, setPassword] = useState("");
  const [twoFactorCode, setTwoFactorCode] = useState("");
  const [needsTwoFactor, setNeedsTwoFactor] = useState(false);
  const [loginBusy, setLoginBusy] = useState(false);
  const [updateHubOpen, setUpdateHubOpen] = useState(false);
  const [updateCheckError, setUpdateCheckError] = useState("");
  const [lastVersionCheckAt, setLastVersionCheckAt] = useState<number>(0);
  const [releaseInfo, setReleaseInfo] = useState<ReleaseLatestInfo | null>(null);
  const [updateDownload, setUpdateDownload] = useState<UpdateDownloadState>({
    active: false,
    progress: null,
    receivedBytes: 0,
    totalBytes: 0,
    path: "",
    error: "",
    downloadId: "",
  });
  const [incrementalSync, setIncrementalSync] = useState<IncrementalSyncState>({
    active: false,
    changed: 0,
    deleted: 0,
    processed: 0,
    total: 0,
    downloadedBytes: 0,
    error: "",
  });
  const [updateNotesOpen, setUpdateNotesOpen] = useState(false);
  const [updateNotesTitle, setUpdateNotesTitle] = useState(t("auth.update_title"));
  const [updateNotesMarkdown, setUpdateNotesMarkdown] = useState("");
  const [updateNotesSignature, setUpdateNotesSignature] = useState("");
  const [launcherRepairError, setLauncherRepairError] = useState("");
  const minBootMs = 2200;
  const bootAuthTimeoutMs = 3500;
  const updateNoticeShownRef = useRef(false);
  const lastSyncedManifestRef = useRef("");
  const lastNotesAttemptRef = useRef("");
  const autoUpdateAttemptRef = useRef("");
  const bootRunIdRef = useRef(0);
  const updateNoticeVersionRef = useRef("");
  const handledAuthStatesRef = useRef<Set<string>>(new Set());
  const authCallbackBusyRef = useRef(false);
  const authPollTimerRef = useRef<number | null>(null);
  const updateAudioRef = useRef<HTMLAudioElement | null>(null);
  const notifiedReleaseVersionRef = useRef("");
  const bootDoneRef = useRef(false);
  const bootLabelRef = useRef("");
  const authTrace = useCallback((event: string, payload?: Record<string, unknown>) => {
    trace("auth-gate", event, payload || {});
  }, []);

  useEffect(() => {
    authTrace("status_changed", { status, bootDone });
  }, [authTrace, bootDone, status]);

  useEffect(() => {
    bootDoneRef.current = bootDone;
    bootLabelRef.current = bootLabel;
  }, [bootDone, bootLabel]);

  const registerUrl = "https://sekailink.com/register.html";
  const forgotPasswordUrl = "https://sekailink.com/forgot-password.html";
  const appVersion = String((runtimeEnv && runtimeEnv.app_version) || __APP_VERSION__ || "0.0.0");
  const currentReleaseVersion = String((runtimeEnv && runtimeEnv.bootstrap_release_version) || "").trim();
  const releaseChannel = useMemo(() => releaseChannelFromEnv(runtimeEnv, appVersion), [appVersion, runtimeEnv]);
  const releaseBuild = useMemo(() => releaseBuildFromEnv(runtimeEnv), [runtimeEnv]);
  const clientPlatform = useMemo(() => clientPlatformFromEnv(runtimeEnv), [runtimeEnv]);

  const loadRuntimeEnv = useCallback(async () => {
    if (!window.sekailink?.getEnv) return null;
    try {
      const env = await window.sekailink.getEnv();
      if (env && typeof env === "object") {
        const normalized = env as Record<string, unknown>;
        setRuntimeEnv(normalized);
        return normalized;
      }
    } catch {
      // ignore env failures
    }
    return null;
  }, []);

  useEffect(() => {
    let cancelled = false;
    const loadEnv = async () => {
      const env = await loadRuntimeEnv();
      if (cancelled || !env) return;
      setRuntimeEnv(env);
    };
    void loadEnv();
    return () => {
      cancelled = true;
    };
  }, [loadRuntimeEnv]);

  useEffect(() => {
    try {
      updateAudioRef.current = new Audio("/assets/sfx/sfx_update_notification.wav");
      updateAudioRef.current.preload = "auto";
      updateAudioRef.current.volume = 0.65;
    } catch {
      updateAudioRef.current = null;
    }
    return () => {
      updateAudioRef.current = null;
    };
  }, []);

  const resolveServerUrl = useCallback((raw: string) => {
    const url = String(raw || "").trim();
    if (!url) return "";
    if (/^https?:\/\//i.test(url)) return url;
    return apiUrl(url.startsWith("/") ? url : `/${url}`);
  }, []);

  const markUpdateNotesSeen = useCallback((signature: string) => {
    const sig = String(signature || "").trim();
    if (!sig) return;
    try {
      window.localStorage.setItem(`skl_update_notes_seen:${sig}`, "1");
    } catch {
      // ignore storage errors
    }
  }, []);

  const hasSeenUpdateNotes = useCallback((signature: string) => {
    const sig = String(signature || "").trim();
    if (!sig) return false;
    try {
      return window.localStorage.getItem(`skl_update_notes_seen:${sig}`) === "1";
    } catch {
      return false;
    }
  }, []);

  const checkAuth = useCallback(async () => {
    const startedAt = performance.now();
    const forcedSolo = hasSoloModeUrlFlag();
    if (forcedSolo) {
      setSoloModeEnabled(true);
      authTrace("solo_mode_authed", { forcedSolo, storedSolo: false });
      setStatus("authed");
      return;
    }
    setSoloModeEnabled(false);
    if (!API_BASE_URL) {
      setStatus("noapi");
      return;
    }
    // Desktop auth source of truth: local desktop token.
    // If it's missing, never treat the client as authenticated.
    const desktopToken = getDesktopToken();
    authTrace("check_auth_start", { hasToken: Boolean(desktopToken), tokenLen: desktopToken ? desktopToken.length : 0 });
    if (!desktopToken) {
      authTrace("check_auth_no_token", { durationMs: Math.round(performance.now() - startedAt) });
      setStatus("unauth");
      return;
    }
    // Accept legacy + current token formats during migration.
    // Desktop device-id enforcement is temporarily disabled server-side.
    // Keep token format validation, but don't force local hash match for now.
    try {
      const res = await apiFetchWithTimeout("/api/identity/me", 6000);
      authTrace("check_auth_response", { status: res.status, ok: res.ok });
      if (res.ok) {
        try {
          const data = await res.json();
          const user = (data as any)?.user || data;
          if (data && typeof data === "object" && (data as any).ok !== false && String(user?.user_id || user?.username || "").trim()) {
            setSoloModeEnabled(false);
            setWebAuthCache({ user, session: (data as any)?.session });
            setStatus("authed");
            authTrace("check_auth_authed");
            return;
          }
        } catch {
          // ignore and treat as unauth below
        }
        setDesktopToken(null);
        setWebAuthCache(null);
        setStatus("unauth");
        return;
      }
      if (res.status === 401 || res.status === 403) {
        authTrace("check_auth_rejected", { status: res.status, durationMs: Math.round(performance.now() - startedAt) });
        setDesktopToken(null);
        setWebAuthCache(null);
        setStatus("unauth");
        return;
      }
      authTrace("check_auth_unreachable_status", { status: res.status, durationMs: Math.round(performance.now() - startedAt) });
      setStatus("unreachable");
    } catch (error) {
      traceError("auth-gate", "check_auth_error", error, { durationMs: Math.round(performance.now() - startedAt) });
      setStatus("unreachable");
    }
  }, []);

  const checkVersion = useCallback(async () => {
    if (!API_BASE_URL) return { required: false, available: false };
    try {
      const suffix = clientPlatform ? `?platform=${encodeURIComponent(clientPlatform)}` : "";
      const res = await apiFetchWithTimeout(`/api/client/version${suffix}`, 6000);
      if (!res.ok) {
        setUpdateCheckError(t("auth.update_check_failed", { status: res.status }));
        return { required: false, available: false, info: null };
      }
      const data: VersionInfo = await res.json();
      setVersionInfo(data);
      const min = String(data.min_supported || "").trim();
      const latest = String(data.latest || "").trim();
      const required = Boolean(min && compareVersions(appVersion, min) < 0);
      const available = Boolean(latest && compareVersions(latest, appVersion) > 0);
      if (!available) {
        updateNoticeShownRef.current = false;
        updateNoticeVersionRef.current = "";
      } else if (latest && updateNoticeVersionRef.current !== latest) {
        updateNoticeShownRef.current = false;
      }
      setUpdateAvailable(available);
      if (required) setStatus("update-required");
      setLastVersionCheckAt(Date.now());
      setUpdateCheckError("");
      return { required, available, info: data };
    } catch {
      setUpdateCheckError(t("auth.update_service_unreachable"));
      return { required: false, available: false, info: null };
    }
  }, [appVersion, clientPlatform, t]);

  const checkReleaseLatest = useCallback(async (envOverride?: Record<string, unknown> | null) => {
    if (!API_BASE_URL) return { available: false };
    const env = envOverride || runtimeEnv;
    const effectivePlatform = clientPlatformFromEnv(env);
    const effectiveChannel = releaseChannelFromEnv(env, appVersion);
    const effectiveBuild = releaseBuildFromEnv(env);
    if (!effectivePlatform) return { available: false };
    try {
      const params = new URLSearchParams();
      params.set("platform", effectivePlatform);
      params.set("channel", effectiveChannel);
      params.set("build", effectiveBuild);
      const res = await apiFetchWithTimeout(`/api/client/release-latest?${params.toString()}`, 6000);
      if (!res.ok) {
        setUpdateCheckError(t("auth.update_check_failed", { status: res.status }));
        return { available: false };
      }
      const data: ReleaseLatestInfo = await res.json();
      setReleaseInfo(data);
      const latestVersion = String(data.version || "").trim();
      const installedVersion = String((env && env.bootstrap_release_version) || currentReleaseVersion || "").trim();
      const bundleSupported = Boolean((env && env.client_update_bundle_supported) || (env && env.bootstrap_install_dir));
      const serverAvailable = data.available !== false;
      const available = Boolean(
        serverAvailable &&
        bundleSupported &&
        latestVersion &&
        (installedVersion ? latestVersion !== installedVersion : latestVersion !== appVersion)
      );
      if (!available) {
        updateNoticeShownRef.current = false;
        updateNoticeVersionRef.current = "";
      } else if (latestVersion && updateNoticeVersionRef.current !== latestVersion) {
        updateNoticeShownRef.current = false;
      }
      setUpdateAvailable(available);
      setLastVersionCheckAt(Date.now());
      setUpdateCheckError("");
      return { available, info: data };
    } catch {
      setUpdateCheckError(t("auth.update_service_unreachable"));
      return { available: false };
    }
  }, [API_BASE_URL, appVersion, currentReleaseVersion, runtimeEnv, t]);

  const buildUpdateOptions = useCallback((info?: ReleaseLatestInfo | null) => {
    const data = info || releaseInfo;
    const downloadUrl = String(data?.download_url || "").trim();
    const sha256 = String(data?.sha256 || "").trim();
    if (!downloadUrl || !sha256) return null;
    return {
      downloadUrl,
      version: String(data?.version || "").trim(),
      latest: String(data?.version || "").trim(),
      sha256,
      signature: String(data?.signature || "").trim(),
      publicKey: String(data?.public_key || "").trim(),
      artifactType: String(data?.artifact_type || "").trim(),
      channel: String(data?.channel || releaseChannel).trim(),
      build: String(data?.build || releaseBuild).trim(),
      fallbackDownloadUrl: String(data?.fallback_download_url || "").trim(),
      fallbackSha256: String(data?.fallback_sha256 || "").trim(),
      fallbackSignature: String(data?.fallback_signature || "").trim(),
      fallbackPublicKey: String(data?.fallback_public_key || "").trim(),
      fallbackArtifactType: String(data?.fallback_artifact_type || "").trim(),
      timeoutMs: 25 * 60 * 1000,
    };
  }, [releaseBuild, releaseChannel, releaseInfo]);

  const runIncrementalSync = useCallback(async () => {
    const rawManifestUrl = String(versionInfo?.incremental_manifest_url || "").trim();
    if (!rawManifestUrl) return;
    const manifestUrl = resolveServerUrl(rawManifestUrl);
    if (!manifestUrl || !window.sekailink?.updaterSyncIncremental) return;
    const token = getDesktopToken() || "";
    setIncrementalSync((prev) => ({ ...prev, active: true, error: "" }));
    const result = await window.sekailink.updaterSyncIncremental({ manifestUrl, authToken: token });
    if (!result?.ok) {
      setIncrementalSync((prev) => ({
        ...prev,
        active: false,
        error: String(result?.error || t("auth.incremental_sync_failed")),
      }));
      return;
    }
    setIncrementalSync({
      active: false,
      changed: Number(result.changed || 0),
      deleted: Number(result.deleted || 0),
      processed: Number(result.processed || 0),
      total: Number(result.total || 0),
      downloadedBytes: Number(result.downloadedBytes || 0),
      error: "",
    });
  }, [resolveServerUrl, t, versionInfo?.incremental_manifest_url]);

  const runLauncherEntrypointGuard = useCallback(async (env?: Record<string, unknown> | null) => {
    const packaged = Boolean(env && env.app_is_packaged === true);
    const launchedByBootstrapper = Boolean(env && env.bootstrap_launch_token_valid === true);
    const runtimeLabDirect = Boolean(env && env.runtime_lab_direct === true);
    if (runtimeLabDirect) {
      authTrace("launcher_guard_runtime_lab_bypass");
      return false;
    }
    if (!packaged || launchedByBootstrapper) return false;

    const bootstrapperPath = String((env && env.bootstrapper_path) || "").trim();
    if (bootstrapperPath && window.sekailink?.updaterLaunchBootstrapperAndQuit) {
      setBootLabel(t("auth.boot_launching_updater"));
      setBootProgress(0.16);
      const result = await window.sekailink.updaterLaunchBootstrapperAndQuit();
      if (result?.ok) {
        authTrace("launcher_guard_relaunch", { path: String(result.path || bootstrapperPath) });
        return true;
      }
      const error = String(result?.error || "bootstrapper_launch_failed");
      setLauncherRepairError(error);
      setStatus("launcher-repair");
      authTrace("launcher_guard_launch_failed", { error });
      return true;
    }

    setLauncherRepairError("bootstrapper_not_found");
    setStatus("launcher-repair");
    authTrace("launcher_guard_missing_bootstrapper", {
      installDir: String((env && (env.bootstrap_install_dir || env.client_update_install_dir)) || ""),
    });
    return true;
  }, [authTrace, t]);

  const maybeOpenUpdateNotes = useCallback(async () => {
    const notesUrlRaw = String(versionInfo?.update_notes_url || "").trim();
    if (!notesUrlRaw) return;
    const notesUrl = resolveServerUrl(notesUrlRaw);
    if (!notesUrl) return;

    const serverSignature = String(versionInfo?.update_notes_version || "").trim();
    if (serverSignature && hasSeenUpdateNotes(serverSignature)) return;

    try {
      const res = await fetch(notesUrl, { credentials: "include" });
      if (!res.ok) return;
      const markdown = await res.text();
      if (!markdown.trim()) return;
      const computedSignature = serverSignature || `${String(versionInfo?.latest || appVersion)}-${hashTextSignature(markdown)}`;
      if (hasSeenUpdateNotes(computedSignature)) return;
      setUpdateNotesTitle(String(versionInfo?.update_notes_title || t("auth.update_title")));
      setUpdateNotesMarkdown(markdown);
      setUpdateNotesSignature(computedSignature);
      setUpdateNotesOpen(true);
    } catch {
      // ignore notes fetch failures
    }
  }, [appVersion, hasSeenUpdateNotes, resolveServerUrl, t, versionInfo]);

  const closeUpdateNotes = useCallback(() => {
    if (updateNotesSignature) {
      markUpdateNotesSeen(updateNotesSignature);
    }
    setUpdateNotesOpen(false);
  }, [markUpdateNotesSeen, updateNotesSignature]);

  useEffect(() => {
    if (!window.sekailink?.onUpdaterEvent) return;
    const off = window.sekailink.onUpdaterEvent((raw: unknown) => {
      const data = raw && typeof raw === "object" ? (raw as Record<string, unknown>) : {};
      const eventName = String(data.event || "");
      if (eventName === "download-started") {
        setUpdateHubOpen(true);
        setUpdateDownload({
          active: true,
          progress: 0,
          receivedBytes: 0,
          totalBytes: 0,
          path: "",
          error: "",
          downloadId: String(data.downloadId || ""),
        });
        if (!bootDoneRef.current && bootLabelRef.current.toLowerCase().includes("updat")) {
          setBootLabel(t("auth.boot_updating"));
          setBootProgress(0.35);
        }
        return;
      }
      if (eventName === "download-progress") {
        const pct = Number.isFinite(Number(data.percent)) ? Number(data.percent) : null;
        setUpdateDownload((prev) => ({
          ...prev,
          active: true,
          progress: pct !== null ? pct : prev.progress,
          receivedBytes: Number.isFinite(Number(data.receivedBytes)) ? Number(data.receivedBytes) : prev.receivedBytes,
          totalBytes: Number.isFinite(Number(data.totalBytes)) ? Number(data.totalBytes) : prev.totalBytes,
        }));
        if (!bootDoneRef.current && bootLabelRef.current.toLowerCase().includes("updat") && pct !== null) {
          // Keep some headroom for apply/restart after the download completes.
          const scaled = 0.35 + Math.max(0, Math.min(1, pct / 100)) * 0.55;
          setBootProgress(scaled);
        }
        return;
      }
      if (eventName === "download-complete") {
        setUpdateDownload((prev) => ({
          ...prev,
          active: false,
          progress: 100,
          path: String(data.path || ""),
          error: "",
          receivedBytes: Number.isFinite(Number(data.receivedBytes)) ? Number(data.receivedBytes) : prev.receivedBytes,
          totalBytes: Number.isFinite(Number(data.totalBytes)) ? Number(data.totalBytes) : prev.totalBytes,
        }));
        if (!bootDoneRef.current && bootLabelRef.current.toLowerCase().includes("updat")) {
          setBootProgress(0.93);
        }
        return;
      }
      if (eventName === "download-error") {
        setUpdateHubOpen(true);
        setUpdateDownload((prev) => ({
          ...prev,
          active: false,
          error: String(data.error || t("auth.download_failed")),
        }));
        if (!bootDoneRef.current && bootLabelRef.current.toLowerCase().includes("updat")) {
          setBootProgress(1);
        }
        return;
      }
      if (eventName === "download-fallback") {
        setUpdateHubOpen(true);
        setUpdateCheckError(String(data.error || ""));
        return;
      }
      if (eventName === "install-started") {
        setUpdateHubOpen(true);
        setUpdateDownload((prev) => ({
          ...prev,
          active: true,
          progress: 100,
          error: "",
        }));
        if (!bootDoneRef.current) {
          setBootLabel(t("auth.boot_updating"));
          setBootProgress(0.96);
        }
        return;
      }
      if (eventName === "update-restarting") {
        setUpdateHubOpen(true);
        setUpdateDownload((prev) => ({
          ...prev,
          active: false,
          progress: 100,
          error: "",
        }));
        if (!bootDoneRef.current) {
          setBootLabel(t("auth.boot_restarting"));
          setBootProgress(1);
        }
        return;
      }
      if (eventName === "incremental-sync-started") {
        setIncrementalSync((prev) => ({
          ...prev,
          active: true,
          error: "",
          processed: 0,
          total: 0,
          downloadedBytes: 0,
        }));
        return;
      }
      if (eventName === "incremental-sync-file") {
        const processed = Number.isFinite(Number(data.processed)) ? Number(data.processed) : null;
        const total = Number.isFinite(Number(data.total)) ? Number(data.total) : null;
        setIncrementalSync((prev) => ({
          ...prev,
          active: true,
          processed: processed !== null ? processed : prev.processed,
          total: total !== null ? total : prev.total,
        }));
        return;
      }
      if (eventName === "incremental-sync-complete") {
        setIncrementalSync({
          active: false,
          changed: Number(data.changed || 0),
          deleted: Number(data.deleted || 0),
          processed: Number(data.processed || 0),
          total: Number(data.total || 0),
          downloadedBytes: Number(data.downloadedBytes || 0),
          error: "",
        });
        return;
      }
      if (eventName === "incremental-sync-error") {
        setIncrementalSync((prev) => ({
          ...prev,
          active: false,
          error: String(data.error || t("auth.incremental_sync_failed")),
        }));
      }
    });
    return () => off?.();
  }, []);

  const runStartupAutoUpdate = useCallback(async (env?: Record<string, unknown> | null) => {
    setBootLabel(t("auth.boot_check_version"));
    setBootProgress(0.18);
    const checked = await checkReleaseLatest(env);
    if (!checked.available || !(checked as any)?.info) {
      authTrace("startup_update_none", { available: checked.available === true });
      return false;
    }
    const info = (checked as any).info as ReleaseLatestInfo;
    const options = buildUpdateOptions(info);
    const downloadUrl = options ? String(options.downloadUrl || "") : "";
    const marker = `${String(info.version || "")}|${downloadUrl}`;
    if (marker.trim()) autoUpdateAttemptRef.current = marker;
    setUpdateAvailable(true);
    authTrace("startup_update_notice_only", {
      version: String(info.version || ""),
      hasDownload: Boolean(downloadUrl),
    });
    return false;
  }, [buildUpdateOptions, checkReleaseLatest, t]);

  useEffect(() => {
    const bootRunId = ++bootRunIdRef.current;
    let cancelled = false;
    const runBoot = async () => {
      const startedAt = Date.now();
      authTrace("boot_start", { bootRunId });
      try {
        const env = await loadRuntimeEnv();
        authTrace("boot_env_loaded", {
          bootRunId,
          hasEnv: Boolean(env),
          appVersion: String((env && env.app_version) || ""),
          platform: String((env && env.platform) || ""),
        });
        if (cancelled) return;
        const launcherHandled = await runLauncherEntrypointGuard(env);
        authTrace("boot_launcher_guard_checked", { bootRunId, launcherHandled });
        if (cancelled || launcherHandled) return;
        const updateStarted = await runStartupAutoUpdate(env);
        authTrace("boot_update_checked", { bootRunId, updateStarted });
        if (cancelled || updateStarted) return;
        setBootLabel(t("auth.boot_check_session"));
        setBootProgress(0.35);
        authTrace("boot_check_session_start", { bootRunId, timeoutMs: bootAuthTimeoutMs });
        const authResult = await Promise.race([
          checkAuth().then(() => "done" as const),
          new Promise<"timeout">((resolve) => {
            window.setTimeout(() => resolve("timeout"), bootAuthTimeoutMs);
          }),
        ]);
        authTrace("boot_check_session_complete", { bootRunId, result: authResult });
        if (authResult === "timeout" && !cancelled) {
          authTrace("check_auth_boot_timeout", { bootRunId, timeoutMs: bootAuthTimeoutMs });
          setStatus((current) => (current === "checking" ? "unreachable" : current));
        }
        if (cancelled) return;
      } catch (error) {
        traceError("auth-gate", "boot_error", error, { bootRunId });
        setStatus((current) => (current === "checking" ? "unreachable" : current));
      } finally {
        if (cancelled) return;
        const elapsed = Date.now() - startedAt;
        if (elapsed < minBootMs) {
          await new Promise((resolve) => setTimeout(resolve, minBootMs - elapsed));
        }
        setBootLabel(t("auth.boot_launching"));
        setBootProgress(1);
        window.setTimeout(() => {
          if (!cancelled) {
            authTrace("boot_done", { bootRunId, durationMs: Date.now() - startedAt });
            setBootDone(true);
          }
        }, 400);
      }
    };
    runBoot();
    return () => {
      cancelled = true;
      authTrace("boot_cancelled", { bootRunId });
    };
    // Boot is intentionally run once per mounted AuthGate. Including runtime callbacks
    // re-arms boot after state changes and can spam session checks.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (status !== "authed") return;
    void checkReleaseLatest();
    const timer = window.setInterval(() => {
      void checkReleaseLatest();
    }, 20 * 1000);
    return () => window.clearInterval(timer);
  }, [checkReleaseLatest, status]);

  useEffect(() => {
    if (status !== "authed" || !updateAvailable || updateNoticeShownRef.current) return;
    const latest = String(releaseInfo?.version || versionInfo?.latest || "").trim();
    updateNoticeShownRef.current = true;
    if (latest) updateNoticeVersionRef.current = latest;
    emitUpdateAvailable({
      version: latest,
      message: latest ? `A new version is available ${latest}!` : "A new version is available!",
    });
  }, [releaseInfo?.version, status, updateAvailable, versionInfo?.latest]);

  useEffect(() => {
    if (status !== "authed" || !bootDone || !updateAvailable) return;
    const latestReleaseVersion = String(releaseInfo?.version || "").trim();
    if (!latestReleaseVersion) return;
    if (notifiedReleaseVersionRef.current === latestReleaseVersion) return;
    notifiedReleaseVersionRef.current = latestReleaseVersion;
    try {
      if (updateAudioRef.current) {
        updateAudioRef.current.currentTime = 0;
        void updateAudioRef.current.play();
      }
    } catch {
      // ignore autoplay errors
    }
  }, [bootDone, releaseInfo?.version, status, updateAvailable]);

  useEffect(() => {
    if (status !== "authed" || !bootDone) return;
    const rawManifestUrl = String(versionInfo?.incremental_manifest_url || "").trim();
    if (!rawManifestUrl) return;
    const manifestUrl = resolveServerUrl(rawManifestUrl);
    if (!manifestUrl) return;
    const marker = `${String(versionInfo?.latest || "")}|${manifestUrl}`;
    if (lastSyncedManifestRef.current === marker) return;
    lastSyncedManifestRef.current = marker;
    void runIncrementalSync();
  }, [bootDone, resolveServerUrl, runIncrementalSync, status, versionInfo?.incremental_manifest_url, versionInfo?.latest]);

  useEffect(() => {
    if (status !== "authed" || !bootDone) return;
    const notesUrl = String(versionInfo?.update_notes_url || "").trim();
    if (!notesUrl) return;
    const marker = `${String(versionInfo?.latest || "")}|${String(versionInfo?.update_notes_version || "")}|${notesUrl}`;
    if (lastNotesAttemptRef.current === marker) return;
    lastNotesAttemptRef.current = marker;
    void maybeOpenUpdateNotes();
  }, [bootDone, maybeOpenUpdateNotes, status, versionInfo?.latest, versionInfo?.update_notes_url, versionInfo?.update_notes_version]);

  const openWebsite = async (url: string) => {
    if (window.sekailink?.openExternal) {
      try {
        await window.sekailink.openExternal(url);
        return;
      } catch (err) {
        authTrace("open_website_throw", { url, error: String(err || "") });
      }
    }
    window.open(url, "_blank", "noopener,noreferrer");
  };

  const authErrorMessage = (data: any, fallback = t("auth.login_failed_retry")) => {
    switch (String(data?.error || "")) {
      case "invalid_credentials":
        return "Identifiants invalides.";
      case "account_disabled":
        return "Ce compte est désactivé.";
      case "two_factor_required":
        return "Ce compte nécessite un code 2FA ou un recovery code.";
      case "missing_identity":
        return "Le champ identité est requis.";
      case "missing_password":
        return "Le mot de passe est requis.";
      case "network_error":
        return String(data?.desc || "Impossible de joindre le serveur d'identité.");
      default:
        return String(data?.desc || data?.error || fallback);
    }
  };

  const submitNativeLogin = async (event: React.FormEvent) => {
    event.preventDefault();
    try {
      setSoloModeEnabled(false);
    } catch {
      // ignore storage errors
    }
    const nextIdentity = identity.trim();
    if (!nextIdentity || !password) {
      setAuthError("Email/nom d'utilisateur et mot de passe requis.");
      return;
    }
    if (isRuntimeLabCredentials(nextIdentity, password)) {
      setRuntimeLabSession(true);
      setDesktopToken(null);
      setWebAuthCache(null);
      setSoloModeEnabled(false);
      setPassword("");
      setTwoFactorCode("");
      setNeedsTwoFactor(false);
      setAuthError("");
      setStatus("authed");
      authTrace("runtime_lab_login_authed");
      return;
    }
    setRuntimeLabSession(false);
    setLoginBusy(true);
    setAuthError("");
    authTrace("native_login_start", { hasTwoFactor: Boolean(twoFactorCode.trim()) });
    try {
      const body: Record<string, string> = { identity: nextIdentity, password };
      if (twoFactorCode.trim()) body.two_factor_code = twoFactorCode.trim();
      const res = await apiFetchWithTimeout("/api/identity/login", 10000, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
      });
      const data = await res.json().catch(() => ({}));
      authTrace("native_login_response", { status: res.status, ok: res.ok, error: String((data as any)?.error || "") });
      if (res.ok && (data as any)?.ok && (data as any)?.session?.session_token) {
        setDesktopToken(String((data as any).session.session_token));
        setWebAuthCache({ user: (data as any).user, session: (data as any).session });
        setSoloModeEnabled(false);
        setPassword("");
        setTwoFactorCode("");
        setNeedsTwoFactor(false);
        setAuthError("");
        setStatus("authed");
        authTrace("native_login_authed");
        return;
      }
      if (String((data as any)?.error || "") === "two_factor_required") {
        setNeedsTwoFactor(true);
      }
      setAuthError(authErrorMessage(data));
      setStatus("unauth");
    } catch {
      setAuthError("Impossible de joindre le serveur d'identité.");
      setStatus("unreachable");
    } finally {
      setLoginBusy(false);
    }
  };

  const enterSoloMode = () => {
    try {
      setSoloModeEnabled(true);
      window.localStorage.removeItem("skl.activeYamlId");
    } catch {
      // ignore storage errors
    }
    setDesktopToken(null);
    setWebAuthCache(null);
    setAuthError("");
    setStatus("authed");
    authTrace("solo_mode_entered");
  };

  const quitApp = () => {
    if (window.sekailink?.windowClose) {
      void window.sekailink.windowClose();
      return;
    }
    window.close();
  };

  const startUpdateDownload = useCallback(async (info?: ReleaseLatestInfo | null) => {
    void info;
    setUpdateDownload((prev) => ({
      ...prev,
      active: true,
      progress: null,
      error: "",
      path: "",
      receivedBytes: 0,
      totalBytes: 0,
    }));
    if (!window.sekailink?.updaterLaunchBootstrapperAndQuit) {
      setUpdateDownload((prev) => ({
        ...prev,
        active: false,
        error: "bootloader_launch_not_supported",
      }));
      return;
    }
    const result = await window.sekailink.updaterLaunchBootstrapperAndQuit();
    if (!result?.ok) {
      setUpdateDownload((prev) => ({
        ...prev,
        active: false,
        error: String(result?.error || "bootloader_launch_failed"),
      }));
      return;
    }
    setUpdateDownload((prev) => ({
      ...prev,
      active: false,
    }));
  }, []);

  const installDownloadedUpdate = useCallback(async () => {
    await startUpdateDownload();
  }, [startUpdateDownload]);

  const revealDownloadedUpdate = useCallback(async () => {
    const installDir = String((runtimeEnv && (runtimeEnv.client_update_install_dir || runtimeEnv.bootstrap_install_dir)) || "").trim();
    if (!installDir) return;
    await window.sekailink?.showItemInFolder?.(installDir);
  }, [runtimeEnv]);

  const retryLauncher = useCallback(async () => {
    setLauncherRepairError("");
    const result = await window.sekailink?.updaterLaunchBootstrapperAndQuit?.();
    if (!result?.ok) {
      setLauncherRepairError(String(result?.error || "bootstrapper_launch_failed"));
    }
  }, []);

  const openBootstrapperDownload = useCallback(async () => {
    const url = String((runtimeEnv && runtimeEnv.bootstrapper_download_url) || "").trim();
    if (url) await window.sekailink?.openExternal?.(url);
  }, [runtimeEnv]);

  const handleAuthCallback = useCallback(
    async (url: string) => {
      try {
        const parsed = new URL(url);
        if (!parsed.protocol.startsWith("sekailink")) return;
        const directToken = String(parsed.searchParams.get("token") || "").trim();
        if (directToken) {
          authTrace("callback_direct_token", { tokenLen: directToken.length });
          setDesktopToken(directToken);
          setAuthError("");
          await checkAuth();
          return;
        }
        const oauthError = parsed.searchParams.get("error");
        if (oauthError) {
          setAuthError(t("auth.login_failed_retry"));
          setStatus("unauth");
          return;
        }
        const code = parsed.searchParams.get("code");
        const state = parsed.searchParams.get("state");
        authTrace("callback_received", { hasCode: Boolean(code), hasState: Boolean(state), protocol: parsed.protocol });
        if (!code || !state) return;
        const deviceId = getDeviceId();
        if (!deviceId) {
          setAuthError(t("auth.login_failed_retry"));
          setStatus("unauth");
          return;
        }
        if (handledAuthStatesRef.current.has(state) || authCallbackBusyRef.current) {
          return;
        }
        handledAuthStatesRef.current.add(state);
        authCallbackBusyRef.current = true;
        const callbackUrl = apiUrl(`/api/auth/desktop-callback?device_id=${encodeURIComponent(deviceId)}`);
        const res = await fetch(callbackUrl, {
          method: "POST",
          credentials: "include",
          headers: {
            "Content-Type": "application/json",
            "X-SekaiLink-Device-Id": deviceId,
          },
          body: JSON.stringify({
            code,
            state,
            device_id: deviceId,
            deviceId: deviceId,
            device: deviceId,
          }),
        });
        authTrace("callback_response_status", { status: res.status, ok: res.ok });
        if (!res.ok) {
          const existingToken = getDesktopToken();
          if (existingToken) {
            await checkAuth();
            return;
          }
          let reason = "";
          try {
            const payload = await res.json();
            reason = String(payload?.error || "").trim();
          } catch {
            try {
              reason = (await res.text()).trim();
            } catch {
              reason = "";
            }
          }
          setAuthError(reason || t("auth.login_failed_retry"));
          setStatus("unauth");
          return;
        }
        const bodyText = await res.text();
        authTrace("callback_response_body_preview", { body: bodyText.slice(0, 220) });
        let data: any = null;
        try {
          data = JSON.parse(bodyText);
        } catch {
          data = null;
        }
        const returnedToken = String(
          data?.token || data?.desktop_token || data?.access_token || data?.auth_token || ""
        ).trim();
        authTrace("callback_parsed", {
          hasToken: Boolean(returnedToken),
          keys: data && typeof data === "object" ? Object.keys(data).slice(0, 12) : [],
        });
        if (returnedToken) {
          setDesktopToken(returnedToken);
        }
        setAuthError("");
        // Do not force authed state here.
        // Re-run the normal auth probe so protected actions like lobby creation
        // only unlock when a real desktop token or valid server session exists.
        await checkAuth();
        return;
      } catch {
        setAuthError(t("auth.login_failed_retry"));
        setStatus("unauth");
      } finally {
        authCallbackBusyRef.current = false;
      }
    },
    [authTrace, t]
  );

  useEffect(() => {
    if (!window.sekailink?.onAuthCallback) return;
    const unsubscribe = window.sekailink.onAuthCallback(handleAuthCallback);
    return () => unsubscribe?.();
  }, [handleAuthCallback]);

  useEffect(() => {
    return () => {
      if (authPollTimerRef.current) {
        window.clearInterval(authPollTimerRef.current);
        authPollTimerRef.current = null;
      }
    };
  }, []);

  if (!bootDone && status !== "update-required" && status !== "launcher-repair") {
    return <BootScreen status={bootLabel} progress={bootProgress} version={`v${appVersion}`} />;
  }

  if (status === "launcher-repair") {
    const installDir = String((runtimeEnv && (runtimeEnv.client_update_install_dir || runtimeEnv.bootstrap_install_dir)) || "").trim();
    const bootstrapperPath = String((runtimeEnv && runtimeEnv.bootstrapper_path) || "").trim();
    return (
      <div className="skl-auth-shell">
        <div className="skl-auth-panel">
          <h1 className="skl-app-title">{t("auth.launcher_required")}</h1>
          <p>{t("auth.launcher_required_body")}</p>
          {!!bootstrapperPath && (
            <p className="skl-auth-meta">
              {t("auth.launcher")}: <span>{bootstrapperPath}</span>
            </p>
          )}
          {!!installDir && (
            <p className="skl-auth-meta">
              {t("auth.install_dir")}: <span>{installDir}</span>
            </p>
          )}
          {!!launcherRepairError && <p className="skl-auth-error">{launcherRepairError}</p>}
          <div className="skl-auth-actions">
            <button className="skl-btn primary" type="button" onClick={() => void retryLauncher()} disabled={!bootstrapperPath}>
              {t("auth.launch_updater")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => void revealDownloadedUpdate()} disabled={!installDir}>
              {t("auth.show_in_folder")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => void openBootstrapperDownload()}>
              {t("auth.download_launcher")}
            </button>
          </div>
        </div>
      </div>
    );
  }

  if (status === "update-required") {
    return (
      <div className="skl-auth-shell">
        <div className="skl-auth-panel">
          <h1 className="skl-app-title">{t("auth.update_required")}</h1>
          <p>{t("auth.client_too_old")}</p>
          <p className="skl-auth-meta">
            {t("auth.installed")}: <span>{appVersion}</span>
          </p>
          {versionInfo?.min_supported && (
            <p className="skl-auth-meta">
              {t("auth.minimum_supported")}: <span>{versionInfo.min_supported}</span>
            </p>
          )}
          {versionInfo?.latest && (
            <p className="skl-auth-meta">
              {t("auth.latest")}: <span>{versionInfo.latest}</span>
            </p>
          )}
          {updateCheckError && <p className="skl-auth-error">{updateCheckError}</p>}
          {updateDownload.active && (
            <div className="skl-update-progress">
              <div className="skl-update-progress-bar" style={{ width: `${Math.max(0, Math.min(100, updateDownload.progress || 0))}%` }} />
            </div>
          )}
          {!!updateDownload.path && (
            <p className="skl-auth-meta">
              Downloaded: <span>{updateDownload.path}</span>
            </p>
          )}
          {!!updateDownload.error && <p className="skl-auth-error">{updateDownload.error}</p>}
          <div className="skl-auth-actions">
            <button className="skl-btn primary" type="button" onClick={() => void startUpdateDownload()} disabled={updateDownload.active}>
              {updateDownload.active ? t("auth.downloading") : t("auth.update")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => void installDownloadedUpdate()} disabled={!updateDownload.path}>
              {t("auth.install_downloaded_file")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => void revealDownloadedUpdate()} disabled={!updateDownload.path}>
              {t("auth.show_in_folder")}
            </button>
            <button className="skl-btn ghost" type="button" onClick={() => void checkVersion()}>
              {t("auth.recheck")}
            </button>
          </div>
          {versionInfo?.notes && (
            <button className="skl-btn ghost" type="button" onClick={() => setShowNotes((prev) => !prev)}>
              {showNotes ? t("auth.hide_release_notes") : t("auth.release_notes")}
            </button>
          )}
          {showNotes && versionInfo?.notes && (
            <pre className="skl-release-notes">{versionInfo.notes}</pre>
          )}
        </div>
      </div>
    );
  }

  if (status === "authed") {
    const updaterVisible = Boolean(updateDownload.path) || Boolean(updateDownload.error) || incrementalSync.active || incrementalSync.changed > 0 || !!incrementalSync.error;
    return (
      <>
        {updaterVisible && (
          <div className="skl-update-hub">
            <button className="skl-update-fab" type="button" onClick={() => setUpdateHubOpen((prev) => !prev)}>
              <span className="skl-update-fab-icon">Up</span>
              <span className="skl-update-fab-label">{t("auth.update")}</span>
            </button>
            {updateHubOpen && (
              <div className="skl-update-popover">
                <h3>{t("auth.client_updates")}</h3>
                <p className="skl-update-meta">
                  {t("auth.installed")}: <span>{appVersion}</span>
                </p>
                {versionInfo?.latest && (
                  <p className="skl-update-meta">
                    {t("auth.latest")}: <span>{versionInfo.latest}</span>
                  </p>
                )}
                {!!currentReleaseVersion && (
                  <p className="skl-update-meta">
                    {t("auth.installed")}: <span>{currentReleaseVersion}</span>
                  </p>
                )}
                {!!releaseInfo?.version && (
                  <p className="skl-update-meta">
                    {t("auth.latest")}: <span>{releaseInfo.version}</span>
                  </p>
                )}
                {!!lastVersionCheckAt && (
                  <p className="skl-update-meta">
                    {t("auth.last_check")}: <span>{new Date(lastVersionCheckAt).toLocaleTimeString()}</span>
                  </p>
                )}
                {updateDownload.active && (
                  <>
                    <div className="skl-update-progress">
                      <div className="skl-update-progress-bar" style={{ width: `${Math.max(0, Math.min(100, updateDownload.progress || 0))}%` }} />
                    </div>
                    <p className="skl-update-meta">
                      {t("auth.downloading")}: <span>{formatBytes(updateDownload.receivedBytes)} / {formatBytes(updateDownload.totalBytes)}</span>
                    </p>
                  </>
                )}
                {incrementalSync.active && (
                  <>
                    <div className="skl-update-progress">
                      <div
                        className="skl-update-progress-bar"
                        style={{
                          width: `${Math.max(
                            0,
                            Math.min(
                              100,
                              incrementalSync.total > 0 ? (incrementalSync.processed / incrementalSync.total) * 100 : 0
                            )
                          )}%`,
                        }}
                      />
                    </div>
                    <p className="skl-update-meta">
                      {t("auth.syncing_runtime")}: <span>{incrementalSync.processed}</span> / <span>{incrementalSync.total}</span>
                    </p>
                  </>
                )}
                {!incrementalSync.active && incrementalSync.changed > 0 && (
                  <p className="skl-update-meta">
                    {t("auth.runtime_sync_applied", { updated: incrementalSync.changed, removed: incrementalSync.deleted, bytes: formatBytes(incrementalSync.downloadedBytes) })}
                  </p>
                )}
                {!!updateDownload.path && (
                  <p className="skl-update-meta">
                    {t("auth.downloaded_file_ready")}
                  </p>
                )}
                {!!updateCheckError && <p className="skl-update-error">{updateCheckError}</p>}
                {!!updateDownload.error && <p className="skl-update-error">{updateDownload.error}</p>}
                {!!incrementalSync.error && <p className="skl-update-error">{t("auth.incremental_sync")}: {incrementalSync.error}</p>}
                <div className="skl-update-actions">
                  <button className="skl-btn primary" type="button" onClick={() => void startUpdateDownload()} disabled={updateDownload.active}>
                    {updateDownload.active ? t("auth.downloading") : t("auth.update")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void installDownloadedUpdate()} disabled={!updateDownload.path}>
                    {t("auth.install")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void revealDownloadedUpdate()} disabled={!updateDownload.path}>
                    {t("auth.folder")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void checkReleaseLatest()}>
                    {t("auth.check_now")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void runIncrementalSync()} disabled={!versionInfo?.incremental_manifest_url || incrementalSync.active}>
                    {incrementalSync.active ? t("auth.syncing") : t("auth.sync_runtime")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void maybeOpenUpdateNotes()} disabled={!versionInfo?.update_notes_url}>
                    {t("auth.update_notes")}
                  </button>
                </div>
                {versionInfo?.notes && (
                  <>
                    <button className="skl-btn ghost" type="button" onClick={() => setShowNotes((prev) => !prev)}>
                      {showNotes ? t("auth.hide_notes") : t("auth.release_notes")}
                    </button>
                    {showNotes && <pre className="skl-release-notes">{versionInfo.notes}</pre>}
                  </>
                )}
              </div>
            )}
          </div>
        )}
        <UpdateNotesModal
          open={updateNotesOpen}
          title={updateNotesTitle}
          markdown={updateNotesMarkdown}
          onClose={closeUpdateNotes}
        />
        {children}
      </>
    );
  }

  return (
    <>
      <AnimatedBackground />
      <div className="skl-boot skl-auth-boot">
        <div className="skl-boot-card">
          <div className="skl-boot-scanline"></div>
          <div className="skl-boot-content">
            <div className="skl-boot-orbit" aria-hidden="true">
              <div className="skl-boot-orbit-ring"></div>
              <div className="skl-boot-orbit-ring inner"></div>
              <div className="skl-boot-orbit-core">
                <img src="assets/img/sekailink-logo-image.png" alt="" />
              </div>
              <div className="skl-boot-orbit-dot dot-a"></div>
              <div className="skl-boot-orbit-dot dot-b"></div>
            </div>
            <div className="skl-boot-logo">
              <img src="assets/img/sekailink-logo-text.png" alt="SekaiLink" />
            </div>
            <div className="skl-auth-titlebar">SekaiLink Core</div>
            <p className="skl-boot-text">Connectez-vous avec votre compte SekaiLink.</p>
            {status === "noapi" && (
              <p className="skl-auth-hint">
                {t("auth.no_api_base_url")} <code>VITE_API_BASE_URL</code> {t("auth.and_rebuild")}
              </p>
            )}
            {status === "unreachable" && <p className="skl-auth-hint">{t("auth.unable_reach_server")}</p>}
            <form className="skl-native-login-form" onSubmit={submitNativeLogin}>
              <label className="skl-native-field">
                <span>Email ou Nom d'utilisateur</span>
                <input
                  value={identity}
                  onChange={(event) => setIdentity(event.target.value)}
                  autoComplete="username"
                  disabled={loginBusy || status === "noapi"}
                />
              </label>
              <label className="skl-native-field">
                <span>Mot de passe</span>
                <input
                  value={password}
                  onChange={(event) => setPassword(event.target.value)}
                  type="password"
                  autoComplete="current-password"
                  disabled={loginBusy || status === "noapi"}
                />
              </label>
              {needsTwoFactor && (
                <label className="skl-native-field">
                  <span>Code 2FA ou recovery code</span>
                  <input
                    value={twoFactorCode}
                    onChange={(event) => setTwoFactorCode(event.target.value)}
                    autoComplete="one-time-code"
                    disabled={loginBusy || status === "noapi"}
                  />
                </label>
              )}
              {authError && <p className="skl-auth-error">{authError}</p>}
              <div className="skl-auth-actions">
                <button className="skl-btn primary skl-native-login-submit" type="submit" disabled={loginBusy || status === "checking" || status === "noapi"}>
                  {loginBusy ? "Connexion..." : "Se connecter"}
                </button>
                <button className="skl-btn ghost" type="button" onClick={quitApp}>
                  {t("common.quit")}
                </button>
              </div>
            </form>
            {SHOW_BETA4_SOLO_ENTRY && (
              <div className="skl-solo-mode-card">
                <div>
                  <strong>Mode Solo</strong>
                  <span>Entrer dans la Library locale sans login. Les fonctions online demanderont une connexion plus tard.</span>
                </div>
                <button className="skl-btn ghost" type="button" onClick={enterSoloMode}>
                  Entrer en Solo
                </button>
              </div>
            )}
            <div className="skl-auth-web-links">
              <button type="button" onClick={() => void openWebsite(forgotPasswordUrl)}>Mot de passe oublié ?</button>
              <button type="button" onClick={() => void openWebsite(registerUrl)}>S'inscrire</button>
            </div>
            <div className="skl-boot-version">v{appVersion}</div>
          </div>
        </div>
      </div>
    </>
  );
};

export default AuthGate;
