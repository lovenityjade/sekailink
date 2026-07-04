import React, { useEffect, useMemo, useState } from "react";
import { useNavigate, useLocation } from "react-router-dom";
import { motion } from "framer-motion";
import { useSfx } from "@/hooks/useSfx";
import { apiFetch, apiJson } from "@/services/api";
import { emitToast } from "@/services/toast";
import { useI18n } from "@/i18n";
import ProfileSettingsModal from "@/components/ProfileSettingsModal";
import { HorizontalTrace } from "@/components/CircuitConnector";

interface SidebarProps {
  me?: {
    discord_id?: string;
    user_id?: string;
    username?: string;
    global_name?: string;
    display_name?: string;
    avatar_url?: string;
    presence_status?: string;
  } | null;
  soloMode?: boolean;
  onOpenFriends: () => void;
}

type Friend = {
  user_id: string;
  display_name: string;
  avatar_url?: string;
  status?: string;
  is_online?: boolean;
};

function PulsingOrb({ color = "#22c55e", size = 6 }: { color?: string; size?: number }) {
  return (
    <span className="relative inline-flex shrink-0">
      <span className="inline-block rounded-full" style={{ width: size, height: size, backgroundColor: color, boxShadow: `0 0 ${size}px ${color}, 0 0 ${size * 2}px ${color}80` }} />
      <span className="absolute inset-0 rounded-full animate-ping" style={{ backgroundColor: color, opacity: 0.3, animationDuration: "1.5s" }} />
    </span>
  );
}

function NavBtn({ icon, label, active, badge, onClick }: { icon: React.ReactNode; label: string; active?: boolean; badge?: number; onClick?: () => void }) {
  return (
    <button
      onClick={onClick}
      className={`relative flex items-center gap-5 w-full px-5 py-5 text-2xl font-header tracking-wider transition-all group border-none bg-transparent shadow-none ${active ? "text-teal text-glow" : "text-phosphor/40 hover:text-phosphor/80"}`}
    >
      {icon}
      <span>{label}</span>
      {badge != null && badge > 0 && <span className="ml-auto text-[10px] bg-magenta/20 text-magenta px-1.5 py-0.5 font-display">{badge}</span>}
      {active && <motion.div layoutId="nav-active-sidebar" className="absolute left-0 top-0 bottom-0 w-[2px] bg-teal shadow-[0_0_10px_#00ffc8]" />}
    </button>
  );
}

const NAV_ICON_HOME = (
  <svg width="30" height="30" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M3.5 10.5L12 3.5l8.5 7" /><path d="M6 10v10h12V10" /><path d="M10 20v-5h4v5" />
  </svg>
);
const NAV_ICON_WORLDS = (
  <svg width="30" height="30" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M4 6.5h16" /><path d="M6.5 4h11l2.5 2.5-2.5 2.5h-11L4 6.5 6.5 4Z" />
    <path d="M7 12h10" /><path d="M9 10h6l2 2-2 2H9l-2-2 2-2Z" />
    <path d="M5 18h14" /><path d="M8 16h8l3 2-3 2H8l-3-2 3-2Z" />
  </svg>
);
const PrototypeSidebar: React.FC<SidebarProps> = ({ me, soloMode = false, onOpenFriends }) => {
  const sfx = useSfx();
  const { t } = useI18n();
  const navigate = useNavigate();
  const location = useLocation();
  const [friends, setFriends] = useState<Friend[]>([]);
  const [onlineOpen, setOnlineOpen] = useState(true);
  const [offlineOpen, setOfflineOpen] = useState(true);
  const [requestsOpen, setRequestsOpen] = useState(true);
  const [incomingRequests, setIncomingRequests] = useState<Array<{ id: number; from_id?: string; from_name?: string }>>([]);
  const [outgoingRequests, setOutgoingRequests] = useState<Array<{ id: number; to_id?: string; to_name?: string }>>([]);
  const [addOpen, setAddOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [searchResults, setSearchResults] = useState<Array<{ user_id: string; display_name: string; avatar_url?: string }>>([]);
  const [searchBusy, setSearchBusy] = useState(false);
  const [statusMenuOpen, setStatusMenuOpen] = useState(false);
  const [profileModalOpen, setProfileModalOpen] = useState(false);
  const [presenceStatus, setPresenceStatus] = useState<string>(me?.presence_status || "online");
  const [friendMenu, setFriendMenu] = useState<{ x: number; y: number; friend: Friend } | null>(null);

  useEffect(() => {
    const closeMenus = () => { setStatusMenuOpen(false); setFriendMenu(null); };
    document.addEventListener("click", closeMenus);
    return () => document.removeEventListener("click", closeMenus);
  }, []);

  useEffect(() => { setPresenceStatus(me?.presence_status || "online"); }, [me?.presence_status]);

  useEffect(() => {
    if (soloMode) {
      setFriends([]);
      setIncomingRequests([]);
      setOutgoingRequests([]);
      return;
    }
    let cancelled = false;
    const load = async () => {
      try {
        const [friendsRes, settingsRes, requestsRes] = await Promise.all([
          apiFetch("/api/social/friends"),
          apiFetch("/api/social/settings"),
          apiFetch("/api/social/requests"),
        ]);
        if (!cancelled && friendsRes.ok) {
          const data = await friendsRes.json();
          setFriends(Array.isArray(data.friends) ? data.friends : []);
        }
        if (!cancelled && settingsRes.ok) await settingsRes.json();
        if (!cancelled && requestsRes.ok) {
          const data = await requestsRes.json();
          setIncomingRequests(Array.isArray(data.incoming) ? data.incoming : []);
          setOutgoingRequests(Array.isArray(data.outgoing) ? data.outgoing : []);
        }
      } catch {
        if (!cancelled) { setFriends([]); setIncomingRequests([]); setOutgoingRequests([]); }
      }
    };
    load();
    const id = window.setInterval(load, 15000);
    return () => { cancelled = true; window.clearInterval(id); };
  }, [soloMode]);

  useEffect(() => {
    if (soloMode) {
      setSearchBusy(false);
      setSearchResults([]);
      return;
    }
    if (!addOpen) return;
    const query = searchQuery.trim();
    if (query.length < 2) { setSearchResults([]); return; }
    let cancelled = false;
    const run = async () => {
      setSearchBusy(true);
      try {
        const res = await apiFetch(`/api/social/users/search?q=${encodeURIComponent(query)}`);
        if (!res.ok) { if (!cancelled) setSearchResults([]); return; }
        const data = await res.json().catch(() => ({}));
        if (!cancelled) setSearchResults(Array.isArray(data.users) ? data.users : []);
      } finally { if (!cancelled) setSearchBusy(false); }
    };
    const id = window.setTimeout(run, 220);
    return () => { cancelled = true; window.clearTimeout(id); };
  }, [addOpen, searchQuery, soloMode]);

  const onlineFriends = useMemo(() => friends.filter((f) => (f.status || "").toLowerCase() === "online"), [friends]);
  const offlineFriends = useMemo(() => friends.filter((f) => (f.status || "").toLowerCase() !== "online"), [friends]);

  const savePresence = async (nextPresence: string) => {
    setPresenceStatus(nextPresence);
    setStatusMenuOpen(false);
    if (soloMode) return;
    try {
      await apiFetch("/api/social/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ presence_status: nextPresence, dm_policy: "friends" }),
      });
    } catch { /* silent */ }
  };

  const removeFriend = async (friend: Friend) => {
    if (soloMode) return;
    try {
      await apiFetch("/api/social/friends/remove", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ user_id: friend.user_id }) });
      const updated = await apiJson<{ friends: Friend[] }>("/api/social/friends");
      setFriends(Array.isArray(updated.friends) ? updated.friends : []);
      sfx.play("success", 0.2);
    } catch { sfx.play("error", 0.2); }
  };

  const respondFriendRequest = async (requestId: number, action: "accept" | "decline") => {
    if (soloMode) return;
    try {
      const res = await apiFetch("/api/social/friends/respond", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ request_id: requestId, action }) });
      if (!res.ok) { sfx.play("error", 0.2); return; }
      const [friendsData, requestsData] = await Promise.all([
        apiJson<{ friends: Friend[] }>("/api/social/friends"),
        apiJson<{ incoming: Array<{ id: number; from_id?: string; from_name?: string }>; outgoing: Array<{ id: number; to_id?: string; to_name?: string }> }>("/api/social/requests"),
      ]);
      setFriends(Array.isArray(friendsData.friends) ? friendsData.friends : []);
      setIncomingRequests(Array.isArray(requestsData.incoming) ? requestsData.incoming : []);
      setOutgoingRequests(Array.isArray(requestsData.outgoing) ? requestsData.outgoing : []);
      sfx.play("success", 0.2);
    } catch { sfx.play("error", 0.2); }
  };

  const sendFriendRequest = async (userId: string, name: string) => {
    if (soloMode) return;
    try {
      const res = await apiFetch("/api/social/friends/request", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ user_id: userId }) });
      if (!res.ok) { sfx.play("error", 0.2); return; }
      const requestsData = await apiJson<{ incoming: Array<{ id: number; from_id?: string; from_name?: string }>; outgoing: Array<{ id: number; to_id?: string; to_name?: string }> }>("/api/social/requests");
      setIncomingRequests(Array.isArray(requestsData.incoming) ? requestsData.incoming : []);
      setOutgoingRequests(Array.isArray(requestsData.outgoing) ? requestsData.outgoing : []);
      setSearchQuery(""); setSearchResults([]); setAddOpen(false);
      sfx.play("success", 0.2);
      emitToast({ message: t("social.friend_request_sent_to_toast", { name }), kind: "success" });
    } catch { sfx.play("error", 0.2); }
  };

  const goHome = () => { sfx.play("confirm", 0.18); navigate(soloMode ? "/" : "/library"); };
  const goRooms = () => { sfx.play("confirm", 0.18); navigate("/rooms"); };
  const activeNav =
    location.pathname === "/library" || (soloMode && location.pathname === "/")
      ? "library"
      : location.pathname === "/rooms" || (!soloMode && location.pathname === "/")
        ? "rooms"
        : "";

  return (
    <aside className="w-52 shrink-0 flex flex-col border-r border-teal/8 backdrop-blur-sm relative z-10" style={{ background: "rgba(13,17,23,0.30)" }}>

      {/* Nav */}
      <div className="pt-4 space-y-0.5">
        <NavBtn icon={NAV_ICON_HOME} label="LIBRARY" active={activeNav === "library"} onClick={goHome} />
        <NavBtn icon={NAV_ICON_WORLDS} label="ROOMS" active={activeNav === "rooms"} onClick={goRooms} />
        <div className="px-3 py-1"><HorizontalTrace className="opacity-30" /></div>
      </div>

      {/* Friends */}
      <div className="flex-1 overflow-y-auto border-t border-teal/8 px-2 py-3 space-y-2">
        <div className="flex items-center justify-between px-1">
          <span className="text-[10px] font-header text-phosphor/25 tracking-widest">
            {soloMode ? "LOCAL SESSION" : `${t("nav.friends")} (${onlineFriends.length}/${friends.length})`}
          </span>
          {!soloMode && <button className="text-phosphor/20 hover:text-teal transition-colors text-xs" onClick={() => setAddOpen((v) => !v)}>+</button>}
        </div>

        {soloMode && (
          <div className="px-1 py-2 text-[10px] font-code text-phosphor/25 leading-relaxed">
            Offline firmware mode. Social, rooms, and account calls are paused.
          </div>
        )}

        {!soloMode && addOpen && (
          <div className="space-y-1 px-1">
            <input type="text" value={searchQuery} onChange={(e) => setSearchQuery(e.target.value)} placeholder="Search username..." autoFocus className="w-full h-7 bg-void/50 border border-teal/10 px-2 text-[11px] text-phosphor font-code placeholder:text-phosphor/15 focus:outline-none focus:border-teal/25" />
            {searchBusy && <div className="text-[10px] text-phosphor/20 px-1">Searching...</div>}
            {!searchBusy && searchQuery.trim().length >= 2 && searchResults.length === 0 && <div className="text-[10px] text-phosphor/20 px-1">No user found.</div>}
            {searchResults.map((u) => (
              <button key={u.user_id} type="button" className="flex items-center gap-2 w-full px-2 py-1 text-[10px] font-code text-phosphor/50 hover:bg-teal/10 transition-colors" onClick={() => void sendFriendRequest(u.user_id, u.display_name)}>
                {u.avatar_url ? <img src={u.avatar_url} alt="" className="w-4 h-4 rounded-full" /> : <span className="w-4 h-4 rounded-full bg-gunmetal-light" />}
                <span className="truncate">{u.display_name}</span>
              </button>
            ))}
          </div>
        )}

        {/* Requests */}
        {(incomingRequests.length > 0 || outgoingRequests.length > 0) && (
          <div className="space-y-0.5">
            <button onClick={() => setRequestsOpen((v) => !v)} className="flex items-center justify-between w-full text-[9px] font-header text-phosphor/15 tracking-widest px-1">
              <span>{t("social.requests")} ({incomingRequests.length})</span>
              <span>{requestsOpen ? "−" : "+"}</span>
            </button>
            {requestsOpen && incomingRequests.map((req) => (
              <div key={`req-in-${req.id}`} className="px-1 py-1 text-[10px] text-phosphor/40 font-code">
                <div className="truncate">{req.from_name}</div>
                <div className="flex gap-1 mt-0.5">
                  <button className="px-1.5 py-0.5 bg-teal/10 text-teal hover:bg-teal/20 transition-colors text-[9px]" onClick={() => void respondFriendRequest(req.id, "accept")}>{t("social.accept")}</button>
                  <button className="px-1.5 py-0.5 bg-void/30 text-phosphor/30 hover:text-phosphor/50 transition-colors text-[9px]" onClick={() => void respondFriendRequest(req.id, "decline")}>{t("social.decline")}</button>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* Online friends */}
        {onlineFriends.length > 0 && (
          <div className="space-y-0.5">
            <button onClick={() => setOnlineOpen((v) => !v)} className="flex items-center justify-between w-full text-[9px] font-header text-phosphor/15 tracking-widest px-1">
              <span>{t("social.status.online")} ({onlineFriends.length})</span>
              <span>{onlineOpen ? "−" : "+"}</span>
            </button>
            {onlineOpen && onlineFriends.map((f) => (
              <button
                key={`on-${f.user_id}`}
                className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/5 hover:text-phosphor/70 transition-colors cursor-pointer"
                onClick={() => onOpenFriends()}
                onContextMenu={(e) => { e.preventDefault(); setFriendMenu({ x: e.clientX, y: e.clientY, friend: f }); }}
              >
                <PulsingOrb color="#22c55e" size={5} />
                <span className="truncate">{f.display_name}</span>
              </button>
            ))}
          </div>
        )}

        {/* Offline friends */}
        {offlineFriends.length > 0 && (
          <div className="space-y-0.5">
            <button onClick={() => setOfflineOpen((v) => !v)} className="flex items-center justify-between w-full text-[9px] font-header text-phosphor/15 tracking-widest px-1 mt-2">
              <span>{t("social.status.offline")} ({offlineFriends.length})</span>
              <span>{offlineOpen ? "−" : "+"}</span>
            </button>
            {offlineOpen && offlineFriends.map((f) => (
              <button
                key={`off-${f.user_id}`}
                className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/20 cursor-pointer hover:text-phosphor/40 transition-colors"
                onClick={() => onOpenFriends()}
                onContextMenu={(e) => { e.preventDefault(); setFriendMenu({ x: e.clientX, y: e.clientY, friend: f }); }}
              >
                <span className="w-[5px] h-[5px] rounded-full bg-phosphor/10 shrink-0" />
                <span className="truncate">{f.display_name}</span>
              </button>
            ))}
          </div>
        )}
      </div>

      {/* Profile separator */}
      <div className="px-3 py-2 shrink-0"><HorizontalTrace className="opacity-30" /></div>

      {/* Profile */}
      <div className="p-3 relative">
        <button
          className="flex items-center gap-2.5 w-full text-left"
          onClick={(e) => { e.stopPropagation(); setStatusMenuOpen((v) => !v); }}
        >
          <div className="relative shrink-0">
            <div className="h-8 w-8 panel-chamfer-sm overflow-hidden bg-gradient-to-br from-purple-600 to-cyan-500">
              {me?.avatar_url && <img src={me.avatar_url} alt="" className="w-full h-full object-cover" />}
            </div>
            <div className="absolute -bottom-0.5 -right-0.5">
              <PulsingOrb color={presenceStatus === "offline" ? "#ef4444" : presenceStatus === "dnd" ? "#ffb347" : "#22c55e"} size={6} />
            </div>
          </div>
          <div className="min-w-0">
            <div className="text-xs font-header text-phosphor truncate tracking-wider">{me?.global_name || me?.username || t("account.dashboard")}</div>
            <div className="text-[10px] font-code text-phosphor/25 capitalize">{presenceStatus === "dnd" ? t("social.status.dnd") : presenceStatus === "offline" ? t("profile.menu.status_appear_offline") : t("social.status.online")}</div>
          </div>
        </button>

        {statusMenuOpen && (
          <div className="absolute bottom-full left-2 mb-1 bg-gunmetal border border-teal/20 panel-chamfer-sm p-1 space-y-0.5 min-w-[140px] z-50" onClick={(e) => e.stopPropagation()}>
            <button className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors" onClick={() => { setStatusMenuOpen(false); setProfileModalOpen(true); }}>
              {t("profile.menu.profile_settings")}
            </button>
            <button className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors" onClick={() => savePresence("online")}>
              <PulsingOrb color="#22c55e" size={5} /> {t("profile.menu.status_online")}
            </button>
            <button className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors" onClick={() => savePresence("dnd")}>
              <PulsingOrb color="#ffb347" size={5} /> {t("profile.menu.status_dnd")}
            </button>
            <button className="flex items-center gap-2 w-full px-2 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors" onClick={() => savePresence("offline")}>
              <PulsingOrb color="#ef4444" size={5} /> {t("profile.menu.status_appear_offline")}
            </button>
          </div>
        )}
      </div>

      {/* Friend context menu */}
      {friendMenu && (
        <div className="fixed inset-0 z-50" onClick={() => setFriendMenu(null)}>
          <div className="absolute bg-gunmetal border border-teal/20 panel-chamfer-sm p-1 min-w-[130px]" style={{ left: friendMenu.x, top: friendMenu.y }} onClick={(e) => e.stopPropagation()}>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-phosphor/50 hover:bg-teal/10 hover:text-phosphor/80 transition-colors" onClick={() => { setFriendMenu(null); onOpenFriends(); }}>{t("social.message")}</button>
            <button className="w-full text-left px-3 py-1.5 text-[11px] font-code text-magenta/50 hover:bg-magenta/10 hover:text-magenta transition-colors" onClick={() => { const f = friendMenu.friend; setFriendMenu(null); void removeFriend(f); }}>{t("social.remove")}</button>
          </div>
        </div>
      )}

      <ProfileSettingsModal open={profileModalOpen} onClose={() => setProfileModalOpen(false)} me={me || null} />
    </aside>
  );
};

export default PrototypeSidebar;
