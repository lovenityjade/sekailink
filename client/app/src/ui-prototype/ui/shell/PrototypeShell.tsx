import React, { useEffect } from "react";
import { useNavigate } from "react-router-dom";
import AboutModal from "@/components/AboutModal";
import ContributeModal from "@/components/ContributeModal";
import SocialDrawer from "@/components/SocialDrawer";
import SettingsPage from "@/pages/Settings";
import { apiJson } from "@/services/api";
import PrototypeSidebar from "./PrototypeSidebar";
import ToastHost from "../components/ToastHost";

interface MeResponse {
  discord_id: string;
  username?: string;
  global_name?: string;
  avatar_url?: string;
  presence_status?: string;
}

interface PrototypeShellProps {
  children: React.ReactNode;
}

const PrototypeShell: React.FC<PrototypeShellProps> = ({ children }) => {
  const navigate = useNavigate();
  const [aboutOpen, setAboutOpen] = React.useState(false);
  const [contributeOpen, setContributeOpen] = React.useState(false);
  const [friendsOpen, setFriendsOpen] = React.useState(false);
  const [settingsOpen, setSettingsOpen] = React.useState(false);
  const [headerMenuOpen, setHeaderMenuOpen] = React.useState(false);
  const [me, setMe] = React.useState<MeResponse | null>(null);

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
    apiJson<MeResponse>("/api/me")
      .then((data) => setMe(data))
      .catch(() => setMe(null));
  }, []);

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

  return (
    <>
      <div className="sklp-atmo" />
      <div className="sklp-topbar">
        <div className="sklp-topbar-left">
          <button
            type="button"
            className="sklp-topbar-menu-btn"
            aria-label="Open menu"
            aria-expanded={headerMenuOpen}
            onClick={(e) => {
              e.stopPropagation();
              setHeaderMenuOpen((v) => !v);
            }}
          >
            <span className="sklp-topbar-dot" />
          </button>
          {headerMenuOpen && (
            <div className="sklp-topbar-menu" role="menu" onClick={(e) => e.stopPropagation()}>
              <button type="button" role="menuitem" onClick={() => { setHeaderMenuOpen(false); navigate("/help"); }}>
                Help
              </button>
              <button type="button" role="menuitem" onClick={() => { setHeaderMenuOpen(false); setAboutOpen(true); }}>
                About
              </button>
              <button type="button" role="menuitem" onClick={() => { setHeaderMenuOpen(false); setContributeOpen(true); }}>
                Contribute
              </button>
            </div>
          )}
        </div>
        <div className="sklp-topbar-title">SEKAILINK</div>
        <div className="sklp-topbar-right">
          <div className="sklp-topbar-window-controls" aria-label="Window controls">
            <button
              type="button"
              className="sklp-win-btn min"
              aria-label="Minimize"
              onClick={() => (window as any)?.sekailink?.windowMinimize?.()}
            />
            <button
              type="button"
              className="sklp-win-btn max"
              aria-label="Maximize"
              onClick={() => (window as any)?.sekailink?.windowToggleMaximize?.()}
            />
            <button
              type="button"
              className="sklp-win-btn close"
              aria-label="Close"
              onClick={() => (window as any)?.sekailink?.windowClose?.()}
            />
          </div>
        </div>
      </div>
      <div className="skl-app-shell">
        <PrototypeSidebar
          me={me}
          onOpenFriends={() => setFriendsOpen(true)}
          onOpenSettings={() => setSettingsOpen(true)}
        />
        <div className="skl-app-main">{children}</div>
      </div>
      {settingsOpen && (
        <div className="sklp-settings-overlay" role="dialog" aria-modal="true" aria-label="Client settings">
          <div className="sklp-settings-modal">
            <div className="sklp-settings-modal-head">
              <div className="sklp-title">Client Settings</div>
              <button
                type="button"
                className="skl-btn"
                onClick={() => setSettingsOpen(false)}
              >
                Close
              </button>
            </div>
            <div className="sklp-settings-modal-body">
              <SettingsPage />
            </div>
          </div>
        </div>
      )}
      <SocialDrawer open={friendsOpen} onClose={() => setFriendsOpen(false)} />
      <AboutModal open={aboutOpen} onClose={() => setAboutOpen(false)} />
      <ContributeModal open={contributeOpen} onClose={() => setContributeOpen(false)} />
      <ToastHost />
    </>
  );
};

export default PrototypeShell;
