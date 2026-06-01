import { apiAssetUrl, apiFetch, apiJson, errorFromResponse, isUsableAvatarUrl } from "./api";

export type SekaiChatMessage = {
  id: string;
  author: string;
  content: string;
  created_at: string;
  kind?: "user" | "system" | "join" | "leave";
  channel?: string;
  avatar_url?: string;
};

export type ChatChannelRef =
  | { kind: "global"; locale?: string }
  | { kind: "lobby"; lobbyId: string }
  | { kind: "dm"; conversationId: string };

export type SekaiChatPresenceUser = {
  user_id: string;
  username?: string;
  display_name?: string;
  name: string;
  avatar_url?: string;
  last_seen?: string;
  status?: string;
  irc_op?: boolean;
  irc_voice?: boolean;
  role?: string;
  ready?: boolean;
  local_ready_known?: boolean;
  local_ready?: boolean;
  local_ready_note?: string;
};

const systemMessage = (id: string, content: string): SekaiChatMessage => ({
  id,
  author: "System",
  content,
  created_at: new Date().toISOString(),
  kind: "system",
});

export const chatChannelId = (channel: ChatChannelRef) => {
  if (channel.kind === "global") return `global:${channel.locale || "fr"}`;
  if (channel.kind === "dm") return `dm:${channel.conversationId}`;
  return `lobby:${channel.lobbyId}`;
};

const stableHash = (value: string) => {
  let hash = 5381;
  for (let i = 0; i < value.length; i += 1) hash = ((hash << 5) + hash) ^ value.charCodeAt(i);
  return (hash >>> 0).toString(36);
};

const stableMessageId = (msg: any, fallbackChannel: string) => {
  const explicit = msg?.id || msg?.message_id || msg?.uuid;
  if (explicit) return String(explicit);
  const channel = String(msg?.channel || fallbackChannel);
  const author = String(msg?.author || msg?.display_name || msg?.username || "System");
  const content = String(msg?.content || msg?.body || "");
  const createdAt = String(msg?.created_at || msg?.timestamp || "");
  return `msg-${stableHash(`${channel}\n${author}\n${createdAt}\n${content}`)}`;
};

const normalizeMessage = (msg: any, fallbackChannel = "Room"): SekaiChatMessage => {
  const createdAt = String(msg?.created_at || msg?.timestamp || new Date().toISOString());
  return {
    id: stableMessageId(msg, fallbackChannel),
    author: String(msg?.author || msg?.display_name || msg?.username || "System"),
    content: String(msg?.content || msg?.body || ""),
    created_at: createdAt,
    kind: (msg?.kind === "system" || msg?.kind === "join" || msg?.kind === "leave") ? msg.kind : "user",
    channel: String(msg?.channel || fallbackChannel),
    avatar_url: isUsableAvatarUrl(msg?.avatar_url) ? apiAssetUrl(msg?.avatar_url) : "",
  };
};

const chatApiPath = (channel: ChatChannelRef) =>
  `/api/chat/channels/${encodeURIComponent(chatChannelId(channel))}/messages`;

const chatPresencePath = (channel: ChatChannelRef) =>
  `/api/chat/channels/${encodeURIComponent(chatChannelId(channel))}/presence`;

const canFallbackToLobby = (channel: ChatChannelRef): channel is { kind: "lobby"; lobbyId: string } =>
  channel.kind === "lobby" && Boolean(channel.lobbyId);

export const chatService = {
  async listPresence(channel: ChatChannelRef): Promise<SekaiChatPresenceUser[]> {
    const data = await apiJson<{ users?: Array<any> }>(chatPresencePath(channel));
    const list = Array.isArray(data?.users) ? data.users : [];
    return list
      .map((user) => ({
        user_id: String(user?.user_id || ""),
        username: typeof user?.username === "string" ? user.username : "",
        display_name: typeof user?.display_name === "string" ? user.display_name : "",
        name: String(user?.name || user?.display_name || user?.username || "SekaiLink"),
        avatar_url: isUsableAvatarUrl(user?.avatar_url) ? apiAssetUrl(user?.avatar_url) : "",
        last_seen: typeof user?.last_seen === "string" ? user.last_seen : "",
        status: typeof user?.status === "string" ? user.status : "online",
        irc_op: Boolean(user?.irc_op),
        irc_voice: Boolean(user?.irc_voice),
        role: typeof user?.role === "string" ? user.role : "",
        ready: Boolean(user?.ready),
        local_ready_known: typeof user?.local_ready_known === "boolean" ? user.local_ready_known : undefined,
        local_ready: typeof user?.local_ready === "boolean" ? user.local_ready : undefined,
        local_ready_note: typeof user?.local_ready_note === "string" ? user.local_ready_note : "",
      }))
      .filter((user) => user.user_id);
  },

  async touchPresence(
    channel: ChatChannelRef,
    state?: { role?: string; ready?: boolean; local_ready_known?: boolean; local_ready?: boolean; local_ready_note?: string }
  ): Promise<void> {
    const response = await apiFetch(chatPresencePath(channel), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(state || {}),
    });
    if (!response.ok) {
      const data = await response.json().catch(() => ({}));
      throw new Error(String((data as any)?.error || "Unable to update chat presence."));
    }
  },

  async listMessages(channel: ChatChannelRef): Promise<SekaiChatMessage[]> {
    try {
      const data = await apiJson<{ messages?: Array<any> }>(chatApiPath(channel));
      const list = Array.isArray(data?.messages) ? data.messages : [];
      return list.map((msg) => normalizeMessage(msg, chatChannelId(channel)));
    } catch (err) {
      // Public routing to the new chat gateway is staged separately; keep the legacy lobby path alive meanwhile.
      if (!canFallbackToLobby(channel)) throw err;
      const message = err instanceof Error ? err.message : "";
      if (/unauthorized|session expired/i.test(message)) throw err;
    }
    if (channel.kind === "lobby" && !channel.lobbyId) {
      return [systemMessage("sys-none", "Select a room to preview its chat.")];
    }
    const data = await apiJson<{ messages?: Array<any> }>(`/api/lobbies/${encodeURIComponent(channel.lobbyId)}/messages`);
    const list = Array.isArray(data?.messages) ? data.messages : [];
    return list.map((msg) => normalizeMessage(msg, chatChannelId(channel)));
  },

  async sendMessage(channel: ChatChannelRef, content: string): Promise<SekaiChatMessage> {
    const text = content.trim();
    if (!text) throw new Error("Message is empty.");
    const chatResponse = await apiFetch(chatApiPath(channel), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ content: text }),
    }).catch(() => null);
    if (chatResponse?.ok) {
      const data = await chatResponse.json().catch(() => ({}));
      return normalizeMessage((data as any)?.message || { channel: chatChannelId(channel), content: text });
    }
    if (!canFallbackToLobby(channel)) {
      if (chatResponse) throw await errorFromResponse(chatResponse, "SekaiLink chat gateway is not connected yet.");
      throw new Error("SekaiLink chat gateway is not connected yet.");
    }
    if (channel.kind === "lobby" && !channel.lobbyId) {
      throw new Error("Select a room before sending a message.");
    }
    const response = await apiFetch(`/api/lobbies/${encodeURIComponent(channel.lobbyId)}/messages`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ content: text }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(String((data as any)?.error || "Unable to send message."));
    }
    const payload = (data as any)?.message || data;
    return normalizeMessage(
      {
        channel: chatChannelId(channel),
        content: (payload as any)?.content || text,
        ...payload,
      },
      chatChannelId(channel)
    );
  },

  async sendSystemMessage(channel: ChatChannelRef, content: string): Promise<SekaiChatMessage> {
    const text = content.trim();
    if (!text) throw new Error("Message is empty.");
    const body = JSON.stringify({ content: text, kind: "system", system: true });
    const chatResponse = await apiFetch(chatApiPath(channel), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body,
    }).catch(() => null);
    if (chatResponse?.ok) {
      const data = await chatResponse.json().catch(() => ({}));
      return normalizeMessage(
        { channel: chatChannelId(channel), content: text, ...((data as any)?.message || {}), kind: "system" },
        chatChannelId(channel)
      );
    }
    if (!canFallbackToLobby(channel)) {
      if (chatResponse) throw await errorFromResponse(chatResponse, "SekaiLink chat gateway is not connected yet.");
      throw new Error("SekaiLink chat gateway is not connected yet.");
    }
    const response = await apiFetch(`/api/lobbies/${encodeURIComponent(channel.lobbyId)}/messages`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body,
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(String((data as any)?.error || "Unable to send system message."));
    }
    return normalizeMessage(
      { channel: chatChannelId(channel), content: text, ...((data as any)?.message || data), kind: "system" },
      chatChannelId(channel)
    );
  },
};
