import { apiAssetUrl, apiFetch, apiJson } from "./api";
import { trace, traceError } from "./trace";

export interface SocialProfile {
  user_id: string;
  username: string;
  display_name: string;
  name: string;
  avatar_url: string;
  status: string;
  presence_status?: string;
  role?: string;
  permissions?: string[];
  patreon_tier?: string;
  patreon_is_supporter?: boolean;
}

export interface SocialRequest extends SocialProfile {
  id: number;
  from_id?: string;
  from_name?: string;
  to_id?: string;
  to_name?: string;
  created_at?: string;
}

export interface SocialDirectMessage {
  id: string;
  from_id: string;
  to_id: string;
  content: string;
  created_at: string;
  read_at?: string;
}

export interface SocialSnapshot {
  friends: SocialProfile[];
  incoming: SocialRequest[];
  outgoing: SocialRequest[];
  unreadCount: number;
  unreadByUser: Record<string, number>;
  presenceStatus: string;
}

export const normalizePresenceStatus = (value: unknown, fallback = "offline") => {
  const normalized = String(value ?? "").trim().toLowerCase();
  if (normalized === "online" || normalized === "dnd" || normalized === "busy" || normalized === "afk") return normalized;
  if (normalized === "offline" || normalized === "appear-offline" || normalized === "invisible") return "offline";
  return fallback;
};

export const isPresenceOnline = (value: unknown) => {
  const normalized = normalizePresenceStatus(value, "offline");
  return normalized === "online" || normalized === "dnd" || normalized === "busy" || normalized === "afk";
};

export const presenceStatusLabel = (value: unknown) => {
  const normalized = normalizePresenceStatus(value, "offline");
  if (normalized === "dnd" || normalized === "busy") return "Busy";
  if (normalized === "afk") return "AFK";
  return normalized === "online" ? "Online" : "Offline";
};

const normalizeProfile = (raw: any): SocialProfile | null => {
  if (!raw || typeof raw !== "object") return null;
  const userId = String(raw.user_id || raw.discord_id || raw.id || "").trim();
  const username = String(raw.username || raw.name || userId || "").trim();
  const displayName = String(raw.display_name || raw.global_name || raw.name || username || userId || "").trim();
  if (!userId && !username && !displayName) return null;
  const status = normalizePresenceStatus(raw.presence_status ?? raw.status ?? (raw.is_online ? "online" : "offline"), "offline");
  return {
    user_id: userId || username || displayName,
    username,
    display_name: displayName,
    name: displayName || username || userId,
    avatar_url: apiAssetUrl(raw.avatar_url),
    status,
    presence_status: status,
    role: typeof raw.role === "string" ? raw.role : "",
    permissions: Array.isArray(raw.permissions) ? raw.permissions.map((entry: unknown) => String(entry)) : [],
    patreon_tier: typeof raw.patreon_tier === "string" ? raw.patreon_tier : "",
    patreon_is_supporter: Boolean(raw.patreon_is_supporter),
  };
};

const normalizeRequest = (raw: any): SocialRequest | null => {
  const profile = normalizeProfile(raw);
  if (!profile) return null;
  const id = Number(raw?.id || raw?.request_id || 0);
  return {
    ...profile,
    id: Number.isFinite(id) ? id : 0,
    from_id: typeof raw?.from_id === "string" ? raw.from_id : undefined,
    from_name: typeof raw?.from_name === "string" ? raw.from_name : undefined,
    to_id: typeof raw?.to_id === "string" ? raw.to_id : undefined,
    to_name: typeof raw?.to_name === "string" ? raw.to_name : undefined,
    created_at: typeof raw?.created_at === "string" ? raw.created_at : undefined,
  };
};

const normalizeMessages = (raw: any): SocialDirectMessage[] => {
  const messages = Array.isArray(raw?.messages) ? raw.messages : [];
  return messages
    .map((entry: any) => ({
      id: String(entry?.id || ""),
      from_id: String(entry?.from_id || ""),
      to_id: String(entry?.to_id || ""),
      content: String(entry?.content || ""),
      created_at: String(entry?.created_at || ""),
      read_at: typeof entry?.read_at === "string" ? entry.read_at : undefined,
    }))
    .filter((entry: SocialDirectMessage) => entry.id && entry.content);
};

const safeJson = async <T>(path: string, fallback: T): Promise<T> => {
  try {
    return await apiJson<T>(path);
  } catch (error) {
    traceError("social-client", "request_failed", error, { path });
    return fallback;
  }
};

export const listSocialSnapshot = async (): Promise<SocialSnapshot> => {
  trace("social-client", "snapshot_start");
  const [friendsData, requestsData, unreadData, unreadByUserData, settingsData] = await Promise.all([
    safeJson<{ friends?: any[] }>("/api/social/friends", { friends: [] }),
    safeJson<{ incoming?: any[]; outgoing?: any[] }>("/api/social/requests", { incoming: [], outgoing: [] }),
    safeJson<{ count?: number; unread_count?: number }>("/api/social/unread-count", { count: 0, unread_count: 0 }),
    safeJson<{ unread?: any[]; unread_by_user?: Record<string, number> }>("/api/social/unread-by-user", { unread: [], unread_by_user: {} }),
    safeJson<{ presence_status?: string; settings?: { presence_status?: string } }>("/api/social/settings", { presence_status: "online" }),
  ]);
  const friends = (Array.isArray(friendsData.friends) ? friendsData.friends : [])
    .map(normalizeProfile)
    .filter(Boolean) as SocialProfile[];
  const incoming = (Array.isArray(requestsData.incoming) ? requestsData.incoming : [])
    .map(normalizeRequest)
    .filter(Boolean) as SocialRequest[];
  const outgoing = (Array.isArray(requestsData.outgoing) ? requestsData.outgoing : [])
    .map(normalizeRequest)
    .filter(Boolean) as SocialRequest[];
  const unreadCount = Number(unreadData.unread_count ?? unreadData.count ?? 0);
  const unreadByUser: Record<string, number> = {};
  if (Array.isArray(unreadByUserData.unread)) {
    for (const row of unreadByUserData.unread) {
      const userId = String(row?.user_id || row?.from_id || "").trim();
      const count = Number(row?.count || row?.unread_count || 0);
      if (userId && Number.isFinite(count) && count > 0) unreadByUser[userId] = count;
    }
  }
  if (unreadByUserData.unread_by_user && typeof unreadByUserData.unread_by_user === "object") {
    for (const [userId, value] of Object.entries(unreadByUserData.unread_by_user)) {
      const count = Number(value);
      if (userId && Number.isFinite(count) && count > 0) unreadByUser[userId] = count;
    }
  }
  const presenceStatus = String(settingsData.settings?.presence_status || settingsData.presence_status || "online");
  trace("social-client", "snapshot_success", {
    friends: friends.length,
    incoming: incoming.length,
    outgoing: outgoing.length,
    unreadCount,
    unreadThreads: Object.keys(unreadByUser).length,
    presenceStatus,
  });
  return {
    friends,
    incoming,
    outgoing,
    unreadCount: Number.isFinite(unreadCount) ? unreadCount : 0,
    unreadByUser,
    presenceStatus,
  };
};

export const updateSocialPresence = async (presenceStatus: string) => {
  trace("social-client", "presence_update_start", { presenceStatus });
  await apiJson<{ ok?: boolean }>("/api/social/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ presence_status: presenceStatus, dm_policy: "friends" }),
  });
  trace("social-client", "presence_update_success", { presenceStatus });
};

export const searchSocialUsers = async (query: string): Promise<SocialProfile[]> => {
  const q = query.trim();
  if (q.length < 2) return [];
  trace("social-client", "search_start", { length: q.length });
  const data = await apiJson<{ users?: any[] }>(`/api/social/users/search?q=${encodeURIComponent(q)}`);
  const users = (Array.isArray(data.users) ? data.users : [])
    .map(normalizeProfile)
    .filter(Boolean) as SocialProfile[];
  trace("social-client", "search_success", { count: users.length });
  return users;
};

export const sendSocialFriendRequest = async (userId: string) => {
  trace("social-client", "friend_request_start", { userId });
  await apiJson<{ ok?: boolean }>("/api/social/friends/request", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user_id: userId }),
  });
  trace("social-client", "friend_request_success", { userId });
};

export const removeSocialFriend = async (userId: string) => {
  trace("social-client", "friend_remove_start", { userId });
  await apiJson<{ ok?: boolean }>("/api/social/friends/remove", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user_id: userId }),
  });
  trace("social-client", "friend_remove_success", { userId });
};

export const blockSocialUser = async (userId: string) => {
  trace("social-client", "block_start", { userId });
  await apiJson<{ ok?: boolean }>("/api/social/blocks", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user_id: userId }),
  });
  trace("social-client", "block_success", { userId });
};

export const respondSocialFriendRequest = async (requestId: number, action: "accept" | "decline") => {
  trace("social-client", "friend_request_respond_start", { requestId, action });
  await apiJson<{ ok?: boolean }>("/api/social/friends/respond", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ request_id: requestId, action }),
  });
  trace("social-client", "friend_request_respond_success", { requestId, action });
};

export const loadDirectMessages = async (userId: string): Promise<SocialDirectMessage[]> => {
  trace("social-client", "dm_load_start", { userId });
  const data = await apiJson<any>(`/api/social/messages?user_id=${encodeURIComponent(userId)}`);
  const messages = normalizeMessages(data);
  trace("social-client", "dm_load_success", { userId, count: messages.length });
  return messages;
};

export const markDirectMessagesRead = async (userId: string): Promise<void> => {
  if (!userId) return;
  trace("social-client", "dm_mark_read_start", { userId });
  await apiJson<{ ok?: boolean }>("/api/social/messages/read", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user_id: userId }),
  });
  trace("social-client", "dm_mark_read_success", { userId });
};

export const sendDirectMessage = async (userId: string, content: string): Promise<SocialDirectMessage | null> => {
  const text = content.trim();
  if (!text) return null;
  trace("social-client", "dm_send_start", { userId, length: text.length });
  const response = await apiFetch("/api/social/messages", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user_id: userId, content: text }),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(String((data as any)?.error || "Unable to send message."));
  const message = normalizeMessages({ messages: [(data as any)?.message] })[0] || null;
  trace("social-client", "dm_send_success", { userId, messageId: message?.id || "" });
  return message;
};
