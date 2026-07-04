export type RoomServerPlayer = {
  slot?: number;
  name?: string;
  game?: string;
};

export type RoomServerDownload = {
  slot?: number;
  download?: string;
};

export type RoomServerLaunchEntry = {
  entry_id?: string;
  user_id?: string;
  username?: string;
  config_id?: string;
  title?: string;
  game?: string;
  slot?: number;
  slot_name?: string;
  artifact_member?: string;
  artifact_extension?: string;
  artifact_kind?: string;
  sync_package_path?: string;
  download?: string;
};

export type RoomServerStatus = {
  tracker?: string;
  room_id?: string;
  id?: string;
  room_url?: string;
  players?: RoomServerPlayer[];
  last_port?: number;
  room_port?: number;
  room_host?: string;
  downloads?: RoomServerDownload[];
  launch_entries?: RoomServerLaunchEntry[];
};

export type RoomServerGeneration = {
  lobby_id?: string;
  generation_id?: string;
  sync_id?: string;
  room_url?: string;
  room_server_url?: string;
  response?: Record<string, unknown> & {
    lobby_id?: string;
    generation_id?: string;
    sync_id?: string;
    room_url?: string;
    room_server_url?: string;
  };
};

type FetchRoomStatusOptions = {
  roomUrl?: string;
  retries?: number;
  fetchJson: <T>(path: string) => Promise<T>;
  onStatus?: (roomStatus: RoomServerStatus | null, roomId: string) => void;
  retryDelayMs?: number;
};

type ProbeRoomServerOptions = {
  roomUrl?: string;
  fetchRoomStatus: (roomUrl?: string, retries?: number) => Promise<RoomServerStatus | null>;
  attempts?: number;
  pollDelayMs?: number;
};

type ResolveLaunchRoomServerOptions<TGeneration extends RoomServerGeneration> = {
  roomStatus?: RoomServerStatus | null;
  generation?: TGeneration | null;
  roomId?: string;
  host: string;
  fetchRoomStatus: (roomUrl?: string, retries?: number) => Promise<RoomServerStatus | null>;
  loadLatestGeneration: () => Promise<TGeneration | null>;
  onGenerationResolved?: (generation: TGeneration) => void;
  launchRetries?: number;
};

const sleep = (delayMs: number) =>
  new Promise((resolve) => window.setTimeout(resolve, delayMs));

export const extractRoomIdFromUrl = (roomUrl?: string): string => {
  const raw = String(roomUrl || "").trim();
  if (!raw) return "";
  try {
    const parsed = new URL(raw, window.location.origin);
    const parts = parsed.pathname.split("/").filter(Boolean);
    return parts[parts.length - 1] || "";
  } catch {
    const parts = raw.split("?")[0].split("#")[0].split("/").filter(Boolean);
    return parts[parts.length - 1] || "";
  }
};

export const isRoomServerReady = (roomStatus?: RoomServerStatus | null) =>
  Number(roomStatus?.last_port || 0) > 0;

export const roomStatusMatchesUrl = (roomStatusRoomId?: string, roomUrl?: string) => {
  const roomId = extractRoomIdFromUrl(roomUrl);
  return Boolean(roomId && roomStatusRoomId && roomId === roomStatusRoomId);
};

export const generationRoomUrl = (generation?: RoomServerGeneration | null) =>
  String(
    generation?.room_url ||
      generation?.room_server_url ||
      generation?.response?.room_url ||
      generation?.response?.room_server_url ||
      ""
  ).trim();

const looksLikeRoomRoute = (value?: string) => {
  const raw = String(value || "").trim();
  if (!raw) return false;
  if (raw.startsWith("/rooms/")) return true;
  try {
    const parsed = new URL(raw, window.location.origin);
    return parsed.pathname.split("/").filter(Boolean)[0] === "rooms";
  } catch {
    return raw.split("?")[0].split("#")[0].split("/").filter(Boolean)[0] === "rooms";
  }
};

export const generationRoomLookupUrl = (generation?: RoomServerGeneration | null, roomId?: string) => {
  const explicitRoomId = String(
    roomId ||
      generation?.lobby_id ||
      generation?.sync_id ||
      generation?.response?.lobby_id ||
      generation?.response?.sync_id ||
      ""
  ).trim();
  if (explicitRoomId) return `/rooms/${explicitRoomId}`;
  const candidates = [
    generation?.room_server_url,
    generation?.response?.room_server_url,
    generation?.room_url,
    generation?.response?.room_url,
  ];
  return String(candidates.find((candidate) => looksLikeRoomRoute(candidate)) || "").trim();
};

const roomStatusIdentity = (roomStatus?: RoomServerStatus | null) =>
  String(roomStatus?.room_id || roomStatus?.id || roomStatus?.tracker || roomStatus?.room_url || "").trim();

export const roomStatusBelongsToGeneration = (
  roomStatus?: RoomServerStatus | null,
  generation?: RoomServerGeneration | null
) => {
  const roomUrl = generationRoomLookupUrl(generation) || generationRoomUrl(generation);
  if (!roomUrl) return true;
  const expectedRoomId = extractRoomIdFromUrl(roomUrl);
  if (!expectedRoomId) return true;
  const statusIdentity = roomStatusIdentity(roomStatus);
  if (!statusIdentity) return false;
  if (statusIdentity === expectedRoomId) return true;
  if (extractRoomIdFromUrl(statusIdentity) === expectedRoomId) return true;
  return statusIdentity.includes(expectedRoomId);
};

export const buildRoomServerAddress = (host: string, roomStatus?: RoomServerStatus | null) =>
  isRoomServerReady(roomStatus)
    ? `${normalizeRoomServerHost(String(roomStatus?.room_host || host || ""))}:${Number(roomStatus?.last_port || 0)}`
    : "";

const normalizeRoomServerHost = (host: string) => {
  const raw = String(host || "").trim();
  if (!raw) return "";
  try {
    return new URL(raw.includes("://") ? raw : `http://${raw}`).hostname;
  } catch {
    return raw.split(":")[0] || raw;
  }
};

export const resolveRoomServerHost = (toApiUrl: (path: string) => string) => {
  try {
    return new URL(toApiUrl("/"), window.location.origin).hostname;
  } catch {
    return window.location.hostname;
  }
};

export const fetchRoomStatusByUrl = async ({
  roomUrl,
  retries = 1,
  fetchJson,
  onStatus,
  retryDelayMs = 500,
}: FetchRoomStatusOptions): Promise<RoomServerStatus | null> => {
  const roomId = extractRoomIdFromUrl(roomUrl);
  if (!roomId) {
    onStatus?.(null, "");
    return null;
  }
  for (let attempt = 0; attempt < retries; attempt += 1) {
    try {
      const roomStatus = await fetchJson<RoomServerStatus>(`/api/room_status/${roomId}`);
      onStatus?.(roomStatus, roomId);
      return roomStatus;
    } catch {
      if (attempt < retries - 1) {
        await sleep(retryDelayMs);
      }
    }
  }
  return null;
};

export const probeRoomServerStatus = async ({
  roomUrl,
  fetchRoomStatus,
  attempts = 45,
  pollDelayMs = 1000,
}: ProbeRoomServerOptions): Promise<RoomServerStatus | null> => {
  let roomStatus: RoomServerStatus | null = null;
  for (let attempt = 0; attempt < attempts; attempt += 1) {
    roomStatus = await fetchRoomStatus(roomUrl, 1);
    if (isRoomServerReady(roomStatus)) return roomStatus;
    await sleep(pollDelayMs);
  }
  return roomStatus;
};

export const resolveLaunchRoomServer = async <TGeneration extends RoomServerGeneration>({
  roomStatus,
  generation,
  roomId,
  host,
  fetchRoomStatus,
  loadLatestGeneration,
  onGenerationResolved,
  launchRetries = 10,
}: ResolveLaunchRoomServerOptions<TGeneration>) => {
  let resolvedGeneration = generation || null;
  let resolvedRoomStatus = roomStatus && roomStatusBelongsToGeneration(roomStatus, resolvedGeneration)
    ? roomStatus
    : null;

  if (!isRoomServerReady(resolvedRoomStatus)) {
    const roomUrl = generationRoomLookupUrl(resolvedGeneration, roomId);
    if (roomUrl) {
      resolvedRoomStatus = await fetchRoomStatus(roomUrl, launchRetries);
    } else {
      const latestGeneration = await loadLatestGeneration();
      if (latestGeneration) {
        resolvedGeneration = latestGeneration;
        onGenerationResolved?.(latestGeneration);
        const latestRoomUrl = generationRoomLookupUrl(latestGeneration, roomId);
        if (latestRoomUrl) {
          resolvedRoomStatus = await fetchRoomStatus(latestRoomUrl, launchRetries);
        }
      }
    }
  }

  return {
    generation: resolvedGeneration,
    roomStatus: resolvedRoomStatus,
    serverAddress: buildRoomServerAddress(host, resolvedRoomStatus),
  };
};
