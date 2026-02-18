import React, { useCallback, useEffect, useMemo, useState } from "react";
import BootScreen from "@/components/BootScreen";
import { apiFetchWithTimeout, API_BASE_URL, getDesktopToken, setDesktopToken } from "@/services/api";
import { useI18n } from "@/i18n";
import InteractiveTutorial from "../components/InteractiveTutorial";

interface AuthGatePrototypeProps {
  children: React.ReactNode;
}

type AuthStatus = "checking" | "authed" | "unauth" | "unreachable" | "noapi";

const AuthGatePrototype: React.FC<AuthGatePrototypeProps> = ({ children }) => {
  const { t } = useI18n();
  const [status, setStatus] = useState<AuthStatus>("checking");
  const [bootLabel, setBootLabel] = useState(t("boot.status_default"));
  const [bootProgress, setBootProgress] = useState(0.12);
  const [bootDone, setBootDone] = useState(false);
  const [authError, setAuthError] = useState("");
  const [copyStatus, setCopyStatus] = useState("");
  const [tutorialOpen, setTutorialOpen] = useState(false);
  const [tutorialSaving, setTutorialSaving] = useState(false);

  const appVersion = String((globalThis as any).__APP_VERSION__ || "");

  const clientPlatform = useMemo(() => {
    const p = (globalThis as any)?.sekailink?.getEnv ? null : null;
    // Keep the same behavior as main app: server picks platform based on request; prototype does not enforce.
    return p;
  }, []);

  const checkAuth = useCallback(async () => {
    if (!API_BASE_URL) {
      setStatus("noapi");
      return;
    }
    setStatus("checking");
    setAuthError("");
    try {
      const token = getDesktopToken() || "";
      const res = await apiFetchWithTimeout("/api/me", 6000, token ? { headers: { Authorization: `Bearer ${token}` } } : undefined);
      if (res.ok) {
        try {
          const me = await res.json();
          const shouldShowTutorial = Boolean(me?.tutorial);
          setTutorialOpen(shouldShowTutorial);
        } catch {
          setTutorialOpen(false);
        }
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

  const markTutorialDone = useCallback(async () => {
    setTutorialSaving(true);
    try {
      await apiFetchWithTimeout("/api/me/tutorial", 6000, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ tutorial: false }),
      });
    } catch {
      // Keep non-blocking: tutorial should not lock app flow if endpoint fails.
    } finally {
      setTutorialSaving(false);
      setTutorialOpen(false);
    }
  }, []);

  useEffect(() => {
    // Disable automatic updates for this prototype:
    // - No version polling
    // - No incremental sync
    void checkAuth();
  }, [checkAuth, appVersion, clientPlatform]);

  useEffect(() => {
    if (bootDone) return;
    const timer = window.setInterval(() => {
      setBootProgress((prev) => Math.min(0.98, prev + 0.05));
    }, 120);
    const labelTimer = window.setInterval(() => {
      setBootLabel((prev) => (prev === t("boot.status_default") ? t("proto.boot.linking_relays") : t("boot.status_default")));
    }, 900);
    const done = window.setTimeout(() => {
      setBootDone(true);
      window.clearInterval(timer);
      window.clearInterval(labelTimer);
    }, 2300);
    return () => {
      window.clearInterval(timer);
      window.clearInterval(labelTimer);
      window.clearTimeout(done);
    };
  }, [bootDone, t]);

  useEffect(() => {
    if (!bootDone || status === "checking") return;
    const api = (window as any).SKL_SFX;
    api?.startBgm?.();
  }, [bootDone, status]);

  if (!bootDone || status === "checking") {
    return <BootScreen status={bootLabel} progress={bootProgress} version={appVersion} />;
  }

  const showServiceError = status === "noapi" || status === "unreachable";
  const errorCode = status === "noapi" ? "E_PROTO_API_BASE_MISSING" : "E_PROTO_SERVICE_UNREACHABLE";
  const errorDescription = status === "noapi"
    ? t("proto.error.api_base_missing")
    : t("proto.error.unable_reach_service");
  const fullError = [
    t("proto.error.title"),
    `${t("proto.error.code")}: ${errorCode}`,
    `${t("proto.error.description")}: ${errorDescription}`,
    authError ? `${t("proto.error.detail")}: ${authError}` : "",
  ].filter(Boolean).join("\n");

  const copyError = async () => {
    try {
      await navigator.clipboard.writeText(fullError);
      setCopyStatus(t("proto.error.copied"));
      window.setTimeout(() => setCopyStatus(""), 1800);
    } catch {
      setCopyStatus(t("proto.error.copy_failed"));
      window.setTimeout(() => setCopyStatus(""), 1800);
    }
  };

  const quitApp = () => {
    const api = (window as any)?.sekailink;
    if (api?.windowClose) {
      void api.windowClose();
      return;
    }
    window.close();
  };

  // In prototype mode we keep existing auth behavior (login gate) minimal;
  // actual client login flows remain in Account page.
  return (
    <>
      {children}
      {showServiceError ? (
        <div className="sklp-error-overlay" role="dialog" aria-modal="true" aria-labelledby="sklp-service-error-title">
          <div className="sklp-error-modal">
            <h3 id="sklp-service-error-title">{t("proto.error.title")}</h3>
            <div className="sklp-error-row">
              <span className="sklp-error-label">{t("proto.error.code")}</span>
              <span className="sklp-error-value">{errorCode}</span>
            </div>
            <div className="sklp-error-row">
              <span className="sklp-error-label">{t("proto.error.description")}</span>
              <span className="sklp-error-value">{errorDescription}</span>
            </div>
            {authError ? (
              <div className="sklp-error-row">
                <span className="sklp-error-label">{t("proto.error.detail")}</span>
                <span className="sklp-error-value">{authError}</span>
              </div>
            ) : null}
            <div className="sklp-error-actions">
              <button type="button" className="skl-btn" onClick={copyError}>{t("proto.error.copy_error")}</button>
              <button type="button" className="skl-btn primary" onClick={quitApp}>{t("proto.error.quit")}</button>
            </div>
            {copyStatus ? <div className="sklp-error-copy-status">{copyStatus}</div> : null}
          </div>
        </div>
      ) : null}
      <InteractiveTutorial
        open={status === "authed" && !showServiceError && tutorialOpen}
        onSkip={markTutorialDone}
        onComplete={markTutorialDone}
        busy={tutorialSaving}
      />
    </>
  );
};

export default AuthGatePrototype;
