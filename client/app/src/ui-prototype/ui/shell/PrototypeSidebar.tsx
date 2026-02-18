import React, { useEffect, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import { useSfx } from "@/hooks/useSfx";
import { apiFetch, apiJson } from "@/services/api";
import { useI18n } from "@/i18n";
import ProfileSettingsModal from "@/components/ProfileSettingsModal";

interface SidebarProps {
  me?: {
    discord_id: string;
    username?: string;
    global_name?: string;
    avatar_url?: string;
    presence_status?: string;
  } | null;
  onOpenFriends: () => void;
  onOpenSettings: () => void;
}

type Friend = {
  user_id: string;
  display_name: string;
  avatar_url?: string;
  status?: string;
  is_online?: boolean;
};

const PrototypeSidebar: React.FC<SidebarProps> = ({ me, onOpenFriends, onOpenSettings }) => {
  const sfx = useSfx();
  const { t } = useI18n();
  const navigate = useNavigate();
  const [friends, setFriends] = useState<Friend[]>([]);
  const [unreadCount, setUnreadCount] = useState(0);
  const [onlineOpen, setOnlineOpen] = useState(true);
  const [offlineOpen, setOfflineOpen] = useState(true);
  const [statusMenuOpen, setStatusMenuOpen] = useState(false);
  const [profileModalOpen, setProfileModalOpen] = useState(false);
  const [presenceStatus, setPresenceStatus] = useState<string>(me?.presence_status || "online");
  const [friendMenu, setFriendMenu] = useState<{ x: number; y: number; friend: Friend } | null>(null);

  useEffect(() => {
    const closeMenus = () => {
      setStatusMenuOpen(false);
      setFriendMenu(null);
    };
    document.addEventListener("click", closeMenus);
    return () => document.removeEventListener("click", closeMenus);
  }, []);

  useEffect(() => {
    setPresenceStatus(me?.presence_status || "online");
  }, [me?.presence_status]);

  useEffect(() => {
    let cancelled = false;
    const load = async () => {
      try {
        const [friendsRes, unreadRes, settingsRes] = await Promise.all([
          apiFetch("/api/social/friends"),
          apiFetch("/api/social/unread-count"),
          apiFetch("/api/social/settings"),
        ]);
        if (!cancelled && friendsRes.ok) {
          const data = await friendsRes.json();
          setFriends(Array.isArray(data.friends) ? data.friends : []);
        }
        if (!cancelled && unreadRes.ok) {
          const data = await unreadRes.json();
          setUnreadCount(Number(data.unread || 0));
        }
        if (!cancelled && settingsRes.ok) await settingsRes.json();
      } catch {
        if (!cancelled) {
          setFriends([]);
          setUnreadCount(0);
        }
      }
    };
    load();
    const id = window.setInterval(load, 15000);
    return () => {
      cancelled = true;
      window.clearInterval(id);
    };
  }, []);

  const goHome = () => {
    sfx.play("confirm", 0.18);
    navigate("/");
  };

  const onlineFriends = useMemo(
    () => friends.filter((f) => (f.status || "").toLowerCase() === "online"),
    [friends]
  );
  const offlineFriends = useMemo(
    () => friends.filter((f) => (f.status || "").toLowerCase() !== "online"),
    [friends]
  );

  const savePresence = async (nextPresence: string) => {
    setPresenceStatus(nextPresence);
    setStatusMenuOpen(false);
    try {
      await apiFetch("/api/social/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ presence_status: nextPresence, dm_policy: "friends" }),
      });
    } catch {
      // silent in prototype
    }
  };

  const presenceLabel =
    presenceStatus === "dnd"
      ? t("social.status.dnd")
      : presenceStatus === "offline"
        ? t("profile.menu.status_appear_offline")
        : t("social.status.online");

  const removeFriend = async (friend: Friend) => {
    try {
      await apiFetch("/api/social/friends/remove", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: friend.user_id }),
      });
      const updated = await apiJson<{ friends: Friend[] }>("/api/social/friends");
      setFriends(Array.isArray(updated.friends) ? updated.friends : []);
      sfx.play("success", 0.2);
    } catch {
      sfx.play("error", 0.2);
    }
  };

  return (
    <aside className="skl-sidebar">
      <nav className="skl-sidebar-nav">
        <button className="skl-sidebar-link skl-sidebar-toggle" type="button" data-tutorial="home-button" onClick={goHome}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3.5 10.5L12 3.5l8.5 7" />
            <path d="M6 10v10h12V10" />
            <path d="M10 20v-5h4v5" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.home")}</span>
        </button>

        <div className="skl-sidebar-link skl-sidebar-link-static" aria-hidden="true">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M17 21v-2a4 4 0 00-4-4H5a4 4 0 00-4 4v2" />
            <circle cx="9" cy="7" r="4" />
            <path d="M23 21v-2a4 4 0 00-3-3.87" />
            <path d="M16 3.13a4 4 0 010 7.75" />
          </svg>
          <span className="skl-sidebar-label">{t("nav.friends")}</span>
          <span className="skl-friends-count" id="skl-friends-count">{onlineFriends.length}/{friends.length}</span>
          <span className={`skl-unread-badge${unreadCount > 0 ? " active" : ""}`} id="skl-friends-badge">
            {unreadCount > 0 ? (unreadCount > 99 ? "99+" : String(unreadCount)) : ""}
          </span>
        </div>

        <div className="sklp-friends-block">
          <button
            type="button"
            className="sklp-friends-group-head"
            onClick={() => setOnlineOpen((v) => !v)}
          >
            <span>{t("social.status.online")} ({onlineFriends.length})</span>
            <span>{onlineOpen ? "−" : "+"}</span>
          </button>
          {onlineOpen && (
            <div className="sklp-friends-list">
              {onlineFriends.map((friend) => (
                <button
                  key={`on-${friend.user_id}`}
                  type="button"
                  className="sklp-friend-row"
                  onContextMenu={(e) => {
                    e.preventDefault();
                    setFriendMenu({ x: e.clientX, y: e.clientY, friend });
                  }}
                  onClick={() => onOpenFriends()}
                >
                  <span className="sklp-friend-avatar">{friend.avatar_url ? <img src={friend.avatar_url} alt="" /> : null}</span>
                  <span className="sklp-friend-name" title={friend.display_name}>{friend.display_name}</span>
                  <span className="sklp-friend-status-dot online"></span>
                  <span className="sklp-friend-card">
                    <span className="sklp-friend-card-name">{friend.display_name}</span>
                    <span className="sklp-friend-card-status">{t("status.online")}</span>
                  </span>
                </button>
              ))}
            </div>
          )}

          <button
            type="button"
            className="sklp-friends-group-head"
            onClick={() => setOfflineOpen((v) => !v)}
          >
            <span>{t("social.status.offline")} ({offlineFriends.length})</span>
            <span>{offlineOpen ? "−" : "+"}</span>
          </button>
          {offlineOpen && (
            <div className="sklp-friends-list">
              {offlineFriends.map((friend) => (
                <button
                  key={`off-${friend.user_id}`}
                  type="button"
                  className="sklp-friend-row"
                  onContextMenu={(e) => {
                    e.preventDefault();
                    setFriendMenu({ x: e.clientX, y: e.clientY, friend });
                  }}
                  onClick={() => onOpenFriends()}
                >
                  <span className="sklp-friend-avatar">{friend.avatar_url ? <img src={friend.avatar_url} alt="" /> : null}</span>
                  <span className="sklp-friend-name" title={friend.display_name}>{friend.display_name}</span>
                  <span className="sklp-friend-status-dot offline"></span>
                    <span className="sklp-friend-card">
                      <span className="sklp-friend-card-name">{friend.display_name}</span>
                      <span className="sklp-friend-card-status">{t("social.status.offline")}</span>
                    </span>
                  </button>
                ))}
            </div>
          )}
        </div>
      </nav>

      <div className="skl-sidebar-profile">
        <div className="skl-sidebar-profile-link">
          <div className="skl-sidebar-avatar">
            {me?.avatar_url ? (
              <img src={me.avatar_url} alt={t("status.online")} />
            ) : (
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <circle cx="12" cy="8" r="4" />
                <path d="M4 20a8 8 0 0116 0" />
              </svg>
            )}
          </div>
          <div className="skl-sidebar-profile-text">
            <button
              type="button"
              className="sklp-profile-name-btn"
              onClick={(e) => {
                e.stopPropagation();
                setStatusMenuOpen((v) => !v);
              }}
            >
              {me?.global_name || me?.username || t("account.dashboard")}
            </button>
            <div className={`skl-sidebar-profile-status${presenceStatus === "offline" ? " offline" : ""}`} id="skl-user-status">
              <span
                className={`skl-status-dot${presenceStatus === "offline" ? " offline" : ""}`}
                id="skl-user-status-dot"
              ></span>
              <span id="skl-user-status-text">{presenceLabel}</span>
            </div>
            {statusMenuOpen && (
              <div className="sklp-status-menu">
                <button
                  type="button"
                  onClick={() => {
                    setStatusMenuOpen(false);
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
          </div>
        </div>
        <button
          className="skl-sidebar-settings"
          type="button"
          aria-label={t("settings.title")}
          onClick={() => {
            sfx.play("confirm", 0.18);
            onOpenSettings();
          }}
        >
          <svg viewBox="0 0 24 24" aria-hidden="true">
            <circle cx="12" cy="12" r="3" />
            <path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06A1.65 1.65 0 004.68 15a1.65 1.65 0 00-1.51-1H3a2 2 0 010-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 012.83-2.83l.06.06A1.65 1.65 0 009 4.68a1.65 1.65 0 001-1.51V3a2 2 0 014 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 2.83l-.06.06A1.65 1.65 0 0019.4 9a1.65 1.65 0 001.51 1H21a2 2 0 010 4h-.09a1.65 1.65 0 00-1.51 1z" />
          </svg>
        </button>
      </div>
      {friendMenu && (
        <div className="sklp-context-overlay">
          <div className="sklp-context-menu" style={{ left: friendMenu.x, top: friendMenu.y }}>
            <button type="button" onClick={() => { setFriendMenu(null); onOpenFriends(); }}>{t("social.message")}</button>
            <button type="button" onClick={() => { const f = friendMenu.friend; setFriendMenu(null); void removeFriend(f); }}>{t("social.remove")}</button>
          </div>
        </div>
      )}
      <ProfileSettingsModal open={profileModalOpen} onClose={() => setProfileModalOpen(false)} me={me || null} />
    </aside>
  );
};

export default PrototypeSidebar;
