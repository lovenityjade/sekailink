import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { io, Socket } from "socket.io-client";
import { apiFetch, API_BASE_URL, getDesktopToken } from "../services/api";
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
  const [typingLabel, setTypingLabel] = useState("");
  const [toasts, setToasts] = useState<Array<{ id: number; text: string }>>([]);
  const socketRef = useRef<Socket | null>(null);
  const typingTimer = useRef<number | null>(null);
  const activeConversationRef = useRef<string | null>(null);
  const sfx = useSfx();

  const showToast = useCallback(
    (text: string, sound: string | null = "global") => {
      setToasts((prev) => [...prev, { id: Date.now(), text }]);
      emitToast({ message: text, kind: "info", durationMs: 10000 });
      if (sound) sfx.play(sound, 0.3);
    },
    [sfx]
  );

  useEffect(() => {
    if (!toasts.length) return;
    const timer = window.setTimeout(() => {
      setToasts((prev) => prev.slice(1));
    }, 3800);
    return () => window.clearTimeout(timer);
  }, [toasts]);

  useEffect(() => {
    if (open) sfx.play("confirm", 0.18);
  }, [open, sfx]);

  const loadFriends = useCallback(async () => {
    try {
      const res = await apiFetch("/api/social/friends");
      if (!res.ok) return;
      const data = await res.json();
      setFriends(data.friends || []);
    } catch {
      // Ignore offline/local file errors; UI can still render.
    }
  }, []);

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
      setUnreadCount(data.unread || 0);
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
    sfx.play("confirm", 0.18);
    setActiveConversationId(userId);
    setActiveConversationName(name || t("social.conversation"));
    setPmOpen(true);
    setMessages([]);
    const res = await apiFetch(`/api/social/messages?user_id=${encodeURIComponent(userId)}`);
    if (!res.ok) return;
    const data = await res.json();
    setMessages(data.messages || []);
    await apiFetch("/api/social/messages/read", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ user_id: userId })
    });
    loadUnread();
  }, [loadUnread, sfx]);

  const sendMessage = useCallback(async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const form = event.currentTarget;
    const input = form.querySelector("input");
    if (!input || !activeConversationId) return;
    const content = input.value.trim();
    if (!content) return;
    sfx.play("confirm", 0.18);
    input.value = "";
    setMessages((prev) => [...prev, { id: Date.now(), direction: "out", content, created_at: new Date().toISOString() }]);
    const res = await apiFetch("/api/social/messages", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ user_id: activeConversationId, content })
    });
    if (!res.ok) {
      if (res.status === 401 || res.status === 403) {
        showToast(t("social.login_required_send"), "error");
        return;
      }
      showToast(t("social.message_send_failed"), "error");
    }
  }, [activeConversationId, showToast, sfx]);

  useEffect(() => {
    loadSettings();
    loadFriends();
    loadRequests();
    loadUnread();
  }, [loadSettings, loadFriends, loadRequests, loadUnread]);

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

  useEffect(() => {
    const isFile = window.location.protocol === "file:";
    if (isFile && !API_BASE_URL) return undefined;
    let socket: Socket;
    try {
      const token = getDesktopToken();
      socket = io(API_BASE_URL || undefined, {
        transports: ["websocket"],
        withCredentials: true,
        auth: token ? { token } : undefined,
        query: token ? { token } : undefined
      });
    } catch {
      return undefined;
    }
    socketRef.current = socket;

    socket.on("friend_presence", (data) => {
      if (!data) return;
      loadFriends();
      if (data.display_name) {
        const statusLabel = data.status === "online" ? t("social.status.online") : data.status === "dnd" ? t("social.status.dnd") : t("social.status.offline");
        showToast(t("social.presence_changed", { name: data.display_name, status: statusLabel }), "global");
      }
    });

    socket.on("friend_activity", (data) => {
      if (!data?.display_name) return;
      if (data.type === "lobby_join") showToast(t("social.lobby_join", { name: data.display_name }), "global");
      if (data.type === "lobby_leave") showToast(t("social.lobby_leave", { name: data.display_name }), "global");
      if (data.type === "room_join") showToast(t("social.room_join", { name: data.display_name }), "global");
      if (data.type === "room_leave") showToast(t("social.room_leave", { name: data.display_name }), "global");
    });

    socket.on("dm_new", (data) => {
      if (!data) return;
      if (activeConversationRef.current === data.from_id) {
        setMessages((prev) => [...prev, { id: Date.now(), direction: "in", content: data.content, created_at: data.created_at }]);
        apiFetch("/api/social/messages/read", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ user_id: data.from_id })
        });
      } else {
        showToast(t("social.new_message_from", { name: data.from_name }), "global");
        loadUnread();
      }
    });

    socket.on("dm_typing", (data) => {
      if (!data || data.from_id !== activeConversationRef.current) return;
      setTypingLabel(t("social.is_typing", { name: data.from_name || t("social.friend") }));
      if (typingTimer.current) window.clearTimeout(typingTimer.current);
      typingTimer.current = window.setTimeout(() => setTypingLabel(""), 1800);
    });

    socket.on("friend_request", () => {
      showToast(t("social.received_friend_request"), "friendrequest");
      loadRequests();
    });

    socket.on("friend_added", (data) => {
      if (!data) return;
      showToast(t("social.accepted_friend_request", { name: data.display_name }), "global");
      loadFriends();
    });

    socket.on("support_ticket_update", (data) => {
      if (!data) return;
      showToast(t("social.support_ticket_update", { subject: data.subject, status: data.status }), "global");
    });

    return () => {
      socket.disconnect();
    };
  }, [loadFriends, loadRequests, loadUnread, showToast, t]);

  const onlineCount = useMemo(() => friends.filter((friend) => friend.status === "online").length, [friends]);

  return (
    <>
      <div className={`skl-social-drawer${open ? " open" : ""}`} id="skl-friends-drawer" aria-hidden={!open}>
        <div className="skl-social-backdrop" onClick={() => { sfx.play("confirm", 0.18); onClose(); }}></div>
        <div className="skl-social-panel">
          <div className="skl-social-header">
            <div>
              <p className="skl-social-eyebrow">{t("nav.friends")}</p>
              <h3>{t("settings.tab.social")}</h3>
            </div>
            <button className="skl-btn ghost" type="button" onClick={() => { sfx.play("confirm", 0.18); onClose(); }}>{t("common.close")}</button>
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
                        sfx.play("confirm", 0.18);
                        await apiFetch("/api/social/friends/remove", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ user_id: friend.user_id })
                        });
                        loadFriends();
                        sfx.play("success", 0.22);
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
                        sfx.play("confirm", 0.18);
                        await apiFetch("/api/social/friends/respond", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ request_id: req.id, action: "accept" })
                        });
                        loadRequests();
                        loadFriends();
                        sfx.play("success", 0.22);
                      }}
                    >
                      {t("social.accept")}
                    </button>
                    <button
                      className="skl-btn ghost"
                      onClick={async () => {
                        sfx.play("confirm", 0.18);
                        await apiFetch("/api/social/friends/respond", {
                          method: "POST",
                          headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ request_id: req.id, action: "decline" })
                        });
                        loadRequests();
                        sfx.play("success", 0.22);
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
        <div className="skl-social-backdrop" onClick={() => { sfx.play("confirm", 0.18); setPmOpen(false); }}></div>
        <div className="skl-social-panel skl-pm-panel">
          <div className="skl-social-header">
            <div>
              <p className="skl-social-eyebrow">{t("social.direct_message")}</p>
              <h3 id="skl-pm-title">{activeConversationName}</h3>
            </div>
            <button className="skl-btn ghost" type="button" onClick={() => { sfx.play("confirm", 0.18); setPmOpen(false); }}>{t("common.close")}</button>
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
                <input id="skl-pm-input" type="text" placeholder={t("social.write_message")} maxLength={2000} onInput={() => {
                  if (socketRef.current && activeConversationId) {
                    socketRef.current.emit("dm_typing", { user_id: activeConversationId });
                  }
                }} />
                <button className="skl-btn primary" type="submit">{t("social.send")}</button>
              </form>
            </div>
            <aside className="skl-pm-profile">
              <div className="skl-profile-card">
                <div className="skl-profile-banner"></div>
                <div className="skl-profile-body">
                  <div className="skl-profile-avatar-wrap">
                    <div className="skl-profile-avatar" id="skl-profile-avatar"></div>
                    <span className="skl-profile-avatar-status"></span>
                  </div>
                  <div className="skl-profile-name" id="skl-profile-name">{activeConversationName}</div>
                  <div className="skl-profile-handle" id="skl-profile-handle">{activeConversationId ? `@${activeConversationId}` : t("social.ready_to_chat")}</div>
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
