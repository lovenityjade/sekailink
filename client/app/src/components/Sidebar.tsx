import React, { useEffect, useState } from "react";
import { NavLink } from "react-router-dom";
import { useSfx } from "../hooks/useSfx";
import { useI18n } from "../i18n";
import { apiFetch } from "../services/api";
import ProfileSettingsModal from "./ProfileSettingsModal";

interface SidebarProps {
  me?: {
    discord_id: string;
    username?: string;
    global_name?: string;
    avatar_url?: string;
    presence_status?: string;
  } | null;
  onOpenAbout: () => void;
  onOpenContribute: () => void;
  onOpenFriends: () => void;
}

const Sidebar: React.FC<SidebarProps> = ({ me, onOpenAbout, onOpenContribute, onOpenFriends }) => {
  const [collapsed, setCollapsed] = useState(false);
  const [profileMenuOpen, setProfileMenuOpen] = useState(false);
  const [profileModalOpen, setProfileModalOpen] = useState(false);
  const [presenceStatus, setPresenceStatus] = useState<string>(me?.presence_status || "online");
  const sfx = useSfx();
  const { t } = useI18n();

  useEffect(() => {
    const stored = window.localStorage.getItem("skl_sidebar_collapsed");
    if (stored === "1") setCollapsed(true);
  }, []);

  useEffect(() => {
    setPresenceStatus(me?.presence_status || "online");
  }, [me?.presence_status]);

  useEffect(() => {
    const closeMenu = () => setProfileMenuOpen(false);
    document.addEventListener("click", closeMenu);
    return () => document.removeEventListener("click", closeMenu);
  }, []);

  const savePresence = async (nextPresence: string) => {
    setPresenceStatus(nextPresence);
    setProfileMenuOpen(false);
    try {
      await apiFetch("/api/social/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ presence_status: nextPresence, dm_policy: "friends" }),
      });
    } catch {
      // keep local state for responsiveness
    }
  };

  const presenceLabel =
    presenceStatus === "dnd"
      ? t("social.status.dnd")
      : presenceStatus === "offline"
        ? t("profile.menu.status_appear_offline")
        : t("social.status.online");

  const toggleCollapsed = () => {
    const next = !collapsed;
    setCollapsed(next);
    window.localStorage.setItem("skl_sidebar_collapsed", next ? "1" : "0");
    sfx.play("confirm", 0.18);
  };

  return (
    <aside className={`skl-sidebar${collapsed ? " collapsed" : ""}`}>
      <nav className="skl-sidebar-nav">
        <button className="skl-sidebar-link skl-sidebar-toggle" type="button" onClick={toggleCollapsed}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="15 18 9 12 15 6" />
            <line x1="19" y1="6" x2="19" y2="18" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.home")}</span>
        </button>

        <NavLink className={({ isActive }) => `skl-sidebar-link${isActive ? " active" : ""}`} to="/rooms" onClick={() => sfx.play("confirm", 0.18)}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <rect x="2" y="3" width="20" height="14" rx="2" ry="2" />
            <line x1="8" y1="21" x2="16" y2="21" />
            <line x1="12" y1="17" x2="12" y2="21" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.room_list")}</span>
        </NavLink>

        <NavLink className={({ isActive }) => `skl-sidebar-link${isActive ? " active" : ""}`} to="/dashboard/yaml/new" onClick={() => sfx.play("confirm", 0.18)}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M14.5 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V7.5L14.5 2z" />
            <polyline points="14 2 14 8 20 8" />
            <line x1="12" y1="18" x2="12" y2="12" />
            <line x1="9" y1="15" x2="15" y2="15" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.game_manager")}</span>
        </NavLink>

        <button className="skl-sidebar-link" type="button" onClick={() => { sfx.play("confirm", 0.18); onOpenFriends(); }}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M17 21v-2a4 4 0 00-4-4H5a4 4 0 00-4 4v2" />
            <circle cx="9" cy="7" r="4" />
            <path d="M23 21v-2a4 4 0 00-3-3.87" />
            <path d="M16 3.13a4 4 0 010 7.75" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.friends")}</span>
          <span className="skl-friends-count" id="skl-friends-count">0/0</span>
          <span className="skl-unread-badge" id="skl-friends-badge"></span>
        </button>

        <NavLink className={({ isActive }) => `skl-sidebar-link${isActive ? " active" : ""}`} to="/help" onClick={() => sfx.play("confirm", 0.18)}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="10" />
            <path d="M9.09 9a3 3 0 015.83 1c0 2-3 3-3 3" />
            <line x1="12" y1="17" x2="12.01" y2="17" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.help")}</span>
        </NavLink>

        <button className="skl-sidebar-link" type="button" onClick={() => { sfx.play("confirm", 0.18); onOpenAbout(); }}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="10" x2="12" y2="16" />
            <circle cx="12" cy="7" r="1" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.about")}</span>
        </button>
        <button className="skl-sidebar-link" type="button" onClick={() => { sfx.play("confirm", 0.18); onOpenContribute(); }}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M20.8 9.4a4.5 4.5 0 00-6.4 0L12 11.8 9.6 9.4a4.5 4.5 0 00-6.4 6.4l2.4 2.4L12 22l6.4-3.8 2.4-2.4a4.5 4.5 0 000-6.4z" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.contribute")}</span>
        </button>
      </nav>

      <div className="skl-sidebar-divider"></div>
      <button
        className="skl-mute-toggle"
        id="skl-mute-btn"
        type="button"
        onClick={() => {
          const wasMuted = sfx.muted;
          sfx.toggleMuted();
          // Only play a sound when turning sounds back on.
          if (wasMuted) sfx.play("confirm", 0.18);
        }}
        aria-label={t("audio.sounds_on")}
      >
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
          <polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5" />
          <path d="M19.07 4.93a10 10 0 010 14.14M15.54 8.46a5 5 0 010 7.07" />
        </svg>
        <span className="skl-sidebar-label" id="skl-mute-label">
          {sfx.muted ? t("audio.sounds_off") : t("audio.sounds_on")}
        </span>
      </button>

      <div className="skl-sidebar-profile">
        <button
          className="skl-sidebar-profile-link skl-sidebar-profile-button"
          type="button"
          onClick={(e) => {
            e.stopPropagation();
            sfx.play("confirm", 0.18);
            setProfileMenuOpen((v) => !v);
          }}
        >
          <div className="skl-sidebar-avatar">
            {me?.avatar_url ? (
              <img src={me.avatar_url} alt="User avatar" />
            ) : (
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <circle cx="12" cy="8" r="4" />
                <path d="M4 20a8 8 0 0116 0" />
              </svg>
            )}
          </div>
          <div className="skl-sidebar-profile-text">
            <div className="skl-sidebar-profile-name">
              {me?.global_name || me?.username || "Dashboard"}
            </div>
            <div className={`skl-sidebar-profile-status${presenceStatus === "offline" ? " offline" : ""}`} id="skl-user-status">
              <span
                className={`skl-status-dot${presenceStatus === "offline" ? " offline" : ""}`}
                id="skl-user-status-dot"
              ></span>
              <span id="skl-user-status-text">{presenceLabel}</span>
            </div>
          </div>
          {profileMenuOpen && (
            <div className="sklp-status-menu" onClick={(e) => e.stopPropagation()}>
              <button
                type="button"
                onClick={() => {
                  setProfileMenuOpen(false);
                  setProfileModalOpen(true);
                }}
              >
                {t("profile.menu.profile_settings")}
              </button>
              <button type="button" onClick={() => savePresence("online")}>{t("profile.menu.status_online")}</button>
              <button type="button" onClick={() => savePresence("dnd")}>{t("profile.menu.status_dnd")}</button>
              <button type="button" onClick={() => savePresence("offline")}>{t("profile.menu.status_appear_offline")}</button>
            </div>
          )}
        </button>
        <NavLink className="skl-sidebar-settings" to="/settings" aria-label="User settings" onClick={() => sfx.play("confirm", 0.18)}>
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <circle cx="12" cy="12" r="3" />
            <path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06A1.65 1.65 0 004.68 15a1.65 1.65 0 00-1.51-1H3a2 2 0 010-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 012.83-2.83l.06.06A1.65 1.65 0 009 4.68a1.65 1.65 0 001-1.51V3a2 2 0 014 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 2.83l-.06.06A1.65 1.65 0 0019.4 9a1.65 1.65 0 001.51 1H21a2 2 0 010 4h-.09a1.65 1.65 0 00-1.51 1z" />
          </svg>
        </NavLink>
      </div>
      <ProfileSettingsModal open={profileModalOpen} onClose={() => setProfileModalOpen(false)} me={me || null} />
    </aside>
  );
};

export default Sidebar;
