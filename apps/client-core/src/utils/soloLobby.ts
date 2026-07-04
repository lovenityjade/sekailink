export const SOLO_LOBBY_MARKER = "__SKL_SOLO__";

export const markSoloDescription = (description: string): string => {
  const trimmed = String(description || "").trim();
  if (!trimmed) return SOLO_LOBBY_MARKER;
  if (trimmed.startsWith(SOLO_LOBBY_MARKER)) return trimmed;
  return `${SOLO_LOBBY_MARKER} ${trimmed}`;
};

export const isSoloLobby = (lobby?: { description?: string | null } | null): boolean => {
  const description = String(lobby?.description || "").trim();
  return description.startsWith(SOLO_LOBBY_MARKER);
};

export const cleanSoloDescription = (description?: string | null): string => {
  const raw = String(description || "");
  if (!raw.trim().startsWith(SOLO_LOBBY_MARKER)) return raw;
  return raw.replace(SOLO_LOBBY_MARKER, "").trim();
};
