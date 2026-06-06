import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { apiCurrentUser, apiFetch, apiJson, apiUrl } from "../services/api";
import { chatService } from "../services/chatService";
import { getLaunchErrorMessage, type LaunchRecoveryAction } from "../services/launchRecovery";
import { getLaunchReadiness } from "../services/launchReadiness";
import {
  buildSelfLaunchContext,
  indexDownloadsBySlot,
  indexPlayersByName,
  isLaunchArtifactEntry,
  resolvePlayerApGameName,
  resolvePlayerSlotId,
} from "../services/lobbyLaunchContext";
import { executeRoomSessionLaunch } from "../services/roomSessionLaunch";
import {
  buildRoomServerAddress,
  fetchRoomStatusByUrl,
  isRoomServerReady,
  probeRoomServerStatus,
  resolveRoomServerHost,
  roomStatusMatchesUrl,
} from "../services/roomServerContext";
import SessionLaunchModals from "../components/SessionLaunchModals";
import { useSessionLaunchState } from "../hooks/useSessionLaunchState";
import { useInterval } from "../hooks/useInterval";
import { useSfx } from "../hooks/useSfx";
import TrackerPanel from "../components/TrackerPanel";
import { runtime, type RuntimeModuleInfo } from "../services/runtime";
import { formatLocalTime, parseServerDate } from "../utils/time";
import { cleanSoloDescription, isSoloLobby } from "../utils/soloLobby";
import { useI18n } from "../i18n";

interface LobbySummary {
  id: string;
  name: string;
  description?: string;
  owner?: string;
  created_at?: string;
  is_private?: boolean;
  allow_custom_yamls?: boolean;
  release_mode?: string;
  collect_mode?: string;
  remaining_mode?: string;
  countdown_mode?: string;
  item_cheat?: boolean;
  spoiler?: number;
  hint_cost?: number;
  max_players?: number;
  plando_items?: boolean;
  plando_bosses?: boolean;
  plando_connections?: boolean;
  plando_texts?: boolean;
  discord_voice_url?: string;
}

interface LobbyMessage {
  id: number | string;
  author: string;
  avatar_url?: string;
  content: string;
  created_at: string;
  pending?: boolean;
}

const normalizeLobbyChatMessage = (message: any): LobbyMessage => ({
  id: String(message?.id || `msg-${Date.now()}-${Math.random().toString(16).slice(2)}`),
  author: String(message?.author || message?.display_name || message?.username || "System"),
  avatar_url: typeof message?.avatar_url === "string" ? message.avatar_url : "",
  content: String(message?.content || ""),
  created_at: String(message?.created_at || new Date().toISOString()),
});

interface LobbyMember {
  name: string;
  discord_id?: string;
  selections?: Array<{
    game?: string;
    linkedworld_id?: string;
    configs?: Array<{ id?: string; yaml_id?: string; title?: string; player_name?: string; custom?: boolean }>;
  }>;
  selection_summary?: string;
  active_yaml_id?: string;
  active_yaml_title?: string;
  active_yaml_game?: string;
  active_yaml_player?: string;
  active_yamls?: Array<{ id: string; title: string; game: string; player_name: string; custom?: boolean }>;
  ready?: boolean;
  online?: boolean;
  is_host?: boolean;
  irc_op?: boolean;
  irc_voice?: boolean;
  role?: string;
  local_ready_known?: boolean;
  local_ready?: boolean;
  local_ready_note?: string;
}

interface YamlEntry {
  id: string;
  title: string;
  game: string;
  player_name: string;
  custom?: boolean;
}

interface GameConfigGroup {
  id: string;
  game: string;
  configs: YamlEntry[];
}

interface SyncLaunchEntry {
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
}

interface GenerationStatus {
  status: string;
  error?: string;
  seed_url?: string;
  room_url?: string;
  completed_at?: string;
  artifact_path?: string;
  artifact_ready?: boolean;
  sync_package_path?: string;
  sync_package_ready?: boolean;
  sync_entries?: unknown[];
  launch_entries?: SyncLaunchEntry[];
  room_status?: string;
  room_host?: string;
  room_runtime_log?: string;
  output_dir?: string;
  response?: {
    error?: string;
    status?: string;
    artifact_path?: string;
    sync_package_path?: string;
    sync_package_ready?: boolean;
    sync_entries?: unknown[];
    launch_entries?: SyncLaunchEntry[];
    room_status?: string;
    room_host?: string;
    room_runtime_log?: string;
    output_dir?: string;
  };
}

type GenerationModalPhase =
  | "started"
  | "queued"
  | "completed"
  | "waiting_room_server"
  | "room_server_available"
  | "error";

interface GenerationProgressState {
  open: boolean;
  phase: GenerationModalPhase;
  title: string;
  message: string;
  progress: number;
  errorCode?: string;
  errorDescription?: string;
}

interface RoomStatus {
  tracker?: string;
  players?: Array<{ slot?: number; name?: string; game?: string }>;
  last_port?: number;
  room_port?: number;
  room_host?: string;
  downloads?: Array<{ slot?: number; download?: string }>;
  launch_entries?: SyncLaunchEntry[];
}

interface TrackerStats {
  players_total?: number;
  checks_done?: number;
  total_locations?: number;
  completed_players?: number;
}

interface MeResponse {
  discord_id: string;
  username?: string;
  global_name?: string;
  avatar_url?: string;
}

interface SocialFriend {
  user_id: string;
}

interface SocialRequest {
  id: number;
  from_id?: string;
  to_id?: string;
}

interface LobbyProfileData {
  display_name: string;
  status: string;
  bio: string;
  badges: string[];
  user_id?: string;
}

interface HintCatalogResponse {
  game?: string;
  player_name?: string;
  items?: string[];
  locations?: string[];
}

type LobbyLaunchReadinessCard = {
  ready: boolean;
  label: string;
  note: string;
  errorText?: string;
  action?: LaunchRecoveryAction | null;
};

type LobbyPresenceState = {
  role?: string;
  ready?: boolean;
  local_ready_known?: boolean;
  local_ready?: boolean;
  local_ready_note?: string;
};

const lobbyMemberIdentity = (member: Partial<LobbyMember>) =>
  String(member.discord_id || member.name || "")
    .trim()
    .toLowerCase();

const lobbyMemberNameIdentity = (member: Partial<LobbyMember>) =>
  String(member.name || "")
    .trim()
    .toLowerCase();

const mergeLobbyMember = (base: LobbyMember | undefined, next: LobbyMember): LobbyMember => {
  if (!base) return next;
  return {
    ...base,
    ...next,
    name: next.name || base.name,
    discord_id: next.discord_id || base.discord_id,
    selections: next.selections?.length ? next.selections : base.selections,
    selection_summary: next.selection_summary || base.selection_summary,
    active_yaml_id: next.active_yaml_id || base.active_yaml_id,
    active_yaml_title: next.active_yaml_title || base.active_yaml_title,
    active_yaml_game: next.active_yaml_game || base.active_yaml_game,
    active_yaml_player: next.active_yaml_player || base.active_yaml_player,
    active_yamls: next.active_yamls?.length ? next.active_yamls : base.active_yamls,
    ready: Boolean(base.ready || next.ready),
    online: Boolean(base.online || next.online),
    is_host: Boolean(base.is_host || next.is_host),
    irc_op: Boolean(base.irc_op || next.irc_op),
    irc_voice: Boolean(base.irc_voice || next.irc_voice),
    role: next.role || base.role,
    local_ready_known: typeof next.local_ready_known === "boolean" ? next.local_ready_known : base.local_ready_known,
    local_ready: typeof next.local_ready === "boolean" ? next.local_ready : base.local_ready,
    local_ready_note: next.local_ready_note || base.local_ready_note,
  };
};

const dedupeLobbyMembers = (members: LobbyMember[]) => {
  const merged = new Map<string, LobbyMember>();
  const nameToKey = new Map<string, string>();
  members.forEach((member) => {
    const rawKey = lobbyMemberIdentity(member);
    const nameKey = lobbyMemberNameIdentity(member);
    const key = (nameKey && nameToKey.get(nameKey)) || rawKey || nameKey;
    if (!key) return;
    merged.set(key, mergeLobbyMember(merged.get(key), member));
    if (nameKey) nameToKey.set(nameKey, key);
  });
  return Array.from(merged.values()).sort((a, b) => {
    if (a.is_host !== b.is_host) return a.is_host ? -1 : 1;
    return String(a.name || "").localeCompare(String(b.name || ""), undefined, { sensitivity: "base" });
  });
};

const lobbySelectionsForMember = (member?: Partial<LobbyMember> | null) => {
  const grouped = Array.isArray(member?.selections) ? member.selections : [];
  if (grouped.length) {
    return grouped.flatMap((group) => {
      const game = String(group?.game || "Unknown");
      const configs = Array.isArray(group?.configs) ? group.configs : [];
      return configs.map((config) => ({
        id: String(config?.id || config?.yaml_id || ""),
        title: String(config?.title || "Default"),
        game,
        player_name: String(config?.player_name || member?.name || "Player"),
        custom: Boolean(config?.custom),
      }));
    });
  }
  const list = Array.isArray(member?.active_yamls) ? member.active_yamls : [];
  if (list.length) return list;
  if (member?.active_yaml_id || member?.active_yaml_title || member?.active_yaml_game) {
    return [{
      id: String(member.active_yaml_id || ""),
      title: String(member.active_yaml_title || member.active_yaml_game || "Unknown"),
      game: String(member.active_yaml_game || "Unknown"),
      player_name: String(member.active_yaml_player || member?.name || "Player"),
      custom: true,
    }];
  }
  return [] as Array<{ id: string; title: string; game: string; player_name: string; custom?: boolean }>;
};

const summarizeSelectionSet = (entries: Array<{ title?: string; game?: string }>) => {
  if (!entries.length) return "No game selected";
  const gameCount = new Set(entries.map((entry) => String(entry.game || "Unknown").trim()).filter(Boolean)).size;
  if (entries.length === 1) {
    const only = entries[0];
    return `${String(only.game || "Unknown")} / ${String(only.title || "Default")}`;
  }
  return `${gameCount} game${gameCount > 1 ? "s" : ""} · ${entries.length} config${entries.length > 1 ? "s" : ""}`;
};

const selectionTooltip = (entries: Array<{ title?: string; game?: string }>) =>
  entries.length
    ? entries.map((entry) => `${String(entry.game || "Unknown")} / ${String(entry.title || "Default")}`).join("\n")
    : "No game selected";

const formatTime = (value?: string) => {
  return formatLocalTime(value);
};

const HINT_COST_OPTIONS = [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75];

const normalizeHintCost = (value?: number) => {
  if (typeof value !== "number" || Number.isNaN(value)) return "5";
  const clamped = Math.max(5, Math.min(75, Math.round(value / 5) * 5));
  return String(clamped);
};

const FORCE_TRACKER_VARIANT_PROMPT_KEY = "skl.forceTrackerVariantPrompt";
const PATCHLESS_AUTO_LAUNCH_AP_GAMES = new Set(["ship of harkinian"]);

const supportsPatchlessAutoLaunch = (apGameName?: string) =>
  Boolean(apGameName && PATCHLESS_AUTO_LAUNCH_AP_GAMES.has(apGameName.trim().toLowerCase()));

const normalizeGenerationStatus = (value?: string): string => {
  const raw = String(value || "").trim().toLowerCase();
  if (!raw) return "none";
  return raw;
};

const GENERATION_RUNNING_STATUSES = new Set(["queued", "in_progress", "running", "generating", "processing"]);
const GENERATION_SUCCESS_STATUSES = new Set(["done", "completed", "success", "succeeded"]);
const GENERATION_ERROR_STATUSES = new Set(["error", "failed"]);

const isGenerationRunningStatus = (value?: string) => GENERATION_RUNNING_STATUSES.has(normalizeGenerationStatus(value));
const isGenerationSuccessStatus = (value?: string) => GENERATION_SUCCESS_STATUSES.has(normalizeGenerationStatus(value));
const isGenerationErrorStatus = (value?: string) => GENERATION_ERROR_STATUSES.has(normalizeGenerationStatus(value));
const generationArtifactReady = (generation?: GenerationStatus | null) =>
  Boolean(
    generation?.sync_package_ready ||
      generation?.sync_package_path ||
      generation?.response?.sync_package_ready ||
      generation?.response?.sync_package_path ||
      generation?.artifact_ready ||
      generation?.artifact_path ||
      generation?.response?.artifact_path
  );

const parseGenerationError = (value?: string): { code: string; description: string } => {
  const description = String(value || "Generation failed.").trim() || "Generation failed.";
  const byTag = description.match(/\bcode\s*[:=]\s*([A-Za-z0-9_.-]+)/i)?.[1];
  const byPrefix = description.match(/^\s*([A-Za-z0-9_.-]+)\s*:/)?.[1];
  const byException = description.match(/\b([A-Za-z]+Error)\b/)?.[1];
  const picked = (byTag || byException || byPrefix || "GENERATION_FAILED").replace(/[^A-Za-z0-9_.-]/g, "_");
  const code = picked
    .replace(/([a-z0-9])([A-Z])/g, "$1_$2")
    .replace(/[.\-]+/g, "_")
    .toUpperCase();
  return { code, description };
};

const gameGroupId = (game?: string) =>
  String(game || "Unknown")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "") || "unknown";

const normalizeRuntimeLookupKey = (value?: string) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");

const localLaunchPresenceState = (
  readiness: LobbyLaunchReadinessCard | null | undefined,
  hasSelection: boolean
): Partial<Pick<LobbyPresenceState, "local_ready_known" | "local_ready" | "local_ready_note">> => {
  if (!hasSelection) {
    return {
      local_ready_known: true,
      local_ready: false,
      local_ready_note: "No local game setup selected for this room yet.",
    };
  }
  if (!readiness) return { local_ready_known: false };
  return {
    local_ready_known: true,
    local_ready: readiness.ready,
    local_ready_note: readiness.ready
      ? "Local setup ready."
      : String(readiness.errorText || readiness.note || "Local setup is incomplete."),
  };
};

const LobbyPage: React.FC = () => {
  const { t } = useI18n();
  const { lobbyId } = useParams<{ lobbyId: string }>();
  const navigate = useNavigate();
  const [lobby, setLobby] = useState<LobbySummary | null>(null);
  const soloMode = isSoloLobby(lobby);
  const [messages, setMessages] = useState<LobbyMessage[]>([]);
  const [members, setMembers] = useState<LobbyMember[]>([]);
  const [yamls, setYamls] = useState<YamlEntry[]>([]);
  const [activeYamls, setActiveYamls] = useState<YamlEntry[]>([]);
  const [ready, setReady] = useState(false);
  const [authed, setAuthed] = useState(false);
  const [joinRequired, setJoinRequired] = useState(false);
  const [passwordError, setPasswordError] = useState("");
  const [statusMessage, setStatusMessage] = useState("");
  const [selectionServerLocked, setSelectionServerLocked] = useState(false);
  const [lobbyPassword, setLobbyPassword] = useState("");
  const [toast, setToast] = useState<{ text: string; kind: "success" | "error" } | null>(null);
  const [generation, setGeneration] = useState<GenerationStatus | null>(null);
  const [generationProgress, setGenerationProgress] = useState<GenerationProgressState | null>(null);
  const [roomStatus, setRoomStatus] = useState<RoomStatus | null>(null);
  const [roomStatusRoomId, setRoomStatusRoomId] = useState<string>("");
  const [trackerStats, setTrackerStats] = useState<TrackerStats | null>(null);
  const [me, setMe] = useState<MeResponse | null>(null);
  const [isOwner, setIsOwner] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [yamlOpen, setYamlOpen] = useState(false);
  const [yamlModalSelection, setYamlModalSelection] = useState("");
  const [gameModalSelection, setGameModalSelection] = useState("");
  const [terminalOpen, setTerminalOpen] = useState(false);
  const [trackerOpen, setTrackerOpen] = useState(false);
  const [trackerPlayer, setTrackerPlayer] = useState<number | null>(null);
  // UI-only: replaces the three collapsible right-side boxes with a single tabbed panel.
  const [rightTab, setRightTab] = useState<"users" | "info">("users");
  const [localClientOpen, setLocalClientOpen] = useState(true);
  const [contextMenu, setContextMenu] = useState<{
    x: number;
    y: number;
    member: LobbyMember;
    slotId?: number;
    trackerAvailable: boolean;
    isSelf: boolean;
    canLaunch: boolean;
    downloadUrl?: string;
    downloadUrls?: string[];
  } | null>(null);
  const [blockedAuthors, setBlockedAuthors] = useState<string[]>(() => {
    try {
      const raw = window.localStorage.getItem("skl.blockedAuthors");
      const parsed = raw ? JSON.parse(raw) : [];
      return Array.isArray(parsed) ? parsed.map((v) => String(v).trim()).filter(Boolean) : [];
    } catch {
      return [];
    }
  });
  const [profileModal, setProfileModal] = useState<{
    open: boolean;
    loading: boolean;
    error: string;
    data: LobbyProfileData | null;
  }>({ open: false, loading: false, error: "", data: null });
  const [reportModal, setReportModal] = useState<{
    open: boolean;
    targetName: string;
    targetId?: string;
    reason: string;
    comment: string;
    submitting: boolean;
    error: string;
  }>({
    open: false,
    targetName: "",
    targetId: "",
    reason: "harassment",
    comment: "",
    submitting: false,
    error: "",
  });
  const [moderationModal, setModerationModal] = useState<{
    open: boolean;
    message: string;
    redirectUrl: string;
  }>({ open: false, message: "", redirectUrl: "/" });
  const [playerModalOpen, setPlayerModalOpen] = useState(false);
  const [playerModal, setPlayerModal] = useState<{
    name: string;
    yaml: string;
    slotId?: number;
    selections?: Array<{ id: string; title: string; game: string; player_name: string; custom?: boolean }>;
  } | null>(null);
  const [hintModalOpen, setHintModalOpen] = useState(false);
  const [hintMode, setHintMode] = useState<"item" | "location">("item");
  const [hintQuery, setHintQuery] = useState("");
  const [hintMessages, setHintMessages] = useState<Array<{ text: string; ts: number }>>([]);
  const [hintCatalog, setHintCatalog] = useState<HintCatalogResponse | null>(null);
  const [hintSelection, setHintSelection] = useState("");
  const [hintErrorModal, setHintErrorModal] = useState<{
    title: string;
    message: string;
  } | null>(null);
  const [apHintPoints, setApHintPoints] = useState<number | null>(null);
  const [apHintCost, setApHintCost] = useState<number | null>(null);
  const [apClientConnected, setApClientConnected] = useState(false);
  const [apBizhawkConnected, setApBizhawkConnected] = useState<"connected" | "tentative" | "not_connected" | "unknown">("unknown");
  const [apHandlerGame, setApHandlerGame] = useState<string>("");
  const [apLastError, setApLastError] = useState<string>("");
  const [apLogLines, setApLogLines] = useState<Array<{ text: string; ts: number; level: string }>>([]);
  const [chatInput, setChatInput] = useState("");
  const [lastMessageId, setLastMessageId] = useState(0);
  const [terminalInput, setTerminalInput] = useState("");
  const [terminalStatus, setTerminalStatus] = useState("");
  const [terminalLog, setTerminalLog] = useState("Waiting for local session logs…");
  const [sm64exTutorialOpen, setSm64exTutorialOpen] = useState(false);
  const [settingsStatus, setSettingsStatus] = useState("");
  const [seedTimer, setSeedTimer] = useState("-");
  const [generateConfirmOpen, setGenerateConfirmOpen] = useState(false);
  const [closeConfirmOpen, setCloseConfirmOpen] = useState(false);
  const [forceTrackerVariantPrompt, setForceTrackerVariantPrompt] = useState<boolean>(() => {
    try {
      return window.localStorage.getItem(FORCE_TRACKER_VARIANT_PROMPT_KEY) === "1";
    } catch (_err) {
      return false;
    }
  });
  const [inactivityModal, setInactivityModal] = useState<{
    mode: "probe" | "released";
    timeoutSeconds: number;
    playerName?: string;
  } | null>(null);
  const [hostStatus, setHostStatus] = useState<{
    host_absent?: boolean;
    host_absent_seconds?: number;
    candidate_name?: string;
    vote_count?: number;
    vote_needed?: number;
  } | null>(null);
  const [runtimeModules, setRuntimeModules] = useState<RuntimeModuleInfo[]>([]);
  const [resolvedLaunchModuleId, setResolvedLaunchModuleId] = useState("");
  const [localLaunchReadiness, setLocalLaunchReadiness] = useState<LobbyLaunchReadinessCard | null>(null);
  const [friendUserIds, setFriendUserIds] = useState<string[]>([]);
  const [incomingFriendIds, setIncomingFriendIds] = useState<string[]>([]);
  const [outgoingFriendIds, setOutgoingFriendIds] = useState<string[]>([]);
  const lastToastSoundAtRef = useRef(0);
  const joinedGameNoticeRef = useRef<Record<string, boolean>>({});
  const lastGenerationStateRef = useRef("");
  const generationHideTimerRef = useRef<number | null>(null);
  const generationRoomProbeRef = useRef<{ key: string; inFlight: boolean }>({ key: "", inFlight: false });
  const sfx = useSfx();
  const selfMember = members.find((member) => member.discord_id === me?.discord_id);
  const selfSelections = lobbySelectionsForMember(selfMember).length ? lobbySelectionsForMember(selfMember) : activeYamls;
  const localPresenceState = useMemo(
    () => localLaunchPresenceState(localLaunchReadiness, Boolean(selfSelections.length)),
    [localLaunchReadiness, selfSelections.length]
  );

  const showToast = (text: string, kind: "success" | "error" = "success") => {
    if (!text) return;
    const now = Date.now();
    if (now - lastToastSoundAtRef.current > 650) {
      if (kind === "error") sfx.play("error", 0.35);
      else sfx.play("success", 0.25);
      lastToastSoundAtRef.current = now;
    }
    setToast({ text, kind });
  };

  const {
    launchStatus,
    manualDownload,
    launchModalOpen,
    launchError,
    launchRecoveryAction,
    launchProgress,
    launchDownloadPct,
    trackerVariantModal,
    setTrackerVariantModal,
    beginLaunch,
    reportLaunchFailure,
    setLaunchManual,
    setLaunchReady,
    closeLaunchModal,
    respondTrackerVariantModal,
  } = useSessionLaunchState({
    onWarning: (text) => showToast(text, "error"),
    onSm64ManualTutorial: () => setSm64exTutorialOpen(true),
  });

  useEffect(() => {
    if (!toast) return;
    const timer = window.setTimeout(() => setToast(null), 3500);
    return () => window.clearTimeout(timer);
  }, [toast]);

  useEffect(() => {
    try {
      window.localStorage.setItem("skl.blockedAuthors", JSON.stringify(blockedAuthors));
    } catch {
      // ignore
    }
  }, [blockedAuthors]);

  useEffect(() => {
    try {
      window.localStorage.setItem(FORCE_TRACKER_VARIANT_PROMPT_KEY, forceTrackerVariantPrompt ? "1" : "0");
    } catch (_err) {
      // ignore storage errors
    }
  }, [forceTrackerVariantPrompt]);

  useEffect(() => {
    let cancelled = false;

    const loadRuntimeModules = async () => {
      try {
        const res = await runtime.listRuntimeModules?.();
        const modules = (res as any)?.modules;
        if (!cancelled) {
          setRuntimeModules((res as any)?.ok && Array.isArray(modules) ? modules : []);
        }
      } catch {
        if (!cancelled) setRuntimeModules([]);
      }
    };

    void loadRuntimeModules();

    return () => {
      cancelled = true;
    };
  }, []);

  const clearGenerationHideTimer = useCallback(() => {
    if (generationHideTimerRef.current !== null) {
      window.clearTimeout(generationHideTimerRef.current);
      generationHideTimerRef.current = null;
    }
  }, []);

  const showGenerationProgress = useCallback((next: Omit<GenerationProgressState, "open">) => {
    setGenerationProgress({
      open: true,
      phase: next.phase,
      title: next.title,
      message: next.message,
      progress: next.progress,
      errorCode: next.errorCode,
      errorDescription: next.errorDescription,
    });
  }, []);

  const closeGenerationProgress = useCallback(() => {
    clearGenerationHideTimer();
    setGenerationProgress((prev) => (prev ? { ...prev, open: false } : prev));
  }, [clearGenerationHideTimer]);

  const scheduleGenerationClose = useCallback((delayMs = 2000) => {
    clearGenerationHideTimer();
    generationHideTimerRef.current = window.setTimeout(() => {
      setGenerationProgress((prev) => (prev ? { ...prev, open: false } : prev));
      generationHideTimerRef.current = null;
    }, delayMs);
  }, [clearGenerationHideTimer]);

  useEffect(() => {
    return () => {
      clearGenerationHideTimer();
    };
  }, [clearGenerationHideTimer]);

  const copyGenerationErrorCode = async () => {
    const code = String(generationProgress?.errorCode || "").trim();
    if (!code) return;
    try {
      await navigator.clipboard.writeText(code);
      showToast("Error code copied.", "success");
    } catch {
      showToast("Unable to copy error code.", "error");
    }
  };

  useEffect(() => {
    const handleClick = () => setContextMenu(null);
    document.addEventListener("click", handleClick);
    return () => document.removeEventListener("click", handleClick);
  }, []);
  const chatLogRef = useRef<HTMLDivElement | null>(null);
  const messageIdsRef = useRef<Set<string>>(new Set());
  const joinedPresenceRef = useRef<Set<string>>(new Set());

  const lobbyKey = lobbyId || "";
  const controlsLocked =
    isGenerationRunningStatus(generation?.status) ||
    isGenerationSuccessStatus(generation?.status) ||
    Boolean(generation?.room_url);

  const loadLobbyMeta = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const data = await apiJson<{ lobbies: LobbySummary[] }>("/api/lobbies");
      const match = data.lobbies.find((item) => item.id === lobbyKey);
      if (match) {
        setLobby(match);
        return;
      }
      // Solo lobbies (and any temporarily non-listed lobby) can still be valid.
      // Keep current metadata and let dedicated lobby endpoints decide true 404 state.
      setLobby((prev) => (prev && prev.id === lobbyKey ? prev : null));
    } catch {
      // ignore
    }
  }, [lobbyKey]);

  const loadMe = useCallback(async () => {
    try {
      const data = await apiCurrentUser();
      setMe({
        discord_id: data.discord_id || data.user_id || data.username || "sekailink-user",
        username: data.username,
        global_name: data.global_name || data.display_name,
        avatar_url: data.avatar_url,
      });
      setAuthed(true);
    } catch {
      setMe(null);
      setAuthed(false);
    }
  }, []);

  const loadMessages = useCallback(async (initial = false) => {
    if (!lobbyKey) return;
    try {
      const newMessages = (await chatService.listMessages({ kind: "lobby", lobbyId: lobbyKey })).map(normalizeLobbyChatMessage);
      if (newMessages.length) {
        const deduped = newMessages.filter((msg) => {
          const id = String(msg.id || "");
          if (!id) return true;
          if (messageIdsRef.current.has(id)) return false;
          messageIdsRef.current.add(id);
          return true;
        });
        if (deduped.length) {
          setMessages((prev) => [...prev, ...deduped].slice(-200));
          setLastMessageId((prev) => Math.max(prev, ...deduped.map((msg) => Number(msg.id) || 0)));
        }
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : "";
      if (/unauthorized|session expired/i.test(message)) setAuthed(false);
    }
  }, [lobbyKey]);

  const loadMembers = useCallback(async () => {
    if (!lobbyKey) return;
    const presenceMembers = new Map<string, LobbyMember>();
    const selfLooksHost = Boolean(
      isOwner ||
      (lobby?.owner && [me?.discord_id, me?.username, me?.global_name]
        .filter(Boolean)
        .map((value) => String(value).toLowerCase())
        .includes(String(lobby.owner).toLowerCase()))
    );
    const memberKeys = (member: Partial<LobbyMember>) => [
      member.discord_id,
      member.name,
      (member as any).username,
      (member as any).display_name,
      (member as any).global_name,
    ]
      .map((value) => String(value || "").trim().toLowerCase())
      .filter(Boolean);
    try {
      const channel = { kind: "lobby" as const, lobbyId: lobbyKey };
      await chatService.touchPresence(channel, {
        role: selfLooksHost ? "host" : "player",
        ready,
        ...localPresenceState,
      });
      const users = await chatService.listPresence(channel);
      users.forEach((user) => {
        const name = user.name || user.display_name || user.username || "SekaiLink";
        const member = {
          name,
          discord_id: user.user_id,
          online: true,
          ready: Boolean(user.irc_voice || user.ready),
          is_host: Boolean(user.irc_op || user.role === "host"),
          irc_op: Boolean(user.irc_op),
          irc_voice: Boolean(user.irc_voice),
          role: user.role,
          local_ready_known: typeof user.local_ready_known === "boolean" ? user.local_ready_known : undefined,
          local_ready: typeof user.local_ready === "boolean" ? user.local_ready : undefined,
          local_ready_note: user.local_ready_note || "",
        };
        memberKeys(member).forEach((key) => presenceMembers.set(key, member));
      });
      if (presenceMembers.size) {
        setMembers(dedupeLobbyMembers(Array.from(new Set(presenceMembers.values()))));
        setJoinRequired(false);
      }
    } catch {
      // The legacy member endpoint below remains a fallback if chat presence is unavailable.
    }
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/members`);
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        if (data.reason === "password") setJoinRequired(true);
        if (response.status === 404) {
          setLobby(null);
          setStatusMessage("Lobby not found.");
        }
        if (response.status === 401 || response.status === 403) setAuthed(false);
        return;
      }
      const data = await response.json();
      const legacyMembers: LobbyMember[] = data.members || [];
      const merged = new Map(presenceMembers);
      legacyMembers.forEach((member) => {
        const legacyName = String(member.name || (member as any).display_name || (member as any).username || (member as any).global_name || "").trim();
        if (!legacyName && presenceMembers.size) return;
        const keys = memberKeys({ ...member, name: legacyName });
        const online = keys.map((key) => merged.get(key)).find(Boolean);
        if (!online && presenceMembers.size) return;
        const mergedMember = {
          ...member,
          name: legacyName || member.name || "SekaiLink",
          ready: online ? Boolean(online.ready || member.ready) : member.ready,
          is_host: online ? Boolean(online.is_host || member.is_host) : member.is_host,
          online: online?.online ?? member.online,
          discord_id: member.discord_id || online?.discord_id,
          irc_op: online?.irc_op ?? member.irc_op,
          irc_voice: online?.irc_voice ?? member.irc_voice,
          role: online?.role || member.role,
          local_ready_known: typeof online?.local_ready_known === "boolean" ? online.local_ready_known : member.local_ready_known,
          local_ready: typeof online?.local_ready === "boolean" ? online.local_ready : member.local_ready,
          local_ready_note: online?.local_ready_note || member.local_ready_note,
        };
        keys.forEach((key) => merged.set(key, mergedMember));
      });
      const nextMembers = dedupeLobbyMembers(Array.from(new Set(merged.values())));
      setMembers(nextMembers);
      setJoinRequired(false);
      if (me?.discord_id) {
        const self = nextMembers.find((member) => member.discord_id === me.discord_id);
        if (self) {
          setReady(soloMode ? true : Boolean(self.ready));
          setIsOwner(Boolean(self.is_host));
        }
      }
    } catch {
      // ignore
    }
  }, [isOwner, localPresenceState, lobby?.owner, lobbyKey, me?.discord_id, me?.global_name, me?.username, ready, soloMode]);

  const ensureLobbyPresence = useCallback(async () => {
    if (!lobbyKey || !authed || joinRequired) return;
    const key = `${lobbyKey}:${me?.discord_id || me?.username || "self"}`;
    if (joinedPresenceRef.current.has(key)) return;
    try {
      await chatService.touchPresence(
        { kind: "lobby", lobbyId: lobbyKey },
        {
          role: isOwner ? "host" : "player",
          ready,
          ...localPresenceState,
        }
      );
      joinedPresenceRef.current.add(key);
      await loadMembers();
    } catch {
      // Best-effort: the normal join flow or refresh can still populate members.
    }
  }, [authed, isOwner, joinRequired, lobbyKey, loadMembers, localPresenceState, me?.discord_id, me?.username, ready]);

  useEffect(() => {
    if (!lobbyKey || !authed || joinRequired) return;
    void chatService.touchPresence(
      { kind: "lobby", lobbyId: lobbyKey },
      {
        role: isOwner ? "host" : "player",
        ready,
        ...localPresenceState,
      }
    ).catch(() => undefined);
  }, [authed, isOwner, joinRequired, lobbyKey, localPresenceState, ready]);

  const loadYamls = useCallback(async () => {
    try {
      const data = await apiJson<{ yamls: YamlEntry[] }>("/api/yamls");
      setYamls(data.yamls || []);
    } catch {
      setYamls([]);
    }
  }, []);

  const loadGameSelections = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const data = await apiJson<{ active_yamls?: YamlEntry[]; locked?: boolean }>(`/api/lobbies/${lobbyKey}/game-selections`);
      setActiveYamls(Array.isArray(data.active_yamls) ? data.active_yamls : []);
      setSelectionServerLocked(Boolean(data.locked));
    } catch {
      setActiveYamls([]);
      setSelectionServerLocked(false);
    }
  }, [lobbyKey]);

  const gameConfigGroups = useMemo<GameConfigGroup[]>(() => {
    const groups = new Map<string, GameConfigGroup>();
    yamls.forEach((entry) => {
      const game = entry.game || "Unknown";
      const id = gameGroupId(game);
      const group = groups.get(id) || { id, game, configs: [] };
      group.configs.push(entry);
      groups.set(id, group);
    });
    return [...groups.values()].sort((a, b) => a.game.localeCompare(b.game));
  }, [yamls]);

  const selectedGameGroup = useMemo(() => {
    return gameConfigGroups.find((group) => group.id === gameModalSelection) || gameConfigGroups[0] || null;
  }, [gameConfigGroups, gameModalSelection]);

  const activeConfigsByGame = useMemo(() => {
    const map = new Map<string, YamlEntry[]>();
    activeYamls.forEach((entry) => {
      const id = gameGroupId(entry.game);
      const list = map.get(id) || [];
      list.push(entry);
      map.set(id, list);
    });
    return map;
  }, [activeYamls]);

  const loadSocialRelations = useCallback(async () => {
    try {
      const [friendsRes, requestsRes] = await Promise.all([
        apiFetch("/api/social/friends"),
        apiFetch("/api/social/requests"),
      ]);
      if (friendsRes.ok) {
        const data = await friendsRes.json().catch(() => ({}));
        const list = Array.isArray(data.friends) ? (data.friends as SocialFriend[]) : [];
        setFriendUserIds(list.map((f) => String(f.user_id || "").trim()).filter(Boolean));
      }
      if (requestsRes.ok) {
        const data = await requestsRes.json().catch(() => ({}));
        const incoming = Array.isArray(data.incoming) ? (data.incoming as SocialRequest[]) : [];
        const outgoing = Array.isArray(data.outgoing) ? (data.outgoing as SocialRequest[]) : [];
        setIncomingFriendIds(incoming.map((r) => String(r.from_id || "").trim()).filter(Boolean));
        setOutgoingFriendIds(outgoing.map((r) => String(r.to_id || "").trim()).filter(Boolean));
      }
    } catch {
      // best-effort; lobby still works without social endpoints
    }
  }, []);

  const loadGeneration = useCallback(async (): Promise<GenerationStatus | null> => {
    if (!lobbyKey) return null;
    try {
      const data = await apiJson<GenerationStatus>(`/api/lobbies/${lobbyKey}/generation`);
      setGeneration(data);
      return data;
    } catch {
      setGeneration(null);
      return null;
    }
  }, [lobbyKey]);

  const loadHostStatus = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const data = await apiJson<{
        host_absent: boolean;
        host_absent_seconds: number;
        candidate_name: string;
        vote_count: number;
        vote_needed: number;
      }>(`/api/lobbies/${lobbyKey}/host-status`);
      setHostStatus(data);
    } catch {
      // ignore
    }
  }, [lobbyKey]);

  const fetchRoomStatus = useCallback(async (roomUrl?: string, retries = 1): Promise<RoomStatus | null> => {
    return fetchRoomStatusByUrl({
      roomUrl,
      retries,
      fetchJson: apiJson,
      onStatus: (data, roomId) => {
        if (data) {
          setRoomStatus(data);
          setRoomStatusRoomId(roomId);
        }
      },
    });
  }, []);

  const probeRoomServerAvailability = useCallback(async (roomUrl?: string) => {
    if (!roomUrl) {
      const err = parseGenerationError("ROOM_SERVER_MISSING: missing room id");
      showGenerationProgress({
        phase: "error",
        title: "Generation Failed",
        message: "Room server URL is missing.",
        progress: 100,
        errorCode: err.code,
        errorDescription: err.description,
      });
      return;
    }
    const probeKey = `${lobbyKey}:${String(roomUrl).trim()}`;
    if (generationRoomProbeRef.current.inFlight && generationRoomProbeRef.current.key === probeKey) return;
    generationRoomProbeRef.current = { key: probeKey, inFlight: true };
    showGenerationProgress({
      phase: "waiting_room_server",
      title: "Generation Progress",
      message: "Waiting on Room Server...",
      progress: 86,
    });
    const status = await probeRoomServerStatus({
      roomUrl,
      fetchRoomStatus,
    });
    generationRoomProbeRef.current.inFlight = false;
    if (!isRoomServerReady(status)) {
      const err = parseGenerationError("ROOM_SERVER_UNAVAILABLE: room server did not become available");
      showGenerationProgress({
        phase: "error",
        title: "Generation Failed",
        message: "Room server is unavailable.",
        progress: 100,
        errorCode: err.code,
        errorDescription: err.description,
      });
      return;
    }
    showGenerationProgress({
      phase: "room_server_available",
      title: "Generation Progress",
      message: "Room server available.",
      progress: 100,
    });
    scheduleGenerationClose(2000);
  }, [fetchRoomStatus, lobbyKey, scheduleGenerationClose, showGenerationProgress]);

  const refreshTrackerStatsFallback = useCallback(async () => {
    const trackerId = String(roomStatus?.tracker || "").trim();
    if (!trackerId) return;
    try {
      const data = await apiJson<any>(`/api/tracker_view/${trackerId}?_=${Date.now()}`);
      const checks = Array.isArray(data?.player_checks_done) ? data.player_checks_done : [];
      const totals = Array.isArray(data?.player_locations_total) ? data.player_locations_total : [];
      const completed = Array.isArray(data?.completed_worlds) ? data.completed_worlds : [];
      const playersTotal = Array.isArray(data?.player_names) ? data.player_names.length : totals.length;
      const checksDone = checks.reduce((sum: number, row: any) => sum + Number(row?.checks_done || 0), 0);
      const totalLocations = totals.reduce((sum: number, row: any) => sum + Number(row?.total_locations || 0), 0);
      setTrackerStats({
        players_total: Number(playersTotal || 0),
        checks_done: Number(checksDone || 0),
        total_locations: Number(totalLocations || 0),
        completed_players: Number(completed.length || 0),
      });
    } catch {
      // fallback only
    }
  }, [roomStatus?.tracker]);

  const terminalDisplayLog = useMemo(() => {
    if (apLogLines.length) {
      return apLogLines
        .map((line) => `[${line.level}] ${line.text}`)
        .join("\n");
    }
    if (apLastError) return `[error] ${apLastError}`;
    if (launchStatus) return `[launch] ${launchStatus}`;
    return terminalLog;
  }, [apLogLines, terminalLog, apLastError, launchStatus]);

  const appendTerminalLine = useCallback((line: string) => {
    const text = String(line || "").trim();
    if (!text) return;
    setTerminalLog((prev) => {
      const base = String(prev || "").trim() === "Waiting for local session logs..." ? "" : prev;
      return `${base}${base ? "\n" : ""}${text}`;
    });
  }, []);

  useEffect(() => {
    const off = runtime.onSessionEvent?.((data: unknown) => {
      const evt = data as any;
      if (!evt || typeof evt !== "object") return;
      if (evt.event === "status" && typeof evt.status === "string") {
        appendTerminalLine(`[launch] ${evt.status}`);
      } else if (evt.event === "ready") {
        appendTerminalLine("[launch] Ready.");
      } else if (evt.event === "error") {
        appendTerminalLine(`[launch:error] ${String(evt.error || "Launch failed.")}`);
      }
    });
    return () => {
      if (typeof off === "function") off();
    };
  }, [appendTerminalLine]);

  useEffect(() => {
    messageIdsRef.current.clear();
    joinedPresenceRef.current.clear();
    setMessages([]);
    setLastMessageId(0);
  }, [lobbyKey]);

  useEffect(() => {
    loadLobbyMeta();
    loadMe();
    loadMessages(true);
    loadMembers();
    loadYamls();
    loadGameSelections();
    loadGeneration();
    loadSocialRelations();
  }, [loadLobbyMeta, loadMe, loadMembers, loadMessages, loadYamls, loadGameSelections, loadGeneration, loadSocialRelations]);

  useEffect(() => {
    void ensureLobbyPresence();
  }, [ensureLobbyPresence]);

  useEffect(() => {
    if (settingsOpen) {
      loadHostStatus();
    }
  }, [settingsOpen, loadHostStatus]);

  useInterval(() => {
    loadMembers();
    loadGameSelections();
    loadGeneration();
    if (generation?.room_url) {
      fetchRoomStatus(generation.room_url, 1);
    }
    if (settingsOpen) loadHostStatus();
    loadSocialRelations();
  }, 10000);

  // IRC/HTTP is the current live source. Keep chat responsive without duplicating the heavier lobby refresh.
  useInterval(() => {
    loadMessages();
  }, 2500);

  useInterval(() => {
    void refreshTrackerStatsFallback();
  }, 1000);

  useEffect(() => {
    if (generation?.room_url) {
      fetchRoomStatus(generation.room_url);
    }
  }, [generation?.room_url, fetchRoomStatus]);

  useEffect(() => {
    const status = normalizeGenerationStatus(generation?.status);
    const generationError = String(generation?.error || generation?.response?.error || "").trim();
    const hasError = Boolean(generationError);
    if (status === "none" && !hasError) {
      generationRoomProbeRef.current = { key: "", inFlight: false };
      closeGenerationProgress();
      return;
    }

    if (hasError || isGenerationErrorStatus(status)) {
      clearGenerationHideTimer();
      const err = parseGenerationError(generationError || "GENERATION_FAILED");
      showGenerationProgress({
        phase: "error",
        title: "Generation Failed",
        message: "Generation failed.",
        progress: 100,
        errorCode: err.code,
        errorDescription: err.description,
      });
      return;
    }

    if (status === "queued") {
      clearGenerationHideTimer();
      setRoomStatus(null);
      setRoomStatusRoomId("");
      showGenerationProgress({
        phase: "queued",
        title: "Generation Progress",
        message: "Generation queued.",
        progress: 24,
      });
      generationRoomProbeRef.current = { key: "", inFlight: false };
      return;
    }

    if (isGenerationRunningStatus(status)) {
      clearGenerationHideTimer();
      showGenerationProgress({
        phase: "started",
        title: "Generation Progress",
        message: "Generation started.",
        progress: 56,
      });
      return;
    }

    if (isGenerationSuccessStatus(status)) {
      clearGenerationHideTimer();
      if (!generation?.room_url && generationArtifactReady(generation)) {
        generationRoomProbeRef.current = { key: "", inFlight: false };
        showGenerationProgress({
          phase: "completed",
          title: "Generation Progress",
          message: "Seed package ready. Room handoff pending.",
          progress: 100,
        });
        scheduleGenerationClose(2500);
        return;
      }
      showGenerationProgress({
        phase: "waiting_room_server",
        title: "Generation Progress",
        message: "Waiting on Room Server...",
        progress: 86,
      });
      if (generation?.room_url) {
        const roomStatusMatches = roomStatusMatchesUrl(roomStatusRoomId, generation.room_url);
        const roomServerReady = roomStatusMatches && isRoomServerReady(roomStatus);
        if (roomServerReady) {
          showGenerationProgress({
            phase: "room_server_available",
            title: "Generation Progress",
            message: "Room server available.",
            progress: 100,
          });
          scheduleGenerationClose(2000);
        } else {
          void probeRoomServerAvailability(generation.room_url);
        }
      } else {
        showGenerationProgress({
          phase: "waiting_room_server",
          title: "Generation Progress",
          message: "Waiting on Room Server...",
          progress: 86,
        });
      }
    }
  }, [
    clearGenerationHideTimer,
    closeGenerationProgress,
    generation?.artifact_path,
    generation?.artifact_ready,
    generation?.error,
    generation?.response?.artifact_path,
    generation?.response?.error,
    generation?.response?.sync_package_path,
    generation?.response?.sync_package_ready,
    generation?.room_url,
    generation?.status,
    generation?.sync_package_path,
    generation?.sync_package_ready,
    probeRoomServerAvailability,
    roomStatus?.last_port,
    roomStatus?.tracker,
    roomStatusRoomId,
    scheduleGenerationClose,
    showGenerationProgress,
  ]);

  useEffect(() => {
    if (generation?.completed_at) {
      const start = new Date(generation.completed_at).getTime();
      const tick = () => {
        const diff = Math.max(0, Math.floor((Date.now() - start) / 1000));
        const mins = Math.floor(diff / 60);
        const secs = diff % 60;
        setSeedTimer(`${mins}m ${secs}s`);
      };
      tick();
      const id = window.setInterval(tick, 1000);
      return () => window.clearInterval(id);
    }
    setSeedTimer("-");
    return undefined;
  }, [generation?.completed_at]);

  // Local session bridge output. Used for hints, launch status, and debug.
  useEffect(() => {
    if (!runtime.onBizHawkClientEvent) return;
    const unsub = runtime.onBizHawkClientEvent((raw) => {
      const data: any = raw as any;
      if (!data || typeof data !== "object") return;
      if (data.event === "log") {
        const msg = typeof data.message === "string" ? data.message : "";
        const lvl = typeof data.level === "string" ? data.level : "info";
        if (msg) {
          appendTerminalLine(`[${lvl}] ${msg}`);
          setApLogLines((prev) => {
            return [...prev, { text: msg, ts: Date.now(), level: lvl }];
          });
          if (/((not enough|insufficient).*(hint|points))|cannot afford.*hint|need .*hint points/i.test(msg)) {
            setHintErrorModal({
              title: "Not enough hint points",
              message: "You need more hint points before requesting a new hint.",
            });
            sfx.play("error", 0.4);
          }
        }
      } else if (data.event === "error") {
        const msg = typeof data.message === "string" ? data.message : "Unknown error";
        appendTerminalLine(`[error] ${msg}`);
        setApLastError(msg);
        setApLogLines((prev) => {
          return [...prev, { text: msg, ts: Date.now(), level: "error" }];
        });
      } else if (data.event === "emu_status") {
        if (typeof data.server === "string") setApClientConnected(data.server === "connected");
        if (typeof data.bizhawk === "string") {
          const v = data.bizhawk === "connected" || data.bizhawk === "tentative" || data.bizhawk === "not_connected" ? data.bizhawk : "unknown";
          setApBizhawkConnected(v);
        }
        if (typeof data.handler === "string") setApHandlerGame(data.handler);
        appendTerminalLine(
          `[status] room_link=${String(data.server || "-")} runtime_bridge=${String(data.bizhawk || "-")} game_profile=${String(data.handler || "-")}`
        );
      } else if (data.event === "status") {
        if (data.server === "connected") setApClientConnected(true);
        if (data.server === "disconnected") setApClientConnected(false);
        if (typeof data.hint_points === "number") setApHintPoints(data.hint_points);
        if (typeof data.server === "string") appendTerminalLine(`[status] server ${data.server}`);
        if (typeof data.hint_points === "number") appendTerminalLine(`[status] hint_points=${data.hint_points}`);
      } else if (data.event === "room_info") {
        if (typeof data.hint_cost === "number") setApHintCost(data.hint_cost);
        appendTerminalLine(
          `[room] seed=${String(data.seed_name || "-")} hint_cost=${typeof data.hint_cost === "number" ? data.hint_cost : "-"}`
        );
      } else if (data.event === "print_json") {
        const t = typeof data.text === "string" ? data.text.trim() : "";
        if (t) appendTerminalLine(`[print] ${t}`);
        if (data.type === "Hint" && t) {
          setHintMessages((prev) => {
            const next = [...prev, { text: t, ts: Date.now() }];
            return next.slice(-200);
          });
        } else if (t && /((not enough|insufficient).*(hint|points))|cannot afford.*hint|need .*hint points/i.test(t)) {
          setHintErrorModal({
            title: "Not enough hint points",
            message: "You need more hint points before requesting a new hint.",
          });
          sfx.play("error", 0.4);
        }
      }
    });
    return () => {
      try {
        unsub?.();
      } catch {
        // ignore
      }
    };
  }, [appendTerminalLine, sfx]);

  const sendApSay = async (text: string) => {
    const trimmed = String(text || "").trim();
    if (!trimmed) return;
    try {
      const res = await runtime.bizhawkClientSend?.({ cmd: "say", text: trimmed });
      if ((res as any)?.ok === false) throw new Error((res as any)?.error || "send_failed");
    } catch (err) {
      showToast(err instanceof Error ? err.message : "Unable to send command.", "error");
    }
  };

  const loadHintCatalog = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const data = await apiJson<HintCatalogResponse>(`/api/lobbies/${lobbyKey}/hint-catalog`);
      setHintCatalog(data);
      setHintSelection("");
    } catch {
      setHintCatalog({ items: [], locations: [], game: "", player_name: "" });
    }
  }, [lobbyKey]);

  const requestHint = useCallback(async (query?: string) => {
    const q = String(query || "").trim();
    if (!apClientConnected) return;
    if (apHintPoints !== null && apHintPoints <= 0) {
      setHintErrorModal({
        title: "Not enough hint points",
        message: "You need more hint points before requesting a new hint.",
      });
      sfx.play("error", 0.4);
      return;
    }
    if (!q) {
      await sendApSay(hintMode === "location" ? "!hint_location" : "!hint");
      return;
    }
    await sendApSay(hintMode === "location" ? `!hint_location ${q}` : `!hint ${q}`);
  }, [apClientConnected, apHintPoints, hintMode, sfx]);

  const openHints = async () => {
    setHintMessages([]);
    setHintQuery("");
    setHintSelection("");
    setHintMode("item");
    sfx.play("confirm", 0.25);
    setHintModalOpen(true);
    await loadHintCatalog();
    // Pull current hint list for this player.
    await sendApSay("!hint");
  };

  const copyApDebug = async () => {
    try {
      sfx.play("confirm", 0.2);
      const payload = {
        serverAddress: roomServerAddress,
        slot: selfPlayerName || "",
        room_link: apClientConnected ? "connected" : "disconnected",
        runtime_bridge: apBizhawkConnected,
        game_profile: apHandlerGame,
        hint_points: apHintPoints,
        hint_cost: apHintCost,
        last_error: apLastError,
        recent_logs: apLogLines.map((l) => `[${l.level}] ${l.text}`),
      };
      await navigator.clipboard.writeText(JSON.stringify(payload, null, 2));
      showToast("Copied debug info.", "success");
    } catch (err) {
      showToast(err instanceof Error ? err.message : "Unable to copy debug info.", "error");
    }
  };

  useEffect(() => {
    if (!chatLogRef.current) return;
    const log = chatLogRef.current;
    // Lobby chat should follow new messages automatically.
    log.scrollTop = log.scrollHeight;
  }, [messages]);

  useEffect(() => {
    const el = document.getElementById("lobby-terminal-log");
    if (!el) return;
    el.scrollTop = el.scrollHeight;
  }, [terminalDisplayLog, terminalOpen]);

  const onJoinPrivate = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);
    const password = (formData.get("password") || "").toString();
    setPasswordError("");
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/join`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ password })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to join lobby.");
      }
      setJoinRequired(false);
      setLobbyPassword(password);
      sfx.play("join", 0.35);
      await loadMembers();
    } catch (err) {
      setPasswordError(err instanceof Error ? err.message : "Unable to join lobby.");
    }
  };

  const sendMessage = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!chatInput.trim()) return;
    const content = chatInput.trim();
    const optimisticId = `local-${Date.now()}-${Math.random().toString(16).slice(2)}`;
    const optimisticMessage: LobbyMessage = {
      id: optimisticId,
      author: me?.global_name || me?.username || "You",
      avatar_url: me?.avatar_url,
      content,
      created_at: new Date().toISOString(),
      pending: true,
    };
    setChatInput("");
    messageIdsRef.current.add(String(optimisticId));
    setMessages((prev) => [...prev, optimisticMessage]);
    try {
      const serverMessage = await chatService.sendMessage({ kind: "lobby", lobbyId: lobbyKey }, content);
      if (serverMessage?.id) {
        messageIdsRef.current.add(String(serverMessage.id));
        setMessages((prev) => prev.map((msg) => (
          msg.id === optimisticId
            ? {
                id: serverMessage.id,
                author: serverMessage.author || optimisticMessage.author,
                avatar_url: serverMessage.avatar_url || optimisticMessage.avatar_url,
                content: serverMessage.content || content,
                created_at: serverMessage.created_at || optimisticMessage.created_at,
              }
            : msg
        )));
        setLastMessageId((prev) => Math.max(prev, Number(serverMessage.id) || 0));
      } else {
        setMessages((prev) => prev.map((msg) => (
          msg.id === optimisticId ? { ...msg, pending: false } : msg
        )));
      }
      sfx.play("confirm", 0.35);
    } catch (err) {
      messageIdsRef.current.delete(String(optimisticId));
      setMessages((prev) => prev.filter((msg) => msg.id !== optimisticId));
      setChatInput(content);
      const message = err instanceof Error ? err.message : "Unable to send message.";
      if (/unauthorized|session expired|login required/i.test(message)) setAuthed(false);
      setStatusMessage(message);
    }
  };

  const postLobbyNotice = useCallback(async (content: string) => {
    const text = String(content || "").trim();
    if (!text || !lobbyKey || !authed || joinRequired) return;
    try {
      await chatService.sendMessage({ kind: "lobby", lobbyId: lobbyKey }, text);
    } catch {
      // best effort
    }
  }, [authed, joinRequired, lobbyKey]);

  const addActiveYaml = useCallback(async (yamlId: string) => {
    if (!yamlId) return false;
    const selfLocked = controlsLocked || selectionServerLocked || Boolean(members.find((member) => member.discord_id === me?.discord_id)?.ready);
    if (selfLocked) {
      setStatusMessage("Selection is locked while you are ready or generation is active.");
      return false;
    }
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/game-selections`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ yaml_id: yamlId, action: "add" })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error === "selection_locked" ? "Selection is locked while you are ready or generation is active." : (data.error || "Unable to add game."));
      }
      const data = await response.json();
      setActiveYamls(data.active_yamls || []);
      setSelectionServerLocked(Boolean(data.locked));
      return true;
    } catch (err) {
      setStatusMessage(err instanceof Error ? err.message : "Unable to add game.");
      return false;
    }
  }, [controlsLocked, selectionServerLocked, lobbyKey, members, me?.discord_id]);

  const removeGameConfig = useCallback(async (yamlId: string) => {
    if (!yamlId) return false;
    const selfLocked = controlsLocked || selectionServerLocked || Boolean(members.find((member) => member.discord_id === me?.discord_id)?.ready);
    if (selfLocked) {
      setStatusMessage("Selection is locked while you are ready or generation is active.");
      return false;
    }
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/game-selections`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ yaml_id: yamlId, action: "remove" })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error === "selection_locked" ? "Selection is locked while you are ready or generation is active." : (data.error || "Unable to remove config."));
      }
      const data = await response.json();
      setActiveYamls(data.active_yamls || []);
      setSelectionServerLocked(Boolean(data.locked));
      return true;
    } catch (err) {
      setStatusMessage(err instanceof Error ? err.message : "Unable to remove config.");
      return false;
    }
  }, [controlsLocked, selectionServerLocked, lobbyKey, members, me?.discord_id]);

  const setSingleActiveYaml = useCallback(async (yamlId: string, announce: "default" | "change" = "change") => {
    const targetId = String(yamlId || "").trim();
    if (!targetId) return false;
    if (controlsLocked) return false;

  const self = members.find((member) => member.discord_id === me?.discord_id);
  const currentId = String(lobbySelectionsForMember(self)[0]?.id || activeYamls[0]?.id || "").trim();
    const selected = yamls.find((entry) => entry.id === targetId);
    const selectedTitle = selected?.title || targetId;

    if (currentId && currentId === targetId) return true;

    const added = await addActiveYaml(targetId);
    if (!added) return false;

    const actor = self?.name || "Player";
    if (announce === "change") await postLobbyNotice(`${actor} added game config: ${selectedTitle}`);
    return true;
  }, [controlsLocked, members, me?.discord_id, activeYamls, yamls, addActiveYaml, postLobbyNotice]);

  useEffect(() => {
    if (!lobbyKey || !authed || !me?.discord_id) return;
    const self = members.find((member) => member.discord_id === me.discord_id);
    const selectionSummary = summarizeSelectionSet(lobbySelectionsForMember(self).length ? lobbySelectionsForMember(self) : activeYamls);
    if (!selectionSummary || selectionSummary === "No game selected") return;
    const key = `${lobbyKey}:${me.discord_id}`;
    if (joinedGameNoticeRef.current[key]) return;
    joinedGameNoticeRef.current[key] = true;
    void postLobbyNotice(`${self?.name || "Player"} joined with: ${selectionSummary}`);
  }, [lobbyKey, authed, me?.discord_id, members, activeYamls, postLobbyNotice]);

  useEffect(() => {
    if (!yamlOpen) return;
    const firstActive = activeYamls[0];
    const firstGame = firstActive ? gameGroupId(firstActive.game) : gameConfigGroups[0]?.id || "";
    setGameModalSelection((current) => current || firstGame);
    setYamlModalSelection(firstActive?.id || "");
  }, [yamlOpen, activeYamls, gameConfigGroups]);

  const toggleReady = async () => {
    if (soloMode) {
      setReady(true);
      return;
    }
    if (controlsLocked) return;
    const selfMember = members.find((member) => member.discord_id === me?.discord_id);
    const hasActiveYaml = Boolean(lobbySelectionsForMember(selfMember).length || activeYamls[0]?.id);
    if (!hasActiveYaml) {
      showToast("Select at least one game/config for this room before marking ready.", "error");
      return;
    }
    const nextReady = !(selfMember?.ready ?? ready);
    if (nextReady && selfLaunchBlockedBySetup) {
      showToast(localLaunchReadiness?.errorText || localLaunchReadiness?.note || "Local setup is incomplete.", "error");
      return;
    }
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/ready`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ready: nextReady })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to update ready.");
      }
      const data = await response.json();
      const nextReadyState = Boolean(data.ready);
      setReady(nextReadyState);
      setSelectionServerLocked(nextReadyState);
      await chatService.touchPresence(
        { kind: "lobby", lobbyId: lobbyKey },
        {
          role: isOwner ? "host" : "player",
          ready: nextReadyState,
          ...localPresenceState,
        }
      ).catch(() => undefined);
      await loadMembers();
      sfx.play(data.ready ? "ready" : "unready", 0.35);
    } catch (err) {
      showToast(err instanceof Error ? err.message : "Unable to update ready.", "error");
    }
  };

  const generateSeed = async () => {
    clearGenerationHideTimer();
    setRoomStatus(null);
    setRoomStatusRoomId("");
    generationRoomProbeRef.current = { key: "", inFlight: false };
    showGenerationProgress({
      phase: "started",
      title: "Generation Progress",
      message: "Generation started.",
      progress: 10,
    });
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/generate`, { method: "POST" });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(String(data.detail || data.error || data.response?.error || "Unable to generate."));
      }
      showGenerationProgress({
        phase: "queued",
        title: "Generation Progress",
        message: "Generation queued.",
        progress: 24,
      });
      sfx.play("success", 0.35);
      const immediate = await apiJson<GenerationStatus>(`/api/lobbies/${lobbyKey}/generation`).catch(() => null);
      if (immediate) setGeneration(immediate);
      let roomUrl = immediate?.room_url || "";
      if (!roomUrl) {
        for (let i = 0; i < 18; i += 1) {
          await new Promise((resolve) => window.setTimeout(resolve, 1000));
          const next = await apiJson<GenerationStatus>(`/api/lobbies/${lobbyKey}/generation`).catch(() => null);
          if (next) setGeneration(next);
          roomUrl = next?.room_url || roomUrl;
          if (
            roomUrl ||
            isGenerationErrorStatus(next?.status) ||
            (isGenerationSuccessStatus(next?.status) && generationArtifactReady(next))
          ) {
            break;
          }
        }
      }
      if (roomUrl) await fetchRoomStatus(roomUrl, 8);
      else await loadGeneration();
    } catch (err) {
      clearGenerationHideTimer();
      const parsed = parseGenerationError(err instanceof Error ? err.message : "Unable to generate.");
      showGenerationProgress({
        phase: "error",
        title: "Generation Failed",
        message: "Generation failed.",
        progress: 100,
        errorCode: parsed.code,
        errorDescription: parsed.description,
      });
      showToast(parsed.description, "error");
    }
  };

  const sendTerminal = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!terminalInput.trim()) return;
    const cmd = terminalInput.trim();
    try {
      if (runtime.bizhawkClientSend) {
        const res = await runtime.bizhawkClientSend({ cmd: "say", text: cmd });
        if ((res as any)?.ok === false) throw new Error((res as any)?.error || "send_failed");
        sfx.play("confirm", 0.25);
        setTerminalStatus("Sent");
        setTerminalInput("");
        return;
      }
    } catch (err) {
      setTerminalStatus(err instanceof Error ? err.message : "Send failed");
      return;
    }
    setTerminalStatus("The local session link is not ready.");
  };

  const updateSettings = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    setSettingsStatus("");
    const formData = new FormData(event.currentTarget);
    const payload = {
      release_mode: formData.get("release_mode"),
      collect_mode: formData.get("collect_mode"),
      remaining_mode: formData.get("remaining_mode"),
      countdown_mode: formData.get("countdown_mode"),
      item_cheat: formData.get("item_cheat"),
      spoiler: formData.get("spoiler"),
      hint_cost: formData.get("hint_cost"),
      max_players: formData.get("max_players"),
      allow_custom_yamls: formData.get("allow_custom_yamls"),
      server_password: formData.get("server_password"),
      clear_password: formData.get("clear_password") === "on",
      plando_items: formData.get("plando_items") === "on",
      plando_bosses: formData.get("plando_bosses") === "on",
      plando_connections: formData.get("plando_connections") === "on",
      plando_texts: formData.get("plando_texts") === "on",
    };
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/settings`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to save settings.");
      }
      setSettingsStatus("Saved.");
      sfx.play("success", 0.25);
      await loadLobbyMeta();
    } catch (err) {
      setSettingsStatus(err instanceof Error ? err.message : "Unable to save settings.");
    }
  };

  const closeLobby = async () => {
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/close`, { method: "POST" });
      if (!response.ok) throw new Error("Unable to close lobby.");
      window.location.hash = "#/rooms";
    } catch (err) {
      setStatusMessage(err instanceof Error ? err.message : "Unable to close lobby.");
    }
  };

  const releasePlayer = async (playerName: string, mode: "release" | "slow") => {
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/release`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ player_name: playerName, mode })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to release player.");
      }
      sfx.play("success", 0.35);
    } catch (err) {
      setStatusMessage(err instanceof Error ? err.message : "Unable to release player.");
    }
  };

  const kickOrBanMember = async (name: string, action: "kick" | "ban") => {
    try {
      const payload: Record<string, unknown> = { name, action };
      if (action === "ban") payload.duration_minutes = 0;
      const res = await apiFetch(`/api/lobbies/${lobbyKey}/kick`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || `Unable to ${action} player.`);
      }
      sfx.play("success", 0.35);
    } catch (err) {
      showToast(err instanceof Error ? err.message : `Unable to ${action} player.`, "error");
    }
  };

  const openMemberProfile = async (member: LobbyMember) => {
    setProfileModal({
      open: true,
      loading: false,
      error: "",
      data: {
        display_name: member.name,
        status: member.online ? "Online" : "Offline",
        bio: (() => {
          const summary = summarizeSelectionSet(lobbySelectionsForMember(member));
          return summary !== "No game selected" ? `Room selection: ${summary}` : "";
        })(),
        badges: member.is_host ? ["Host"] : [],
        user_id: member.discord_id,
      },
    });
  };

  const toggleBlockMember = (member: LobbyMember) => {
    const key = String(member.name || "").trim().toLowerCase();
    if (!key) return;
    setBlockedAuthors((prev) => (prev.includes(key) ? prev.filter((v) => v !== key) : [...prev, key]));
    sfx.play("confirm", 0.2);
  };

  const sendFriendRequest = async (targetId: string, targetName: string) => {
    const cleaned = String(targetId || "").trim();
    if (!cleaned) return;
    try {
      const res = await apiFetch("/api/social/friends/request", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: cleaned }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || t("social.unable_send_friend_request"));
      }
      setOutgoingFriendIds((prev) => (prev.includes(cleaned) ? prev : [...prev, cleaned]));
      showToast(t("social.friend_request_sent_to_toast", { name: targetName || t("social.user_generic") }), "success");
      sfx.play("success", 0.35);
      loadSocialRelations();
    } catch (err) {
      showToast(err instanceof Error ? err.message : t("social.unable_send_friend_request"), "error");
    }
  };

  const acceptIncomingFriendRequest = async (targetId: string, targetName: string) => {
    const cleaned = String(targetId || "").trim();
    if (!cleaned) return;
    try {
      const reqRes = await apiFetch("/api/social/requests");
      if (!reqRes.ok) throw new Error(t("social.unable_load_pending_requests"));
      const reqData = await reqRes.json().catch(() => ({}));
      const incoming = Array.isArray(reqData.incoming) ? (reqData.incoming as SocialRequest[]) : [];
      const request = incoming.find((item) => String(item.from_id || "").trim() === cleaned);
      if (!request?.id) throw new Error(t("social.no_pending_request_from_user"));

      const res = await apiFetch("/api/social/friends/respond", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ request_id: request.id, action: "accept" }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || t("social.unable_accept_friend_request"));
      }
      showToast(t("social.friend_added_toast", { name: targetName || t("social.user_generic") }), "success");
      sfx.play("success", 0.35);
      loadSocialRelations();
    } catch (err) {
      showToast(err instanceof Error ? err.message : t("social.unable_accept_friend_request"), "error");
    }
  };

  const submitReport = async () => {
    setReportModal((prev) => ({ ...prev, submitting: true, error: "" }));
    try {
      const reasonLabel =
        {
          harassment: "Harassment",
          hate: "Hate speech",
          cheating: "Cheating",
          spam: "Spam",
          impersonation: "Impersonation",
          other: "Other",
        }[reportModal.reason] || "Other";
      const subject = `[Lobby Report] ${reportModal.targetName || "Unknown user"} · ${reasonLabel}`;
      const message = [
        `Lobby ID: ${lobbyKey}`,
        `Reported user: ${reportModal.targetName || "-"}`,
        `Reported user ID: ${reportModal.targetId || "-"}`,
        `Reason: ${reasonLabel}`,
        "",
        "Comment:",
        reportModal.comment || "(no additional comment)",
      ].join("\n");
      const res = await apiFetch(`/api/support/tickets`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          category: "lobby",
          subject,
          message,
        }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || "Unable to submit report.");
      }
      setReportModal({
        open: false,
        targetName: "",
        targetId: "",
        reason: "harassment",
        comment: "",
        submitting: false,
        error: "",
      });
      showToast("Report submitted for admin review.", "success");
    } catch (err) {
      setReportModal((prev) => ({
        ...prev,
        submitting: false,
        error: err instanceof Error ? err.message : "Unable to submit report.",
      }));
    }
  };

  const handleLaunch = async (downloadUrls?: string | string[]) => {
    window.SKL_SFX?.stopBgm?.();
    const launchRes = await executeRoomSessionLaunch({
      downloadUrls,
      roomStatus,
      generation,
      host,
      fetchRoomStatus,
      loadLatestGeneration: () => loadGeneration(),
      playerName: selfLaunchContext.playerName || selfPlayerName,
      playerAlias: me?.global_name || me?.username || selfPlayerName,
      apGameName: selfApGameName,
      playersByName,
      password: lobbyPassword,
      forceTrackerVariantPrompt,
      lobbyId: lobbyKey,
      onLaunchBegin: beginLaunch,
    });
    if (!launchRes.ok) {
      if (launchRes.surface === "toast") {
        showToast(launchRes.message, "error");
      } else {
        reportLaunchFailure(launchRes.errorMessage, launchRes.recoveryAction);
      }
      return;
    }
    if (launchRes.status === "manual") {
      setLaunchManual({
        path: launchRes.manualDownload.path,
        ext: launchRes.manualDownload.ext,
        moduleId: launchRes.manualDownload.moduleId,
        apGameName: launchRes.manualDownload.apGameName,
        installedPath: launchRes.manualDownload.installedPath,
        note: launchRes.manualDownload.note,
      });
    } else {
      setLaunchReady();
    }
  };

  const downloadsIndex = useMemo(
    () => {
      const launchEntries = roomStatus?.launch_entries?.filter((entry) => isLaunchArtifactEntry(entry)) || [];
      return indexDownloadsBySlot(
        launchEntries.length ? launchEntries : roomStatus?.downloads,
        (download) => apiUrl(download)
      );
    },
    [roomStatus?.downloads, roomStatus?.launch_entries]
  );
  const downloadsBySlot = downloadsIndex.single;
  const downloadsBySlotMulti = downloadsIndex.multi;

  const playersByName = useMemo(() => indexPlayersByName(roomStatus?.players), [roomStatus?.players]);

  const ownerName = lobby?.owner || "Unknown";
  const isOwnerByName = Boolean(
    lobby?.owner &&
      me &&
      (lobby.owner === me.global_name ||
        lobby.owner === me.username ||
        lobby.owner === `${me.username}#${me.discord_id}`)
  );
  const canManageLobby = isOwner || isOwnerByName;
  const seedUrl = generation?.seed_url ? apiUrl(generation.seed_url) : "";
  const roomUrl = generation?.room_url ? apiUrl(generation.room_url) : "";
  const spoilerSeedId = generation?.seed_url ? generation.seed_url.split("/").filter(Boolean).pop() || "" : "";
  const spoilerLogUrl = spoilerSeedId ? apiUrl(`/dl_spoiler/${spoilerSeedId}`) : "";
  const spoilerMode = Number(lobby?.spoiler ?? 0);
  const roomReadyMembers = members.filter((member) => member.ready);
  const localReadyMembers = members.filter((member) => member.local_ready_known && member.local_ready);
  const localSetupUnknownMembers = members.filter((member) => !member.local_ready_known);
  const localSetupBlockedMembers = members.filter((member) => member.local_ready_known && !member.local_ready);
  const allReady = soloMode || (members.length > 0 && members.every((member) => member.ready));
  const allLocalReady = soloMode || (members.length > 0 && members.every((member) => member.local_ready_known && member.local_ready));
  const generationState = generation?.status || "idle";
  const generationLocked = controlsLocked;
  const host = useMemo(() => resolveRoomServerHost(apiUrl), []);
  const roomServerAddress = useMemo(() => buildRoomServerAddress(host, roomStatus), [host, roomStatus]);

  const selectedGameLabel = summarizeSelectionSet(selfSelections);
  const selfSelectionLocked = generationLocked || Boolean(selfMember?.ready ?? ready) || selectionServerLocked;
  const selfPlayerName = lobbySelectionsForMember(selfMember)[0]?.player_name || selfMember?.active_yaml_player || selfMember?.name;
  const selfLaunchContext = useMemo(
    () =>
      buildSelfLaunchContext({
        roomStatus,
        playerName: selfPlayerName,
        selection: selfSelections[0],
        toUrl: (download) => apiUrl(download),
        downloadsBySlot,
        downloadsBySlotMulti,
        playersByName,
      }),
    [downloadsBySlot, downloadsBySlotMulti, playersByName, roomStatus, selfPlayerName, selfSelections]
  );
  const selfSlotId = selfLaunchContext.slotId;
  const selfDownloadUrl = selfLaunchContext.downloadUrl;
  const selfDownloadUrls = selfLaunchContext.downloadUrls;
  const selfApGameName = selfLaunchContext.apGameName;
  const selfCanPatchlessLaunch = Boolean(selfMember && supportsPatchlessAutoLaunch(selfApGameName));
  const selfCanLaunch = Boolean((selfDownloadUrls?.length || selfDownloadUrl) || selfCanPatchlessLaunch);
  const selfSelectionGame = String(selfSelections[0]?.game || "").trim();
  const selfSelectionKeys = useMemo(
    () =>
      [selfSelectionGame, selfApGameName]
        .map((value) => normalizeRuntimeLookupKey(value))
        .filter(Boolean),
    [selfApGameName, selfSelectionGame]
  );
  const selfSelectedModuleId = useMemo(() => {
    if (resolvedLaunchModuleId) return resolvedLaunchModuleId;

    for (const module of runtimeModules) {
      const manifest = module?.manifest || {};
      const keys = [
        normalizeRuntimeLookupKey(String(module.moduleId || "")),
        normalizeRuntimeLookupKey(String((manifest as any).game_id || "")),
        normalizeRuntimeLookupKey(String((manifest as any).display_name || "")),
        normalizeRuntimeLookupKey(String((manifest as any).ap_game_name || "")),
      ].filter(Boolean);
      if (keys.some((key) => selfSelectionKeys.includes(key))) {
        return String(module.moduleId || "").trim();
      }
    }

    return "";
  }, [resolvedLaunchModuleId, runtimeModules, selfSelectionKeys]);
  const trackerId = roomStatus?.tracker || "";
  const trackerReady = Boolean(trackerId && generation?.room_url);
  const lobbyDescription = cleanSoloDescription(lobby?.description || "No description yet.");

  useEffect(() => {
    let cancelled = false;

    const loadLocalLaunchReadiness = async () => {
      if (!selfSelections.length || !selfSelectedModuleId) {
        if (!cancelled) setLocalLaunchReadiness(null);
        return;
      }

      const readiness = await getLaunchReadiness(selfSelectedModuleId).catch(() => null);
      if (cancelled) return;
      if (!readiness) {
        setLocalLaunchReadiness(null);
        return;
      }

      if (readiness.ready) {
        setLocalLaunchReadiness({
          ready: true,
          label: "Local setup ready",
          note: "Your local game setup is ready for this lobby selection.",
          action: null,
        });
        return;
      }

      setLocalLaunchReadiness({
        ready: false,
        label: "Local setup required",
        note: getLaunchErrorMessage(readiness),
        errorText: getLaunchErrorMessage(readiness),
        action: readiness.correctionAction || null,
      });
    };

    void loadLocalLaunchReadiness();

    return () => {
      cancelled = true;
    };
  }, [selfSelectedModuleId, selfSelectionGame]);

  useEffect(() => {
    let cancelled = false;

    const resolveLaunchModule = async () => {
      if (!selfDownloadUrl) {
        if (!cancelled) setResolvedLaunchModuleId("");
        return;
      }

      try {
        const resolved = await runtime.resolveModuleForDownload?.(selfDownloadUrl);
        const moduleId =
          resolved && typeof resolved === "object" && (resolved as any).ok
            ? String((resolved as any).moduleId || "").trim()
            : "";
        if (!cancelled) setResolvedLaunchModuleId(moduleId);
      } catch {
        if (!cancelled) setResolvedLaunchModuleId("");
      }
    };

    void resolveLaunchModule();

    return () => {
      cancelled = true;
    };
  }, [selfDownloadUrl]);

  const selfLaunchBlockedBySetup = Boolean(selfSelectionGame && localLaunchReadiness && !localLaunchReadiness.ready);
  const selfLaunchDisabled = !selfCanLaunch || selfLaunchBlockedBySetup;
  const selfLaunchTitle = !selfCanLaunch
    ? "No launch package is available for you yet."
    : selfLaunchBlockedBySetup
      ? String(localLaunchReadiness?.errorText || localLaunchReadiness?.note || "Local setup is incomplete.")
      : "Launch your game through SekaiLink.";

  return (
    <div className="skl-lobby-scroll">
      <section className="skl-lobby-page" id="lobby-room">
        <div className="skl-lobby-hero skl-lobby-hero-compact">
          <div>
            <span className="skl-eyebrow">Sync</span>
            <h1 className="skl-title">{soloMode ? `${lobby?.name || "Lobby"} · Solo` : (lobby?.name || "Lobby")}</h1>
            <p className="skl-lobby-description">{lobbyDescription || "No description yet."}</p>
          </div>
          <div className="skl-lobby-meta">
            <div><span>Owner:</span> <span id="lobby-owner-name">{ownerName}</span></div>
            <div><span>Created:</span> {(() => {
              const d = parseServerDate(lobby?.created_at);
              return d ? d.toLocaleDateString() : "-";
            })()}</div>
          </div>
        </div>

        {lobby?.is_private && joinRequired && (
          <div className="skl-card skl-private-lobby">
            <div className="skl-card-header">
              <h2>Private lobby</h2>
              <span className="skl-card-sub">Enter the lobby password to join and chat.</span>
            </div>
            <form id="lobby-password-form" className="skl-password-form" onSubmit={onJoinPrivate}>
              <input type="password" name="password" id="lobby-password-input" placeholder="Lobby password" maxLength={64} />
              <button className="skl-btn primary" type="submit">Join lobby</button>
            </form>
            {!authed && (
              <div className="skl-ready-status">Sign in with your SekaiLink account to join this lobby and unlock chat.</div>
            )}
            <div id="lobby-password-status" className="skl-ready-status">{passwordError}</div>
          </div>
        )}

        <div className="skl-lobby-grid">
          {!soloMode && (
            <div className="skl-card skl-chat-card">
              <div className="skl-card-header skl-card-header-row">
                <div className="skl-card-header-text">
                  <h2>Chatroom</h2>
                  <span className="skl-card-sub">Live updates for this lobby.</span>
                </div>
                {selfMember && (
                  <div style={{ display: "flex", gap: 8, alignItems: "center", flexWrap: "wrap" }}>
                    {lobby?.discord_voice_url && (
                      <button
                        className="skl-btn ghost"
                        type="button"
                        onClick={() => {
                          sfx.play("confirm", 0.2);
                          const url = lobby.discord_voice_url || "";
                          if (runtime.openExternal) runtime.openExternal(url);
                          else window.open(url, "_blank", "noopener,noreferrer");
                        }}
                        title="Open the external voice room for this lobby."
                      >
                        Open Voice Room
                      </button>
                    )}
                    <button
                      className="skl-btn primary"
                      type="button"
                      disabled={selfLaunchDisabled}
                      onClick={() => {
                        sfx.play("confirm", 0.2);
                        handleLaunch(selfDownloadUrls || selfDownloadUrl);
                      }}
                      title={selfLaunchTitle}
                    >
                      Launch
                    </button>
                  </div>
                )}
              </div>
                  <div id="lobby-chat-log" className="skl-chat-log" ref={chatLogRef}>
                    {messages
                      .filter((message) => {
                        const key = String(message.author || "").trim().toLowerCase();
                        return !blockedAuthors.includes(key);
                      })
                      .map((message) => (
                  <div className="skl-chat-message" key={message.id}>
                    {message.avatar_url ? (
                      <img className="skl-chat-avatar" src={message.avatar_url} alt="Avatar" />
                    ) : (
                      <div style={{ width: "28px" }}></div>
                    )}
                    <div className="skl-chat-body">
                      <div className="skl-chat-header">
                        <span className="skl-chat-author">{message.author}</span>
                        <span>{formatTime(message.created_at)}</span>
                      </div>
                      <div>{message.content}</div>
                    </div>
                  </div>
                ))}
              </div>
              <form id="lobby-chat-form" className="skl-chat-form" onSubmit={sendMessage}>
                <input
                  name="message"
                  type="text"
                  placeholder="Type a message…"
                  maxLength={800}
                  className="input"
                  value={chatInput}
                  onChange={(event) => setChatInput(event.target.value)}
                  disabled={!authed || joinRequired}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.preventDefault();
                      (event.currentTarget.form as HTMLFormElement | null)?.requestSubmit?.();
                    }
                  }}
                />
                <button className="skl-btn primary" type="submit" disabled={!authed || joinRequired}>Send</button>
              </form>
              {!authed && (
                <div className="skl-chat-hint">Sign in with your SekaiLink account to post messages in this lobby.</div>
              )}
            </div>
          )}

          <div className="skl-side-stack">
            <div className="skl-card skl-side-tabs">
              <div className="skl-card-header">
                <h2>Panel</h2>
                <span className="skl-card-sub">{soloMode ? "Solo tools and local client." : "Lobby tools and players."}</span>
              </div>

              {!soloMode && (
                <div className="sklp-lobby-tabbar" role="tablist" aria-label="Lobby panels">
                  <button type="button" className={`sklp-tab-btn${rightTab === "users" ? " active" : ""}`} onClick={() => setRightTab("users")}>
                    <svg className="sklp-tab-ico" viewBox="0 0 24 24" fill="none" aria-hidden="true">
                      <path d="M16 19c0-2.2-1.8-4-4-4s-4 1.8-4 4" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
                      <circle cx="12" cy="9" r="3" stroke="currentColor" strokeWidth="1.8" />
                    </svg>
                    Users
                  </button>
                  <button type="button" className={`sklp-tab-btn${rightTab === "info" ? " active" : ""}`} onClick={() => setRightTab("info")}>
                    <svg className="sklp-tab-ico" viewBox="0 0 24 24" fill="none" aria-hidden="true">
                      <circle cx="12" cy="12" r="8.2" stroke="currentColor" strokeWidth="1.8" />
                      <path d="M12 10v6M12 7.5h.01" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
                    </svg>
                    Info
                  </button>
                </div>
              )}

              {soloMode && (
                <div className="skl-card-header-row" style={{ marginBottom: 10 }}>
                  {selfMember && (
                    <button
                      className="skl-btn primary"
                      type="button"
                      disabled={selfLaunchDisabled}
                      onClick={() => {
                        sfx.play("confirm", 0.2);
                        handleLaunch(selfDownloadUrls || selfDownloadUrl);
                      }}
                      title={selfLaunchTitle}
                    >
                      Launch
                    </button>
                  )}
                </div>
              )}

              <div className="sklp-tab-content">
                {!soloMode && rightTab === "users" && (
                  <>
                    <div id="lobby-members" className="skl-member-list">
                      {members.map((member) => {
                        const memberSelections = lobbySelectionsForMember(member);
                        const memberSelectionSummary = summarizeSelectionSet(memberSelections);
                        const playerName = memberSelections[0]?.player_name || member.active_yaml_player || member.name;
                        const slotId = resolvePlayerSlotId(playersByName, playerName);
                        const trackerAvailable = Boolean(trackerId && generation?.room_url);
                        const isSelf = Boolean(member.discord_id && member.discord_id === me?.discord_id);
                        const downloadUrl = slotId ? downloadsBySlot.get(slotId) : undefined;
                        const downloadUrls = slotId ? downloadsBySlotMulti.get(slotId) : undefined;
                        const hasDownloads = Boolean(downloadUrls?.length || downloadUrl);
                        const apGameName = resolvePlayerApGameName(roomStatus, slotId, playerName);
                        const canPatchlessLaunch = Boolean(isSelf && supportsPatchlessAutoLaunch(apGameName));
                        const canLaunch = hasDownloads || canPatchlessLaunch;
                        const localReadyKnown = Boolean(member.local_ready_known);
                        const localReadyText = member.local_ready ? "Local ready" : "Local setup required";
                        const localReadyTitle = member.local_ready
                          ? "This player's local launch setup is ready."
                          : String(member.local_ready_note || "This player's local setup is not ready yet.");

                        return (
                          <div
                            key={`${lobbyMemberIdentity(member)}-${memberSelectionSummary}`}
                            className={`skl-member-row${member.ready ? " ready" : ""}`}
                            onContextMenu={(event) => {
                              event.preventDefault();
                              setContextMenu({
                                x: event.clientX,
                                y: event.clientY,
                                member,
                                slotId,
                                trackerAvailable,
                                isSelf,
                                canLaunch,
                                downloadUrl,
                                downloadUrls: downloadUrls || undefined,
                              });
                            }}
                            onClick={() => {
                              setPlayerModal({
                                name: member.name,
                                yaml: memberSelectionSummary,
                                slotId,
                                selections: memberSelections,
                              });
                              setPlayerModalOpen(true);
                            }}
                          >
                            <div className="skl-member-left">
                              {member.is_host && <span className="skl-member-crown">👑</span>}
                              <div className="skl-member-meta">
                                <div className="skl-member-name">{member.name}</div>
                                <div className="skl-member-selection" title={selectionTooltip(memberSelections)}>
                                  {memberSelectionSummary}
                                </div>
                              </div>
                              <span className="skl-member-yaml-icon" title={selectionTooltip(memberSelections)}>G</span>
                              {isSelf && downloadUrls && downloadUrls.length > 1 && (
                                <span className="skl-member-yaml-icon" title={`${downloadUrls.length} patches available`}>+</span>
                              )}
                            </div>
                            <div className="skl-member-right">
                              <div className="skl-member-ready">
                                <span className={`skl-ready-badge ${member.ready ? "" : "not-ready"}`}>{member.ready ? "Room ready" : "Room not ready"}</span>
                                {localReadyKnown && (
                                  <span
                                    className={`skl-ready-badge skl-ready-badge-secondary ${member.local_ready ? "" : "not-ready"}`}
                                    title={localReadyTitle}
                                  >
                                    {localReadyText}
                                  </span>
                                )}
                              </div>
                            </div>
                          </div>
                        );
                      })}
                    </div>
                    {members.length > 0 && (
                      <div className="skl-ready-status" style={{ marginTop: 10 }}>
                        Room-ready players: {roomReadyMembers.length} · Local-ready players: {localReadyMembers.length}
                        {localSetupBlockedMembers.length > 0 && ` · Local blockers: ${localSetupBlockedMembers.length}`}
                        {localSetupUnknownMembers.length > 0 && ` · Local checks pending: ${localSetupUnknownMembers.length}`}
                      </div>
                    )}
                    <div id="lobby-members-empty" className="skl-empty-note">
                      {members.length ? "" : "No players yet."}
                    </div>
                    <div className="skl-control-grid skl-user-action-grid">
                      <button className="skl-control-btn" type="button" disabled={selfSelectionLocked} onClick={() => { sfx.play("confirm", 0.2); setYamlOpen(true); }}>
                        <span className="skl-control-icon">◇</span>
                        Select Seed
                      </button>
                      {selfMember && !soloMode && (
                        <button
                          className="skl-control-btn"
                          type="button"
                          onClick={toggleReady}
                          disabled={generationLocked}
                          title={
                            generationLocked
                              ? "Generation is already completed or in progress."
                              : selfLaunchBlockedBySetup
                                ? String(localLaunchReadiness?.errorText || localLaunchReadiness?.note || "Local setup is incomplete.")
                                : "Mark yourself room-ready for generation."
                          }
                        >
                          <span className="skl-control-icon">{ready ? "II" : "RDY"}</span>
                          {ready ? "Unready" : "Ready"}
                        </button>
                      )}
                      <button className="skl-control-btn" type="button" onClick={() => { sfx.play("confirm", 0.2); setTerminalOpen(true); }}>
                        <span className="skl-control-icon">⌨</span>
                        Room Terminal
                      </button>
                      {canManageLobby && (
                        <button className="skl-control-btn" type="button" onClick={() => { sfx.play("confirm", 0.2); setSettingsOpen(true); }}>
                          <span className="skl-control-icon">⚙</span>
                          Sync Settings
                        </button>
                      )}
                      {canManageLobby && (
                        <button
                          className="skl-control-btn primary"
                          type="button"
                          disabled={generationLocked}
                          onClick={() => {
                            if (!allReady) {
                              showToast("Someone is not ready.", "error");
                              return;
                            }
                            if (!allLocalReady) {
                              if (localSetupUnknownMembers.length > 0) {
                                showToast("Some players are still checking their local setup.", "error");
                                return;
                              }
                              showToast("Some players are room-ready but still blocked by local setup.", "error");
                              return;
                            }
                            sfx.play("confirm", 0.2);
                            setGenerateConfirmOpen(true);
                          }}
                        >
                          <span className="skl-control-icon">›</span>
                          Generate
                        </button>
                      )}
                    </div>
                  </>
                )}

                {soloMode && (
                  <>
                    <div className="skl-control-grid">
                      <button className="skl-control-btn" type="button" disabled={selfSelectionLocked} onClick={() => { sfx.play("confirm", 0.2); setYamlOpen(true); }}>
                        <span className="skl-control-icon">◇</span>
                        Game Selection
                      </button>
                      <button className="skl-control-btn" type="button" onClick={() => { sfx.play("confirm", 0.2); setTerminalOpen(true); }}>
                        <span className="skl-control-icon">⌨</span>
                        Room Terminal
                      </button>
                      {canManageLobby && (
                        <button className="skl-control-btn" type="button" onClick={() => { sfx.play("confirm", 0.2); setSettingsOpen(true); }}>
                          <span className="skl-control-icon">⚙</span>
                          Room Settings
                        </button>
                      )}
                      <button
                        className="skl-control-btn"
                        type="button"
                        disabled={!trackerReady || !selfSlotId}
                        onClick={() => {
                          if (!trackerReady) {
                            showToast("Tracker is available after generation.", "error");
                            return;
                          }
                          if (!selfSlotId) {
                            showToast("No player slot found for tracker.", "error");
                            return;
                          }
                          sfx.play("confirm", 0.2);
                          setTrackerPlayer(selfSlotId);
                          setTrackerOpen(true);
                        }}
                      >
                        <span className="skl-control-icon">◎</span>
                        Tracker
                      </button>
                      {selfMember && !soloMode && (
                        <button
                          className="skl-control-btn"
                          type="button"
                          onClick={toggleReady}
                          disabled={generationLocked}
                          title={
                            generationLocked
                              ? "Generation is already completed or in progress."
                              : selfLaunchBlockedBySetup
                                ? String(localLaunchReadiness?.errorText || localLaunchReadiness?.note || "Local setup is incomplete.")
                                : "Mark yourself room-ready for generation."
                          }
                        >
                          <span className="skl-control-icon">{ready ? "II" : "RDY"}</span>
                          {ready ? "Room Unready" : "Room Ready"}
                        </button>
                      )}
                      {selfMember && (
                        <button
                          className="skl-control-btn"
                          type="button"
                          disabled={!apClientConnected}
                          onClick={openHints}
                          title={!apClientConnected ? "Launch your game first to connect the local session runtime." : "Open the hint list and request hints."}
                        >
                          <span className="skl-control-icon">i</span>
                          Hints
                        </button>
                      )}
                      {canManageLobby && (
                        <button
                          className="skl-control-btn primary"
                          type="button"
                          disabled={generationLocked}
                          onClick={() => {
                            if (!canManageLobby) {
                              showToast("Only the host can generate.", "error");
                              return;
                            }
                            if (generationLocked) {
                              showToast("Generation is already completed or in progress.", "error");
                              return;
                            }
                            if (!allReady) {
                              showToast("Someone is not ready.", "error");
                              return;
                            }
                            if (!allLocalReady) {
                              if (localSetupUnknownMembers.length > 0) {
                                showToast("Some players are still checking their local setup.", "error");
                                return;
                              }
                              showToast("Some players are room-ready but still blocked by local setup.", "error");
                              return;
                            }
                            sfx.play("confirm", 0.2);
                            setGenerateConfirmOpen(true);
                          }}
                        >
                          <span className="skl-control-icon">›</span>
                          Generate
                        </button>
                      )}
                    </div>

                    <div className="skl-client-status">
                      <div className="skl-client-status-head">
                        <div>
                          <div className="skl-client-title">Local Session Runtime</div>
                          <div className="skl-client-sub">
                            Room Link: <strong>{apClientConnected ? "Connected" : "Disconnected"}</strong>{" "}
                            · Runtime Bridge: <strong>{apBizhawkConnected === "unknown" ? "-" : apBizhawkConnected}</strong>{" "}
                            · Game Profile: <strong>{apHandlerGame || "-"}</strong>
                          </div>
                        </div>
                        <div className="skl-client-status-actions">
                          <button className="skl-btn ghost" type="button" onClick={copyApDebug}>Copy Debug</button>
                          <button
                            className="skl-btn ghost sklp-collapse-btn"
                            type="button"
                            onClick={() => {
                              sfx.play("confirm", 0.15);
                              setLocalClientOpen((prev) => !prev);
                            }}
                            aria-expanded={localClientOpen}
                            title={localClientOpen ? "Collapse" : "Expand"}
                          >
                            {localClientOpen ? "Hide" : "Show"}
                          </button>
                        </div>
                      </div>
                      {localClientOpen && (
                        <>
                          <div className="skl-client-launch-options">
                            <label className="skl-client-launch-toggle" title="When enabled, SekaiLink asks the tracker layout each time before launch.">
                              <input
                                type="checkbox"
                                checked={forceTrackerVariantPrompt}
                                onChange={(event) => setForceTrackerVariantPrompt(event.target.checked)}
                              />
                              Always ask tracker layout before launch
                            </label>
                          </div>

                          {localLaunchReadiness && (
                            <div className="skl-ready-status" style={{ display: "flex", gap: 10, alignItems: "center", flexWrap: "wrap", marginBottom: 10 }}>
                              <strong style={{ color: localLaunchReadiness.ready ? "rgba(0,255,200,0.88)" : "rgba(255,206,138,0.92)" }}>
                                {localLaunchReadiness.label}
                              </strong>
                              <span>{localLaunchReadiness.note}</span>
                              {localLaunchReadiness.action?.route && (
                                <button
                                  className="skl-btn ghost"
                                  type="button"
                                  onClick={() => navigate(localLaunchReadiness.action?.route || "/settings")}
                                >
                                  {localLaunchReadiness.action?.label || "Open Setup"}
                                </button>
                              )}
                            </div>
                          )}

                          <div className="skl-client-metrics">
                            <div className="skl-client-metric">
                              <span className="skl-client-metric-k">Hint points</span>
                              <span className="skl-client-metric-v">{apHintPoints === null ? "-" : apHintPoints}</span>
                            </div>
                            <div className="skl-client-metric">
                              <span className="skl-client-metric-k">Hint cost</span>
                              <span className="skl-client-metric-v">{apHintCost === null ? "-" : `${apHintCost}%`}</span>
                            </div>
                            <div className="skl-client-metric">
                              <span className="skl-client-metric-k">Last error</span>
                              <span className="skl-client-metric-v">{apLastError ? apLastError : "-"}</span>
                            </div>
                          </div>

                          <pre className="skl-client-log" aria-label="Local client log">
                            {apLogLines.length
                              ? apLogLines.map((l) => `[${l.level}] ${l.text}`).join("\n")
                              : "No local session logs yet. Use Launch to start the runtime."}
                          </pre>
                        </>
                      )}
                    </div>
                    <div className="skl-control-status">
                      <span className={`skl-ready-badge ${ready ? "" : "not-ready"}`}>{soloMode ? "Auto room-ready" : (ready ? "Room ready" : "Room not ready")}</span>
                      <span className="skl-ready-status">Room generation: {generationState}</span>
                      <span className="skl-ready-status">{generation?.error || (soloMode ? "Solo mode marks room readiness automatically." : (allReady ? "All players are room-ready." : "Waiting for all players to become room-ready."))}</span>
                      {!soloMode && (
                        <span className="skl-ready-status">
                          {allLocalReady
                            ? "All players with a room slot are locally ready to launch."
                            : localSetupUnknownMembers.length > 0
                              ? `Waiting for ${localSetupUnknownMembers.length} local setup check${localSetupUnknownMembers.length > 1 ? "s" : ""}.`
                              : `${localSetupBlockedMembers.length} player${localSetupBlockedMembers.length === 1 ? "" : "s"} still blocked by local setup.`}
                        </span>
                      )}
                      {localLaunchReadiness && !localLaunchReadiness.ready && (
                        <span className="skl-ready-status">Local launch gate: {localLaunchReadiness.note}</span>
                      )}
                    </div>
                  </>
                )}

                {!soloMode && rightTab === "info" && (
                  <div id="lobby-room-info" className="skl-room-info">
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Room</span>
                      <span id="lobby-room-link" className="skl-room-value">
                        {roomUrl ? <a href={roomUrl} target="_blank" rel="noopener">Open room</a> : "Not generated"}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Seed timer</span>
                      <span id="lobby-seed-timer" className="skl-room-value">{seedTimer}</span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Server</span>
                      <span id="lobby-room-server" className="skl-room-value">
                        {roomServerAddress || "-"}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Seed</span>
                      <span id="lobby-seed-link" className="skl-room-value">
                        {seedUrl ? <a href={seedUrl} target="_blank" rel="noopener">View seed</a> : "-"}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Spoiler log</span>
                      <span className="skl-room-value">
                        {spoilerMode > 0 && spoilerLogUrl ? (
                          <a href={spoilerLogUrl} target="_blank" rel="noopener">Open spoiler log</a>
                        ) : (
                          "Disabled"
                        )}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Room snapshot</span>
                      <span className="skl-room-value">
                        {(() => {
                          const totalConfigs = members.reduce((sum, member) => sum + lobbySelectionsForMember(member).length, 0);
                          const totalGames = new Set(
                            members.flatMap((member) => lobbySelectionsForMember(member).map((entry) => String(entry.game || "Unknown")))
                          ).size;
                          if (!totalConfigs) return "No room selections yet";
                          return `${members.length} players · ${totalGames} games · ${totalConfigs} configs`;
                        })()}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Launch readiness</span>
                      <span className="skl-room-value">
                        {soloMode
                          ? "Managed locally"
                          : `${localReadyMembers.length}/${members.length || 0} local-ready · ${localSetupBlockedMembers.length} blocked · ${localSetupUnknownMembers.length} pending`}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Your selection</span>
                      <span className="skl-room-value" title={selectionTooltip(selfSelections)}>
                        {selectedGameLabel}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Players</span>
                      <span id="lobby-room-players" className="skl-room-value">
                        {trackerStats?.players_total ? `${trackerStats.players_total} players` : "-"}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Checks</span>
                      <span id="lobby-room-checks" className="skl-room-value">
                        {trackerStats?.total_locations ? `${Math.round(((trackerStats.checks_done || 0) / trackerStats.total_locations) * 100)}% (${trackerStats.checks_done || 0}/${trackerStats.total_locations})` : "-"}
                      </span>
                    </div>
                    <div className="skl-room-info-row">
                      <span className="skl-room-label">Completed</span>
                      <span id="lobby-room-complete" className="skl-room-value">
                        {trackerStats?.players_total ? `${trackerStats.completed_players || 0}/${trackerStats.players_total}` : "-"}
                      </span>
                    </div>
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>

        {contextMenu && (
          <div
            className="sklp-context-overlay"
            onClick={() => setContextMenu(null)}
            onContextMenu={(event) => {
              event.preventDefault();
              setContextMenu(null);
            }}
          >
            <div
              className="sklp-context-menu"
              style={{ left: contextMenu.x, top: contextMenu.y }}
              onClick={(event) => event.stopPropagation()}
            >
              <button
                type="button"
                onClick={() => {
                  sfx.play("confirm", 0.2);
                  openMemberProfile(contextMenu.member);
                  setContextMenu(null);
                }}
              >
                View Profile
              </button>
              {!contextMenu.isSelf && (
                <>
                  {(() => {
                    const targetId = String(contextMenu.member.discord_id || "").trim();
                    const isFriend = targetId ? friendUserIds.includes(targetId) : false;
                    const incomingPending = targetId ? incomingFriendIds.includes(targetId) : false;
                    const outgoingPending = targetId ? outgoingFriendIds.includes(targetId) : false;
                    if (!targetId || isFriend) return null;
                    if (incomingPending) {
                      return (
                        <button
                          type="button"
                          onClick={() => {
                            sfx.play("confirm", 0.2);
                            void acceptIncomingFriendRequest(targetId, contextMenu.member.name);
                            setContextMenu(null);
                          }}
                        >
                          {t("social.accept_friend_request")}
                        </button>
                      );
                    }
                    if (outgoingPending) {
                      return (
                        <button type="button" disabled title={t("social.friend_request_already_sent_title")}>
                          {t("social.friend_request_sent")}
                        </button>
                      );
                    }
                    return (
                      <button
                        type="button"
                        onClick={() => {
                          sfx.play("confirm", 0.2);
                          void sendFriendRequest(targetId, contextMenu.member.name);
                          setContextMenu(null);
                        }}
                      >
                        {t("social.send_friend_request")}
                      </button>
                    );
                  })()}
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      toggleBlockMember(contextMenu.member);
                      setContextMenu(null);
                    }}
                  >
                    {blockedAuthors.includes(String(contextMenu.member.name || "").trim().toLowerCase()) ? "Unblock" : "Block"}
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      setReportModal({
                        open: true,
                        targetName: contextMenu.member.name,
                        targetId: contextMenu.member.discord_id || "",
                        reason: "harassment",
                        comment: "",
                        submitting: false,
                        error: "",
                      });
                      setContextMenu(null);
                    }}
                  >
                    Report
                  </button>
                </>
              )}
              {(lobbySelectionsForMember(contextMenu.member)[0]?.player_name && (canManageLobby || contextMenu.isSelf)) && (
                <>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      releasePlayer(lobbySelectionsForMember(contextMenu.member)[0]?.player_name || contextMenu.member.active_yaml_player || contextMenu.member.name, "release");
                      setContextMenu(null);
                    }}
                  >
                    Release
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      releasePlayer(lobbySelectionsForMember(contextMenu.member)[0]?.player_name || contextMenu.member.active_yaml_player || contextMenu.member.name, "slow");
                      setContextMenu(null);
                    }}
                  >
                    Slow Release
                  </button>
                </>
              )}
              {canManageLobby && contextMenu.member.name && contextMenu.member.discord_id !== me?.discord_id && (
                <>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      kickOrBanMember(contextMenu.member.name, "kick");
                      setContextMenu(null);
                    }}
                  >
                    Kick
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      kickOrBanMember(contextMenu.member.name, "ban");
                      setContextMenu(null);
                    }}
                  >
                    Ban
                  </button>
                </>
              )}
            </div>
          </div>
        )}

        <div className={`skl-modal${moderationModal.open ? " open" : ""}`} aria-hidden={!moderationModal.open}>
          <div
            className="skl-modal-backdrop"
            onClick={() => {
              if (!moderationModal.open) return;
              const redirect = moderationModal.redirectUrl || "/";
              setModerationModal({ open: false, message: "", redirectUrl: "/" });
              window.location.hash = redirect;
            }}
          ></div>
          <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-moderation-title">
            <div className="skl-modal-header">
              <h3 id="lobby-moderation-title">Lobby Access Updated</h3>
            </div>
            <div className="skl-modal-body">
              <p style={{ margin: 0 }}>{moderationModal.message || "You can no longer stay in this lobby."}</p>
            </div>
            <div className="skl-modal-actions">
              <button
                className="skl-btn primary"
                type="button"
                onClick={() => {
                  const redirect = moderationModal.redirectUrl || "/";
                  setModerationModal({ open: false, message: "", redirectUrl: "/" });
                  window.location.hash = redirect;
                }}
              >
                Back to Home
              </button>
            </div>
          </div>
        </div>

        {statusMessage && <div className="skl-ready-status">{statusMessage}</div>}
        {launchStatus && <div className="skl-ready-status">{launchStatus}</div>}
        {manualDownload && (
          <div className="skl-ready-status" style={{ display: "flex", gap: 10, flexWrap: "wrap", alignItems: "center" }}>
            <div style={{ flex: "1 1 320px" }}>
              <div style={{ fontWeight: 700 }}>Manual Action Required</div>
              <div style={{ fontFamily: "monospace", fontSize: 12, opacity: 0.9, wordBreak: "break-all" }}>
                {manualDownload.path}
              </div>
              {manualDownload.installedPath && (
                <div style={{ fontFamily: "monospace", fontSize: 12, opacity: 0.85, wordBreak: "break-all", marginTop: 6 }}>
                  Installed: {manualDownload.installedPath}
                </div>
              )}
              <div style={{ fontSize: 12, opacity: 0.75 }}>
                {manualDownload.apGameName ? `Game: ${manualDownload.apGameName}` : ""}
                {manualDownload.ext ? `  File: ${manualDownload.ext}` : ""}
              </div>
              {manualDownload.note && <div style={{ fontSize: 12, opacity: 0.85, marginTop: 6 }}>{manualDownload.note}</div>}
            </div>
            {manualDownload.ext === ".apsm64ex" && (
              <button
                className="skl-btn primary"
                type="button"
                onClick={() => {
                  sfx.play("confirm", 0.25);
                  setSm64exTutorialOpen(true);
                }}
              >
                SM64EX Setup Guide
              </button>
            )}
            <button
              className="skl-btn ghost"
              type="button"
              onClick={async () => {
                try {
                  await runtime.showItemInFolder?.(manualDownload.path);
                } catch {
                  // ignore
                }
              }}
            >
              Open Folder
            </button>
            {manualDownload.installedPath && (
              <button
                className="skl-btn ghost"
                type="button"
                onClick={async () => {
                  try {
                    await runtime.showItemInFolder?.(manualDownload.installedPath || "");
                  } catch {
                    // ignore
                  }
                }}
              >
                Open Install
              </button>
            )}
            <button
              className="skl-btn ghost"
              type="button"
              onClick={async () => {
                try {
                  await navigator.clipboard.writeText(manualDownload.path);
                  showToast("Copied path.", "success");
                } catch {
                  showToast("Unable to copy path.", "error");
                }
              }}
            >
              Copy Path
            </button>
          </div>
        )}
      </section>

      {toast && (
        <div className="skl-toast-stack" aria-live="polite">
          <div className={`skl-toast ${toast.kind}`}>{toast.text}</div>
        </div>
      )}

      <div className={`skl-terminal-drawer${terminalOpen ? " open" : ""}`} id="lobby-terminal-drawer" aria-hidden={!terminalOpen}>
        <div className="skl-terminal-backdrop" data-terminal-close onClick={() => setTerminalOpen(false)}></div>
        <div className="skl-terminal-panel">
          <div className="skl-terminal-header">
            <div>
              <p className="skl-eyebrow">Room Terminal</p>
              <h2>Live output</h2>
            </div>
            <button className="skl-btn ghost" type="button" onClick={() => setTerminalOpen(false)}>Close</button>
          </div>
          <div id="lobby-terminal-log" className="skl-terminal-log">{terminalDisplayLog}</div>
          <form id="lobby-terminal-form" className="skl-terminal-form" onSubmit={sendTerminal}>
            <input type="text" id="lobby-terminal-input" placeholder="Enter room command" value={terminalInput} onChange={(event) => setTerminalInput(event.target.value)} />
            <button className="skl-btn ghost" type="submit">Send</button>
          </form>
          <div id="lobby-terminal-status" className="skl-ready-status">{terminalStatus}</div>
        </div>
      </div>

      <div className={`skl-modal${yamlOpen ? " open" : ""}`} id="lobby-yaml-modal" aria-hidden={!yamlOpen}>
        <div className="skl-modal-backdrop" data-yaml-close onClick={() => setYamlOpen(false)}></div>
        <div className="skl-modal-panel skl-game-selection-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-yaml-title">
          <div className="skl-modal-header">
            <div>
              <p className="skl-eyebrow">Lobby Selection</p>
              <h3 id="lobby-yaml-title">Select Seed</h3>
            </div>
            <button className="skl-modal-close" type="button" onClick={() => setYamlOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <div className="skl-game-selection-layout">
              <section className="skl-game-selection-column">
                <div className="skl-selection-step">
                  <span>1</span>
                  <strong>Game</strong>
                </div>
                <div className="skl-yaml-active-list" id="lobby-game-selection-list">
                  {gameConfigGroups.map((group) => {
                    const isSelected = selectedGameGroup?.id === group.id;
                    const count = activeConfigsByGame.get(group.id)?.length || 0;
                    return (
                      <button
                        key={group.id}
                        type="button"
                        className={`skl-yaml-card${isSelected ? " active" : ""}`}
                        onClick={() => {
                          setGameModalSelection(group.id);
                          setYamlModalSelection("");
                        }}
                        style={{ textAlign: "left", cursor: "pointer" }}
                        disabled={selfSelectionLocked}
                      >
                        <div className="skl-yaml-title">{group.game || "Unknown"}</div>
                        <div className="skl-yaml-meta">
                          {group.configs.length} config{group.configs.length > 1 ? "s" : ""} available
                          {count ? ` · ${count} selected` : ""}
                        </div>
                      </button>
                    );
                  })}
                </div>
              </section>

              <section className="skl-game-selection-column">
                <div className="skl-selection-step">
                  <span>2</span>
                  <strong>Seed</strong>
                </div>
                <div className="skl-yaml-active-list" id="lobby-config-selection-list">
                  {(selectedGameGroup?.configs || []).map((entry) => {
                    const isSelected = yamlModalSelection === entry.id;
                    const isActive = activeYamls.some((item) => item.id === entry.id);
                    return (
                      <button
                        key={entry.id}
                        type="button"
                        className={`skl-yaml-card${isSelected || isActive ? " active" : ""}`}
                        onClick={() => setYamlModalSelection(entry.id)}
                        style={{ textAlign: "left", cursor: "pointer" }}
                        disabled={generationLocked}
                      >
                        <div className="skl-yaml-title">{entry.title}</div>
                        <div className="skl-yaml-meta">
                          Player: {entry.player_name || "Default"}{entry.custom ? " · Custom" : ""}
                          {isActive ? " · Added to sync" : ""}
                        </div>
                      </button>
                    );
                  })}
                  {!selectedGameGroup && <div className="skl-empty-note">No seeds available for this game yet.</div>}
                </div>
              </section>
            </div>

            <section className="skl-selected-configs">
              <div className="skl-selection-step">
                <span>3</span>
                <strong>This Sync</strong>
              </div>
              <div className="skl-yaml-selected-list">
                {activeYamls.map((entry) => (
                  <div className="skl-yaml-active-row" key={entry.id}>
                    <div className="skl-yaml-active-text">
                      <strong>{entry.game || "Unknown"}</strong> / {entry.title}
                    </div>
                    <button
                      className="skl-yaml-remove"
                      type="button"
                      disabled={selfSelectionLocked}
                      onClick={() => void removeGameConfig(entry.id)}
                    >
                      Remove
                    </button>
                  </div>
                ))}
                {!activeYamls.length && <div className="skl-empty-note">No seed selected for this sync yet.</div>}
              </div>
            </section>

            <div className="skl-ready-row" style={{ justifyContent: "flex-end" }}>
              <button className="skl-btn ghost" type="button" onClick={() => setYamlOpen(false)}>
                Done
              </button>
              <button
                className="skl-btn primary"
                type="button"
                disabled={selfSelectionLocked || !yamlModalSelection}
                onClick={async () => {
                  const ok = await setSingleActiveYaml(yamlModalSelection, "change");
                  if (ok) setYamlModalSelection("");
                }}
              >
                Add Seed
              </button>
            </div>
            <div id="lobby-yaml-status" className="skl-empty-note">
              {selfSelectionLocked
                ? "Selection is locked for this player until you unready or generation finishes."
                : "Selection is per sync and remains editable until Ready -> Generation."}
            </div>
            {!yamls.length && <div className="skl-empty-note">No seeds yet. Create one from Library or Quick Select.</div>}
          </div>
        </div>
      </div>

      <div className={`skl-modal${playerModalOpen ? " open" : ""}`} id="lobby-player-modal" aria-hidden={!playerModalOpen}>
        <div className="skl-modal-backdrop" data-player-close onClick={() => setPlayerModalOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-player-title">
          <div className="skl-modal-header">
            <h3 id="lobby-player-title">Player info</h3>
            <button className="skl-modal-close" type="button" onClick={() => setPlayerModalOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <div className="skl-player-info">
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Player</span>
                <span id="lobby-player-name" className="skl-player-info-value">{playerModal?.name || "-"}</span>
              </div>
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Selection Summary</span>
                <span id="lobby-player-yaml" className="skl-player-info-value">{playerModal?.yaml || "-"}</span>
              </div>
              {playerModal?.selections?.length ? (
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Selections</span>
                  <div className="skl-player-info-value skl-player-selection-list">
                    {playerModal.selections.map((entry) => (
                      <div key={entry.id || `${entry.game}-${entry.title}`} className="skl-player-selection-entry">
                        <strong>{entry.game || "Unknown"}</strong> / {entry.title || "Default"}
                      </div>
                    ))}
                  </div>
                </div>
              ) : null}
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Patches</span>
                <span className="skl-player-info-value">
                  {(() => {
                    const patches = playerModal?.slotId ? downloadsBySlotMulti.get(playerModal.slotId) || [] : [];
                    return patches.length ? `${patches.length} available` : "None";
                  })()}
                </span>
              </div>
              {playerModal?.slotId && downloadsBySlotMulti.get(playerModal.slotId)?.length ? (
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Launch</span>
                  <div className="skl-player-info-value">
                    {(() => {
                      const patches = downloadsBySlotMulti.get(playerModal.slotId) || [];
                      const isSelf = playerModal.name === selfMember?.name;
                      return (
                        <>
                          {patches.length > 1 && (
                            <button
                              type="button"
                              className="skl-btn ghost"
                              onClick={() => handleLaunch(patches)}
                              disabled={!isSelf}
                            >
                              {isSelf ? "Launch All" : "All Patches"}
                            </button>
                          )}
                          {patches.map((url) => {
                            const label = url.split("/").pop() || "Patch";
                            return (
                              <button
                                key={url}
                                type="button"
                                className="skl-btn ghost"
                                onClick={() => handleLaunch(url)}
                                disabled={!isSelf}
                              >
                                {isSelf ? `Launch ${label}` : label}
                              </button>
                            );
                          })}
                        </>
                      );
                    })()}
                  </div>
                </div>
              ) : null}
            </div>
          </div>
        </div>
      </div>

      <div className={`skl-modal${profileModal.open ? " open" : ""}`} id="lobby-profile-modal" aria-hidden={!profileModal.open}>
        <div className="skl-modal-backdrop" onClick={() => setProfileModal({ open: false, loading: false, error: "", data: null })}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-profile-title">
          <div className="skl-modal-header">
            <h3 id="lobby-profile-title">Player Profile</h3>
            <button className="skl-modal-close" type="button" onClick={() => setProfileModal({ open: false, loading: false, error: "", data: null })}>Close</button>
          </div>
          <div className="skl-modal-body">
            {profileModal.loading && <div className="skl-empty-note">Loading profile…</div>}
            {profileModal.error && !profileModal.loading && <div className="skl-ready-status">{profileModal.error}</div>}
            {!profileModal.loading && !profileModal.error && profileModal.data && (
              <div className="skl-player-info">
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Name</span>
                  <span className="skl-player-info-value">{profileModal.data.display_name || "-"}</span>
                </div>
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Status</span>
                  <span className="skl-player-info-value">{profileModal.data.status || "-"}</span>
                </div>
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Bio</span>
                  <span className="skl-player-info-value">{profileModal.data.bio || "-"}</span>
                </div>
                <div className="skl-player-info-row">
                  <span className="skl-player-info-label">Badges</span>
                  <span className="skl-player-info-value">
                    {Array.isArray(profileModal.data.badges) && profileModal.data.badges.length
                      ? profileModal.data.badges.join(", ")
                      : "-"}
                  </span>
                </div>
                {(() => {
                  const targetId = String(profileModal.data?.user_id || "").trim();
                  if (!targetId || targetId === me?.discord_id) return null;
                  const isFriend = friendUserIds.includes(targetId);
                  const incomingPending = incomingFriendIds.includes(targetId);
                  const outgoingPending = outgoingFriendIds.includes(targetId);
                  return (
                    <div className="skl-modal-actions" style={{ marginTop: 12 }}>
                      {!isFriend && !incomingPending && !outgoingPending && (
                        <button
                          className="skl-btn primary"
                          type="button"
                          onClick={() => void sendFriendRequest(targetId, profileModal.data?.display_name || "")}
                        >
                          {t("social.send_friend_request")}
                        </button>
                      )}
                      {!isFriend && incomingPending && (
                        <button
                          className="skl-btn primary"
                          type="button"
                          onClick={() => void acceptIncomingFriendRequest(targetId, profileModal.data?.display_name || "")}
                        >
                          {t("social.accept_friend_request")}
                        </button>
                      )}
                      {!isFriend && outgoingPending && (
                        <button className="skl-btn ghost" type="button" disabled>
                          {t("social.friend_request_sent")}
                        </button>
                      )}
                      {isFriend && (
                        <button className="skl-btn ghost" type="button" disabled>
                          {t("social.already_friends")}
                        </button>
                      )}
                    </div>
                  );
                })()}
              </div>
            )}
          </div>
        </div>
      </div>

      <div className={`skl-modal${reportModal.open ? " open" : ""}`} id="lobby-report-modal" aria-hidden={!reportModal.open}>
        <div
          className="skl-modal-backdrop"
          onClick={() =>
            setReportModal({
              open: false,
              targetName: "",
              targetId: "",
              reason: "harassment",
              comment: "",
              submitting: false,
              error: "",
            })
          }
        ></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-report-title">
          <div className="skl-modal-header">
            <h3 id="lobby-report-title">Report Player</h3>
            <button
              className="skl-modal-close"
              type="button"
              onClick={() =>
                setReportModal({
                  open: false,
                  targetName: "",
                  targetId: "",
                  reason: "harassment",
                  comment: "",
                  submitting: false,
                  error: "",
                })
              }
            >
              Close
            </button>
          </div>
          <div className="skl-modal-body">
            <div className="skl-player-info-row">
              <span className="skl-player-info-label">Player</span>
              <span className="skl-player-info-value">{reportModal.targetName || "-"}</span>
            </div>
            <div className="skl-chat-form" style={{ marginTop: 10 }}>
              <select
                value={reportModal.reason}
                onChange={(e) => setReportModal((prev) => ({ ...prev, reason: e.target.value }))}
                disabled={reportModal.submitting}
              >
                <option value="harassment">Harassment</option>
                <option value="hate">Hate speech</option>
                <option value="cheating">Cheating</option>
                <option value="spam">Spam</option>
                <option value="impersonation">Impersonation</option>
                <option value="other">Other</option>
              </select>
            </div>
            <div className="skl-chat-form" style={{ marginTop: 10 }}>
              <textarea
                value={reportModal.comment}
                placeholder="Comment (optional)"
                maxLength={500}
                onChange={(e) => setReportModal((prev) => ({ ...prev, comment: e.target.value }))}
                disabled={reportModal.submitting}
                style={{ minHeight: 100, resize: "vertical" as const }}
              />
            </div>
            {reportModal.error && <div className="skl-ready-status">{reportModal.error}</div>}
          </div>
          <div className="skl-modal-actions">
            <button
              className="skl-btn ghost"
              type="button"
              onClick={() =>
                setReportModal({
                  open: false,
                  targetName: "",
                  targetId: "",
                  reason: "harassment",
                  comment: "",
                  submitting: false,
                  error: "",
                })
              }
              disabled={reportModal.submitting}
            >
              Cancel
            </button>
            <button className="skl-btn primary" type="button" onClick={submitReport} disabled={reportModal.submitting}>
              {reportModal.submitting ? "Submitting..." : "Submit"}
            </button>
          </div>
        </div>
      </div>

      <div className={`skl-modal${hintModalOpen ? " open" : ""}`} id="lobby-hint-modal" aria-hidden={!hintModalOpen}>
        <div className="skl-modal-backdrop" data-hint-close onClick={() => setHintModalOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-hint-title">
          <div className="skl-modal-header">
            <h3 id="lobby-hint-title">Hints</h3>
            <button className="skl-modal-close" type="button" onClick={() => setHintModalOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <div className="skl-player-info">
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Room Link</span>
                <span className="skl-player-info-value">{apClientConnected ? "Connected" : "Disconnected"}</span>
              </div>
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Hint points</span>
                <span className="skl-player-info-value">{apHintPoints === null ? "-" : apHintPoints}</span>
              </div>
              <div className="skl-player-info-row">
                <span className="skl-player-info-label">Hint cost</span>
                <span className="skl-player-info-value">{apHintCost === null ? "-" : `${apHintCost}%`}</span>
              </div>
            </div>

            <div className="skl-ready-row" style={{ gap: 10, flexWrap: "wrap" as any, justifyContent: "flex-start" }}>
              <button
                type="button"
                className="skl-btn ghost"
                disabled={!apClientConnected}
                onClick={() => {
                  setHintMessages([]);
                  loadHintCatalog();
                  sendApSay("!hint");
                }}
              >
                Refresh My Hints
              </button>
              <button
                type="button"
                className={`skl-btn ghost${hintMode === "item" ? " primary" : ""}`}
                onClick={() => { setHintMode("item"); setHintSelection(""); }}
              >
                Item Hint
              </button>
              <button
                type="button"
                className={`skl-btn ghost${hintMode === "location" ? " primary" : ""}`}
                onClick={() => { setHintMode("location"); setHintSelection(""); }}
              >
                Location Hint
              </button>
            </div>

            <div className="skl-chat-form" style={{ marginTop: 8 }}>
              <select
                value={hintSelection}
                onChange={(e) => {
                  const next = e.target.value;
                  setHintSelection(next);
                  setHintQuery(next);
                }}
                disabled={!apClientConnected}
              >
                <option value="">
                  {hintMode === "location" ? "Select a location" : "Select an item"}
                </option>
                {(hintMode === "location" ? (hintCatalog?.locations || []) : (hintCatalog?.items || [])).map((entry) => (
                  <option key={entry} value={entry}>
                    {entry}
                  </option>
                ))}
              </select>
              <button
                type="button"
                className="skl-btn ghost"
                disabled={!apClientConnected || !hintSelection}
                onClick={async () => {
                  await requestHint(hintSelection);
                  setHintSelection("");
                  setHintQuery("");
                }}
              >
                Request Selected
              </button>
            </div>

            <form
              className="skl-chat-form"
              onSubmit={async (e) => {
                e.preventDefault();
                const q = hintQuery.trim();
                await requestHint(q);
                setHintQuery("");
              }}
            >
              <input
                type="text"
                value={hintQuery}
                placeholder={hintMode === "location" ? "Location name (or leave blank)" : "Item name (or leave blank)"}
                onChange={(e) => setHintQuery(e.target.value)}
                disabled={!apClientConnected}
              />
              <button type="submit" className="skl-btn primary" disabled={!apClientConnected}>
                Hint
              </button>
            </form>

            <div className="skl-terminal">
              <div className="skl-terminal-head">
                <h2>Available Hints</h2>
                <span className="skl-terminal-status">{hintMessages.length ? `${hintMessages.length} shown` : "No hints yet."}</span>
              </div>
              <pre className="skl-terminal-log" style={{ maxHeight: 280, overflow: "auto" }}>
                {hintMessages.length ? hintMessages.map((m, idx) => `${idx + 1}. ${m.text}`).join("\n") : "Use Refresh My Hints or request a hint."}
              </pre>
            </div>
          </div>
        </div>
      </div>

      <div className={`skl-modal${hintErrorModal ? " open" : ""}`} id="lobby-hint-error-modal" aria-hidden={!hintErrorModal}>
        <div className="skl-modal-backdrop" onClick={() => setHintErrorModal(null)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-hint-error-title">
          <div className="skl-modal-header">
            <h3 id="lobby-hint-error-title">{hintErrorModal?.title || "Hint error"}</h3>
            <button className="skl-modal-close" type="button" onClick={() => setHintErrorModal(null)}>Close</button>
          </div>
          <p className="skl-modal-body">{hintErrorModal?.message || "Unable to request hint."}</p>
          <div className="skl-modal-actions">
            <button className="skl-btn primary" type="button" onClick={() => setHintErrorModal(null)}>OK</button>
          </div>
        </div>
      </div>

      <div className={`skl-modal${sm64exTutorialOpen ? " open" : ""}`} id="lobby-sm64ex-modal" aria-hidden={!sm64exTutorialOpen}>
        <div className="skl-modal-backdrop" data-sm64ex-close onClick={() => setSm64exTutorialOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-sm64ex-title">
          <div className="skl-modal-header">
            <h3 id="lobby-sm64ex-title">Super Mario 64 EX Setup (One-Time)</h3>
            <button className="skl-modal-close" type="button" onClick={() => setSm64exTutorialOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <p style={{ marginTop: 0 }}>
              SekaiLink downloaded a <code>.apsm64ex</code> slot file. This is not a ROM patch. To play, you need a compiled SM64EX (Archipelago-compatible)
              build that can load this file via <code>--sm64ap_file</code>.
            </p>

            <div style={{ marginTop: 12 }}>
              <div style={{ fontWeight: 700, marginBottom: 6 }}>1. Build SM64EX (recommended)</div>
              <ol style={{ marginTop: 0 }}>
                <li>Download SM64AP-Launcher and run it. It guides requirements and compiles the correct SM64EX Archipelago build.</li>
                <li>In the launcher: run <strong>Check Requirements</strong>, then <strong>Compile default SM64AP build</strong>, then play the build once.</li>
                <li>You will need a legally-dumped Super Mario 64 US or JP ROM for the build step.</li>
              </ol>
              <div style={{ display: "flex", gap: 10, flexWrap: "wrap" }}>
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={() => runtime.openExternal?.("https://github.com/N00byKing/SM64AP-Launcher/releases")}
                >
                  Open SM64AP-Launcher Releases
                </button>
                <button
                  className="skl-btn ghost"
                  type="button"
                  onClick={() => runtime.openExternal?.("https://github.com/N00byKing/sm64ex")}
                >
                  Open SM64EX (Archipelago) Repo
                </button>
              </div>

              <details style={{ marginTop: 12 }}>
                <summary style={{ cursor: "pointer" }}>Linux notes (Steam Deck, Bazzite, desktop)</summary>
                <p style={{ marginTop: 8 }}>
                  On Linux, the launcher and build require some system packages. Names vary by distro; install the equivalents of:
                </p>
                <ul style={{ marginTop: 0 }}>
                  <li><strong>Launcher</strong>: Qt6, git, patch</li>
                  <li><strong>Build</strong>: SDL2, GLEW, CMake, Python 3, make (optional: jsoncpp)</li>
                </ul>
                <p style={{ marginTop: 0 }}>
                  If the launcher does not start, run it from a terminal to see missing library errors, then install the corresponding packages.
                </p>
              </details>
            </div>

            <div style={{ marginTop: 16 }}>
              <div style={{ fontWeight: 700, marginBottom: 6 }}>2. Tell SekaiLink where the game binary is</div>
              <ol style={{ marginTop: 0 }}>
                <li>Open Client Settings.</li>
                <li>Go to <strong>Games (Automation)</strong> then <strong>Super Mario 64 EX (sm64ex)</strong>.</li>
                <li>Set <strong>Executable path</strong> to your compiled SM64EX binary.</li>
              </ol>
              <div style={{ display: "flex", gap: 10, flexWrap: "wrap" }}>
                <button
                  className="skl-btn primary"
                  type="button"
                  onClick={() => {
                    setSm64exTutorialOpen(false);
                    navigate("/settings");
                  }}
                >
                  Open Client Settings
                </button>
              </div>
            </div>

            <div style={{ marginTop: 16 }}>
              <div style={{ fontWeight: 700, marginBottom: 6 }}>3. Launch again</div>
              <p style={{ marginTop: 0, marginBottom: 0 }}>
                After the executable path is set, come back and press <strong>Launch</strong> again. SekaiLink will start SM64EX with the downloaded{" "}
                <code>.apsm64ex</code> file.
              </p>
            </div>

            {manualDownload?.path ? (
              <div style={{ marginTop: 16 }}>
                <div style={{ fontWeight: 700, marginBottom: 6 }}>Downloaded slot file</div>
                <div style={{ fontFamily: "monospace", fontSize: 12, opacity: 0.9, wordBreak: "break-all" }}>{manualDownload.path}</div>
                <div style={{ display: "flex", gap: 10, flexWrap: "wrap", marginTop: 10 }}>
                  <button className="skl-btn ghost" type="button" onClick={() => runtime.showItemInFolder?.(manualDownload.path)}>
                    Show in Folder
                  </button>
                  <button
                    className="skl-btn ghost"
                    type="button"
                    onClick={async () => {
                      try {
                        await navigator.clipboard.writeText(manualDownload.path);
                        showToast("Copied path.", "success");
                      } catch {
                        showToast("Unable to copy path.", "error");
                      }
                    }}
                  >
                    Copy Path
                  </button>
                </div>
              </div>
            ) : null}

            <details style={{ marginTop: 16 }}>
              <summary style={{ cursor: "pointer" }}>Advanced: manual compilation</summary>
              <p>
                Manual builds require a toolchain. Linux: SDL2/GLEW/CMake/Python/Make (plus git/patch). Windows: MSYS2 MinGW x64 with SDL2/GLEW/CMake, git, make,
                python. Build the Archipelago-compatible SM64EX sources.
              </p>
            </details>
          </div>
        </div>
      </div>

      {canManageLobby && (
        <div className={`skl-modal${settingsOpen ? " open" : ""}`} id="lobby-settings-modal" aria-hidden={!settingsOpen}>
          <div className="skl-modal-backdrop" data-settings-close onClick={() => setSettingsOpen(false)}></div>
          <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-settings-title">
            <div className="skl-modal-header">
              <h3 id="lobby-settings-title">Room Settings</h3>
              <button className="skl-modal-close" type="button" onClick={() => setSettingsOpen(false)}>Close</button>
            </div>
            <div className="skl-modal-body">
              <div className="skl-host-status" id="lobby-host-status">
                <div className="skl-host-line">Host status: <span id="lobby-host-presence">{hostStatus?.host_absent ? "Inactive" : "Active"}</span></div>
                <div className="skl-host-line">Next host: <span id="lobby-host-candidate">{hostStatus?.candidate_name || "-"}</span></div>
                <div className="skl-host-actions" id="lobby-host-actions">
                  <button
                    className="skl-btn ghost"
                    id="lobby-host-vote"
                    type="button"
                    onClick={async () => {
                      try {
                        const res = await apiFetch(`/api/lobbies/${lobbyKey}/vote-host`, { method: "POST" });
                        if (!res.ok) {
                          const data = await res.json().catch(() => ({}));
                          throw new Error(data.error || "Unable to vote.");
                        }
                        await loadHostStatus();
                      } catch (err) {
                        setSettingsStatus(err instanceof Error ? err.message : "Unable to vote.");
                      }
                    }}
                  >
                    Vote to assign new host
                  </button>
                  <span id="lobby-host-vote-status" className="skl-ready-status">
                    {hostStatus ? `${hostStatus.vote_count || 0}/${hostStatus.vote_needed || 0}` : ""}
                  </span>
                </div>
              </div>
              <form id="lobby-settings-form" className="skl-settings-form" onSubmit={updateSettings}>
                <div className="skl-settings-grid">
                  <label>
                    Release Permission
                    <select name="release_mode" defaultValue={lobby?.release_mode || "enabled"}>
                      <option value="disabled">Disabled</option>
                      <option value="enabled">Manual !release</option>
                      <option value="goal">After goal completion</option>
                      <option value="auto">Auto</option>
                      <option value="auto-enabled">Auto + Manual</option>
                    </select>
                  </label>
                  <label>
                    Collect Permission
                    <select name="collect_mode" defaultValue={lobby?.collect_mode || "goal"}>
                      <option value="disabled">Disabled</option>
                      <option value="enabled">Manual !collect</option>
                      <option value="goal">Allow after goal completion</option>
                      <option value="auto">Auto</option>
                      <option value="auto-enabled">Auto + Manual</option>
                    </select>
                  </label>
                  <label>
                    Remaining Permission
                    <select name="remaining_mode" defaultValue={lobby?.remaining_mode || "enabled"}>
                      <option value="disabled">Disabled</option>
                      <option value="enabled">Manual !remaining</option>
                      <option value="goal">After goal completion</option>
                    </select>
                  </label>
                  <label>
                    Countdown Permission
                    <select name="countdown_mode" defaultValue={lobby?.countdown_mode || "auto"}>
                      <option value="auto">Auto (under 30 slots)</option>
                      <option value="enabled">Enabled</option>
                      <option value="disabled">Disabled</option>
                    </select>
                  </label>
                  <label>
                    Item Cheat
                    <select name="item_cheat" defaultValue={lobby?.item_cheat ? "1" : "0"}>
                      <option value="0">Disabled</option>
                      <option value="1">Enabled</option>
                    </select>
                  </label>
                  <label>
                    Spoiler Log
                    <select name="spoiler" defaultValue={String(lobby?.spoiler ?? 0)}>
                      <option value="0">Disabled</option>
                      <option value="1">Basic</option>
                      <option value="2">Playthrough</option>
                      <option value="3">Full</option>
                    </select>
                  </label>
                  <label>
                    Hint Cost (%)
                    <select name="hint_cost" defaultValue={normalizeHintCost(lobby?.hint_cost)}>
                      {HINT_COST_OPTIONS.map((value) => (
                        <option key={value} value={value}>{value}%</option>
                      ))}
                    </select>
                  </label>
                  <label>
                    Max Players
                    <input type="number" name="max_players" min="1" max="50" defaultValue={lobby?.max_players ?? 50} />
                  </label>
                  <label>
                    Custom Games
                    <select name="allow_custom_yamls" defaultValue={lobby?.allow_custom_yamls ? "1" : "0"}>
                      <option value="1">Allowed</option>
                      <option value="0">Blocked</option>
                    </select>
                  </label>
                  <label>
                    Server Password (Private lobby)
                    <input type="password" name="server_password" placeholder="Leave empty to keep current" maxLength={64} />
                  </label>
                  {lobby?.is_private && (
                    <label className="skl-checkbox-row">
                      <input type="checkbox" name="clear_password" />
                      Clear password (make lobby public)
                    </label>
                  )}
                </div>
                <div className="skl-settings-plando">
                  <span>Plando Options</span>
                  <label><input type="checkbox" name="plando_items" defaultChecked={lobby?.plando_items} /> Items</label>
                  <label><input type="checkbox" name="plando_bosses" defaultChecked={lobby?.plando_bosses} /> Bosses</label>
                  <label><input type="checkbox" name="plando_connections" defaultChecked={lobby?.plando_connections} /> Connections</label>
                  <label><input type="checkbox" name="plando_texts" defaultChecked={lobby?.plando_texts} /> Text</label>
                </div>
                <div className="skl-settings-actions">
                  <button className="skl-btn ghost" type="submit">Save settings</button>
                  <span id="lobby-settings-status" className="skl-ready-status">{settingsStatus}</span>
                  <button className="skl-btn ghost" type="button" onClick={() => setCloseConfirmOpen(true)}>Close lobby</button>
                </div>
              </form>
            </div>
          </div>
        </div>
      )}

      <div className={`skl-tracker-drawer${trackerOpen ? " open" : ""}`} id="tracker-drawer" aria-hidden={!trackerOpen}>
        <div className="skl-tracker-backdrop" data-tracker-close onClick={() => setTrackerOpen(false)}></div>
        <div className="skl-tracker-panel" role="dialog" aria-modal="true" aria-labelledby="tracker-title">
          <div className="skl-tracker-header">
            <div>
              <p className="skl-tracker-eyebrow">Live tracker</p>
              <h3 id="tracker-title">Tracker</h3>
            </div>
            <div className="skl-tracker-actions">
              <button className="skl-btn ghost" type="button" onClick={() => setTrackerOpen(false)}>Close</button>
            </div>
          </div>
          <div className="skl-tracker-frame">
            <TrackerPanel
              trackerId={trackerId || undefined}
              defaultTeam={0}
              playerSlot={trackerPlayer ?? (selfSlotId ?? null)}
              onPlayerSelect={(player) => setTrackerPlayer(player)}
            />
          </div>
        </div>
      </div>

      {canManageLobby && (
        <div className={`skl-modal${generateConfirmOpen ? " open" : ""}`} id="lobby-generate-modal" aria-hidden={!generateConfirmOpen}>
          <div className="skl-modal-backdrop" data-modal-close onClick={() => setGenerateConfirmOpen(false)}></div>
          <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-generate-title">
            <div className="skl-modal-header">
              <h3 id="lobby-generate-title">Start generation</h3>
              <button className="skl-modal-close" type="button" onClick={() => setGenerateConfirmOpen(false)}>Close</button>
            </div>
            <div className="skl-modal-body">
              <p>
                This will lock all room settings for this seed and start generation. You will not be able to change
                any configuration until the generation fails or completes.
              </p>
              {!soloMode && (
                <p style={{ marginTop: 12 }}>
                  Room-ready players: {roomReadyMembers.length}/{members.length || 0} ·
                  Local-ready players: {localReadyMembers.length}/{members.length || 0}
                  {localSetupBlockedMembers.length > 0 && ` · Blocked: ${localSetupBlockedMembers.length}`}
                  {localSetupUnknownMembers.length > 0 && ` · Pending checks: ${localSetupUnknownMembers.length}`}
                </p>
              )}
              <p style={{ marginTop: 12 }}>Continue?</p>
            </div>
            <div className="skl-modal-actions">
              <button className="skl-btn ghost" type="button" onClick={() => setGenerateConfirmOpen(false)}>Cancel</button>
              <button className="skl-btn primary" type="button" onClick={() => { setGenerateConfirmOpen(false); generateSeed(); }}>
                Start generation
              </button>
            </div>
          </div>
        </div>
      )}

      {canManageLobby && (
        <div className={`skl-modal${closeConfirmOpen ? " open" : ""}`} id="lobby-close-modal" aria-hidden={!closeConfirmOpen}>
          <div className="skl-modal-backdrop" data-modal-close onClick={() => setCloseConfirmOpen(false)}></div>
          <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-close-title">
            <div className="skl-modal-header">
              <h3 id="lobby-close-title">Close lobby</h3>
              <button className="skl-modal-close" type="button" onClick={() => setCloseConfirmOpen(false)}>Close</button>
            </div>
            <p className="skl-modal-body">
              Closing the lobby will shut down the room and disconnect everyone. This cannot be undone.
            </p>
            <div className="skl-modal-actions">
              <button className="skl-btn ghost" type="button" onClick={() => setCloseConfirmOpen(false)}>Cancel</button>
              <button className="skl-btn primary" type="button" onClick={() => { setCloseConfirmOpen(false); closeLobby(); }}>
                Close lobby
              </button>
            </div>
          </div>
        </div>
      )}

      <div
        className={`skl-modal${generationProgress?.open ? " open" : ""}`}
        id="lobby-generation-progress-modal"
        aria-hidden={!generationProgress?.open}
      >
        <div className="skl-modal-backdrop"></div>
        <div
          className="skl-modal-panel skl-generation-modal-panel"
          role="dialog"
          aria-modal="true"
          aria-labelledby="lobby-generation-progress-title"
        >
          <div className="skl-modal-header">
            <h3 id="lobby-generation-progress-title">{generationProgress?.title || "Generation Progress"}</h3>
          </div>
          <div className="skl-modal-body skl-generation-modal-body">
            {generationProgress?.phase === "error" ? (
              <div className="skl-generation-error-icon" aria-hidden="true">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <circle cx="12" cy="12" r="10" />
                  <line x1="15" y1="9" x2="9" y2="15" />
                  <line x1="9" y1="9" x2="15" y2="15" />
                </svg>
              </div>
            ) : (
              <div className="skl-generation-spinner" aria-hidden="true">
                <svg viewBox="0 0 50 50" className="skl-spinner-svg">
                  <circle cx="25" cy="25" r="20" fill="none" strokeWidth="4" />
                </svg>
              </div>
            )}
            <div className="skl-launch-progress" aria-label="Generation progress">
              <div
                className="skl-launch-progress-bar"
                style={{ width: `${Math.max(0, Math.min(100, Number(generationProgress?.progress || 0)))}%` }}
              />
            </div>
            <div className="skl-launch-status">{generationProgress?.message || "Generating..."}</div>
            {generationProgress?.phase === "error" && (
              <div className="skl-generation-error-meta">
                <div><strong>Code:</strong> {generationProgress.errorCode || "GENERATION_FAILED"}</div>
                <div>{generationProgress.errorDescription || "Unknown generation error."}</div>
              </div>
            )}
          </div>
          {generationProgress?.phase === "error" && (
            <div className="skl-modal-actions">
              <button className="skl-btn ghost" type="button" onClick={copyGenerationErrorCode}>
                Copy error code
              </button>
              <button className="skl-btn primary" type="button" onClick={closeGenerationProgress}>
                Close
              </button>
            </div>
          )}
        </div>
      </div>

      <div className={`skl-modal${inactivityModal ? " open" : ""}`} id="lobby-inactivity-modal" aria-hidden={!inactivityModal}>
        <div className="skl-modal-backdrop"></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-inactivity-title">
          <div className="skl-modal-header">
            <h3 id="lobby-inactivity-title">
              {inactivityModal?.mode === "probe" ? "Inactivity Check" : "Slow Release Applied"}
            </h3>
          </div>
          <p className="skl-modal-body">
            {inactivityModal?.mode === "probe"
              ? `No checks detected for ${Math.max(1, Math.round((inactivityModal.timeoutSeconds || 1800) / 60))} minutes${inactivityModal.playerName ? ` on ${inactivityModal.playerName}` : ""}. Confirm you're still active to avoid automatic slow release.`
              : `You have been slow released for inactivity after ${Math.max(1, Math.round((inactivityModal?.timeoutSeconds || 1800) / 60))} minutes without checks.`}
          </p>
          <div className="skl-modal-actions">
            {inactivityModal?.mode === "probe" ? (
              <button
                className="skl-btn primary"
                type="button"
                onClick={() => {
                  setInactivityModal(null);
                  sfx.play("confirm", 0.25);
                }}
              >
                I'm active
              </button>
            ) : (
              <button className="skl-btn primary" type="button" onClick={() => setInactivityModal(null)}>
                OK
              </button>
            )}
          </div>
        </div>
      </div>

      <SessionLaunchModals
        launchModalOpen={launchModalOpen}
        launchError={launchError}
        launchRecoveryAction={launchRecoveryAction}
        launchStatus={launchStatus}
        launchProgress={launchProgress}
        launchDownloadPct={launchDownloadPct}
        onCloseLaunch={closeLaunchModal}
        onOpenRecoveryRoute={(route) => {
          closeLaunchModal();
          if (route) navigate(route);
        }}
        trackerVariantModal={trackerVariantModal}
        onTrackerVariantChange={(variantId) => setTrackerVariantModal((prev) => (prev ? { ...prev, selected: variantId } : prev))}
        onTrackerVariantCancel={() => { void respondTrackerVariantModal(true); }}
        onTrackerVariantContinue={() => { void respondTrackerVariantModal(false); }}
      />
    </div>
  );
};

export default LobbyPage;
