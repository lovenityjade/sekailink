import React, { useEffect } from "react";
import { useLocation, useNavigate } from "react-router-dom";
import { motion, AnimatePresence } from "framer-motion";
import AboutModal from "@/components/AboutModal";
import BugReportModal from "@/components/BugReportModal";
import ContributeModal from "@/components/ContributeModal";
import SocialDrawer from "@/components/SocialDrawer";
import AnimatedBackground from "@/components/AnimatedBackground";
import SettingsPage from "@/pages/Settings";
import { apiCurrentUser, apiFetch, apiJson, setDesktopToken } from "@/services/api";
import { isSoloModeEnabled, setSoloModeEnabled, SOLO_MODE_EVENT } from "@/services/soloMode";
import { runtime } from "@/services/runtime";
import { emitToast } from "@/services/toast";
import PrototypeSidebar from "./PrototypeSidebar";
import NotificationCenter from "../components/NotificationCenter";
import ToastHost from "../components/ToastHost";

interface MeResponse {
  discord_id?: string;
  user_id?: string;
  username?: string;
  global_name?: string;
  display_name?: string;
  avatar_url?: string;
  presence_status?: string;
}

interface PrototypeShellProps {
  children: React.ReactNode;
}

const TICKER_EVENTS = [
  "Multiworld session in progress...",
  "New worlds available — check Room List",
  "SekaiLink v0.3 — Circuit Forge Edition",
];

function PulsingOrb({ color = "#22c55e", size = 6 }: { color?: string; size?: number }) {
  return (
    <span className="relative inline-flex shrink-0">
      <span
        className="inline-block rounded-full"
        style={{
          width: size,
          height: size,
          backgroundColor: color,
          boxShadow: `0 0 ${size}px ${color}, 0 0 ${size * 2}px ${color}80`,
        }}
      />
      <span
        className="absolute inset-0 rounded-full animate-ping"
        style={{ backgroundColor: color, opacity: 0.3, animationDuration: "1.5s" }}
      />
    </span>
  );
}

const PrototypeShell: React.FC<PrototypeShellProps> = ({ children }) => {
  const navigate = useNavigate();
  const location = useLocation();
  const [aboutOpen, setAboutOpen] = React.useState(false);
  const [contributeOpen, setContributeOpen] = React.useState(false);
  const [friendsOpen, setFriendsOpen] = React.useState(false);
  const [settingsOpen, setSettingsOpen] = React.useState(false);
  const [bugReportOpen, setBugReportOpen] = React.useState(false);
  const [bugReportSubmitting, setBugReportSubmitting] = React.useState(false);
  const [bugReportContext, setBugReportContext] = React.useState({ title: "", description: "" });
  const [headerMenuOpen, setHeaderMenuOpen] = React.useState(false);
  const [logoutConfirmOpen, setLogoutConfirmOpen] = React.useState(false);
  const [me, setMe] = React.useState<MeResponse | null>(null);
  const [soloMode, setSoloMode] = React.useState(() => isSoloModeEnabled());
  const [disconnectModal, setDisconnectModal] = React.useState<{ open: boolean; reason: string }>({ open: false, reason: "" });
  const [tick, setTick] = React.useState(0);

  useEffect(() => {
    document.body.classList.add("sklp-ui");
    const v = (window.localStorage.getItem("sklp_metal_variant") || "c").toLowerCase();
    if (v === "b") document.body.classList.add("sklp-metal-b");
    if (v === "c") document.body.classList.add("sklp-metal-c");
    return () => {
      document.body.classList.remove("sklp-ui");
      document.body.classList.remove("sklp-metal-b", "sklp-metal-c");
    };
  }, []);

  useEffect(() => {
    const openReport = (event: Event) => {
      const detail = (event as CustomEvent<{ title?: string; description?: string }>).detail || {};
      setBugReportContext({ title: String(detail.title || ""), description: String(detail.description || "") });
      setBugReportOpen(true);
    };
    window.addEventListener("sekailink:report-bug", openReport);
    return () => window.removeEventListener("sekailink:report-bug", openReport);
  }, []);

  useEffect(() => {
    const interval = setInterval(() => setTick((t) => t + 1), 5000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    const shouldSkip = (element: HTMLElement | null) => {
      if (!element) return true;
      if (element.matches(":disabled,[aria-disabled='true']")) return true;
      if (element.closest("[role='tab'], .po-mode-btn, .settings-tabs, .settings-tab")) return true;
      if (element.closest(".sklp-topbar-window-controls, .sklp-topbar-menu, .skl-sidebar-link, .skl-sidebar-settings")) return true;
      return false;
    };

    let lastHoverElement: HTMLElement | null = null;

    const onPointerOver = (event: Event) => {
      const target = ((event.target as HTMLElement | null)?.closest("button, a.skl-btn, .yaml-create, .yaml-import") as HTMLElement | null);
      if (!target || target === lastHoverElement || shouldSkip(target)) return;
      lastHoverElement = target;
      window.SKL_SFX?.play?.("hover", 0.08);
    };

    const onPointerOut = (event: Event) => {
      const target = ((event.target as HTMLElement | null)?.closest("button, a.skl-btn, .yaml-create, .yaml-import") as HTMLElement | null);
      if (target && target === lastHoverElement) {
        lastHoverElement = null;
      }
    };

    const onClick = (event: Event) => {
      const target = ((event.target as HTMLElement | null)?.closest("button, a.skl-btn, .yaml-create, .yaml-import") as HTMLElement | null);
      if (!target || shouldSkip(target)) return;
      window.SKL_SFX?.play?.("confirm", 0.08);
    };

    document.addEventListener("pointerover", onPointerOver, true);
    document.addEventListener("pointerout", onPointerOut, true);
    document.addEventListener("click", onClick, true);
    return () => {
      document.removeEventListener("pointerover", onPointerOver, true);
      document.removeEventListener("pointerout", onPointerOut, true);
      document.removeEventListener("click", onClick, true);
    };
  }, []);

  useEffect(() => {
    const syncSoloMode = () => setSoloMode(isSoloModeEnabled());
    window.addEventListener(SOLO_MODE_EVENT, syncSoloMode);
    window.addEventListener("storage", syncSoloMode);
    syncSoloMode();
    return () => {
      window.removeEventListener(SOLO_MODE_EVENT, syncSoloMode);
      window.removeEventListener("storage", syncSoloMode);
    };
  }, []);

  useEffect(() => {
    if (soloMode) {
      setMe(null);
      setDisconnectModal({ open: false, reason: "" });
      return;
    }
    apiCurrentUser()
      .then((data) => setMe(data))
      .catch(() => setMe(null));
  }, [soloMode]);

  useEffect(() => {
    if (soloMode) {
      setDisconnectModal({ open: false, reason: "" });
      return;
    }
    let cancelled = false;
    const checkConnectivity = async () => {
      try {
        const res = await apiFetch("/api/identity/me");
        if (cancelled) return;
        if (res.ok) return;
        const reason =
          res.status === 401
            ? "Session expired or unauthorized."
            : `Server returned HTTP ${res.status}.`;
        setDisconnectModal({ open: true, reason });
      } catch (err) {
        if (cancelled) return;
        setDisconnectModal({
          open: true,
          reason: err instanceof Error ? err.message : "Unable to reach SekaiLink server.",
        });
      }
    };
    const timer = window.setInterval(() => {
      if (disconnectModal.open) return;
      void checkConnectivity();
    }, 10000);
    void checkConnectivity();
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [disconnectModal.open, soloMode]);

  useEffect(() => {
    const onDocClick = () => setHeaderMenuOpen(false);
    const onEsc = (e: KeyboardEvent) => {
      if (e.key === "Escape") setHeaderMenuOpen(false);
    };
    document.addEventListener("click", onDocClick);
    document.addEventListener("keydown", onEsc);
    return () => {
      document.removeEventListener("click", onDocClick);
      document.removeEventListener("keydown", onEsc);
    };
  }, []);

  const submitBugReport = React.useCallback(async ({ title, description, screenshotBase64 }: { title: string; description: string; screenshotBase64?: string }) => {
    setBugReportSubmitting(true);
    setBugReportOpen(false);
    setHeaderMenuOpen(false);
    await new Promise((resolve) => window.setTimeout(resolve, 120));
    try {
      const artifactsResult = await runtime.collectBugReportArtifacts?.({
        includeScreenshot: Boolean(screenshotBase64),
        screenshotBase64,
        maxLogBytes: 700 * 1024,
      });
      const artifacts = artifactsResult?.ok ? artifactsResult.artifacts || {} : {};
      let runtimeVersions: { bizhawk_version?: string; poptracker_version?: string } = {};
      try {
        runtimeVersions = await apiJson<{ bizhawk_version?: string; poptracker_version?: string }>("/api/client/runtime-versions");
      } catch (_err) {
        runtimeVersions = {};
      }
      const appInfo = {
        ...(artifacts.appInfo || {}),
        source: "client-core",
        component: "sekailink-client-core",
        bizhawk_sekailink_version: String(runtimeVersions?.bizhawk_version || ""),
        poptracker_sekailink_version: String(runtimeVersions?.poptracker_version || ""),
      };
      const reporterName = String(me?.display_name || me?.global_name || me?.username || "SekaiLink Client").slice(0, 80);
      const response = await apiFetch("/api/client/bug-report", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          title,
          description,
          reporter_name: reporterName,
          screenshot_base64: artifacts.screenshotBase64 || "",
          logs_text: artifacts.logsText || "",
          bundle_base64: artifacts.bundleBase64 || "",
          bundle_manifest: artifacts.bundleManifest || {},
          system_info: artifacts.systemInfo || {},
          app_info: appInfo,
        }),
      });
      if (!response.ok) {
        const errText = await response.text();
        throw new Error(errText || "bug_report_failed");
      }
      emitToast({ kind: "success", message: "Bug report sent. Thank you." });
    } catch (err) {
      emitToast({ kind: "error", message: err instanceof Error ? err.message : "Unable to send bug report." });
    } finally {
      setBugReportSubmitting(false);
    }
  }, [me]);

  const logout = React.useCallback(async () => {
    setHeaderMenuOpen(false);
    setLogoutConfirmOpen(false);
    try {
      await apiFetch("/api/identity/me/sessions/revoke", { method: "POST" });
    } catch {
      // best effort
    }
    setDesktopToken(null);
    setSoloModeEnabled(false);
    try {
      window.localStorage.removeItem("skl.activeYamlId");
    } catch {
      // ignore storage errors
    }
    window.location.reload();
  }, []);

  return (
    <>
      {/* Circuit Forge animated canvas background */}
      <AnimatedBackground />

      {/* Topbar */}
      <div className="fixed top-0 left-0 right-0 z-50 h-10 flex items-center justify-between px-4 border-b border-teal/10 backdrop-blur-sm" style={{ background: "rgba(13,17,23,0.60)", WebkitAppRegion: "drag" } as React.CSSProperties}>
        <div className="flex items-center gap-3" style={{ WebkitAppRegion: "no-drag" } as React.CSSProperties}>
          {/* Menu button */}
          <div className="relative">
            <button
              type="button"
              className="flex items-center justify-center w-6 h-6 rounded hover:bg-white/5 transition-colors"
              aria-label="Open menu"
              aria-expanded={headerMenuOpen}
              onClick={(e) => { e.stopPropagation(); setHeaderMenuOpen((v) => !v); }}
            >
              <span className="w-1.5 h-1.5 rounded-full bg-teal shadow-[0_0_6px_#00ffc8]" />
            </button>
            {headerMenuOpen && (
              <div className="absolute top-full left-0 mt-1 bg-gunmetal border border-teal/20 panel-chamfer-sm p-1 min-w-[140px] z-50 space-y-0.5" role="menu" onClick={(e) => e.stopPropagation()}>
                {[
                  { label: "Settings", action: () => { setHeaderMenuOpen(false); setSettingsOpen(true); } },
                  { label: "Help", action: () => { setHeaderMenuOpen(false); navigate("/help"); } },
                  { label: "About", action: () => { setHeaderMenuOpen(false); setAboutOpen(true); } },
                  { label: "Contribute", action: () => { setHeaderMenuOpen(false); setContributeOpen(true); } },
                  { label: "Report a Bug", action: () => { setHeaderMenuOpen(false); setBugReportOpen(true); } },
                  { label: "Logout", action: () => { setHeaderMenuOpen(false); setLogoutConfirmOpen(true); }, danger: true },
                ].map((item) => (
                  <button
                    key={item.label}
                    type="button"
                    role="menuitem"
                    className={`w-full text-left px-3 py-1.5 text-xs font-code transition-colors ${
                      item.danger
                        ? "text-red-400/80 hover:bg-red-500/10 hover:text-red-300"
                        : "text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80"
                    }`}
                    onClick={item.action}
                  >
                    {item.label}
                  </button>
                ))}
              </div>
            )}
          </div>

          {/* Logo */}
          <div className="flex items-center gap-2">
            <img className="h-6 w-auto" src="assets/img/sekailink-logo-image.png" alt="" />
            <div className="flex flex-col leading-none">
              <img className="h-3.5 w-auto" src="assets/img/sekailink-logo-text.png" alt="SekaiLink" />
              <span className="text-[8px] font-header text-teal/40 tracking-[0.2em] mt-0.5">MULTIWORLD DESKTOP SYSTEM</span>
            </div>
          </div>
        </div>

        {/* Live ticker */}
        <div className="flex items-center gap-4" style={{ WebkitAppRegion: "no-drag" } as React.CSSProperties}>
          <AnimatePresence mode="wait">
            <motion.span
              key={tick % TICKER_EVENTS.length}
              initial={{ opacity: 0, y: 6 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -6 }}
              className="text-[10px] font-code text-phosphor/30"
            >
              {TICKER_EVENTS[tick % TICKER_EVENTS.length]}
            </motion.span>
          </AnimatePresence>

          <div className="flex items-center gap-1.5">
            <PulsingOrb color={soloMode ? "#00ffc8" : "#22c55e"} size={6} />
            <span className="text-[10px] font-header text-phosphor/40 tracking-wider">{soloMode ? "SOLO LOCAL" : "CONNECTED"}</span>
          </div>

          {/* Window controls — macOS traffic-light style */}
          <div className="sklp-topbar-window-controls flex items-center gap-2 ml-3">
            <button
              type="button"
              className="w-[11px] h-[11px] rounded-full transition-all hover:opacity-80"
              style={{ background: "rgba(0,255,200,0.15)" }}
              aria-label="Minimize"
              onClick={() => (window as any)?.sekailink?.windowMinimize?.()}
            />
            <button
              type="button"
              className="w-[11px] h-[11px] rounded-full transition-all hover:opacity-80"
              style={{ background: "rgba(0,255,200,0.25)" }}
              aria-label="Maximize"
              onClick={() => (window as any)?.sekailink?.windowToggleMaximize?.()}
            />
            <button
              type="button"
              className="w-[11px] h-[11px] rounded-full transition-all hover:opacity-80"
              style={{ background: "rgba(0,255,200,0.35)" }}
              aria-label="Close"
              onClick={() => (window as any)?.sekailink?.windowClose?.()}
            />
          </div>
        </div>
      </div>

      {/* Main layout */}
      <div className="fixed inset-0 pt-10 flex overflow-hidden">
        <PrototypeSidebar
          me={me}
          soloMode={soloMode}
          onOpenFriends={() => setFriendsOpen(true)}
        />
        <main className="flex-1 overflow-hidden relative">
          <div key={location.pathname} className="h-full overflow-y-auto">
            {children}
          </div>
        </main>
      </div>

      {/* Settings Overlay */}
      {settingsOpen && (
        <div className="fixed inset-0 z-[60] flex items-center justify-center" role="dialog" aria-modal="true" aria-label="Client settings">
          <div className="absolute inset-0 bg-black/70 backdrop-blur-sm" onClick={() => setSettingsOpen(false)} />
          <div className="relative w-[700px] max-h-[85vh] overflow-y-auto panel-chamfer bg-gunmetal border border-teal/20 p-6" style={{ boxShadow: "0 0 40px rgba(0,255,200,0.08)" }}>
            <div className="flex items-center justify-between mb-4">
              <span className="font-header text-lg text-teal tracking-widest text-glow">CLIENT SETTINGS</span>
              <button type="button" className="text-phosphor/30 hover:text-teal transition-colors text-xs font-header tracking-wider" onClick={() => setSettingsOpen(false)}>CLOSE</button>
            </div>
            <SettingsPage />
          </div>
        </div>
      )}

      {!soloMode && <SocialDrawer open={friendsOpen} onClose={() => setFriendsOpen(false)} />}
      <AboutModal open={aboutOpen} onClose={() => setAboutOpen(false)} />
      <ContributeModal open={contributeOpen} onClose={() => setContributeOpen(false)} />
      <BugReportModal
        open={bugReportOpen}
        submitting={bugReportSubmitting}
        initialTitle={bugReportContext.title}
        initialDescription={bugReportContext.description}
        onClose={() => setBugReportOpen(false)}
        onSubmit={submitBugReport}
      />

      <div className={`skl-modal${logoutConfirmOpen ? " open" : ""}`} aria-hidden={!logoutConfirmOpen}>
        <div className="skl-modal-backdrop" onClick={() => setLogoutConfirmOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="skl-logout-title">
          <div className="skl-modal-header">
            <h3 id="skl-logout-title">Confirm Logout</h3>
            <button className="skl-modal-close" type="button" onClick={() => setLogoutConfirmOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <div
              className="panel-chamfer-sm"
              style={{
                border: "1px solid rgba(255, 92, 92, 0.45)",
                background: "linear-gradient(180deg, rgba(90, 10, 18, 0.35) 0%, rgba(30, 6, 10, 0.55) 100%)",
                boxShadow: "0 0 16px rgba(255, 92, 92, 0.18), inset 0 0 18px rgba(255, 92, 92, 0.08)",
                padding: "14px 16px",
                marginBottom: "12px",
              }}
            >
              <div
                className="font-header text-sm tracking-[0.18em]"
                style={{ color: "rgba(255, 126, 126, 0.96)", textShadow: "0 0 10px rgba(255, 92, 92, 0.45)" }}
              >
                WARNING
              </div>
              <div className="text-xs font-code mt-2" style={{ color: "rgba(255, 228, 228, 0.82)" }}>
                Logging out will clear the current desktop session and return this client to the authentication gate.
              </div>
            </div>
            <p>Do you want to log out of SekaiLink now?</p>
          </div>
          <div className="skl-modal-actions">
            <button className="skl-btn ghost" type="button" onClick={() => setLogoutConfirmOpen(false)}>
              Cancel
            </button>
            <button
              className="skl-btn"
              type="button"
              onClick={() => { void logout(); }}
              style={{
                borderColor: "rgba(255, 92, 92, 0.45)",
                color: "rgba(255, 126, 126, 0.96)",
                boxShadow: "0 0 14px rgba(255, 92, 92, 0.16)",
              }}
            >
              Logout
            </button>
          </div>
        </div>
      </div>

      {/* Disconnect modal */}
      <div className={`skl-modal${disconnectModal.open ? " open" : ""}`} aria-hidden={!disconnectModal.open}>
        <div className="skl-modal-backdrop"></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="skl-disconnect-title">
          <div className="skl-modal-header">
            <h3 id="skl-disconnect-title">Disconnected from SekaiLink</h3>
          </div>
          <div className="skl-modal-body">
            <p>Connection to SekaiLink server was lost.</p>
            <p><strong>Reason:</strong> {disconnectModal.reason || "Unknown connection error."}</p>
          </div>
          <div className="skl-modal-actions">
            <button
              className="skl-btn ghost"
              type="button"
              onClick={() => (window as any)?.sekailink?.windowClose?.() || window.close()}
            >
              Quit
            </button>
            <button
              className="skl-btn"
              type="button"
              onClick={() => window.location.reload()}
            >
              Reload Session
            </button>
          </div>
        </div>
      </div>
      <NotificationCenter />
      <ToastHost />
    </>
  );
};

export default PrototypeShell;
