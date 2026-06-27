export type LobbyRoomPlayer = {
  slot?: number;
  name?: string;
  game?: string;
};

export type LobbyRoomDownload = {
  slot?: number;
  download?: string;
  artifact_extension?: string;
  artifact_kind?: string;
  artifact_member?: string;
};

export type LobbyRoomLaunchEntry = {
  entry_id?: string;
  user_id?: string;
  username?: string;
  config_id?: string;
  title?: string;
  game?: string;
  game_key?: string;
  game_display_name?: string;
  display_game?: string;
  display_player?: string;
  slot?: number;
  slot_name?: string;
  compat_player_name?: string;
  artifact_member?: string;
  artifact_extension?: string;
  artifact_kind?: string;
  tracker_variant?: string;
  trackerVariant?: string;
  tracker_game_id?: string;
  trackerGameId?: string;
  sync_package_path?: string;
  download?: string;
};

export type LobbyRoomStatusShape = {
  players?: LobbyRoomPlayer[];
  downloads?: LobbyRoomDownload[];
  launch_entries?: LobbyRoomLaunchEntry[];
  last_port?: number;
  room_port?: number;
  room_host?: string;
};

export type LobbySelectionShape = {
  id?: string;
  config_id?: string;
  yaml_id?: string;
  game?: string;
  player_name?: string;
  title?: string;
  tracker_variant?: string;
  trackerVariant?: string;
  tracker_game_id?: string;
  trackerGameId?: string;
};

export type SelfLaunchContext = {
  /** AP slot name. Kept as playerName for older call sites, but this must never be an account/display name. */
  playerName: string;
  slotName: string;
  accountName: string;
  slotId?: number;
  downloadUrl?: string;
  downloadUrls: string[];
  apGameName: string;
  matched: boolean;
  matchError?: string;
};

const knownNonLaunchExtensions = new Set([
  ".archipelago",
  ".txt",
  ".html",
  ".htm",
  ".json",
  ".yaml",
  ".yml",
  ".log",
  ".zip",
]);

const entryExtension = (entry: { artifact_extension?: string; artifact_member?: string; download?: string }) => {
  const explicit = String(entry.artifact_extension || "").trim().toLowerCase();
  if (explicit) return explicit.startsWith(".") ? explicit : `.${explicit}`;
  const raw = String(entry.artifact_member || entry.download || "").trim();
  if (!raw) return "";
  const clean = raw.split("?")[0]?.split("#")[0] || raw;
  const last = clean.split("/").pop() || "";
  const dot = last.lastIndexOf(".");
  return dot >= 0 ? last.slice(dot).toLowerCase() : "";
};

export const isLaunchArtifactEntry = (entry: LobbyRoomDownload | LobbyRoomLaunchEntry | undefined) => {
  if (!entry?.download) return false;
  const kind = String(entry.artifact_kind || "").trim().toLowerCase();
  if (["patch", "apdelta", "rom", "slot"].includes(kind)) return true;
  if (["sync_package", "package", "multidata", "metadata", "none"].includes(kind)) return false;
  const ext = entryExtension(entry);
  return Boolean(ext && !knownNonLaunchExtensions.has(ext));
};

export const indexPlayersByName = (players?: LobbyRoomPlayer[]) => {
  const map = new Map<string, number>();
  (players || []).forEach((player) => {
    if (player?.name && typeof player.slot === "number") {
      map.set(player.name, player.slot);
      map.set(player.name.toLowerCase(), player.slot);
    }
  });
  return map;
};

export const indexDownloadsBySlot = (
  downloads: LobbyRoomDownload[] | undefined,
  toUrl: (download: string) => string
) => {
  const single = new Map<number, string>();
  const multi = new Map<number, string[]>();
  (downloads || []).forEach((entry) => {
    if (typeof entry?.slot !== "number" || !entry?.download) return;
    if (!isLaunchArtifactEntry(entry)) return;
    const url = toUrl(entry.download);
    single.set(entry.slot, url);
    const list = multi.get(entry.slot) || [];
    list.push(url);
    multi.set(entry.slot, list);
  });
  return { single, multi };
};

export const resolvePlayerSlotId = (playersByName: Map<string, number>, playerName?: string) => {
  const safeName = String(playerName || "").trim();
  if (!safeName) return undefined;
  return playersByName.get(safeName) || playersByName.get(safeName.toLowerCase());
};

export const resolvePlayerApGameName = (
  roomStatus: LobbyRoomStatusShape | null | undefined,
  slotId: number | undefined,
  playerName?: string
) => {
  const safeName = String(playerName || "").trim();
  return String(
    (roomStatus?.players || []).find((player) =>
      (typeof slotId === "number" ? player?.slot === slotId : false) || player?.name === safeName
    )?.game || ""
  ).trim();
};

const normalizeLookupKey = (value?: string) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "");

const firstSelectionId = (selection?: LobbySelectionShape | null) =>
  String(selection?.id || selection?.config_id || selection?.yaml_id || "").trim();

const entryLaunchSlotName = (entry?: LobbyRoomLaunchEntry) =>
  String(entry?.slot_name || entry?.compat_player_name || "").trim();

const scoreLaunchEntryForSelection = (
  entry: LobbyRoomLaunchEntry,
  options: {
    playerName?: string;
    selection?: LobbySelectionShape | null;
    slotId?: number;
  }
) => {
  let score = 0;
  const playerKey = normalizeLookupKey(options.playerName);
  const selectionId = firstSelectionId(options.selection);
  const selectionPlayerKey = normalizeLookupKey(options.selection?.player_name);
  const selectionGameKey = normalizeLookupKey(options.selection?.game);
  const selectionTitleKey = normalizeLookupKey(options.selection?.title);
  const entryConfigId = String(entry.config_id || "").trim();
  const entrySlotNameKey = normalizeLookupKey(entry.slot_name);
  const entryUsernameKey = normalizeLookupKey(entry.username);
  const entryGameKey = normalizeLookupKey(entry.game);
  const entryTitleKey = normalizeLookupKey(entry.title);
  const hasSelection = Boolean(selectionId || selectionGameKey || selectionTitleKey);

  if (selectionId && entryConfigId && selectionId === entryConfigId) score += 100;
  if (selectionPlayerKey && entrySlotNameKey && selectionPlayerKey === entrySlotNameKey) score += 80;
  if (selectionGameKey && entryGameKey && selectionGameKey === entryGameKey) score += 15;
  if (selectionTitleKey && entryTitleKey && selectionTitleKey === entryTitleKey) score += 10;
  if (!hasSelection && playerKey && entrySlotNameKey && playerKey === entrySlotNameKey) score += 70;
  if (!hasSelection && playerKey && entryUsernameKey && playerKey === entryUsernameKey) score += 50;
  if (typeof options.slotId === "number" && typeof entry.slot === "number" && options.slotId === entry.slot) score += 25;
  return score;
};

export const buildSelfLaunchContext = (options: {
  roomStatus?: LobbyRoomStatusShape | null;
  playerName?: string;
  selection?: LobbySelectionShape | null;
  toUrl?: (download: string) => string;
  downloadsBySlot: Map<number, string>;
  downloadsBySlotMulti: Map<number, string[]>;
  playersByName: Map<string, number>;
}) => {
  const accountName = String(options.playerName || "").trim();
  const slotId = resolvePlayerSlotId(options.playersByName, accountName);
  const hasSelection = Boolean(
    firstSelectionId(options.selection) ||
      normalizeLookupKey(options.selection?.game) ||
      normalizeLookupKey(options.selection?.title)
  );
  const launchEntries = Array.isArray(options.roomStatus?.launch_entries) ? options.roomStatus?.launch_entries || [] : [];
  const scoredEntries = launchEntries
    .map((entry) => ({
      entry,
      score: scoreLaunchEntryForSelection(entry, {
        playerName: accountName,
        selection: options.selection,
        slotId,
      }),
    }))
    .filter((entry) => entry.score > 0)
    .sort((a, b) => b.score - a.score);
  const bestScore = scoredEntries[0]?.score || 0;
  const selectedEntries = bestScore > 0 ? scoredEntries.filter((entry) => entry.score === bestScore).map((entry) => entry.entry) : [];
  const launchableSelectedEntries = selectedEntries.filter((entry) => isLaunchArtifactEntry(entry));
  const slotKeysForSelection = new Set(
    (launchableSelectedEntries.length ? launchableSelectedEntries : selectedEntries)
      .map((entry) => String(entry.slot ?? entryLaunchSlotName(entry) ?? "").trim())
      .filter(Boolean)
  );
  const ambiguousSelection = hasSelection && slotKeysForSelection.size > 1;
  const selectedSlotId =
    !ambiguousSelection && selectedEntries.length > 0
      ? selectedEntries.find((entry) => typeof entry.slot === "number")?.slot
      : hasSelection
        ? undefined
        : slotId;
  const selectedLaunchSlotName =
    (!ambiguousSelection ? selectedEntries.map(entryLaunchSlotName).find(Boolean) : "") ||
    (typeof selectedSlotId === "number"
      ? String((options.roomStatus?.players || []).find((player) => player?.slot === selectedSlotId)?.name || "").trim()
      : "");
  const selectedDownloads = launchableSelectedEntries
    .map((entry) => String(entry.download || "").trim())
    .filter(Boolean)
    .map((download) => (options.toUrl ? options.toUrl(download) : download));
  const slotDownloadUrl = typeof selectedSlotId === "number" ? options.downloadsBySlot.get(selectedSlotId) : undefined;
  const slotDownloadUrls = typeof selectedSlotId === "number" ? options.downloadsBySlotMulti.get(selectedSlotId) || [] : [];
  const downloadUrls = selectedDownloads.length ? selectedDownloads : slotDownloadUrls;
  const downloadUrl = downloadUrls[0] || slotDownloadUrl;
  const apGameName =
    (!ambiguousSelection ? String(selectedEntries.find((entry) => entry.game)?.game || "").trim() : "") ||
    resolvePlayerApGameName(options.roomStatus, selectedSlotId, selectedLaunchSlotName);
  const matched = Boolean(selectedLaunchSlotName && typeof selectedSlotId === "number" && !ambiguousSelection);
  return {
    playerName: selectedLaunchSlotName,
    slotName: selectedLaunchSlotName,
    accountName,
    slotId: selectedSlotId,
    downloadUrl,
    downloadUrls,
    apGameName,
    matched,
    matchError: ambiguousSelection
      ? "ambiguous_launch_entry"
      : hasSelection && !matched
        ? "launch_entry_not_found"
        : undefined,
  } satisfies SelfLaunchContext;
};
