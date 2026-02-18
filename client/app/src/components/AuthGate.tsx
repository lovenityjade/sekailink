import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { apiFetch, apiFetchWithTimeout, apiUrl, API_BASE_URL, getDesktopToken, setDesktopToken } from "../services/api";
import BootScreen from "./BootScreen";
import UpdateNotesModal from "./UpdateNotesModal";
import { useI18n } from "../i18n";

interface AuthGateProps {
  children: React.ReactNode;
}

type AuthStatus = "checking" | "authed" | "unauth" | "unreachable" | "noapi" | "update-required";

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
  const [updateHubOpen, setUpdateHubOpen] = useState(false);
  const [updateNoticeVisible, setUpdateNoticeVisible] = useState(false);
  const [updateCheckError, setUpdateCheckError] = useState("");
  const [lastVersionCheckAt, setLastVersionCheckAt] = useState<number>(0);
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
  const minBootMs = 2200;
  const updateNoticeShownRef = useRef(false);
  const lastSyncedManifestRef = useRef("");
  const lastNotesAttemptRef = useRef("");
  const autoUpdateAttemptRef = useRef("");
  const bootDoneRef = useRef(false);
  const bootLabelRef = useRef("");

  useEffect(() => {
    bootDoneRef.current = bootDone;
    bootLabelRef.current = bootLabel;
  }, [bootDone, bootLabel]);

  const loginUrl = useMemo(() => apiUrl("/api/auth/desktop-login"), []);
  const appVersion = String((runtimeEnv && runtimeEnv.app_version) || __APP_VERSION__ || "0.0.0");
  const clientPlatform = useMemo(() => {
    const platform = String((runtimeEnv && runtimeEnv.platform) || "").toLowerCase();
    if (platform === "win32") return "windows";
    if (platform === "linux") return "linux";
    if (platform === "darwin") return "macos";
    const ua = String(window.navigator?.userAgent || "").toLowerCase();
    if (ua.includes("windows")) return "windows";
    if (ua.includes("linux")) return "linux";
    if (ua.includes("mac os") || ua.includes("macintosh")) return "macos";
    return "";
  }, [runtimeEnv]);

  useEffect(() => {
    let cancelled = false;
    const loadEnv = async () => {
      if (!window.sekailink?.getEnv) return;
      try {
        const env = await window.sekailink.getEnv();
        if (!cancelled && env && typeof env === "object") setRuntimeEnv(env as Record<string, unknown>);
      } catch {
        // ignore env failures
      }
    };
    void loadEnv();
    return () => {
      cancelled = true;
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
    if (!API_BASE_URL) {
      setStatus("noapi");
      return;
    }
    try {
      const res = await apiFetchWithTimeout("/api/me", 6000);
      if (res.ok) {
        setStatus("authed");
        return;
      }
      if (res.status === 401 || res.status === 403) {
        setDesktopToken(null);
        setStatus("unauth");
        return;
      }
      setStatus("unreachable");
    } catch {
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
        setUpdateNoticeVisible(true);
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

  useEffect(() => {
    let cancelled = false;
    const runBoot = async () => {
      const startedAt = Date.now();
      setBootLabel(t("auth.boot_check_version"));
      setBootProgress(0.25);
      const versionState = await checkVersion();
      if (cancelled) return;
      const latest = String(versionState?.info?.latest || "").trim();
      const downloadUrl = String(versionState?.info?.download_url || "").trim();
      const autoMarker = `${latest}|${downloadUrl}|${clientPlatform}`;
      const shouldAutoApply = Boolean((versionState.required || versionState.available) && latest && downloadUrl);
      if (
        shouldAutoApply
        && autoUpdateAttemptRef.current !== autoMarker
        && window.sekailink?.updaterDownloadAndApply
      ) {
        autoUpdateAttemptRef.current = autoMarker;
        setBootLabel(t("auth.boot_updating"));
        setBootProgress(0.45);
        const token = getDesktopToken() || "";
        const applyRes = await window.sekailink.updaterDownloadAndApply({
          downloadUrl,
          version: latest,
          sha256: String(versionState?.info?.sha256 || ""),
          authToken: token,
          timeoutMs: 25 * 60 * 1000,
        });
        if (cancelled) return;
        if (applyRes?.ok) {
          setBootLabel(t("auth.boot_restarting"));
          setBootProgress(1);
          return;
        }
        setUpdateCheckError(t("auth.auto_update_failed", { error: String(applyRes?.error || "unknown_error") }));
      }
      if (versionState.required) {
        setBootLabel(t("auth.update_required"));
        setBootProgress(1);
        setBootDone(true);
        return;
      }
      setBootLabel(t("auth.boot_check_session"));
      setBootProgress(0.6);
      await checkAuth();
      if (cancelled) return;
      const elapsed = Date.now() - startedAt;
      if (elapsed < minBootMs) {
        await new Promise((resolve) => setTimeout(resolve, minBootMs - elapsed));
      }
      setBootLabel(t("auth.boot_launching"));
      setBootProgress(1);
      setTimeout(() => {
        if (!cancelled) setBootDone(true);
      }, 400);
    };
    runBoot();
    return () => {
      cancelled = true;
    };
  }, [checkAuth, checkVersion]);

  useEffect(() => {
    if (status !== "authed") return;
    const timer = window.setInterval(() => {
      void checkVersion();
    }, 15 * 60 * 1000);
    return () => window.clearInterval(timer);
  }, [status, checkVersion]);

  useEffect(() => {
    if (status !== "authed" || !updateAvailable || updateNoticeShownRef.current) return;
    updateNoticeShownRef.current = true;
    setUpdateNoticeVisible(true);
    const timer = window.setTimeout(() => setUpdateNoticeVisible(false), 9000);
    return () => window.clearTimeout(timer);
  }, [status, updateAvailable]);

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

  const openLogin = () => {
    if (window.sekailink?.openExternal) {
      window.sekailink.openExternal(loginUrl);
      return;
    }
    window.open(loginUrl, "_blank", "noopener,noreferrer");
  };

  const openUpdateExternal = useCallback(() => {
    const url = String(versionInfo?.download_url || "").trim();
    if (!url) return;
    if (window.sekailink?.openExternal) {
      window.sekailink.openExternal(url);
      return;
    }
    window.open(url, "_blank", "noopener,noreferrer");
  }, [versionInfo]);

  const startUpdateDownload = useCallback(async () => {
    if (!versionInfo?.download_url) return;
    setUpdateDownload((prev) => ({
      ...prev,
      active: true,
      progress: 0,
      error: "",
      path: "",
    }));
    const token = getDesktopToken() || "";
    if (window.sekailink?.updaterDownloadAndApply) {
      const result = await window.sekailink.updaterDownloadAndApply({
        downloadUrl: versionInfo.download_url,
        version: versionInfo.latest || "",
        sha256: versionInfo.sha256 || "",
        authToken: token,
        timeoutMs: 25 * 60 * 1000,
      });
      if (result?.ok) return;
      setUpdateDownload((prev) => ({
        ...prev,
        active: false,
        error: result?.error || t("auth.update_apply_failed"),
      }));
      return;
    }
    if (window.sekailink?.updaterDownload) {
      const result = await window.sekailink.updaterDownload({
        downloadUrl: versionInfo.download_url,
        version: versionInfo.latest || "",
        sha256: versionInfo.sha256 || "",
        authToken: token,
      });
      if (result?.ok) return;
      setUpdateDownload((prev) => ({
        ...prev,
        active: false,
        error: result?.error || t("auth.download_start_failed"),
      }));
    }
    setUpdateDownload((prev) => ({
      ...prev,
      active: false,
      progress: null,
    }));
    openUpdateExternal();
  }, [openUpdateExternal, versionInfo]);

  const installDownloadedUpdate = useCallback(async () => {
    const downloadedPath = String(updateDownload.path || "").trim();
    if (!downloadedPath) return;
    if (!window.sekailink?.updaterOpenDownloaded) {
      openUpdateExternal();
      return;
    }
    const result = await window.sekailink.updaterOpenDownloaded(downloadedPath);
    if (!result?.ok) {
      setUpdateDownload((prev) => ({
        ...prev,
        error: result?.error || t("auth.install_open_failed"),
      }));
    }
  }, [openUpdateExternal, updateDownload.path]);

  const revealDownloadedUpdate = useCallback(async () => {
    const downloadedPath = String(updateDownload.path || "").trim();
    if (!downloadedPath) return;
    await window.sekailink?.showItemInFolder?.(downloadedPath);
  }, [updateDownload.path]);

  const handleAuthCallback = useCallback(
    async (url: string) => {
      try {
        const parsed = new URL(url);
        if (!parsed.protocol.startsWith("sekailink")) return;
        const code = parsed.searchParams.get("code");
        const state = parsed.searchParams.get("state");
        if (!code || !state) return;
        const res = await apiFetch("/api/auth/desktop-callback", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ code, state }),
        });
        if (!res.ok) {
          setAuthError(t("auth.login_failed_retry"));
          setStatus("unauth");
          return;
        }
        const data = await res.json();
        if (data?.token) {
          setDesktopToken(data.token);
          setAuthError("");
          await checkAuth();
          return;
        }
        setAuthError(t("auth.no_token_retry"));
        setStatus("unauth");
      } catch {
        setAuthError(t("auth.login_failed_retry"));
        setStatus("unauth");
      }
    },
    [checkAuth, t]
  );

  useEffect(() => {
    if (!window.sekailink?.onAuthCallback) return;
    const unsubscribe = window.sekailink.onAuthCallback(handleAuthCallback);
    return () => unsubscribe?.();
  }, [handleAuthCallback]);

  if (!bootDone && status !== "update-required") {
    return <BootScreen status={bootLabel} progress={bootProgress} version={`v${appVersion}`} />;
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
            <button className="skl-btn primary" type="button" onClick={() => void startUpdateDownload()} disabled={!versionInfo?.download_url || updateDownload.active}>
              {updateDownload.active ? t("auth.downloading") : t("auth.download_update")}
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
    const updaterVisible = updateAvailable || Boolean(updateDownload.path) || Boolean(updateDownload.error) || incrementalSync.active || incrementalSync.changed > 0 || !!incrementalSync.error;
    return (
      <>
        {updaterVisible && (
          <div className="skl-update-hub">
            {updateNoticeVisible && (
              <div className="skl-update-toast">
                <strong>{t("auth.update_available")}</strong>
                <span>{t("auth.version_ready", { version: versionInfo?.latest || t("auth.new_release") })}</span>
                <button className="skl-btn ghost" type="button" onClick={() => setUpdateHubOpen(true)}>
                  {t("common.open")}
                </button>
              </div>
            )}
            <button className={`skl-update-fab${updateAvailable ? " has-update" : ""}`} type="button" onClick={() => setUpdateHubOpen((prev) => !prev)}>
              <span className="skl-update-fab-icon">UP</span>
              <span className="skl-update-fab-label">{t("auth.update")}</span>
              {updateAvailable && <span className="skl-update-dot"></span>}
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
                  <button className="skl-btn primary" type="button" onClick={() => void startUpdateDownload()} disabled={!versionInfo?.download_url || updateDownload.active}>
                    {updateDownload.active ? t("auth.downloading") : t("auth.download")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void installDownloadedUpdate()} disabled={!updateDownload.path}>
                    {t("auth.install")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void revealDownloadedUpdate()} disabled={!updateDownload.path}>
                    {t("auth.folder")}
                  </button>
                  <button className="skl-btn ghost" type="button" onClick={() => void checkVersion()}>
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
    <div className="skl-auth-shell">
      <div className="skl-auth-panel">
        <h1 className="skl-app-title">{t("auth.welcome")}</h1>
        {status === "checking" && <p>{t("auth.checking_session")}</p>}
        {status === "noapi" && (
          <p>
            {t("auth.no_api_base_url")} <code>VITE_API_BASE_URL</code> {t("auth.and_rebuild")}
          </p>
        )}
        {status === "unreachable" && (
          <p>{t("auth.unable_reach_server")}</p>
        )}
        {status === "unauth" && (
          <p>{t("auth.login_with_discord_continue")}</p>
        )}
        <div className="skl-auth-actions">
          <button className="skl-btn primary" type="button" onClick={openLogin} disabled={status === "checking" || status === "noapi"}>
            {t("auth.login_with_discord")}
          </button>
          <button
            className="skl-btn ghost"
            type="button"
            onClick={async () => {
              await checkAuth();
              setAuthError((prev) => prev || t("auth.still_not_authenticated"));
            }}
            disabled={status === "checking"}
          >
            {t("auth.ive_logged_in")}
          </button>
        </div>
        {authError && <p className="skl-auth-error">{authError}</p>}
      </div>
    </div>
  );
};

export default AuthGate;
