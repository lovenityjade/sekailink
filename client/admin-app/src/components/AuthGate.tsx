import React, { useCallback, useEffect, useMemo, useState } from "react";
import { API_BASE_URL, apiUrl, getAdminToken, setAdminToken } from "@/services/api";
import { adminApi, type AdminMe } from "@/services/adminApi";

interface AuthGateProps {
  onAuthed: (me: AdminMe) => void;
  children: React.ReactNode;
}

type AuthState = "checking" | "unauth" | "forbidden" | "unreachable" | "authed";

const AuthGate: React.FC<AuthGateProps> = ({ onAuthed, children }) => {
  const [state, setState] = useState<AuthState>("checking");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const loginUrl = useMemo(() => apiUrl("/api/auth/desktop-login?scheme=sekailink-admin"), []);

  const runAuthCheck = useCallback(async () => {
    if (!API_BASE_URL) {
      setState("unreachable");
      setError("VITE_API_BASE_URL is not configured.");
      return;
    }
    setBusy(true);
    try {
      const me = await adminApi.getMe();
      if (!me?.is_admin) {
        setState("forbidden");
        setError("Your account is not an administrator.");
        return;
      }
      onAuthed(me);
      setState("authed");
      setError("");
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err || "");
      if (message.includes("401")) {
        setState("unauth");
        return;
      }
      if (message.includes("403")) {
        setState("forbidden");
        setError("Administrator permission required.");
        return;
      }
      setState("unreachable");
      setError("Unable to reach admin API.");
    } finally {
      setBusy(false);
    }
  }, [onAuthed]);

  const openLogin = useCallback(() => {
    if (window.sekailinkAdmin?.openExternal) {
      window.sekailinkAdmin.openExternal(loginUrl);
      return;
    }
    window.open(loginUrl, "_blank", "noopener,noreferrer");
  }, [loginUrl]);

  const handleAuthCallback = useCallback(async (url: string) => {
    try {
      const parsed = new URL(url);
      if (!parsed.protocol.startsWith("sekailink-admin")) return;
      const code = parsed.searchParams.get("code") || "";
      const stateParam = parsed.searchParams.get("state") || "";
      if (!code || !stateParam) return;

      const response = await adminApi.desktopCallback(code, stateParam);
      if (!response.ok) {
        setState("unauth");
        setError("Login failed. Try again.");
        return;
      }
      const data = await response.json();
      const token = String(data?.token || "").trim();
      if (!token) {
        setState("unauth");
        setError("Desktop token missing in callback response.");
        return;
      }
      setAdminToken(token);
      await runAuthCheck();
    } catch {
      setState("unauth");
      setError("OAuth callback handling failed.");
    }
  }, [runAuthCheck]);

  useEffect(() => {
    void runAuthCheck();
  }, [runAuthCheck]);

  useEffect(() => {
    if (!window.sekailinkAdmin?.onAuthCallback) return;
    const off = window.sekailinkAdmin.onAuthCallback(handleAuthCallback);
    return () => off?.();
  }, [handleAuthCallback]);

  if (state === "checking") {
    return <div className="auth-screen"><div className="auth-card"><h1>Checking admin session...</h1></div></div>;
  }

  if (state === "authed") {
    return <>{children}</>;
  }

  if (state === "forbidden") {
    return (
      <div className="auth-screen">
        <div className="auth-card">
          <h1>Access denied</h1>
          <p>This account is authenticated but not authorized for admin tools.</p>
          {error && <p className="error-text">{error}</p>}
          <button className="btn ghost" onClick={() => { setAdminToken(null); setState("unauth"); }} type="button">Sign out</button>
        </div>
      </div>
    );
  }

  return (
    <div className="auth-screen">
      <div className="auth-card">
        <h1>SekaiLink Admin</h1>
        <p>Administrator login is required to continue.</p>
        {error && <p className="error-text">{error}</p>}
        <div className="actions-row">
          <button className="btn" onClick={openLogin} type="button" disabled={busy}>Login with Discord</button>
          <button className="btn ghost" onClick={() => { setAdminToken(null); void runAuthCheck(); }} type="button">Retry</button>
          {!!getAdminToken() && <button className="btn ghost" onClick={() => { setAdminToken(null); setState("unauth"); }} type="button">Clear token</button>}
        </div>
      </div>
    </div>
  );
};

export default AuthGate;
