import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { apiAssetUrl, apiFetch, isUsableAvatarUrl } from "../services/api";
import { useSfx } from "../hooks/useSfx";
import { emitToast } from "@/services/toast";
import { useI18n } from "../i18n";

interface SocialDrawerProps {
  open: boolean;
  onClose: () => void;
}

interface Friend {
  user_id: string;
  display_name: string;
  avatar_url?: string;
  status?: string;
  is_online?: boolean;
}

interface FriendRequest {
  id: number;
  from_id?: string;
  from_name?: string;
  to_id?: string;
  to_name?: string;
  avatar_url?: string;
  created_at?: string;
}

interface DirectMessage {
  id: number;
  direction: "in" | "out";
  content: string;
  created_at?: string;
}

const SocialDrawer: React.FC<SocialDrawerProps> = ({ open, onClose }) => {
  const { t } = useI18n();
  const [friends, setFriends] = useState<Friend[]>([]);
  const [incoming, setIncoming] = useState<FriendRequest[]>([]);
  const [outgoing, setOutgoing] = useState<FriendRequest[]>([]);
  const [unreadCount, setUnreadCount] = useState(0);
  const [presence, setPresence] = useState("online");
  const [dmPolicy, setDmPolicy] = useState("friends");
  const [activeConversationId, setActiveConversationId] = useState<string | null>(null);
  const [activeConversationName, setActiveConversationName] = useState(t("social.conversation"));
  const [messages, setMessages] = useState<DirectMessage[]>([]);
  const [pmOpen, setPmOpen] = useState(false);
  const [typingLabel] = useState("");
  const [toasts, setToasts] = useState<Array<{ id: number; text: string }>>([]);
  const activeConversationRef = useRef<string | null>(null);
  const tRef = useRef(t);
  const { play } = useSfx();
  const friendPresenceRef = useRef<Map<string, string>>(new Map());
  const friendPresenceSeenAtRef = useRef<Map<string, number>>(new Map());
  const friendPresenceReadyRef = useRef(false);

  const normalizeAvatar = useCallback((value?: string) => {
    return isUsableAvatarUrl(value) ? apiAssetUrl(value) : "";
  }, []);

  const normalizeDirectMessages = useCallback((rawMessages: any[], conversationId: string): DirectMessage[] => {
    return rawMessages
      .map((msg) => {
        const id = Number(msg?.id || Date.now());
        const fromId = String(msg?.from_id || "").trim();
        const direction: DirectMessage["direction"] = msg?.direction === "in" || fromId === conversationId ? "in" : "out";
        return {
          id,
          direction,
          content: String(msg?.content || ""),
          created_at: String(msg?.created_at || new Date().toISOString()),
        };
      })
      .filter((msg) => msg.content);
  }, []);

  const showToast = useCallback(
    (text: string, sound: string | null = "global") => {
      setToasts((prev) => [...prev, { id: Date.now(), text }]);
      emitToast({ message: text, kind: "info", durationMs: 10000 });
      if (sound) play(sound, 0.3);
    },
    [play]
  );

  useEffect(() => {
    if (!toasts.length) return;
    const timer = window.setTimeout(() => {
      setToasts((prev) => prev.slice(1));
    }, 3800);
    return () => window.clearTimeout(timer);
  }, [toasts]);

  useEffect(() => {
    if (open) play("confirm", 0.18);
  }, [open, play]);

  useEffect(() => {
    tRef.current = t;
  }, [t]);

  const loadFriends = useCallback(async () => {
    try {
      const res = await apiFetch("/api/social/friends");
      if (!res.ok) return;
      const data = await res.json();
      const list = Array.isArray(data.friends) ? data.friends : [];
      setFriends(list.map((friend: Friend) => ({ ...friend, avatar_url: normalizeAvatar(friend.avatar_url) })));
      const nextMap = new Map<string, string>();
      const nextSeen = new Map<string, number>();
      const now = Date.now();
      for (const friend of list) {
        const userId = String(friend?.user_id || "").trim();
        if (!userId) continue;
        const statusRaw = String(friend?.status || (friend?.is_online ? "online" : "offline")).toLowerCase();
        const status = statusRaw === "online" || statusRaw === "dnd" ? statusRaw : "offline";
        nextMap.set(userId, status);
        nextSeen.set(userId, now);
      }
      friendPresenceRef.current = nextMap;
      friendPresenceSeenAtRef.current = nextSeen;
      friendPresenceReadyRef.current = true;
    } catch {
      // Ignore offline/local file errors; UI can still render.
    }
  }, [normalizeAvatar]);

  const loadRequests = useCallback(async () => {
    try {
      const res = await apiFetch("/api/social/requests");
      if (!res.ok) return;
      const data = await res.json();
      setIncoming(data.incoming || []);
      setOutgoing(data.outgoing || []);
    } catch {
      // Ignore offline/local file errors.
    }
  }, []);

  const loadUnread = useCallback(async () => {
    try {
      const res = await apiFetch("/api/social/unread-count");
      if (!res.ok) return;
      const data = await res.json();
      setUnreadCount(Number(data.unread ?? data.unread_count ?? data.count ?? 0));
    } catch {
      // Ignore offline/local file errors.
    }
  }, []);

  const loadSettings = useCallback(async () => {
    try {
      const res = await apiFetch("/api/social/settings");
      if (!res.ok) return;
      const data = await res.json();
      setPresence(data.presence_status || "online");
      setDmPolicy(data.dm_policy || "friends");
    } catch {
      // Ignore offline/local file errors.
    }
  }, []);

  const saveSettings = useCallback(async (nextPresence: string, nextPolicy: string) => {
    const res = await apiFetch("/api/social/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ presence_status: nextPresence, dm_policy: nextPolicy })
    });
    if (!res.ok) {
      showToast(t("social.update_settings_error"), "error");
    }
  }, [showToast]);

  const openConversation = useCallback(async (userId: string, name: string) => {
    play("confirm", 0.18);
    setActiveConversationId(userId);
    setActiveConversationName(name || tRef.current("social.conversation"));
    setPmOpen(true);
    setMessages([]);
    const res = await apiFetch(`/api/social/messages?user_id=${encodeURIComponent(userId)}`);
    if (!res.ok) return;
    const data = await res.json();
    setMessages(normalizeDirectMessages(Array.isArray(data.messages) ? data.messages : [], userId));
    await apiFetch("/api/social/messages/read", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ user_id: userId })
    });
    loadUnread();
  }, [loadUnread, normalizeDirectMessages, play]);

  const refreshConversation = useCallback(async () => {
    if (!activeConversationRef.current || !pmOpen) return;
    const userId = activeConversationRef.current;
    try {
      const res = await apiFetch(`/api/social/messages?user_id=${encodeURIComponent(userId)}`);
      if (!res.ok) return;
      const data = await res.json();
      setMessages(normalizeDirectMessages(Array.isArray(data.messages) ? data.messages : [], userId));
      await apiFetch("/api/social/messages/read", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: userId })
      });
      loadUnread();
    } catch {
      // Keep the current local conversation visible if polling misses once.
    }
  }, [loadUnread, normalizeDirectMessages, pmOpen]);

  const sendMessage = useCallback(async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const form = event.currentTarget;
    const input = form.querySelector("input");
    if (!input || !activeConversationId) return;
    const content = input.value.trim();
    if (!content) return;
    play("confirm", 0.18);
    input.value = "";
    const optimisticId = Date.now();
    setMessages((prev) => [...prev, { id: optimisticId, direction: "out", content, created_at: new Date().toISOString() }]);
    const res = await apiFetch("/api/social/messages", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ user_id: activeConversationId, content })
    });
    if (!res.ok) {
      if (res.status === 401 || res.status === 403) {
        showToast(tRef.current("social.login_required_send"), "error");
        return;
      }
      showToast(tRef.current("social.message_send_failed"), "error");
      setMessages((prev) => prev.filter((msg) => msg.id !== optimisticId));
      input.value = content;
      return;
    }
    const data = await res.json().catch(() => ({}));
    if (data?.message?.id) {
      setMessages((prev) => prev.map((msg) => (
        msg.id === optimisticId
          ? { id: Number(data.message.id), direction: "out", content: data.message.content || content, created_at: data.message.created_at || new Date().toISOString() }
          : msg
      )));
    }
  }, [activeConversationId, showToast, play]);

  useEffect(() => {
    loadSettings();
    loadFriends();
    loadRequests();
    loadUnread();
  }, [loadSettings, loadFriends, loadRequests, loadUnread]);

  useEffect(() => {
    if (!open) return undefined;
    const timer = window.setInterval(() => {
      loadFriends();
      loadRequests();
      loadUnread();
    }, 10000);
    return () => window.clearInterval(timer);
  }, [loadFriends, loadRequests, loadUnread, open]);

  useEffect(() => {
    if (!pmOpen) return undefined;
    void refreshConversation();
    const timer = window.setInterval(() => {
      void refreshConversation();
    }, 2500);
    return () => window.clearInterval(timer);
  }, [pmOpen, refreshConversation]);

  useEffect(() => {
    const badge = document.getElementById("skl-friends-badge");
    const count = document.getElementById("skl-friends-count");
    if (count) {
      const online = friends.filter((friend) => friend.status === "online").length;
      count.textContent = `${online}/${friends.length}`;
    }
    if (badge) {
      if (unreadCount > 0) {
        badge.classList.add("active");
        badge.textContent = unreadCount > 99 ? "99+" : `${unreadCount}`;
      } else {
        badge.classList.remove("active");
        badge.textContent = "";
      }
    }
  }, [friends, unreadCount]);

  useEffect(() => {
    activeConversationRef.current = activeConversationId;
  }, [activeConversationId]);

  const activeFriend = useMemo(
    () => friends.find((friend) => friend.user_id === activeConversationId) || null,
    [friends, activeConversationId]
  );

  const activeFriendInitial = useMemo(() => {
    const source = String(activeConversationName || "").trim();
    return source ? source.slice(0, 1).toUpperCase() : "?";
  }, [activeConversationName]);

  const onlineCount = useMemo(() => friends.filter((friend) => friend.status === "online").length, [friends]);

  return (
    <>
      <div className={`skl-social-drawer${open ? " open" : ""}`} id="skl-friends-drawer" aria-hidden={!open}>
        <div className="skl-social-backdrop" onClick={() => { play("confirm", 0.18); onClose(); }}></div>
        <div className="skl-social-panel">
          <div className="skl-social-header">
            <div>
              <p className="skl-social-eyebrow">{t("nav.friends")}</p>
              <h3>{t("settings.tab.social")}</h3>
            </div>
            <button className="skl-btn ghost" type="button" onClick={() => { play("confirm", 0.18); onClose(); }}>{t("common.close")}</button>
          </div>
          <div className="skl-social-controls">
            <label>
              {t("social.status")}
              <select value={presence} onChange={(event) => { setPresence(event.target.value); saveSettings(event.target.value, dmPolicy); }}>
                <option value="online">{t("social.status.online")}</option>
                <option value="dnd">{t("social.status.dnd")}</option>
                <option value="offline">{t("social.status.offline")}</option>
              </select>
            </label>
            <label>
              {t("social.direct_messages")}
              <select value={dmPolicy} onChange={(event) => { setDmPolicy(event.target.value); saveSettings(presence, event.target.value); }}>
                <option value="anyone">{t("social.dm.anyone")}</option>
                <option value="friends">{t("social.dm.friends_only")}</option>
                <option value="none">{t("social.dm.none")}</option>
              </select>
            </label>
          </div>
          <div className="skl-social-section">
            <h4>{t("nav.friends")}</h4>
            <div id="skl-friends-list" className="skl-social-list">
              {friends.length === 0 && <div className="skl-friend-row">{t("social.no_friends")}</div>}
              {friends.map((friend) => (
                <div className="skl-friend-row" key={friend.user_id} onClick={() => openConversation(friend.user_id, friend.display_name)}>
                  {friend.avatar_url ? (
                    <img className="skl-friend-avatar" src={friend.avatar_url} alt={friend.display_name} />
                  ) : (
                    <div className="skl-friend-avatar"></div>
                  )}
                  <div className="skl-friend-info">
                    <div className="skl-friend-name">{friend.display_name}</div>
                    <div className="skl-friend-status">
                      <span className={`skl-status-dot ${friend.status || "offline"}`}></span>
                      {friend.status === "online" ? t("social.status.online") : friend.status === "dnd" ? t("social.status.dnd") : t("social.status.offline")}
                    </div>
                  </div>
                  <div className="skl-friend-actions">
                    <button className="skl-btn ghost" type="button" onClick={(event) => { event.stopPropagation(); openConversation(friend.user_id, friend.display_name); }}>
                      {t("social.message")}
                    </button>
                    <button
                      className="skl-btn ghost"
                      type="button"
                      onClick={async (event) => {
                        event.stopPropagation();
                        play("confirm", 0.18);
                        await apiFetch("/api/social/friends/remove", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ user_id: friend.user_id })
                        });
                        loadFriends();
                        play("success", 0.22);
                      }}
                    >
                      {t("social.remove")}
                    </button>
                  </div>
                </div>
              ))}
              {!!friends.length && (
                <div className="skl-friend-row">{t("social.online_summary", { online: onlineCount, total: friends.length })}</div>
              )}
            </div>
          </div>
          <div className="skl-social-section">
            <h4>{t("social.requests")}</h4>
            <div id="skl-friend-requests" className="skl-social-list">
              {!incoming.length && !outgoing.length && <div className="skl-friend-row">{t("social.no_pending_requests")}</div>}
              {incoming.map((req) => (
                <div className="skl-friend-row" key={`in-${req.id}`}>
                  <div className="skl-friend-name">{t("social.sent_request", { name: req.from_name || "" })}</div>
                  <div className="skl-friend-actions">
                    <button
                      className="skl-btn ghost"
                      onClick={async () => {
                        play("confirm", 0.18);
                        await apiFetch("/api/social/friends/respond", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ request_id: req.id, action: "accept" })
                        });
                        loadRequests();
                        loadFriends();
                        play("success", 0.22);
                      }}
                    >
                      {t("social.accept")}
                    </button>
                    <button
                      className="skl-btn ghost"
                      onClick={async () => {
                        play("confirm", 0.18);
                        await apiFetch("/api/social/friends/respond", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ request_id: req.id, action: "decline" })
                        });
                        loadRequests();
                        play("success", 0.22);
                      }}
                    >
                      {t("social.decline")}
                    </button>
                  </div>
                </div>
              ))}
              {outgoing.map((req) => (
                <div className="skl-friend-row" key={`out-${req.id}`}>{t("social.request_sent_to", { name: req.to_name || "" })}</div>
              ))}
            </div>
          </div>
        </div>
      </div>

      <div className={`skl-social-drawer${pmOpen ? " open" : ""}`} id="skl-pm-drawer" aria-hidden={!pmOpen}>
        <div className="skl-social-backdrop" onClick={() => { play("confirm", 0.18); setPmOpen(false); }}></div>
        <div className="skl-social-panel skl-pm-panel">
          <div className="skl-social-header">
            <div>
              <p className="skl-social-eyebrow">{t("social.direct_message")}</p>
              <h3 id="skl-pm-title">{activeConversationName}</h3>
            </div>
            <button className="skl-btn ghost" type="button" onClick={() => { play("confirm", 0.18); setPmOpen(false); }}>{t("common.close")}</button>
          </div>
          <div className="skl-pm-layout">
            <div className="skl-pm-chat">
              <div id="skl-pm-messages" className="skl-pm-messages">
                {messages.map((msg) => (
                  <div className={`skl-pm-bubble ${msg.direction}`} key={msg.id}>
                    {msg.content}
                    {msg.created_at && (
                      <div className="skl-pm-bubble-time">
                        {new Date(msg.created_at).toLocaleString(undefined, {
                          month: "short",
                          day: "numeric",
                          hour: "2-digit",
                          minute: "2-digit"
                        })}
                      </div>
                    )}
                  </div>
                ))}
              </div>
              <div id="skl-pm-typing" className="skl-pm-typing" aria-live="polite">{typingLabel}</div>
              <form id="skl-pm-form" className="skl-pm-form" onSubmit={sendMessage}>
                <input id="skl-pm-input" type="text" placeholder={t("social.write_message")} maxLength={2000} />
                <button className="skl-btn primary" type="submit">{t("social.send")}</button>
              </form>
            </div>
            <aside className="skl-pm-profile">
              <div className="skl-profile-card">
                <div className="skl-profile-banner"></div>
                <div className="skl-profile-body">
                  <div className="skl-profile-avatar-wrap">
                    {activeFriend?.avatar_url ? (
                      <img className="skl-profile-avatar" id="skl-profile-avatar" src={activeFriend.avatar_url} alt={activeConversationName} />
                    ) : (
                      <div className="skl-profile-avatar skl-profile-avatar-fallback" id="skl-profile-avatar" aria-hidden="true">{activeFriendInitial}</div>
                    )}
                    <span className="skl-profile-avatar-status"></span>
                  </div>
                  <div className="skl-profile-name" id="skl-profile-name">{activeConversationName}</div>
                  <div className="skl-profile-handle" id="skl-profile-handle">{t("social.ready_to_chat")}</div>
                  <div className="skl-profile-actions">
                    <button className="skl-btn primary" type="button">{t("social.invite_to_room")}</button>
                    <button className="skl-btn ghost" type="button">{t("social.more")}</button>
                  </div>
                  <div className="skl-profile-section">
                    <h4>{t("nav.about")}</h4>
                    <p id="skl-profile-about">{t("social.start_conversation_details")}</p>
                  </div>
                </div>
              </div>
            </aside>
          </div>
        </div>
      </div>

      <div className="skl-toast-stack" id="skl-toast-stack">
        {toasts.map((toast) => (
          <div className="skl-toast" key={toast.id}>{toast.text}</div>
        ))}
      </div>
    </>
  );
};

export default SocialDrawer;
