import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { io, Socket } from "socket.io-client";
import { apiFetch, apiJson, apiUrl, API_BASE_URL, getDesktopToken } from "../services/api";
import { useInterval } from "../hooks/useInterval";
import { useSfx } from "../hooks/useSfx";
import TrackerPanel from "../components/TrackerPanel";
import { runtime } from "../services/runtime";
import { formatLocalTime, parseServerDate } from "../utils/time";
import { cleanSoloDescription, isSoloLobby } from "../utils/soloLobby";
import { getPreferredYamlId, setPreferredYamlId } from "../utils/yamlSelection";
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
  id: number;
  author: string;
  avatar_url?: string;
  content: string;
  created_at: string;
}

interface LobbyMember {
  name: string;
  discord_id?: string;
  active_yaml_id?: string;
  active_yaml_title?: string;
  active_yaml_game?: string;
  active_yaml_player?: string;
  active_yamls?: Array<{ id: string; title: string; game: string; player_name: string; custom?: boolean }>;
  ready?: boolean;
  online?: boolean;
  is_host?: boolean;
}

interface YamlEntry {
  id: string;
  title: string;
  game: string;
  player_name: string;
  custom?: boolean;
}

interface GenerationStatus {
  status: string;
  error?: string;
  seed_url?: string;
  room_url?: string;
  completed_at?: string;
}

interface RoomStatus {
  tracker?: string;
  players?: Array<{ slot: number; name: string; game: string }>;
  last_port?: number;
  downloads?: Array<{ slot: number; download: string }>;
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

const LobbyPage: React.FC = () => {
  const { t } = useI18n();
  const { lobbyId } = useParams<{ lobbyId: string }>();
  const navigate = useNavigate();
  const [lobby, setLobby] = useState<LobbySummary | null>(null);
  const [messages, setMessages] = useState<LobbyMessage[]>([]);
  const [members, setMembers] = useState<LobbyMember[]>([]);
  const [yamls, setYamls] = useState<YamlEntry[]>([]);
  const [activeYamls, setActiveYamls] = useState<YamlEntry[]>([]);
  const [ready, setReady] = useState(false);
  const [authed, setAuthed] = useState(false);
  const [joinRequired, setJoinRequired] = useState(false);
  const [passwordError, setPasswordError] = useState("");
  const [statusMessage, setStatusMessage] = useState("");
  const [lobbyPassword, setLobbyPassword] = useState("");
  const [toast, setToast] = useState<{ text: string; kind: "success" | "error" } | null>(null);
  const [launchStatus, setLaunchStatus] = useState<string>("");
  const [generation, setGeneration] = useState<GenerationStatus | null>(null);
  const [roomStatus, setRoomStatus] = useState<RoomStatus | null>(null);
  const [trackerStats, setTrackerStats] = useState<TrackerStats | null>(null);
  const [me, setMe] = useState<MeResponse | null>(null);
  const [isOwner, setIsOwner] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [yamlOpen, setYamlOpen] = useState(false);
  const [yamlModalSelection, setYamlModalSelection] = useState("");
  const [terminalOpen, setTerminalOpen] = useState(false);
  const [trackerOpen, setTrackerOpen] = useState(false);
  const [trackerPlayer, setTrackerPlayer] = useState<number | null>(null);
  // UI-only: replaces the three collapsible right-side boxes with a single tabbed panel.
  const [rightTab, setRightTab] = useState<"users" | "controls" | "info">("users");
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
  const [playerModal, setPlayerModal] = useState<{ name: string; yaml: string; slotId?: number } | null>(null);
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
  const [terminalLog, setTerminalLog] = useState("Waiting for room…");
  const [manualDownload, setManualDownload] = useState<{
    path: string;
    ext?: string;
    moduleId?: string;
    apGameName?: string;
    installedPath?: string;
    note?: string;
  } | null>(null);
  const [sm64exTutorialOpen, setSm64exTutorialOpen] = useState(false);
  const [settingsStatus, setSettingsStatus] = useState("");
  const [seedTimer, setSeedTimer] = useState("-");
  const [generateConfirmOpen, setGenerateConfirmOpen] = useState(false);
  const [closeConfirmOpen, setCloseConfirmOpen] = useState(false);
  const [launchModalOpen, setLaunchModalOpen] = useState(false);
  const [launchError, setLaunchError] = useState<string | null>(null);
  const [launchProgress, setLaunchProgress] = useState<number>(0);
  const [launchDownloadPct, setLaunchDownloadPct] = useState<number | null>(null);
  const [forceTrackerVariantPrompt, setForceTrackerVariantPrompt] = useState<boolean>(() => {
    try {
      return window.localStorage.getItem(FORCE_TRACKER_VARIANT_PROMPT_KEY) === "1";
    } catch (_err) {
      return false;
    }
  });
  const [socketStatus, setSocketStatus] = useState<"connected" | "disconnected" | "error">("disconnected");
  const [socketStatusMsg, setSocketStatusMsg] = useState("");
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
  const [friendUserIds, setFriendUserIds] = useState<string[]>([]);
  const [incomingFriendIds, setIncomingFriendIds] = useState<string[]>([]);
  const [outgoingFriendIds, setOutgoingFriendIds] = useState<string[]>([]);
  const lastToastSoundAtRef = useRef(0);
  const preloadAttemptedRef = useRef<Record<string, boolean>>({});
  const missingYamlPromptedRef = useRef<Record<string, boolean>>({});
  const defaultLoadedAnnouncedRef = useRef<Record<string, string>>({});

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
    const off = runtime.onSessionEvent?.((data: unknown) => {
      const evt = data as any;
      if (!evt || typeof evt !== "object") return;
      if (evt.event === "status" && typeof evt.status === "string") {
        setLaunchStatus(evt.status);
        setLaunchError(null);
        if (!String(evt.status || "").toLowerCase().includes("downloading")) {
          setLaunchDownloadPct(null);
        }
        // Open modal on any status event (indicates launch in progress)
        setLaunchModalOpen(true);

        const s = evt.status.toLowerCase();
        if (s.includes("starting")) setLaunchProgress(6);
        else if (s.includes("downloading")) setLaunchProgress(18);
        else if (s.includes("patching")) setLaunchProgress(38);
        else if (s.includes("launching bizhawk")) setLaunchProgress(56);
        else if (s.includes("connecting")) setLaunchProgress(72);
        else if (s.includes("launching tracker")) setLaunchProgress(88);
        else if (s.includes("ready")) setLaunchProgress(100);
      } else if (evt.event === "download-progress") {
        // Real download progress for the patch artifact (event-driven; no placeholder movement).
        const pct = Number.isFinite(Number(evt.percent)) ? Number(evt.percent) : null;
        setLaunchDownloadPct(pct);
        if (pct !== null) {
          // Keep download bounded between the "Downloading" (18) and "Patching" (38) milestones.
          const start = 18;
          const end = 38;
          const scaled = start + Math.max(0, Math.min(1, pct / 100)) * (end - start);
          setLaunchProgress(scaled);
        }
      } else if (evt.event === "ready") {
        setLaunchStatus("Ready.");
        setManualDownload(null);
        setLaunchModalOpen(false);
        setLaunchError(null);
        setLaunchProgress(100);
        setLaunchDownloadPct(null);
      } else if (evt.event === "manual" && typeof evt.downloadedPath === "string" && evt.downloadedPath) {
        const ext = typeof evt.ext === "string" ? String(evt.ext) : "";
        const apGameName = typeof evt.apGameName === "string" ? String(evt.apGameName) : "";
        setManualDownload({
          path: String(evt.downloadedPath),
          ext,
          moduleId: typeof evt.moduleId === "string" ? evt.moduleId : "",
          apGameName,
          installedPath: typeof evt.installedPath === "string" ? evt.installedPath : "",
          note: typeof evt.note === "string" ? evt.note : "",
        });
        setLaunchStatus("Manual mode.");
        setLaunchModalOpen(false);
        setLaunchDownloadPct(null);
        if (ext === ".apsm64ex" || apGameName.trim().toLowerCase() === "super mario 64") {
          setSm64exTutorialOpen(true);
        }
      } else if (evt.event === "error") {
        const err = String(evt.error || "Launch failed.");
        setLaunchError(err);
        setLaunchStatus("");
        setLaunchProgress(0);
        setLaunchDownloadPct(null);
      } else if (evt.event === "warning") {
        const err = String(evt.error || "warning");
        // Non-blocking feedback; do not spam errors for expected best-effort steps.
        if (!["wmctrl_missing", "no_pack", "pack_missing", "poptracker_missing"].includes(err)) {
          showToast(`Warning: ${err}`, "error");
        }
      }
    });
    return () => {
      if (typeof off === "function") off();
    };
  }, []);

  useEffect(() => {
    if (!launchModalOpen || launchError) return;
    // Keep progress purely event-driven; no fake progress animation.
    return;
  }, [launchError, launchModalOpen]);

  const showToast = (text: string, kind: "success" | "error" = "success") => {
    if (!text) return;
    // Sounds: keep it simple and consistent.
    const now = Date.now();
    if (now - lastToastSoundAtRef.current > 650) {
      if (kind === "error") sfx.play("error", 0.35);
      else sfx.play("success", 0.25);
      lastToastSoundAtRef.current = now;
    }
    setToast({ text, kind });
  };

  useEffect(() => {
    const handleClick = () => setContextMenu(null);
    document.addEventListener("click", handleClick);
    return () => document.removeEventListener("click", handleClick);
  }, []);
  const chatLogRef = useRef<HTMLDivElement | null>(null);
  const socketRef = useRef<Socket | null>(null);
  const messageIdsRef = useRef<Set<number>>(new Set());
  const sfx = useSfx();

  const lobbyKey = lobbyId || "";
  const controlsLocked =
    generation?.status === "queued" ||
    generation?.status === "in_progress" ||
    generation?.status === "completed" ||
    generation?.status === "success" ||
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
      setLobby(null);
      setStatusMessage("Lobby not found.");
    } catch {
      // ignore
    }
  }, [lobbyKey]);

  const loadMe = useCallback(async () => {
    try {
      const data = await apiJson<MeResponse>("/api/me");
      setMe(data);
      setAuthed(true);
    } catch {
      setMe(null);
      setAuthed(false);
    }
  }, []);

  const loadMessages = useCallback(async (initial = false) => {
    if (!lobbyKey) return;
    try {
      const query = !initial && lastMessageId ? `?after_id=${lastMessageId}` : "";
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/messages${query}`);
      if (!response.ok) {
        if (response.status === 401 || response.status === 403) {
          setAuthed(false);
        }
        return;
      }
      const data = await response.json();
      const newMessages: LobbyMessage[] = data.messages || [];
      if (newMessages.length) {
        const deduped = newMessages.filter((msg) => {
          if (typeof msg.id !== "number") return true;
          if (messageIdsRef.current.has(msg.id)) return false;
          messageIdsRef.current.add(msg.id);
          return true;
        });
        if (deduped.length) {
          setMessages((prev) => [...prev, ...deduped]);
          const maxId = Math.max(lastMessageId, ...deduped.map((msg) => msg.id || 0));
          setLastMessageId(maxId);
        }
      }
    } catch {
      // ignore polling errors
    }
  }, [lobbyKey, lastMessageId]);

  const loadMembers = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/members`);
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        if (data.reason === "password") setJoinRequired(true);
        if (response.status === 401 || response.status === 403) setAuthed(false);
        return;
      }
      const data = await response.json();
      const nextMembers: LobbyMember[] = data.members || [];
      setMembers(nextMembers);
      setJoinRequired(false);
      if (me?.discord_id) {
        const self = nextMembers.find((member) => member.discord_id === me.discord_id);
        if (self) {
          setReady(Boolean(self.ready));
          setActiveYamls(self.active_yamls || []);
          setIsOwner(Boolean(self.is_host));
        }
      }
    } catch {
      // ignore
    }
  }, [lobbyKey, me?.discord_id]);

  const loadYamls = useCallback(async () => {
    try {
      const data = await apiJson<{ yamls: YamlEntry[] }>("/api/yamls");
      setYamls(data.yamls || []);
    } catch {
      setYamls([]);
    }
  }, []);

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

  const loadGeneration = useCallback(async () => {
    if (!lobbyKey) return;
    try {
      const data = await apiJson<GenerationStatus>(`/api/lobbies/${lobbyKey}/generation`);
      setGeneration(data);
    } catch {
      setGeneration(null);
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

  const fetchRoomStatus = useCallback(async (roomUrl?: string) => {
    if (!roomUrl) return;
    const parts = roomUrl.split("/").filter(Boolean);
    const roomId = parts[parts.length - 1];
    if (!roomId) return;
    try {
      const data = await apiJson<RoomStatus>(`/api/room_status/${roomId}`);
      setRoomStatus(data);
      if (socketRef.current) {
        socketRef.current.emit("watch_room", { lobby_id: lobbyKey, room_id: roomId });
      }
    } catch {
      // ignore
    }
  }, [lobbyKey]);

  useEffect(() => {
    messageIdsRef.current.clear();
    setMessages([]);
    setLastMessageId(0);
  }, [lobbyKey]);

  useEffect(() => {
    loadLobbyMeta();
    loadMe();
    loadMessages(true);
    loadMembers();
    loadYamls();
    loadGeneration();
    loadSocialRelations();
  }, [loadLobbyMeta, loadMe, loadMembers, loadMessages, loadYamls, loadGeneration, loadSocialRelations]);

  useEffect(() => {
    if (settingsOpen) {
      loadHostStatus();
    }
  }, [settingsOpen, loadHostStatus]);

  useInterval(() => {
    if (socketStatus !== "connected") {
      loadMessages();
    }
    loadMembers();
    loadGeneration();
    if (settingsOpen) loadHostStatus();
    loadSocialRelations();
  }, 10000);

  useEffect(() => {
    if (generation?.room_url) {
      fetchRoomStatus(generation.room_url);
    }
  }, [generation?.room_url, fetchRoomStatus]);

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

  useEffect(() => {
    if (!lobbyKey) return;
    const isFile = window.location.protocol === "file:";
    if (isFile && !API_BASE_URL) return;
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
      setSocketStatus("error");
      return;
    }
    socketRef.current = socket;

    socket.on("connect", () => {
      setSocketStatus("connected");
      setSocketStatusMsg("");
      socket.emit("join_lobby", { lobby_id: lobbyKey });
    });

    socket.on("connect_error", () => {
      setSocketStatus("error");
      setSocketStatusMsg("Live connection failed. Using fallback polling.");
    });

    socket.on("disconnect", () => {
      setSocketStatus("disconnected");
    });

    socket.on("friend_request", () => {
      loadSocialRelations();
    });

    socket.on("friend_added", () => {
      loadSocialRelations();
    });

    socket.on("lobby_message", (message: LobbyMessage) => {
      if (typeof message.id === "number") {
        if (messageIdsRef.current.has(message.id)) return;
        messageIdsRef.current.add(message.id);
      }
      setMessages((prev) => [...prev, message]);
      setLastMessageId((prev) => Math.max(prev, message.id || 0));
    });

    socket.on("members_update", (data: { members: LobbyMember[] }) => {
      setMembers(data.members || []);
    });

    socket.on("generation_update", (data: GenerationStatus) => {
      setGeneration((prev) => {
        const merged: GenerationStatus = { ...(prev || {}), ...(data || {}) } as GenerationStatus;
        if (!data?.completed_at && prev?.completed_at) merged.completed_at = prev.completed_at;
        return merged;
      });
    });

    socket.on("lobby_host_changed", (data: { host_name?: string }) => {
      if (!data?.host_name) return;
      setLobby((prev) => (prev ? { ...prev, owner: data.host_name } : prev));
    });

    socket.on("room_info", (data: { room_url?: string; last_port?: number }) => {
      if (data?.room_url) {
        fetchRoomStatus(data.room_url);
      }
      if (data?.last_port) {
        setRoomStatus((prev) => ({ ...(prev || {}), last_port: data.last_port }));
      }
    });

    socket.on("room_stats", (data: TrackerStats) => {
      setTrackerStats(data);
    });

    socket.on("terminal_output", (data: { text?: string }) => {
      if (data.text) {
        setTerminalLog((prev) => `${prev}${data.text}`);
      }
    });

    socket.on("terminal_ack", (data: { status?: string }) => {
      setTerminalStatus(data?.status === "sent" ? "Sent" : "");
    });

    socket.on("lobby_kicked", (data: { message?: string; redirect_url?: string }) => {
      setModerationModal({
        open: true,
        message: data.message || "You were removed from this lobby.",
        redirectUrl: data.redirect_url || "/",
      });
      sfx.play("error", 0.45);
    });

    socket.on("lobby_auth_required", (data: { message?: string }) => {
      setStatusMessage(data?.message || "Connect with Discord to join.");
      setAuthed(false);
    });

    socket.on("lobby_password_required", (data: { message?: string }) => {
      setJoinRequired(true);
      setStatusMessage(data?.message || "Lobby password required.");
    });

    socket.on("inactivity_probe", (data: { timeout_seconds?: number; player_name?: string }) => {
      const timeoutSeconds = Number(data?.timeout_seconds || 1800);
      setInactivityModal({
        mode: "probe",
        timeoutSeconds: Number.isFinite(timeoutSeconds) ? timeoutSeconds : 1800,
        playerName: data?.player_name || "",
      });
      sfx.play("error", 0.35);
    });

    socket.on("inactivity_slow_released", (data: { timeout_seconds?: number; player_name?: string }) => {
      const timeoutSeconds = Number(data?.timeout_seconds || 1800);
      setInactivityModal({
        mode: "released",
        timeoutSeconds: Number.isFinite(timeoutSeconds) ? timeoutSeconds : 1800,
        playerName: data?.player_name || "",
      });
      sfx.play("error", 0.45);
    });

    return () => {
      socket.emit("leave_lobby", { lobby_id: lobbyKey });
      socket.disconnect();
    };
  }, [lobbyKey, fetchRoomStatus, loadSocialRelations, sfx]);

  // Local Archipelago client output (BizHawkClient wrapper). Used for Hints + debug.
  useEffect(() => {
    if (!runtime.onBizHawkClientEvent) return;
    const unsub = runtime.onBizHawkClientEvent((raw) => {
      const data: any = raw as any;
      if (!data || typeof data !== "object") return;
      if (data.event === "log") {
        const msg = typeof data.message === "string" ? data.message : "";
        const lvl = typeof data.level === "string" ? data.level : "info";
        if (msg) {
          setApLogLines((prev) => {
            const next = [...prev, { text: msg, ts: Date.now(), level: lvl }];
            return next.slice(-250);
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
        setApLastError(msg);
        setApLogLines((prev) => {
          const next = [...prev, { text: msg, ts: Date.now(), level: "error" }];
          return next.slice(-250);
        });
      } else if (data.event === "emu_status") {
        if (typeof data.server === "string") setApClientConnected(data.server === "connected");
        if (typeof data.bizhawk === "string") {
          const v = data.bizhawk === "connected" || data.bizhawk === "tentative" || data.bizhawk === "not_connected" ? data.bizhawk : "unknown";
          setApBizhawkConnected(v);
        }
        if (typeof data.handler === "string") setApHandlerGame(data.handler);
      } else if (data.event === "status") {
        if (data.server === "connected") setApClientConnected(true);
        if (data.server === "disconnected") setApClientConnected(false);
        if (typeof data.hint_points === "number") setApHintPoints(data.hint_points);
      } else if (data.event === "room_info") {
        if (typeof data.hint_cost === "number") setApHintCost(data.hint_cost);
      } else if (data.event === "print_json") {
        const t = typeof data.text === "string" ? data.text.trim() : "";
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
  }, [sfx]);

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
      const serverAddress = roomStatus?.last_port ? `${host}:${roomStatus.last_port}` : "";
      const payload = {
        serverAddress,
        slot: selfPlayerName || "",
        connected: apClientConnected,
        bizhawk: apBizhawkConnected,
        handler: apHandlerGame,
        hint_points: apHintPoints,
        hint_cost: apHintCost,
        last_error: apLastError,
        recent_logs: apLogLines.slice(-30).map((l) => `[${l.level}] ${l.text}`),
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
    setChatInput("");
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/messages`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content })
      });
      if (!response.ok) {
        if (response.status === 401 || response.status === 403) {
          throw new Error("Login required to send messages.");
        }
        throw new Error("Unable to send message.");
      }
      sfx.play("confirm", 0.35);
    } catch {
      setStatusMessage("Unable to send message.");
    }
  };

  const postLobbyNotice = useCallback(async (content: string) => {
    const text = String(content || "").trim();
    if (!text || !lobbyKey || !authed || joinRequired) return;
    try {
      await apiFetch(`/api/lobbies/${lobbyKey}/messages`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content: text })
      });
    } catch {
      // best effort
    }
  }, [authed, joinRequired, lobbyKey]);

  const addActiveYaml = useCallback(async (yamlId: string) => {
    if (!yamlId) return false;
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/active-yaml`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ yaml_id: yamlId, action: "add" })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to add game.");
      }
      const data = await response.json();
      setActiveYamls(data.active_yamls || []);
      return true;
    } catch (err) {
      setStatusMessage(err instanceof Error ? err.message : "Unable to add game.");
      return false;
    }
  }, [lobbyKey]);

  const removeActiveYaml = useCallback(async (yamlId: string) => {
    if (!yamlId) return false;
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/active-yaml`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ yaml_id: yamlId, action: "remove" })
      });
      if (!response.ok) throw new Error("Unable to remove game.");
      const data = await response.json();
      setActiveYamls(data.active_yamls || []);
      return true;
    } catch {
      setStatusMessage("Unable to remove game.");
      return false;
    }
  }, [lobbyKey]);

  const setSingleActiveYaml = useCallback(async (yamlId: string, announce: "default" | "change" = "change") => {
    const targetId = String(yamlId || "").trim();
    if (!targetId) return false;
    if (controlsLocked) return false;

    const self = members.find((member) => member.discord_id === me?.discord_id);
    const currentId = String(self?.active_yaml_id || activeYamls[0]?.id || "").trim();
    const selected = yamls.find((entry) => entry.id === targetId);
    const selectedTitle = selected?.title || targetId;

    if (currentId && currentId === targetId) {
      setPreferredYamlId(targetId);
      return true;
    }

    for (const entry of activeYamls) {
      if (entry.id !== targetId) {
        await removeActiveYaml(entry.id);
      }
    }

    const added = await addActiveYaml(targetId);
    if (!added) return false;
    setPreferredYamlId(targetId);

    const actor = self?.name || "Player";
    if (announce === "default") {
      await postLobbyNotice(`${actor} joined with default game: ${selectedTitle}`);
    } else {
      await postLobbyNotice(`${actor} changed active game to: ${selectedTitle}`);
    }
    return true;
  }, [controlsLocked, members, me?.discord_id, activeYamls, yamls, removeActiveYaml, addActiveYaml, postLobbyNotice]);

  useEffect(() => {
    if (!lobbyKey || !authed || joinRequired || !me?.discord_id) return;
    const self = members.find((member) => member.discord_id === me.discord_id);
    if (!self) return;
    if (self.active_yaml_id) return;
    if (!yamls.length) return;

    const attemptKey = `${lobbyKey}:${me.discord_id}`;
    const preferredYamlId = getPreferredYamlId();
    const preferredExists = preferredYamlId && yamls.some((entry) => entry.id === preferredYamlId);

    if (preferredExists && !preloadAttemptedRef.current[attemptKey] && !controlsLocked) {
      preloadAttemptedRef.current[attemptKey] = true;
      void setSingleActiveYaml(preferredYamlId, "default");
      return;
    }

    if (!controlsLocked && !missingYamlPromptedRef.current[attemptKey]) {
      missingYamlPromptedRef.current[attemptKey] = true;
      setYamlOpen(true);
      showToast("No pre-selected game found. Select a game in this lobby.", "error");
    }
  }, [lobbyKey, authed, joinRequired, me?.discord_id, members, yamls, controlsLocked, setSingleActiveYaml, showToast]);

  useEffect(() => {
    if (!yamlOpen) return;
    const currentId = activeYamls[0]?.id || "";
    const preferredId = getPreferredYamlId();
    const fallback = currentId || (preferredId && yamls.some((entry) => entry.id === preferredId) ? preferredId : "");
    setYamlModalSelection(fallback);
  }, [yamlOpen, activeYamls, yamls]);

  useEffect(() => {
    if (!lobbyKey || !me?.discord_id || !authed || joinRequired) return;
    const self = members.find((member) => member.discord_id === me.discord_id);
    if (!self?.active_yaml_id) return;
    const announceKey = `${lobbyKey}:${me.discord_id}`;
    const already = defaultLoadedAnnouncedRef.current[announceKey];
    if (already === self.active_yaml_id) return;
    defaultLoadedAnnouncedRef.current[announceKey] = self.active_yaml_id;
    const loadedTitle = self.active_yaml_title || self.active_yaml_id;
    void postLobbyNotice(`${self.name || "Player"} joined with default game: ${loadedTitle}`);
  }, [lobbyKey, me?.discord_id, members, authed, joinRequired, postLobbyNotice]);

  const toggleReady = async () => {
    if (controlsLocked) return;
    const selfMember = members.find((member) => member.discord_id === me?.discord_id);
    if (!selfMember?.active_yaml_id) {
      showToast("Select an active game before marking ready.", "error");
      return;
    }
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/ready`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ready: !ready })
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to update ready.");
      }
      const data = await response.json();
      setReady(Boolean(data.ready));
      sfx.play(data.ready ? "ready" : "unready", 0.35);
    } catch (err) {
      showToast(err instanceof Error ? err.message : "Unable to update ready.", "error");
    }
  };

  const generateSeed = async () => {
    try {
      const response = await apiFetch(`/api/lobbies/${lobbyKey}/generate`, { method: "POST" });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to generate.");
      }
      sfx.play("success", 0.35);
      await loadGeneration();
    } catch (err) {
      showToast(err instanceof Error ? err.message : "Unable to generate.", "error");
    }
  };

  const sendTerminal = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!terminalInput.trim()) return;
    const roomUrl = generation?.room_url;
    if (!roomUrl) return;
    const parts = roomUrl.split("/").filter(Boolean);
    const roomId = parts[parts.length - 1];
    if (!roomId || !socketRef.current) return;
    socketRef.current.emit("terminal_command", { lobby_id: lobbyKey, room_id: roomId, cmd: terminalInput.trim() });
    sfx.play("confirm", 0.25);
    setTerminalInput("");
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
        bio: member.active_yaml_title ? `Active game: ${member.active_yaml_title}` : "",
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

  const validateSetupForModule = async (moduleId: string) => {
    const config = await runtime.configGet?.();
    const roms = (config as any)?.roms || {};
    const trackerStatus = await runtime.trackerStatus?.();
    const trackerExists = Boolean((trackerStatus as any)?.exists);

    const manifestRes = await runtime.getModuleManifest?.(moduleId);
    const manifest = (manifestRes as any)?.manifest;
    if (!(manifestRes as any)?.ok || !manifest) return { ok: false, error: "manifest_missing" };

    const requiredRomIds: string[] = Array.isArray(manifest.required_roms)
      ? manifest.required_roms
      : manifest.game_id
        ? [manifest.game_id]
        : [];
    for (const gameId of requiredRomIds) {
      if (!roms[gameId]) return { ok: false, error: "rom_missing" };
    }

    // PopTracker is optional per module, but if a module declares a pack source, then PopTracker must exist.
    if (manifest.tracker_pack_uid && !trackerExists) return { ok: false, error: "poptracker_missing" };

    return { ok: true };
  };

  const handleLaunch = async (downloadUrls?: string | string[]) => {
    const urls = Array.isArray(downloadUrls) ? downloadUrls : downloadUrls ? [downloadUrls] : [];
    const serverAddress = roomStatus?.last_port ? `${host}:${roomStatus.last_port}` : "";
    if (!serverAddress) {
      showToast("Room server is not ready yet.", "error");
      return;
    }
    if (!selfPlayerName) {
      showToast("Player slot name not available.", "error");
      return;
    }
    const slotId = playersByName.get(selfPlayerName) || playersByName.get(selfPlayerName.toLowerCase());
    const apGameName =
      (roomStatus?.players || []).find((p: any) => (slotId ? p?.slot === slotId : false) || p?.name === selfPlayerName)?.game ||
      "";

    if (!urls.length) {
      if (runtime.sessionAutoLaunch && apGameName) {
        try {
          setManualDownload(null);
          setLaunchError(null);
          setLaunchModalOpen(true);
          setLaunchStatus("Starting...");
          const launchRes = await runtime.sessionAutoLaunch({
            downloadUrl: "",
            serverAddress,
            slot: selfPlayerName,
            password: lobbyPassword,
            apGameName,
            forceTrackerVariantPrompt,
          });
          const launchData = launchRes as any;
          if (!launchData?.ok) {
            const err = String(launchData?.error || "Launch failed.");
            let errorMsg = err;
            if (err === "missing_patch_url") errorMsg = "No patch file is required for this game, but auto-launch route was not found.";
            if (err === "soh_not_found") errorMsg = "Ship of Harkinian executable not found. Set it in Settings -> Games (Automation).";
            if (err === "soh_install_failed") errorMsg = "Unable to auto-install Ship of Harkinian release.";
            const detail = typeof launchData?.detail === "string" ? launchData.detail.trim() : "";
            if (detail) errorMsg = `${errorMsg}\n\nDetails: ${detail}`;
            setLaunchError(errorMsg);
            setLaunchStatus("");
            return;
          }
          setLaunchStatus("Ready.");
          return;
        } catch (err) {
          setLaunchStatus("");
          setLaunchError(err instanceof Error ? err.message : "Launch failed.");
          return;
        }
      }
      showToast("No patch file available for this player.", "error");
      return;
    }

    for (const downloadUrl of urls) {
          try {
            if (runtime.sessionAutoLaunch) {
              setManualDownload(null);
              setLaunchError(null);
              setLaunchModalOpen(true);
              setLaunchStatus("Starting...");
              const launchRes = await runtime.sessionAutoLaunch({
                downloadUrl,
                serverAddress,
                slot: selfPlayerName,
                password: lobbyPassword,
                apGameName,
                forceTrackerVariantPrompt,
              });
              const launchData = launchRes as any;
              if (!launchData?.ok) {
                const err = String(launchData?.error || "Launch failed.");
                let errorMsg = err;
                if (err === "rom_missing") {
                  errorMsg = "Game setup incomplete: ROM missing. Run Game Setup first.";
                } else if (err === "poptracker_missing") {
                  errorMsg = "Game setup incomplete: PopTracker missing. Run Game Setup first.";
                } else if (err === "mono_missing") {
                  errorMsg =
                    "Game setup incomplete: Mono missing (required for BizHawk on Linux). This should be bundled with SekaiLink; if you are on a dev build, install mono or bundle the pinned mono runtime.";
                } else if (err === "unsupported_patch_type") {
                  errorMsg = "Unsupported patch type for auto-launch.";
                } else if (err === "soh_not_found") {
                  errorMsg = "Ship of Harkinian executable not found. Set it in Settings -> Games (Automation).";
                } else if (err === "soh_install_failed") {
                  errorMsg = "Unable to auto-install Ship of Harkinian release.";
                }
                const detail = typeof launchData?.detail === "string" ? launchData.detail.trim() : "";
                if (detail) errorMsg = `${errorMsg}\n\nDetails: ${detail}`;
                setLaunchError(errorMsg);
                setLaunchStatus("");
                return;
              }

              if (launchData?.manual && typeof launchData?.downloadedPath === "string" && launchData.downloadedPath) {
                setManualDownload({
                  path: String(launchData.downloadedPath),
                  ext: typeof launchData.ext === "string" ? launchData.ext : "",
                  moduleId: typeof launchData.moduleId === "string" ? launchData.moduleId : "",
                  apGameName,
                  installedPath: typeof launchData.installedPath === "string" ? launchData.installedPath : "",
                  note: typeof launchData.note === "string" ? launchData.note : "",
                });
                setLaunchStatus("Manual mode.");
              }
            } else {
              const resolved = await runtime.resolveModuleForDownload?.(downloadUrl);
              const moduleId = (resolved as any)?.moduleId;
              if (!(resolved as any)?.ok || !moduleId) {
            const ext = (resolved as any)?.ext ? ` (${(resolved as any).ext})` : "";
            showToast(`Unsupported patch type for auto-launch${ext}.`, "error");
            return;
          }

          const setupCheck = await validateSetupForModule(moduleId);
          if (!setupCheck.ok) {
            if (setupCheck.error === "rom_missing") {
              showToast("Game setup incomplete: ROM missing. Run Game Setup first.", "error");
            } else if (setupCheck.error === "poptracker_missing") {
              showToast("Game setup incomplete: PopTracker missing. Run Game Setup first.", "error");
            } else {
              showToast("Game setup incomplete. Run Game Setup first.", "error");
            }
            return;
          }

          setLaunchStatus("Patching game...");
          const patchRes = await runtime.patcherApply({ patchUrl: downloadUrl });
          const patchData = patchRes as { ok?: boolean; output?: string; error?: string };
          if (!patchData?.ok || !patchData.output) {
            throw new Error(patchData?.error || "Patch failed.");
          }

          setLaunchStatus("Launching BizHawk...");
          const launchRes = await runtime.bizhawkLaunch({
            romPath: patchData.output,
            moduleId
          });
          const launchData = launchRes as { ok?: boolean; error?: string; detail?: string };
          if (!launchData?.ok) {
            const err = launchData?.error || "BizHawk failed to launch.";
            if (err === "mono_missing") {
              const detail = typeof launchData?.detail === "string" ? launchData.detail.trim() : "";
              const msg = detail
                ? `Mono missing (required for BizHawk on Linux).\n\nDetails: ${detail}`
                : "Mono missing (required for BizHawk on Linux).";
              throw new Error(msg);
            }
            throw new Error(err);
          }

          setLaunchStatus("Connecting to server...");
          await runtime.bizhawkClientStart({
            address: serverAddress,
            slot: selfPlayerName
          });
          await runtime.bizhawkClientSend({
            cmd: "connect",
            address: serverAddress,
            slot: selfPlayerName,
            password: lobbyPassword
          });

          setLaunchStatus("Launching tracker...");
          const trackerRes = await runtime.trackerLaunch({
            moduleId,
            apHost: serverAddress,
            apSlot: selfPlayerName,
            apPass: lobbyPassword,
            forceTrackerVariantPrompt,
          });
          const trackerData = trackerRes as { ok?: boolean; error?: string };
          if (!trackerData?.ok) {
            const err = trackerData?.error || "tracker_failed";
            if (!["no_pack", "poptracker_missing", "pack_missing"].includes(err)) {
              showToast(`Tracker launch failed: ${err}`, "error");
            }
          }

          setLaunchStatus("Ready.");
        }
      } catch (err) {
        setLaunchStatus("");
        setLaunchError(err instanceof Error ? err.message : "Launch failed.");
        return;
      }
    }
  };

  const downloadsBySlot = useMemo(() => {
    const map = new Map<number, string>();
    (roomStatus?.downloads || []).forEach((entry) => {
      map.set(entry.slot, apiUrl(entry.download));
    });
    return map;
  }, [roomStatus?.downloads]);

  const downloadsBySlotMulti = useMemo(() => {
    const map = new Map<number, string[]>();
    (roomStatus?.downloads || []).forEach((entry) => {
      const list = map.get(entry.slot) || [];
      list.push(apiUrl(entry.download));
      map.set(entry.slot, list);
    });
    return map;
  }, [roomStatus?.downloads]);

  const playersByName = useMemo(() => {
    const map = new Map<string, number>();
    (roomStatus?.players || []).forEach((player) => {
      if (player.name) {
        map.set(player.name, player.slot);
        map.set(player.name.toLowerCase(), player.slot);
      }
    });
    return map;
  }, [roomStatus?.players]);

  const gameBySlot = useMemo(() => {
    const map = new Map<number, string>();
    (roomStatus?.players || []).forEach((player) => {
      if (typeof player.slot === "number" && typeof player.game === "string") {
        map.set(player.slot, player.game);
      }
    });
    return map;
  }, [roomStatus?.players]);

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

  const allReady = members.length > 0 && members.every((member) => member.ready);
  const generationState = generation?.status || "idle";
  const generationLocked = controlsLocked;
  const host = (() => {
    try {
      return new URL(apiUrl("/"), window.location.origin).host;
    } catch {
      return window.location.host;
    }
  })();

  const selfMember = members.find((member) => member.discord_id === me?.discord_id);
  const selfPlayerName = selfMember?.active_yaml_player || selfMember?.name;
  const selfSlotId =
    selfPlayerName
      ? playersByName.get(selfPlayerName) || playersByName.get(selfPlayerName.toLowerCase())
      : undefined;
  const selfDownloadUrl = selfSlotId ? downloadsBySlot.get(selfSlotId) : undefined;
  const selfDownloadUrls = selfSlotId ? downloadsBySlotMulti.get(selfSlotId) : undefined;
  const selfApGameName = selfSlotId ? gameBySlot.get(selfSlotId) || "" : "";
  const selfCanPatchlessLaunch = Boolean(selfMember && supportsPatchlessAutoLaunch(selfApGameName));
  const selfCanLaunch = Boolean((selfDownloadUrls?.length || selfDownloadUrl) || selfCanPatchlessLaunch);
  const trackerId = roomStatus?.tracker || "";
  const trackerReady = Boolean(trackerId && generation?.room_url);
  const soloMode = isSoloLobby(lobby);
  const lobbyDescription = cleanSoloDescription(lobby?.description || "No description yet.");

  return (
    <div className="skl-lobby-scroll">
      <section className="skl-lobby-page" id="lobby-room">
        <div className="skl-lobby-hero">
          <div>
            <span className="skl-eyebrow">Lobby</span>
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
              <div className="skl-ready-status">Connect with Discord to join this lobby. It is free and uses your Discord account.</div>
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
                        title="Join Discord Voice for this room."
                      >
                        Join Discord Voice
                      </button>
                    )}
                    <button
                      className="skl-btn primary"
                      type="button"
                      disabled={!selfCanLaunch}
                      onClick={() => {
                        sfx.play("confirm", 0.2);
                        handleLaunch(selfDownloadUrls || selfDownloadUrl);
                      }}
                      title={!selfCanLaunch ? "No patch available to launch yet." : "Launch your game with the downloaded patch."}
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
                <div className="skl-chat-hint">Connect with Discord to post messages. It is free and uses your Discord account.</div>
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
                  <button type="button" className={`sklp-tab-btn${rightTab === "controls" ? " active" : ""}`} onClick={() => setRightTab("controls")}>
                    <svg className="sklp-tab-ico" viewBox="0 0 24 24" fill="none" aria-hidden="true">
                      <path d="M12 3v4M12 17v4M3 12h4M17 12h4M5.6 5.6l2.8 2.8M15.6 15.6l2.8 2.8M18.4 5.6l-2.8 2.8M8.4 15.6l-2.8 2.8" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
                      <circle cx="12" cy="12" r="3.2" stroke="currentColor" strokeWidth="1.8" />
                    </svg>
                    Controls
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
                      disabled={!selfCanLaunch}
                      onClick={() => {
                        sfx.play("confirm", 0.2);
                        handleLaunch(selfDownloadUrls || selfDownloadUrl);
                      }}
                      title={!selfCanLaunch ? "No patch available to launch yet." : "Launch your game with the downloaded patch."}
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
                        const playerName = member.active_yaml_player || member.name;
                        const slotId = playerName ? playersByName.get(playerName) || playersByName.get(playerName.toLowerCase()) : undefined;
                        const trackerAvailable = Boolean(trackerId && generation?.room_url);
                        const isSelf = Boolean(member.discord_id && member.discord_id === me?.discord_id);
                        const downloadUrl = slotId ? downloadsBySlot.get(slotId) : undefined;
                        const downloadUrls = slotId ? downloadsBySlotMulti.get(slotId) : undefined;
                        const hasDownloads = Boolean(downloadUrls?.length || downloadUrl);
                        const apGameName = slotId ? gameBySlot.get(slotId) || "" : "";
                        const canPatchlessLaunch = Boolean(isSelf && supportsPatchlessAutoLaunch(apGameName));
                        const canLaunch = hasDownloads || canPatchlessLaunch;

                        return (
                          <div
                            key={`${member.name}-${member.active_yaml_id || ""}`}
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
                              setPlayerModal({ name: member.name, yaml: member.active_yaml_title || "-", slotId });
                              setPlayerModalOpen(true);
                            }}
                          >
                            <div className="skl-member-left">
                              {member.is_host && <span className="skl-member-crown">👑</span>}
                              <div className="skl-member-name">{member.name}</div>
                              <span className="skl-member-yaml-icon" title={member.active_yaml_title || "Game: not set"}>G</span>
                              {isSelf && downloadUrls && downloadUrls.length > 1 && (
                                <span className="skl-member-yaml-icon" title={`${downloadUrls.length} patches available`}>+</span>
                              )}
                            </div>
                            <div className="skl-member-right">
                              <div className="skl-member-ready">
                                <span className={`skl-ready-badge ${member.ready ? "" : "not-ready"}`}>{member.ready ? "Ready" : "Not ready"}</span>
                              </div>
                            </div>
                          </div>
                        );
                      })}
                    </div>
                    <div id="lobby-members-empty" className="skl-empty-note">
                      {members.length ? "" : "No players yet."}
                    </div>
                    {socketStatusMsg && <div className="skl-empty-note">{socketStatusMsg}</div>}
                  </>
                )}

                {(soloMode || rightTab === "controls") && (
                  <>
                    <div className="skl-control-grid">
                      <button className="skl-control-btn" type="button" disabled={generationLocked} onClick={() => { sfx.play("confirm", 0.2); setYamlOpen(true); }}>
                        <span className="skl-control-icon">◇</span>
                        Select a Game
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
                      {selfMember && (
                        <button className="skl-control-btn" type="button" onClick={toggleReady} disabled={generationLocked}>
                          <span className="skl-control-icon">{ready ? "II" : "RDY"}</span>
                          {ready ? "Unready" : "Ready"}
                        </button>
                      )}
                      {selfMember && (
                        <button
                          className="skl-control-btn"
                          type="button"
                          disabled={!apClientConnected}
                          onClick={openHints}
                          title={!apClientConnected ? "Start the game (Launch) to connect the client first." : "Open hint list and request hints."}
                        >
                          <span className="skl-control-icon">i</span>
                          Hints
                        </button>
                      )}
                      {canManageLobby && (
                        <button
                          className="skl-control-btn primary"
                          type="button"
                          disabled={generationLocked || !allReady}
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
                              showToast("All players must be ready before generating.", "error");
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
                          <div className="skl-client-title">Local Game Client</div>
                          <div className="skl-client-sub">
                            Server: <strong>{apClientConnected ? "Connected" : "Disconnected"}</strong>{" "}
                            · BizHawk: <strong>{apBizhawkConnected === "unknown" ? "-" : apBizhawkConnected}</strong>{" "}
                            · Handler: <strong>{apHandlerGame || "-"}</strong>
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
                              ? apLogLines.slice(-12).map((l) => `[${l.level}] ${l.text}`).join("\n")
                              : "No client logs yet. Use Launch to start the local client."}
                          </pre>
                        </>
                      )}
                    </div>
                    <div className="skl-control-status">
                      <span className={`skl-ready-badge ${ready ? "" : "not-ready"}`}>{ready ? "Ready" : "Not ready"}</span>
                      <span className="skl-ready-status">Generation: {generationState}</span>
                      <span className="skl-ready-status">{generation?.error || (allReady ? "All members ready." : "Waiting for all members to be ready.")}</span>
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
                        {roomStatus?.last_port ? `${host}:${roomStatus.last_port}` : "-"}
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
              {(contextMenu.member.active_yaml_player && (canManageLobby || contextMenu.isSelf)) && (
                <>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      releasePlayer(contextMenu.member.active_yaml_player || contextMenu.member.name, "release");
                      setContextMenu(null);
                    }}
                  >
                    Release
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      sfx.play("confirm", 0.2);
                      releasePlayer(contextMenu.member.active_yaml_player || contextMenu.member.name, "slow");
                      setContextMenu(null);
                    }}
                  >
                    Slow Release
                  </button>
                </>
              )}
              {contextMenu.trackerAvailable && contextMenu.slotId && (
                <button
                  type="button"
                  onClick={() => {
                    sfx.play("confirm", 0.2);
                    setTrackerPlayer(contextMenu.slotId || null);
                    setTrackerOpen(true);
                    setContextMenu(null);
                  }}
                >
                  Tracker
                </button>
              )}
              {contextMenu.isSelf && (
                <button
                  type="button"
                  disabled={!apClientConnected}
                  onClick={() => {
                    openHints();
                    setContextMenu(null);
                  }}
                  title={!apClientConnected ? "Start the game (Launch) to connect the client first." : "Open hint list and request hints."}
                >
                  Hints
                </button>
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
              <div style={{ fontWeight: 700 }}>Manual Download</div>
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
          <div id="lobby-terminal-log" className="skl-terminal-log">{terminalLog}</div>
          {canManageLobby && (
            <form id="lobby-terminal-form" className="skl-terminal-form" onSubmit={sendTerminal}>
              <input type="text" id="lobby-terminal-input" placeholder="Enter server command" value={terminalInput} onChange={(event) => setTerminalInput(event.target.value)} />
              <button className="skl-btn ghost" type="submit">Send</button>
            </form>
          )}
          {canManageLobby && <div id="lobby-terminal-status" className="skl-ready-status">{terminalStatus}</div>}
        </div>
      </div>

      <div className={`skl-modal${yamlOpen ? " open" : ""}`} id="lobby-yaml-modal" aria-hidden={!yamlOpen}>
        <div className="skl-modal-backdrop" data-yaml-close onClick={() => setYamlOpen(false)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-yaml-title">
          <div className="skl-modal-header">
            <h3 id="lobby-yaml-title">Select a Game</h3>
            <button className="skl-modal-close" type="button" onClick={() => setYamlOpen(false)}>Close</button>
          </div>
          <div className="skl-modal-body">
            <div className="skl-yaml-active-list" id="lobby-yaml-active-list">
              {yamls.map((entry) => {
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
                      {entry.game || "Unknown"} · Player: {entry.player_name}{entry.custom ? " · Custom" : ""}
                      {isActive ? " · Active" : ""}
                    </div>
                  </button>
                );
              })}
            </div>
            <div className="skl-ready-row" style={{ justifyContent: "flex-end" }}>
              <button className="skl-btn ghost" type="button" onClick={() => setYamlOpen(false)}>
                Cancel
              </button>
              <button
                className="skl-btn primary"
                type="button"
                disabled={generationLocked || !yamlModalSelection}
                onClick={async () => {
                  const ok = await setSingleActiveYaml(yamlModalSelection, "change");
                  if (ok) setYamlOpen(false);
                }}
              >
                OK
              </button>
            </div>
            <div id="lobby-yaml-status" className="skl-empty-note">Active Game: {activeYamls[0]?.title || "-"}</div>
            {!yamls.length && <div className="skl-empty-note">No games yet. Create one from your dashboard.</div>}
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
                <span className="skl-player-info-label">Active Game</span>
                <span id="lobby-player-yaml" className="skl-player-info-value">{playerModal?.yaml || "-"}</span>
              </div>
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
                <span className="skl-player-info-label">Client</span>
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
            <p className="skl-modal-body">
              This will lock all room settings for this seed and start generation. You will not be able to change
              any configuration until the generation fails or completes. Continue?
            </p>
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

      {/* Launch Progress Modal */}
      <div className={`skl-modal${launchModalOpen ? " open" : ""}`} id="lobby-launch-modal" aria-hidden={!launchModalOpen}>
        <div className="skl-modal-backdrop"></div>
        <div className="skl-modal-panel skl-launch-modal-panel" role="dialog" aria-modal="true" aria-labelledby="lobby-launch-title">
          <div className="skl-modal-header">
            <h3 id="lobby-launch-title">{launchError ? "Launch Failed" : "Launching Game"}</h3>
          </div>
          <div className="skl-modal-body skl-launch-modal-body">
            {!launchError && (
              <div className="skl-launch-spinner">
                <svg viewBox="0 0 50 50" className="skl-spinner-svg">
                  <circle cx="25" cy="25" r="20" fill="none" strokeWidth="4" />
                </svg>
              </div>
            )}
            {!launchError && (
              <div className="skl-launch-progress" aria-label="Launch progress">
                <div className="skl-launch-progress-bar" style={{ width: `${Math.max(0, Math.min(100, launchProgress))}%` }} />
              </div>
            )}
            {launchError && (
              <div className="skl-launch-error-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
                  <circle cx="12" cy="12" r="10" />
                  <line x1="15" y1="9" x2="9" y2="15" />
                  <line x1="9" y1="9" x2="15" y2="15" />
                </svg>
              </div>
            )}
            <div className="skl-launch-status">
              {launchError || (launchDownloadPct !== null && String(launchStatus || "").toLowerCase().includes("downloading")
                ? `${launchStatus} (${launchDownloadPct.toFixed(1)}%)`
                : launchStatus) || "Initializing..."}
            </div>
          </div>
          {launchError && (
            <div className="skl-modal-actions">
              <button
                className="skl-btn primary"
                type="button"
                onClick={() => {
                  setLaunchModalOpen(false);
                  setLaunchError(null);
                  setLaunchStatus("");
                }}
              >
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
                  socketRef.current?.emit("inactivity_pong", { lobby_id: lobbyKey });
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
    </div>
  );
};

export default LobbyPage;
