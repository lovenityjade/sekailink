import { apiFetch, apiJson, apiUrl } from "./api";
import { trace, traceError } from "./trace";

export interface LobbySummary {
  id: string;
  name: string;
  description?: string;
  owner?: string;
  is_private?: boolean;
  is_locked?: boolean;
  item_cheat?: boolean;
  spoiler?: string;
  max_players?: number;
  last_activity?: string;
  member_count?: number;
  message_count?: number;
  url?: string;
  asynchronous?: boolean;
  is_async?: boolean;
  room_type?: string;
  metadata?: Record<string, unknown>;
  async_state?: Record<string, unknown> | null;
}

export interface CreateLobbyInput {
  name: string;
  description?: string;
  server_password?: string;
  max_players?: string;
  spoiler?: string;
  item_cheat?: boolean;
  asynchronous?: boolean;
}

type TimedCache<T> = {
  value?: T;
  expiresAt: number;
  inFlight?: Promise<T>;
};

const LOBBIES_CACHE_TTL_MS = 2500;
const lobbiesCache: TimedCache<LobbySummary[]> = { expiresAt: 0 };

const parseLobbyIdFromUrl = (value: unknown) => {
  const raw = String(value || "").trim();
  if (!raw) return "";
  try {
    const url = new URL(raw, apiUrl("/"));
    return url.pathname.split("/").filter(Boolean).pop() || "";
  } catch {
    return raw.split("/").filter(Boolean).pop() || "";
  }
};

const clearLobbiesCache = () => {
  lobbiesCache.value = undefined;
  lobbiesCache.expiresAt = 0;
  lobbiesCache.inFlight = undefined;
};

export const listLobbies = async (options: { force?: boolean } = {}): Promise<LobbySummary[]> => {
  const now = Date.now();
  if (!options.force && lobbiesCache.value && lobbiesCache.expiresAt > now) {
    trace("lobby-client", "list_lobbies_cache_hit", { count: lobbiesCache.value.length });
    return lobbiesCache.value;
  }
  if (!options.force && lobbiesCache.inFlight) {
    trace("lobby-client", "list_lobbies_deduped");
    return lobbiesCache.inFlight;
  }
  trace("lobby-client", "list_lobbies_start");
  lobbiesCache.inFlight = (async () => {
    const data = await apiJson<{ lobbies?: LobbySummary[] }>("/api/lobbies");
    const lobbies = Array.isArray(data?.lobbies) ? data.lobbies : [];
    lobbiesCache.value = lobbies;
    lobbiesCache.expiresAt = Date.now() + LOBBIES_CACHE_TTL_MS;
    trace("lobby-client", "list_lobbies_success", { count: lobbies.length });
    return lobbies;
  })();
  try {
    return await lobbiesCache.inFlight;
  } catch (error) {
    traceError("lobby-client", "list_lobbies_failed", error);
    throw error;
  } finally {
    lobbiesCache.inFlight = undefined;
  }
};

export const joinLobby = async (lobby: LobbySummary, password?: string): Promise<void> => {
  trace("lobby-client", "join_lobby_start", { lobbyId: lobby.id, private: lobby.is_private === true });
  const payload: Record<string, unknown> = {};
  if (password) payload.password = password;

  const doJoin = async (forceAbandon: boolean) => {
    const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobby.id)}/join`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ...payload, force_abandon: forceAbandon }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      const error = String((data as any)?.error || "Unable to join lobby.");
      const reason = String((data as any)?.reason || "");
      const needsConfirm = (data as any)?.requires_confirmation === true || reason === "active_game_conflict";
      return { ok: false as const, error, needsConfirm };
    }
    return { ok: true as const };
  };

  const first = await doJoin(false);
  if (first.ok) {
    clearLobbiesCache();
    trace("lobby-client", "join_lobby_success", { lobbyId: lobby.id, forceAbandon: false });
    return;
  }
  if (!first.needsConfirm) throw new Error(first.error);
  if (!window.confirm(`${first.error}\n\nContinue and slow-release the previous room?`)) return;
  const second = await doJoin(true);
  if (!second.ok) throw new Error(second.error);
  clearLobbiesCache();
  trace("lobby-client", "join_lobby_success", { lobbyId: lobby.id, forceAbandon: true });
};

export const createLobby = async (input: CreateLobbyInput): Promise<{ lobbyId?: string }> => {
  trace("lobby-client", "create_lobby_start", {
    hasName: Boolean(input.name.trim()),
    private: Boolean(input.server_password),
    itemCheat: Boolean(input.item_cheat),
  });
  const response = await apiFetch("/api/lobbies", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      name: input.name.trim(),
      description: String(input.description || "").trim(),
      server_password: String(input.server_password || ""),
      max_players: String(input.max_players || "50"),
      spoiler: String(input.spoiler || "off"),
      item_cheat: Boolean(input.item_cheat),
      asynchronous: Boolean(input.asynchronous),
      is_async: Boolean(input.asynchronous),
      room_type: input.asynchronous ? "async" : "active",
      mode: input.asynchronous ? "async" : "active",
      release_mode: "auto",
      collect_mode: "auto",
      remaining_mode: "auto",
      countdown_mode: "auto",
      hint_cost: "5",
      plando_items: false,
      plando_bosses: false,
      plando_connections: false,
      plando_texts: false,
      allow_custom_yamls: false,
    }),
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    const error = String((data as any)?.error || "Unable to create lobby.");
    trace("lobby-client", "create_lobby_failed", { status: response.status, error }, "warn");
    if (error === "async_room_requires_patreon_tier_2_or_3") {
      throw new Error("Async rooms require an active Patreon Tier 2 (Super Supporter) or Tier 3 (Ultra Supporter) membership.");
    }
    throw new Error(error);
  }
  const lobbyId = String((data as any)?.lobby_id || (data as any)?.id || parseLobbyIdFromUrl((data as any)?.url));
  clearLobbiesCache();
  trace("lobby-client", "create_lobby_success", { lobbyId });
  return { lobbyId: lobbyId || undefined };
};
