import { apiFetch, apiJson } from "./api";

export interface AdminMe {
  discord_id: string;
  display_name: string;
  role: string;
  is_admin: boolean;
  email?: string;
  last_login?: string | null;
}

export interface ServiceStatus {
  key: string;
  unit: string;
  active: string;
  enabled: string;
  substate: string;
  pid: number;
  uptime_seconds: number;
  ok: boolean;
}

export interface SystemHealth {
  timestamp: string;
  host: string;
  app_version: string;
  services: Record<string, ServiceStatus>;
  metrics: {
    cpu_count: number;
    loadavg: number[];
    uptime_seconds: number;
    mem_total_bytes: number;
    mem_available_bytes: number;
    disk_total_bytes: number;
    disk_free_bytes: number;
  };
}

export interface SteamGridAsset {
  id: number;
  url: string;
  thumb?: string;
  width?: number;
  height?: number;
  mime?: string;
  style?: string;
  score?: number;
}

export interface GameBoxArtMapping {
  game_id: string;
  display_name?: string;
  provider?: string;
  provider_asset_id?: string;
  image_url: string;
  thumb_url?: string;
  updated_at?: string;
}

export const adminApi = {
  getMe: () => apiJson<AdminMe>("/api/admin/me"),
  getUsers: (search = "") => apiJson<{ users: Array<Record<string, unknown>> }>(`/api/admin/users${search ? `?search=${encodeURIComponent(search)}` : ""}`),
  setUserRole: (discordId: string, role: string) => apiJson<{ status: string }>(`/api/admin/users/${encodeURIComponent(discordId)}/role`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ role })
  }),
  setUserBan: (discordId: string, banned: boolean, reason: string) => apiJson<{ status: string }>(`/api/admin/users/${encodeURIComponent(discordId)}/ban`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ banned, reason })
  }),
  getLobbies: () => apiJson<{ lobbies: Array<Record<string, unknown>> }>("/api/admin/lobbies"),
  getLobby: (id: string) => apiJson<Record<string, unknown>>(`/api/admin/lobbies/${encodeURIComponent(id)}`),
  closeLobby: (id: string) => apiJson<{ status: string }>(`/api/admin/lobbies/${encodeURIComponent(id)}/close`, { method: "POST" }),
  getRooms: () => apiJson<{ rooms: Array<Record<string, unknown>> }>("/api/admin/rooms"),
  closeRoom: (id: string) => apiJson<{ status: string }>(`/api/admin/rooms/${encodeURIComponent(id)}/close`, { method: "POST" }),
  purgeRooms: () => apiJson<{ status: string; closed: number }>("/api/admin/rooms/purge", { method: "POST" }),
  purgeRoomsFiltered: (payload: Record<string, unknown>) => apiJson<{ status: string; closed: number; matched: number }>("/api/admin/rooms/purge-filtered", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  }),
  getLogs: () => apiJson<{ logs: Array<Record<string, unknown>> }>("/api/admin/logs"),
  getLogContent: (name: string, lines = 300) => apiJson<{ name: string; content: string }>(`/api/admin/logs/${encodeURIComponent(name)}?lines=${encodeURIComponent(String(lines))}`),
  getSupport: (category = "") => apiJson<{ tickets: Array<Record<string, unknown>> }>(`/api/admin/support${category ? `?category=${encodeURIComponent(category)}` : ""}`),
  updateSupport: (id: number, status: "open" | "closed", response = "") => apiJson<{ status: string }>(`/api/admin/support/${id}/status`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ status, response })
  }),
  getDbEntity: (entity: "users" | "lobbies" | "rooms" | "generations" | "tickets", limit = 100, offset = 0) =>
    apiJson<{ entity: string; limit: number; offset: number; total: number; rows: Array<Record<string, unknown>> }>(
      `/api/admin/db/${encodeURIComponent(entity)}?limit=${encodeURIComponent(String(limit))}&offset=${encodeURIComponent(String(offset))}`
    ),
  getSystemHealth: () => apiJson<SystemHealth>("/api/admin/system/health"),
  restartService: (service: "webhost" | "workers") => apiJson<{ status: string; service: string }>(`/api/admin/system/services/${service}/restart`, {
    method: "POST"
  }),
  getJournal: (service: "webhost" | "workers", lines = 200) => apiJson<{ service: string; lines: number; content: string }>(`/api/admin/system/journal?service=${encodeURIComponent(service)}&lines=${encodeURIComponent(String(lines))}`),
  rebootHost: (reason: string, confirm = "REBOOT") => apiJson<{ status: string }>("/api/admin/system/reboot", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ reason, confirm })
  }),
  getClientGames: () => apiJson<{ games: Array<{ game_id: string; display_name: string }> }>("/api/client/games"),
  getGameBoxArtMappings: () => apiJson<{ items: GameBoxArtMapping[] }>("/api/admin/boxart"),
  searchSteamGridDB: (query: string, kind: "grid" | "hero" | "logo" = "grid", limit = 24) =>
    apiJson<{ items: SteamGridAsset[] }>(
      `/api/admin/boxart/steamgriddb/search?q=${encodeURIComponent(query)}&kind=${encodeURIComponent(kind)}&limit=${encodeURIComponent(String(limit))}`
    ),
  setGameBoxArtMapping: (gameId: string, payload: Record<string, unknown>) =>
    apiJson<{ status: string }>(`/api/admin/boxart/${encodeURIComponent(gameId)}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    }),
  clearGameBoxArtMapping: (gameId: string) =>
    apiJson<{ status: string }>(`/api/admin/boxart/${encodeURIComponent(gameId)}`, {
      method: "DELETE"
    }),
  desktopCallback: async (code: string, state: string) => {
    const res = await apiFetch("/api/auth/desktop-callback", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ code, state })
    });
    return res;
  }
};
