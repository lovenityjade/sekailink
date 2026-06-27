import { Users, Tag, Send, Settings, Terminal, Crown, Plus, X, Check, Loader, AlertCircle, ChevronDown, Search, Trash2, UserCircle, UserPlus, Volume2, Ban, LogOut } from 'lucide-react';
import { useCallback, useEffect, useRef, useState } from 'react';
import AnimatedBackground from './AnimatedBackground';
import LobbySettingsModal from './LobbySettingsModal';
import HelpButton from './HelpButton';
import { apiCurrentUser, apiFetch, apiJson, apiUrl, getCachedCurrentUser, type CurrentUser } from '../../services/api';
import { chatService, type SekaiChatMessage, type SekaiChatPresenceUser } from '../../services/chatService';
import { canonicalSeedGameKey, listSeeds, type SeedEntry } from '../../services/seedConfig';
import { listLobbies } from '../../services/lobbyClient';
import { buildSelfLaunchContext, indexDownloadsBySlot, indexPlayersByName, type SelfLaunchContext } from '../../services/lobbyLaunchContext';
import { executeRoomSessionLaunch } from '../../services/roomSessionLaunch';
import { fetchRoomStatusByUrl, resolveRoomServerHost, type RoomServerStatus } from '../../services/roomServerContext';
import { trace, traceError } from '../../services/trace';
import { gameSetupRegistry } from '../../data/gameSetup';
import { runtime } from '../../services/runtime';
import { ErrorModal, LoadingModal } from './FeedbackModal';
import { EmoteText } from './EmoteText';

interface LobbyRoomPageProps {
  lobbyId?: string;
  lobbyName?: string;
  owner?: string;
  createdAt?: string;
  onInitialLoadComplete?: (lobbyId: string) => void;
  onLeaveLobby?: () => void;
}

type LobbyStatus = 'waiting' | 'ready' | 'generating' | 'generated' | 'error';
type PlayerReadyState = 'not-ready' | 'ready' | 'missing-config' | 'generating' | 'generated';
type GenerationPhase = 'idle' | 'validating' | 'sending' | 'generating' | 'preparing' | 'ready' | 'error';

interface SeedConfig {
  id: string;
  game: string;
  configName: string;
  source: 'easy' | 'advanced';
  status: 'valid' | 'draft' | 'error';
  instance?: number;
  trackerVariant?: string;
  trackerVariantName?: string;
  trackerGameId?: string;
}

interface Player {
  id: string;
  username: string;
  isHost: boolean;
  readyState: PlayerReadyState;
  seedConfigs: SeedConfig[];
}

interface LobbyMetadata {
  id: string;
  name: string;
  description?: string;
  owner?: string;
  created_at?: string;
  created?: string;
  last_activity?: string;
}

interface LobbyGenerationStatus {
  status?: string;
  error?: string;
  detail?: string;
  message?: string;
  stderr?: string;
  stdout?: string;
  generation_exit_code?: string | number;
  generation_log_path?: string;
  room_status?: string;
  room_runtime_error?: string;
  room_runtime_log?: string;
  room_runtime_alive?: boolean;
  room_runtime_pid?: number;
  room_url?: string;
  room_server_url?: string;
  seed_url?: string;
  sync_package_ready?: boolean;
  sync_package_path?: string;
  artifact_ready?: boolean;
  artifact_path?: string;
  response?: {
    status?: string;
    error?: string;
    detail?: string;
    message?: string;
    stderr?: string;
    stdout?: string;
    generation_exit_code?: string | number;
    generation_log_path?: string;
    room_status?: string;
    room_runtime_error?: string;
    room_runtime_log?: string;
    room_runtime_alive?: boolean;
    room_runtime_pid?: number;
    room_url?: string;
    sync_package_ready?: boolean;
    sync_package_path?: string;
    artifact_ready?: boolean;
    artifact_path?: string;
  };
}

interface LobbyMemberPayload {
  discord_id?: string;
  user_id?: string;
  name?: string;
  username?: string;
  display_name?: string;
  global_name?: string;
  is_host?: boolean;
  ready?: boolean;
  selections?: Array<{ game?: string; configs?: Array<{ id?: string; yaml_id?: string; title?: string; custom?: boolean; tracker_variant?: string; trackerVariant?: string; tracker_variant_name?: string; trackerVariantName?: string; tracker_game_id?: string; trackerGameId?: string }> }>;
  active_yamls?: Array<{ id: string; title: string; game: string; player_name?: string; custom?: boolean; tracker_variant?: string; trackerVariant?: string; tracker_variant_name?: string; trackerVariantName?: string; tracker_game_id?: string; trackerGameId?: string }>;
  active_yaml_id?: string;
  active_yaml_title?: string;
  active_yaml_game?: string;
  active_yaml_player?: string;
  active_yaml_tracker_variant?: string;
  active_yaml_tracker_variant_name?: string;
  active_yaml_tracker_game_id?: string;
}

type TrackerVariantOption = {
  id: string;
  name: string;
};

type TrackerVariantPromptState = {
  gameId: string;
  gameLabel: string;
  configName: string;
  variants: TrackerVariantOption[];
  selected: string;
  resolve: (variant: TrackerVariantOption | null) => void;
};

type LaunchChoice = {
  id: string;
  config: SeedConfig;
  selfLaunch: SelfLaunchContext;
  downloadCount: number;
};

type LaunchChoicePromptState = {
  choices: LaunchChoice[];
  selectedId: string;
  resolve: (choice: LaunchChoice | null) => void;
};

const seedSource = (source?: string): 'easy' | 'advanced' =>
  String(source || '').toLowerCase().includes('pulse') || String(source || '').toLowerCase().includes('easy')
    ? 'easy'
    : 'advanced';

const mapSeedEntry = (seed: SeedEntry): SeedConfig => ({
  id: seed.id,
  game: seed.game || seed.game_key || 'Unknown',
  configName: seed.title || 'Untitled Seed',
  source: seedSource(seed.source),
  status: 'valid',
});

const trackerGameIdForSeedGame = (game: string) => {
  const setup = setupForGame(game);
  return setup?.gameId || setup?.romKey || canonicalSeedGameKey(game);
};

const normalizeTrackerVariantOptions = (payload: unknown): TrackerVariantOption[] => {
  const variants = Array.isArray((payload as any)?.variants) ? (payload as any).variants : [];
  return variants
    .map((variant: any) => ({
      id: String(variant?.id || variant?.key || '').trim(),
      name: String(variant?.name || variant?.label || variant?.displayName || variant?.display_name || variant?.id || '').trim(),
    }))
    .filter((variant: TrackerVariantOption) => variant.id && variant.name);
};

const selectionsForMember = (member: LobbyMemberPayload): SeedConfig[] => {
  const grouped = Array.isArray(member.selections) ? member.selections : [];
  if (grouped.length) {
    return grouped.flatMap((group) => {
      const game = String(group?.game || 'Unknown');
      const configs = Array.isArray(group?.configs) ? group.configs : [];
      return configs.map((config) => ({
        id: String(config?.id || config?.yaml_id || ''),
        game,
        configName: String(config?.title || 'Default'),
        source: 'advanced' as const,
        status: 'valid' as const,
        trackerVariant: String(config?.tracker_variant || config?.trackerVariant || ''),
        trackerVariantName: String(config?.tracker_variant_name || config?.trackerVariantName || ''),
        trackerGameId: String(config?.tracker_game_id || config?.trackerGameId || ''),
      }));
    }).filter((entry) => entry.id);
  }
  const active = Array.isArray(member.active_yamls) ? member.active_yamls : [];
  if (active.length) {
    return active.map((seed) => ({
      id: String(seed.id || ''),
      game: String(seed.game || 'Unknown'),
      configName: String(seed.title || 'Default'),
      source: 'advanced' as const,
      status: 'valid' as const,
      trackerVariant: String(seed.tracker_variant || seed.trackerVariant || ''),
      trackerVariantName: String(seed.tracker_variant_name || seed.trackerVariantName || ''),
      trackerGameId: String(seed.tracker_game_id || seed.trackerGameId || ''),
    })).filter((entry) => entry.id);
  }
  if (member.active_yaml_id || member.active_yaml_title || member.active_yaml_game) {
    return [{
      id: String(member.active_yaml_id || ''),
      game: String(member.active_yaml_game || 'Unknown'),
      configName: String(member.active_yaml_title || 'Default'),
      source: 'advanced' as const,
      status: 'valid' as const,
      trackerVariant: String(member.active_yaml_tracker_variant || ''),
      trackerVariantName: String(member.active_yaml_tracker_variant_name || ''),
      trackerGameId: String(member.active_yaml_tracker_game_id || ''),
    }].filter((entry) => entry.id);
  }
  return [];
};

const generationReady = (generation?: LobbyGenerationStatus | null) =>
  Boolean(
    generation?.room_url ||
      generation?.room_server_url ||
      generation?.sync_package_ready ||
      generation?.sync_package_path ||
      generation?.artifact_ready ||
      generation?.artifact_path ||
      generation?.response?.room_url ||
      generation?.response?.sync_package_ready ||
      generation?.response?.sync_package_path ||
      generation?.response?.artifact_ready ||
      generation?.response?.artifact_path
  );

const generationRunning = (generation?: LobbyGenerationStatus | null) => {
  const status = String(generation?.status || generation?.response?.status || '').toLowerCase();
  const roomStatus = String(generation?.room_status || generation?.response?.room_status || '').toLowerCase();
  return ['queued', 'starting', 'in_progress', 'running', 'generating', 'pending', 'processing'].includes(status) ||
    ['starting', 'binding', 'waiting'].includes(roomStatus);
};

const compactGenerationError = (value: unknown) => {
  const text = String(value || '').trim();
  if (!text) return '';
  const lines = text
    .split(/\r?\n/)
    .map((line) => line.trimEnd())
    .filter(Boolean);
  if (!lines.length) return text;
  const preferred = [...lines].reverse().find((line) =>
    /error|exception|traceback|notimplemented|optionerror|failed|invalid|missing/i.test(line)
  );
  return preferred || lines[lines.length - 1] || text;
};

const generationErrorText = (generation?: LobbyGenerationStatus | null) => {
  const status = String(generation?.status || generation?.response?.status || '').toLowerCase();
  const queuedOrRunning = ['queued', 'starting', 'in_progress', 'running', 'generating', 'pending', 'processing'].includes(status);
  const response = generation?.response;
  const parts = [
    generation?.error,
    response?.error,
    generation?.detail,
    response?.detail,
    generation?.message,
    response?.message,
    generation?.room_runtime_error,
    response?.room_runtime_error,
    generation?.stderr,
    response?.stderr,
    generation?.stdout,
    response?.stdout,
  ];
  for (const part of parts) {
    const compact = compactGenerationError(part);
    if (queuedOrRunning && /queued for live worlds orchestration/i.test(compact)) continue;
    if (compact) return compact;
  }
  const code = generation?.generation_exit_code ?? response?.generation_exit_code;
  if (code !== undefined && code !== null && String(code).trim()) {
    return `Worlds generation exited with code ${code}.`;
  }
  return '';
};

const generationErrorDetails = (generation?: LobbyGenerationStatus | null) => {
  const response = generation?.response;
  const detail = String(
    generation?.error ||
      response?.error ||
      generation?.detail ||
      response?.detail ||
      generation?.room_runtime_error ||
      response?.room_runtime_error ||
      generation?.stderr ||
      response?.stderr ||
      generation?.stdout ||
      response?.stdout ||
      ''
  ).trim();
  return detail.length > 2400 ? `${detail.slice(-2400)}` : detail;
};

const generationFailed = (generation?: LobbyGenerationStatus | null) => {
  const status = String(generation?.status || generation?.response?.status || '').toLowerCase();
  const roomStatus = String(generation?.room_status || generation?.response?.room_status || '').toLowerCase();
  return Boolean(generationErrorText(generation)) || ['failed', 'error', 'cancelled'].includes(status) || roomStatus === 'failed';
};

const generationPhaseFromStatus = (generation?: LobbyGenerationStatus | null): GenerationPhase | null => {
  const status = String(generation?.status || generation?.response?.status || '').toLowerCase();
  const roomStatus = String(generation?.room_status || generation?.response?.room_status || '').toLowerCase();
  if (generationReady(generation)) return 'ready';
  if (generationFailed(generation)) return 'error';
  if (['starting', 'binding', 'waiting'].includes(roomStatus)) return 'preparing';
  if (['queued', 'pending'].includes(status)) return 'sending';
  if (['in_progress', 'running', 'generating', 'processing'].includes(status)) return 'generating';
  return null;
};

const generationPhaseRank: Record<GenerationPhase, number> = {
  idle: 0,
  validating: 1,
  sending: 2,
  generating: 3,
  preparing: 4,
  ready: 5,
  error: 6,
};

const normalizeGenerationPayload = (value: unknown): LobbyGenerationStatus | null => {
  if (!value || typeof value !== 'object') return null;
  const data = value as Record<string, unknown>;
  if (data.generation && typeof data.generation === 'object') return data.generation as LobbyGenerationStatus;
  if (data.response && typeof data.response === 'object') return data as LobbyGenerationStatus;
  if (
    'status' in data ||
    'room_url' in data ||
    'room_server_url' in data ||
    'sync_package_ready' in data ||
    'artifact_ready' in data
  ) {
    return data as LobbyGenerationStatus;
  }
  return null;
};

const displayDate = (value?: string) => {
  if (!value) return '';
  const parsed = new Date(value);
  return Number.isNaN(parsed.getTime()) ? value : parsed.toLocaleDateString();
};

const normalizedIdentityValue = (value: unknown) => String(value || '').trim().toLowerCase();

const currentPlayerFrom = (list: Player[], identity: CurrentUser | null): Player | null => {
  const identityValues = new Set(
    [
      identity?.user_id,
      identity?.discord_id,
      identity?.display_name,
      identity?.global_name,
      identity?.username,
    ]
      .map(normalizedIdentityValue)
      .filter(Boolean)
  );
  if (!identityValues.size) return null;
  return list.find((player) => (
    [player.id, player.username].map(normalizedIdentityValue).some((value) => identityValues.has(value))
  )) || null;
};

const setupForGame = (game: string) => {
  const key = canonicalSeedGameKey(game);
  return gameSetupRegistry.find((entry) => {
    if (entry.available === false) return false;
    const candidates = [entry.gameId, entry.apGameId, entry.displayName, entry.romKey || '']
      .filter(Boolean)
      .map((value) => canonicalSeedGameKey(value));
    return candidates.includes(key);
  });
};

const hasConfiguredRomForSetup = (setup: NonNullable<ReturnType<typeof setupForGame>>, configuredRoms: Record<string, string>) => {
  const directKeys = [setup.romKey, setup.gameId, setup.moduleId, setup.apGameId, setup.displayName]
    .filter(Boolean)
    .map((value) => String(value));
  if (directKeys.some((key) => Boolean(configuredRoms[key]))) return true;

  const configuredByCanonicalKey = new Set(
    Object.keys(configuredRoms)
      .filter((key) => Boolean(configuredRoms[key]))
      .map((key) => canonicalSeedGameKey(key))
  );
  return directKeys.some((key) => configuredByCanonicalKey.has(canonicalSeedGameKey(key)));
};

const presenceForPlayer = (presenceUsers: SekaiChatPresenceUser[], player: Player) =>
  presenceUsers.find((presence) =>
    presence.user_id === player.id ||
    presence.username === player.username ||
    presence.display_name === player.username ||
    presence.name === player.username
  ) || null;

const localSystemMessage = (content: string): SekaiChatMessage => ({
  id: `local-system-${Date.now()}-${Math.random().toString(36).slice(2)}`,
  author: 'System',
  content,
  created_at: new Date().toISOString(),
  kind: 'system',
  channel: 'Lobby',
});

const selectionForSeedConfig = (config: SeedConfig, username: string) => ({
  id: config.id,
  config_id: config.id,
  yaml_id: config.id,
  game: config.game,
  title: config.configName,
  player_name: username,
  tracker_variant: config.trackerVariant,
  tracker_game_id: config.trackerGameId,
});

export default function LobbyRoomPage({
  lobbyId = '',
  lobbyName = '',
  owner = '',
  createdAt = '',
  onInitialLoadComplete,
  onLeaveLobby
}: LobbyRoomPageProps) {
  const [lobbyStatus, setLobbyStatus] = useState<LobbyStatus>('waiting');
  const [message, setMessage] = useState('');
  const [showSeedSelector, setShowSeedSelector] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedPlayers, setExpandedPlayers] = useState<Set<string>>(new Set());
  const [showLobbySettings, setShowLobbySettings] = useState(false);
  const [contextMenu, setContextMenu] = useState<{ playerId: string; x: number; y: number } | null>(null);
  const [players, setPlayers] = useState<Player[]>([]);
  const [availableSeedConfigs, setAvailableSeedConfigs] = useState<SeedConfig[]>([]);
  const [chatMessages, setChatMessages] = useState<SekaiChatMessage[]>([]);
  const [currentIdentity, setCurrentIdentity] = useState<CurrentUser | null>(() => getCachedCurrentUser());
  const [lobbyMetadata, setLobbyMetadata] = useState<LobbyMetadata | null>(null);
  const [generation, setGeneration] = useState<LobbyGenerationStatus | null>(null);
  const [generationPhase, setGenerationPhase] = useState<GenerationPhase>('idle');
  const [generationModalDismissed, setGenerationModalDismissed] = useState(false);
  const [generationSubmitInFlight, setGenerationSubmitInFlight] = useState(false);
  const generationSubmitRef = useRef(false);
  const [generationResetInFlight, setGenerationResetInFlight] = useState(false);
  const [launchInFlight, setLaunchInFlight] = useState(false);
  const launchSubmitRef = useRef(false);
  const [addingSeedName, setAddingSeedName] = useState('');
  const [trackerVariantPrompt, setTrackerVariantPrompt] = useState<TrackerVariantPromptState | null>(null);
  const [launchChoicePrompt, setLaunchChoicePrompt] = useState<LaunchChoicePromptState | null>(null);
  const [error, setError] = useState('');
  const [presenceUsers, setPresenceUsers] = useState<SekaiChatPresenceUser[]>([]);
  const lastPresencePayload = useRef('');
  const lobbyLoadInFlight = useRef(false);
  const lastMetadataLoad = useRef(0);
  const lastSeedListLoad = useRef(0);
  const lastChatLoad = useRef(0);
  const lastPresenceLoad = useRef(0);
  const pendingLobbyReload = useRef<'initial' | 'poll' | 'action' | null>(null);
  const initialLoadReported = useRef(false);

  const currentUser = currentPlayerFrom(players, currentIdentity) || players[0] || {
    id: currentIdentity?.user_id || 'self',
    username: currentIdentity?.display_name || currentIdentity?.username || 'Player',
    isHost: false,
    readyState: 'not-ready' as const,
    seedConfigs: [],
  };
  const readyPlayers = players.filter(p => p.readyState === 'ready').length;
  const missingConfigs = players.filter(p => p.seedConfigs.length === 0).length;
  const totalConfigs = players.reduce((sum, p) => sum + p.seedConfigs.length, 0);

  const filteredConfigs = availableSeedConfigs.filter(config =>
    config.game.toLowerCase().includes(searchQuery.toLowerCase()) ||
    config.configName.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const launchAvailable = lobbyStatus === 'generated' || generationReady(generation);
  const canLaunch = launchAvailable && !launchInFlight && !launchChoicePrompt;
  const canGenerate =
    players.every(p => p.readyState === 'ready' && p.seedConfigs.length > 0) &&
    lobbyStatus === 'ready' &&
    !canLaunch &&
    !generationRunning(generation) &&
    !generationSubmitInFlight;
  const effectiveLobbyName = lobbyMetadata?.name || lobbyName || lobbyId || 'No lobby selected';
  const effectiveOwner = lobbyMetadata?.owner || owner || 'Unknown';
  const effectiveCreatedAt = displayDate(lobbyMetadata?.created_at || lobbyMetadata?.created || lobbyMetadata?.last_activity) || createdAt || '-';

  const advanceGenerationPhase = useCallback((phase: GenerationPhase, reset = false) => {
    setGenerationPhase((current) => {
      if (reset) return phase;
      return generationPhaseRank[phase] > generationPhaseRank[current] ? phase : current;
    });
  }, []);

  const inspectLocalReadiness = useCallback(async (player: Player) => {
    if (!player.seedConfigs.length) {
      return {
        ready: false,
        note: `${player.username}'s configuration is invalid at generation. Please select another or fix your config.`,
      };
    }
    const invalidConfig = player.seedConfigs.find((config) => config.status !== 'valid');
    if (invalidConfig) {
      return {
        ready: false,
        note: `${player.username}'s configuration is invalid at generation. Please select another or fix your config.`,
      };
    }

    const appConfig = await Promise.resolve(runtime.configGet?.()).catch(() => null);
    const configuredRoms = appConfig && typeof appConfig === 'object' && (appConfig as any).roms && typeof (appConfig as any).roms === 'object'
      ? (appConfig as any).roms as Record<string, string>
      : {};
    for (const config of player.seedConfigs) {
      const setup = setupForGame(config.game);
      if (setup?.romKey && !hasConfiguredRomForSetup(setup, configuredRoms)) {
        return {
          ready: false,
          note: `${player.username}'s ${setup.displayName} ROM is not set.`,
        };
      }
    }
    return { ready: true, note: 'Local setup ready.' };
  }, []);

  const publishLocalReadiness = useCallback(async (nextPlayers: Player[], identity: CurrentUser | null) => {
    if (!lobbyId) return;
    const localPlayer = currentPlayerFrom(nextPlayers, identity);
    if (!localPlayer) return;
    const local = await inspectLocalReadiness(localPlayer);
    const payload = {
      role: localPlayer.isHost ? 'host' : 'player',
      ready: localPlayer.readyState === 'ready',
      local_ready_known: true,
      local_ready: local.ready,
      local_ready_note: local.note,
    };
    const serialized = JSON.stringify({ lobbyId, userId: localPlayer.id, ...payload });
    if (serialized === lastPresencePayload.current) return;
    lastPresencePayload.current = serialized;
    await chatService.touchPresence({ kind: 'lobby', lobbyId }, payload).catch((err) => {
      traceError('lobby-room', 'presence_publish_failed', err, { lobbyId });
    });
  }, [inspectLocalReadiness, lobbyId]);

  const postSystemNotice = useCallback(async (content: string) => {
    if (!lobbyId) return;
    try {
      const sent = await chatService.sendSystemMessage({ kind: 'lobby', lobbyId }, content);
      setChatMessages((list) => [...list, sent].slice(-80));
    } catch (err) {
      traceError('lobby-room', 'system_chat_send_failed', err, { lobbyId });
      setChatMessages((list) => [...list, localSystemMessage(content)].slice(-80));
    }
  }, [lobbyId]);

  const refreshAvailableSeedConfigs = useCallback(async (force = false) => {
    const seeds = await listSeeds(undefined, { force }).catch(() => []);
    setAvailableSeedConfigs(seeds.map(mapSeedEntry));
    lastSeedListLoad.current = Date.now();
  }, []);

  const requestTrackerVariantChoice = useCallback((prompt: Omit<TrackerVariantPromptState, 'selected' | 'resolve'>) => (
    new Promise<TrackerVariantOption | null>((resolve) => {
      const defaultChoice = prompt.variants.find((variant) => /horizontal/i.test(variant.name)) || prompt.variants[0];
      setTrackerVariantPrompt({
        ...prompt,
        selected: defaultChoice?.id || '',
        resolve,
      });
    })
  ), []);

  const resolveTrackerVariantForConfig = useCallback(async (config: SeedConfig) => {
    const gameId = trackerGameIdForSeedGame(config.game);
    if (!gameId || !runtime.trackerListPackVariants) {
      return { gameId, variant: '', variantName: '' };
    }
    const response = await Promise.resolve(runtime.trackerListPackVariants({ gameId })).catch((err) => {
      traceError('lobby-room', 'tracker_variant_list_failed', err, { gameId, game: config.game });
      return null;
    });
    const variants = normalizeTrackerVariantOptions(response);
    if (!variants.length) return { gameId, variant: '', variantName: '' };

    const chosen = variants.length === 1
      ? variants[0]
      : await requestTrackerVariantChoice({
          gameId,
          gameLabel: config.game,
          configName: config.configName,
          variants,
        });
    if (!chosen) return null;
    await Promise.resolve(runtime.trackerSetVariant?.(gameId, chosen.id)).catch((err) => {
      traceError('lobby-room', 'tracker_variant_save_failed', err, { gameId, variant: chosen.id });
    });
    return { gameId, variant: chosen.id, variantName: chosen.name };
  }, [requestTrackerVariantChoice]);

  useEffect(() => {
    initialLoadReported.current = false;
    setGenerationPhase('idle');
    setGenerationModalDismissed(false);
  }, [lobbyId]);

  const loadLobbyState = useCallback(async (reason: 'initial' | 'poll' | 'action' = 'poll') => {
    if (!lobbyId) return;
    if (lobbyLoadInFlight.current) {
      trace('lobby-room', 'load_state_skipped_in_flight', { lobbyId, reason });
      pendingLobbyReload.current = reason === 'action' ? 'action' : pendingLobbyReload.current;
      return;
    }
    const now = Date.now();
    const includeMetadata = reason === 'initial' || now - lastMetadataLoad.current > 30000;
    const includeSeeds = reason === 'initial' || now - lastSeedListLoad.current > 60000;
    const includeChat = reason !== 'poll' || now - lastChatLoad.current > 10000;
    const includePresence = reason !== 'poll' || now - lastPresenceLoad.current > 15000;
    lobbyLoadInFlight.current = true;
    trace('lobby-room', 'load_state_start', { lobbyId, reason, includeMetadata, includeSeeds, includeChat, includePresence });
    try {
      const metadataPromise: Promise<LobbyMetadata[] | null> = includeMetadata
        ? listLobbies().then((list) => list as LobbyMetadata[]).catch(() => [])
        : Promise.resolve(null);
      const seedsPromise: Promise<SeedEntry[] | null> = includeSeeds
        ? listSeeds().catch(() => [])
        : Promise.resolve(null);
      const messagesPromise: Promise<SekaiChatMessage[] | null> = includeChat
        ? chatService.listMessages({ kind: 'lobby', lobbyId }).catch(() => [])
        : Promise.resolve(null);
      const presencePromise: Promise<SekaiChatPresenceUser[] | null> = includePresence
        ? chatService.listPresence({ kind: 'lobby', lobbyId }).catch(() => [])
        : Promise.resolve(null);

      const [identity, membersResponse, generationResponse, lobbies, seeds, messages, presence] = await Promise.all([
        apiCurrentUser().catch(() => getCachedCurrentUser()),
        apiJson<{ members?: LobbyMemberPayload[] }>(`/api/lobbies/${encodeURIComponent(lobbyId)}/members`).catch(() => ({ members: [] })),
        apiJson<LobbyGenerationStatus>(`/api/lobbies/${encodeURIComponent(lobbyId)}/generation`).catch(() => null),
        metadataPromise,
        seedsPromise,
        messagesPromise,
        presencePromise,
      ]);
      setCurrentIdentity(identity);
      if (Array.isArray(lobbies)) {
        const lobbyMatch = lobbies.find((lobby) => String(lobby.id || '') === lobbyId) || null;
        if (lobbyMatch) setLobbyMetadata(lobbyMatch);
        lastMetadataLoad.current = Date.now();
      }
      setGeneration(generationResponse);
      if (generationFailed(generationResponse)) {
        advanceGenerationPhase('error', true);
      } else if (generationReady(generationResponse)) {
        advanceGenerationPhase('ready');
      } else if (generationRunning(generationResponse)) {
        advanceGenerationPhase('generating');
      }
      const members = Array.isArray(membersResponse.members) ? membersResponse.members : [];
      const nextPlayers = members.map((member) => {
        const id = String(member.user_id || member.discord_id || member.username || member.name || '');
        const username = String(member.display_name || member.global_name || member.name || member.username || id || 'Player');
        const seedConfigs = selectionsForMember(member);
        return {
          id: id || username,
          username,
          isHost: Boolean(member.is_host),
          readyState: member.ready ? 'ready' as const : seedConfigs.length ? 'not-ready' as const : 'missing-config' as const,
          seedConfigs,
        };
      }).filter((player) => player.id || player.username);
      setPlayers(nextPlayers);
      if (Array.isArray(presence)) {
        setPresenceUsers(presence);
        lastPresenceLoad.current = Date.now();
      }
      if (Array.isArray(seeds)) {
        setAvailableSeedConfigs(seeds.map(mapSeedEntry));
        lastSeedListLoad.current = Date.now();
      }
      if (Array.isArray(messages)) {
        setChatMessages(messages.slice(-80));
        lastChatLoad.current = Date.now();
      }
      await publishLocalReadiness(nextPlayers, identity);
      const allReady = nextPlayers.length > 0 && nextPlayers.every((player) => player.readyState === 'ready' && player.seedConfigs.length > 0);
      setLobbyStatus((current) => {
        if (generationFailed(generationResponse)) return 'error';
        if (generationReady(generationResponse)) return 'generated';
        if (generationRunning(generationResponse)) return 'generating';
        return current === 'generating' || current === 'generated' ? current : allReady ? 'ready' : 'waiting';
      });
      setError('');
      trace('lobby-room', 'load_state_success', {
        lobbyId,
        reason,
        playerCount: nextPlayers.length,
        seedCount: Array.isArray(seeds) ? seeds.length : -1,
        messageCount: Array.isArray(messages) ? messages.length : -1,
        lobbyStatus: generationReady(generationResponse)
          ? 'generated'
          : generationFailed(generationResponse)
            ? 'error'
            : generationRunning(generationResponse)
              ? 'generating'
              : allReady
                ? 'ready'
                : 'waiting',
      });
      if (reason === 'initial' && !initialLoadReported.current) {
        initialLoadReported.current = true;
        onInitialLoadComplete?.(lobbyId);
      }
    } catch (err) {
      traceError('lobby-room', 'load_state_failed', err, { lobbyId });
      setError(err instanceof Error ? err.message : 'Unable to load lobby.');
      if (reason === 'initial' && !initialLoadReported.current) {
        initialLoadReported.current = true;
        onInitialLoadComplete?.(lobbyId);
      }
    } finally {
      lobbyLoadInFlight.current = false;
      const pending = pendingLobbyReload.current;
      pendingLobbyReload.current = null;
      if (pending) window.setTimeout(() => void loadLobbyState(pending), 0);
    }
  }, [lobbyId, onInitialLoadComplete, publishLocalReadiness]);

  useEffect(() => {
    void loadLobbyState('initial');
    const interval = window.setInterval(() => void loadLobbyState('poll'), 10000);
    return () => window.clearInterval(interval);
  }, [loadLobbyState]);

  useEffect(() => {
    if (!showSeedSelector) return;
    void refreshAvailableSeedConfigs(true);
  }, [refreshAvailableSeedConfigs, showSeedSelector]);

  const handleAddSeed = async (config: SeedConfig) => {
    if (!lobbyId || !config.id || addingSeedName) return;
    const label = String(config.configName || config.game || 'config').trim() || 'config';
    trace('lobby-room', 'add_seed_start', { lobbyId, seedId: config.id, game: config.game });
    try {
      const trackerVariant = await resolveTrackerVariantForConfig(config);
      if (trackerVariant === null) {
        trace('lobby-room', 'add_seed_tracker_variant_cancelled', { lobbyId, seedId: config.id, game: config.game });
        return;
      }
      setAddingSeedName(label);
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/game-selections`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          yaml_id: config.id,
          action: 'add',
          tracker_variant: trackerVariant.variant,
          tracker_variant_name: trackerVariant.variantName,
          tracker_game_id: trackerVariant.gameId,
        }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to add seed config.'));
      setShowSeedSelector(false);
      await loadLobbyState('action');
      trace('lobby-room', 'add_seed_success', { lobbyId, seedId: config.id });
    } catch (err) {
      traceError('lobby-room', 'add_seed_failed', err, { lobbyId, seedId: config.id });
      setError(err instanceof Error ? err.message : 'Unable to add seed config.');
    } finally {
      setAddingSeedName('');
    }
  };

  const handleRemoveSeed = async (configId: string) => {
    if (!lobbyId || !configId) return;
    trace('lobby-room', 'remove_seed_start', { lobbyId, seedId: configId });
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/game-selections`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ yaml_id: configId, action: 'remove' }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to remove seed config.'));
      await loadLobbyState('action');
      trace('lobby-room', 'remove_seed_success', { lobbyId, seedId: configId });
    } catch (err) {
      traceError('lobby-room', 'remove_seed_failed', err, { lobbyId, seedId: configId });
      setError(err instanceof Error ? err.message : 'Unable to remove seed config.');
    }
  };

  const getReadyStateConfig = (state: PlayerReadyState) => {
    switch (state) {
      case 'ready':
        return { icon: <Check className="w-4 h-4" />, label: 'Ready', color: 'text-[#4ecdc4]', bg: 'bg-[#4ecdc4]' };
      case 'not-ready':
        return { icon: <AlertCircle className="w-4 h-4" />, label: 'Not Ready', color: 'text-[#8e8f94]', bg: 'bg-[#8e8f94]' };
      case 'missing-config':
        return { icon: <AlertCircle className="w-4 h-4" />, label: 'Missing Config', color: 'text-[#ff6b35]', bg: 'bg-[#ff6b35]' };
      case 'generating':
        return { icon: <Loader className="w-4 h-4 animate-spin" />, label: 'Generating', color: 'text-[#f69d50]', bg: 'bg-[#f69d50]' };
      case 'generated':
        return { icon: <Check className="w-4 h-4" />, label: 'Generated', color: 'text-[#4ecdc4]', bg: 'bg-[#4ecdc4]' };
    }
  };

  const getStatusConfig = (status: LobbyStatus) => {
    switch (status) {
      case 'waiting':
        return { label: 'Waiting', color: 'bg-[#8e8f94]' };
      case 'ready':
        return { label: 'Ready', color: 'bg-[#4ecdc4]' };
      case 'generating':
        return { label: 'Generating', color: 'bg-[#f69d50]' };
      case 'generated':
        return { label: 'Generated', color: 'bg-[#4ecdc4]' };
      case 'error':
        return { label: 'Error', color: 'bg-[#f85149]' };
    }
  };

  const statusConfig = getStatusConfig(lobbyStatus);

  const toggleCurrentReady = async () => {
    if (!lobbyId || !currentUser.seedConfigs.length) {
      setError('Select at least one seed config before marking ready.');
      return;
    }
    const nextReady = currentUser.readyState !== 'ready';
    trace('lobby-room', 'ready_toggle_start', { lobbyId, ready: nextReady });
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/ready`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ready: nextReady }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to update ready state.'));
      await publishLocalReadiness(players.map((player) => (
        player.id === currentUser.id
          ? { ...player, readyState: nextReady ? 'ready' as const : 'not-ready' as const }
          : player
      )), currentIdentity);
      await loadLobbyState('action');
      trace('lobby-room', 'ready_toggle_success', { lobbyId, ready: nextReady });
    } catch (err) {
      traceError('lobby-room', 'ready_toggle_failed', err, { lobbyId, ready: nextReady });
      setError(err instanceof Error ? err.message : 'Unable to update ready state.');
    }
  };

  const handleGenerate = async () => {
    if (!lobbyId || !canGenerate || generationSubmitRef.current) return;
    generationSubmitRef.current = true;
    setGenerationSubmitInFlight(true);
    setGenerationModalDismissed(false);
    advanceGenerationPhase('validating', true);
    try {
      const latestGeneration = await apiJson<LobbyGenerationStatus>(`/api/lobbies/${encodeURIComponent(lobbyId)}/generation`).catch(() => generation);
      if (generationReady(latestGeneration) || generationRunning(latestGeneration)) {
        if (latestGeneration) setGeneration(latestGeneration);
        const message = generationReady(latestGeneration)
          ? 'This Sync has already been generated. Regeneration is locked for this lobby.'
          : 'Generation is already starting for this lobby.';
        setError(message);
        await postSystemNotice(message);
        advanceGenerationPhase(generationReady(latestGeneration) ? 'ready' : 'preparing', true);
        return;
      }
      const localPlayer = currentPlayerFrom(players, currentIdentity);
      let localReadiness: { ready: boolean; note: string } | null = null;
      if (localPlayer) {
        localReadiness = await inspectLocalReadiness(localPlayer);
        await chatService.touchPresence({ kind: 'lobby', lobbyId }, {
          role: localPlayer.isHost ? 'host' : 'player',
          ready: localPlayer.readyState === 'ready',
          local_ready_known: true,
          local_ready: localReadiness.ready,
          local_ready_note: localReadiness.note,
        }).catch((err) => traceError('lobby-room', 'presence_preflight_publish_failed', err, { lobbyId }));
      }
      const latestPresence = await chatService.listPresence({ kind: 'lobby', lobbyId }).catch(() => presenceUsers);
      setPresenceUsers(latestPresence);
      const missingConfig = players.find((player) => !player.seedConfigs.length);
      const invalidConfig = players.find((player) => player.seedConfigs.some((config) => config.status !== 'valid'));
      const localNotReady = players.find((player) => {
        const isLocalPlayer = Boolean(localPlayer && (player.id === localPlayer.id || player.username === localPlayer.username));
        if (isLocalPlayer && localReadiness) return localReadiness.ready === false;
        const presence = presenceForPlayer(latestPresence, player);
        if (!presence || presence.local_ready_known !== true) return true;
        return presence.local_ready === false;
      });
      const localNotReadyIsSelf = Boolean(
        localNotReady &&
          localPlayer &&
          localReadiness &&
          (localNotReady.id === localPlayer.id || localNotReady.username === localPlayer.username)
      );
      const failure =
        missingConfig
          ? `${missingConfig.username}'s configuration is invalid at generation. Please select another or fix your config.`
          : invalidConfig
            ? `${invalidConfig.username}'s configuration is invalid at generation. Please select another or fix your config.`
            : localNotReady
              ? (localNotReadyIsSelf
                  ? localReadiness?.note
                  : presenceForPlayer(latestPresence, localNotReady)?.local_ready_note ||
                    `${localNotReady.username}'s local setup has not reported readiness yet. Please ask them to open the lobby and retry.`)
              : '';
      if (failure) {
        setError(failure);
        await postSystemNotice(failure);
        advanceGenerationPhase('idle', true);
        return;
      }
      setLobbyStatus('generating');
      advanceGenerationPhase('sending');
      trace('lobby-room', 'generate_start', { lobbyId });
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/generate`, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.detail || (data as any)?.error || 'Unable to generate sync.'));
      const responseGeneration = normalizeGenerationPayload(data);
      if (responseGeneration) setGeneration(responseGeneration);
      if (generationFailed(responseGeneration)) {
        throw new Error(generationErrorText(responseGeneration) || 'Generation failed.');
      }
      if (generationReady(responseGeneration)) {
        setLobbyStatus('generated');
        advanceGenerationPhase('ready');
        window.SKL_SFX?.play?.('sfx_generation_completed', 0.85);
      } else {
        setLobbyStatus('generating');
        advanceGenerationPhase(generationRunning(responseGeneration) ? 'generating' : 'generating');
      }
      await loadLobbyState('action');
      trace('lobby-room', 'generate_success', { lobbyId });
    } catch (err) {
      setLobbyStatus('error');
      advanceGenerationPhase('error', true);
      traceError('lobby-room', 'generate_failed', err, { lobbyId });
      const message = err instanceof Error ? err.message : 'Unable to generate sync.';
      window.SKL_SFX?.play?.('sfx_generation_error', 0.85);
      setError(message);
      await postSystemNotice(`Generation failed: ${message}`);
    } finally {
      generationSubmitRef.current = false;
      setGenerationSubmitInFlight(false);
    }
  };

  const handleResetGeneration = async () => {
    if (!lobbyId || generationResetInFlight) return;
    setGenerationResetInFlight(true);
    trace('lobby-room', 'generation_reset_start', { lobbyId });
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/generation/reset`, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      if (!response.ok || (data && typeof data === 'object' && (data as any).ok === false)) {
        throw new Error(String((data as any)?.message || (data as any)?.error || 'Unable to reset failed generation.'));
      }
      setGeneration(null);
      setGenerationPhase('idle');
      setGenerationModalDismissed(false);
      setLobbyStatus(players.every((player) => player.readyState === 'ready' && player.seedConfigs.length > 0) ? 'ready' : 'waiting');
      await postSystemNotice('Failed generation state was reset. You can generate the sync again.');
      await loadLobbyState('action');
      trace('lobby-room', 'generation_reset_success', { lobbyId });
    } catch (err) {
      traceError('lobby-room', 'generation_reset_failed', err, { lobbyId });
      setError(err instanceof Error ? err.message : 'Unable to reset failed generation.');
    } finally {
      setGenerationResetInFlight(false);
    }
  };

  const loadLatestGeneration = useCallback(async () => {
    if (!lobbyId) return null;
    const latest = await apiJson<LobbyGenerationStatus>(`/api/lobbies/${encodeURIComponent(lobbyId)}/generation`);
    setGeneration(latest);
    return latest;
  }, [lobbyId]);

  const fetchRoomStatus = useCallback(async (roomUrl?: string, retries = 1) => {
    return await fetchRoomStatusByUrl({
      roomUrl,
      retries,
      fetchJson: async <T,>(path: string) => await apiJson<T>(path),
    });
  }, []);

  const chooseLaunchConfig = useCallback((choices: LaunchChoice[]) => new Promise<LaunchChoice | null>((resolve) => {
    if (choices.length <= 1) {
      resolve(choices[0] || null);
      return;
    }
    setLaunchChoicePrompt({
      choices,
      selectedId: choices[0].id,
      resolve,
    });
  }), []);

  const handleLaunch = async () => {
    if (!lobbyId || launchSubmitRef.current) return;
    launchSubmitRef.current = true;
    setError('');
    trace('lobby-room', 'launch_start', { lobbyId });
    try {
      const latestGeneration = generationReady(generation) ? generation : await loadLatestGeneration();
      const roomUrl = String(latestGeneration?.room_url || latestGeneration?.room_server_url || latestGeneration?.response?.room_url || '');
      const roomStatus = await fetchRoomStatus(roomUrl, 3);
      const playersByName = indexPlayersByName(roomStatus?.players);
      const downloads = indexDownloadsBySlot(roomStatus?.downloads, apiUrl);
      const launchConfigs = currentUser.seedConfigs.length ? currentUser.seedConfigs : [{
        id: 'default-launch',
        game: '',
        configName: 'Default launch',
        source: 'advanced' as const,
        status: 'valid' as const,
      }];
      const choices = launchConfigs.map((config, index) => {
        const selfLaunch = buildSelfLaunchContext({
          roomStatus,
          playerName: currentUser.username,
          selection: config.id === 'default-launch' ? null : selectionForSeedConfig(config, currentUser.username),
          toUrl: apiUrl,
          downloadsBySlot: downloads.single,
          downloadsBySlotMulti: downloads.multi,
          playersByName,
        });
        return {
          id: `${config.id || 'launch'}-${index}`,
          config,
          selfLaunch,
          downloadCount: selfLaunch.downloadUrls.length || (selfLaunch.downloadUrl ? 1 : 0),
        } satisfies LaunchChoice;
      });
      const selectedChoice = await chooseLaunchConfig(choices);
      if (!selectedChoice) {
        trace('lobby-room', 'launch_cancelled_config_selection', { lobbyId });
        return;
      }
      if (!selectedChoice.selfLaunch.matched || !selectedChoice.selfLaunch.playerName) {
        const gameLabel = selectedChoice.config.game || selectedChoice.config.configName || 'selected game';
        const message = `No generated Archipelago slot was found for ${gameLabel}. Regenerate the room or refresh the lobby before launching.`;
        trace('lobby-room', 'launch_slot_match_failed', { lobbyId, game: selectedChoice.config.game, configId: selectedChoice.config.id }, 'warn');
        setError(message);
        return;
      }
      const multiGameEntries = choices.filter((choice) => choice.selfLaunch.matched && choice.selfLaunch.playerName).map((choice) => ({
        id: choice.id,
        label: choice.selfLaunch.apGameName || choice.config.game || choice.config.configName || choice.id,
        configName: choice.config.configName,
        downloadUrl: choice.selfLaunch.downloadUrls[0] || choice.selfLaunch.downloadUrl || '',
        apGameName: choice.selfLaunch.apGameName || choice.config.game || '',
        slot: choice.selfLaunch.playerName || currentUser.username,
        playerAlias: currentUser.username,
        trackerVariant: choice.config.trackerVariant || '',
      }));
      setLaunchInFlight(true);
      const { selfLaunch } = selectedChoice;
      const launchResult = await executeRoomSessionLaunch({
        downloadUrls: selfLaunch.downloadUrls.length ? selfLaunch.downloadUrls : selfLaunch.downloadUrl,
        roomStatus,
        generation: latestGeneration,
        host: resolveRoomServerHost(apiUrl),
        fetchRoomStatus,
        loadLatestGeneration,
        playerName: selfLaunch.playerName || currentUser.username,
        playerAlias: currentUser.username,
        apGameName: selfLaunch.apGameName,
        trackerVariant: selectedChoice.config.trackerVariant || '',
        playersByName,
        lobbyId,
        multiGameEntries,
      });
      if (!launchResult.ok) {
        const message = launchResult.surface === 'toast' ? launchResult.message : launchResult.errorMessage;
        trace('lobby-room', 'launch_result_failed', { lobbyId, surface: launchResult.surface, message }, 'warn');
        setError(message);
        return;
      }
      if (launchResult.status === 'manual') {
        setError(`Manual launch package prepared: ${launchResult.manualDownload.path}`);
      }
      trace('lobby-room', 'launch_success', { lobbyId, status: launchResult.status });
    } catch (err) {
      traceError('lobby-room', 'launch_failed', err, { lobbyId });
      setError(err instanceof Error ? err.message : 'Unable to launch sync.');
    } finally {
      launchSubmitRef.current = false;
      setLaunchInFlight(false);
    }
  };

  const sendLobbyMessage = async () => {
    const text = message.trim();
    if (!lobbyId || !text) return;
    setMessage('');
    trace('lobby-room', 'chat_send_start', { lobbyId, length: text.length });
    try {
      const sent = await chatService.sendMessage({ kind: 'lobby', lobbyId }, text);
      setChatMessages((list) => [...list, sent].slice(-80));
      trace('lobby-room', 'chat_send_success', { lobbyId, messageId: sent.id });
    } catch (err) {
      traceError('lobby-room', 'chat_send_failed', err, { lobbyId });
      setError(err instanceof Error ? err.message : 'Unable to send message.');
      setMessage(text);
    }
  };

  const handleLeaveLobby = async () => {
    if (!lobbyId) return;
    trace('lobby-room', 'leave_start', { lobbyId });
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/leave`, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to leave lobby.'));
      onLeaveLobby?.();
      trace('lobby-room', 'leave_success', { lobbyId });
    } catch (err) {
      traceError('lobby-room', 'leave_failed', err, { lobbyId });
      setError(err instanceof Error ? err.message : 'Unable to leave lobby.');
    }
  };

  const handleKickPlayer = async (playerId: string) => {
    if (!lobbyId || !playerId) return;
    const target = players.find((player) => player.id === playerId);
    try {
      const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/kick`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: playerId, name: target?.username || playerId }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Unable to kick player.'));
      setContextMenu(null);
      await loadLobbyState('action');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unable to kick player.');
    }
  };

  if (!lobbyId) {
    return (
      <div className="h-full flex flex-col relative overflow-hidden">
        <div className="absolute inset-0 z-0">
          <AnimatedBackground />
        </div>
        <div className="relative z-10 flex-1 flex items-center justify-center p-8 bg-[#0d1117]/90">
          <div className="bg-gradient-card border-2 border-[#2a2b30] rounded-xl p-8 max-w-md text-center card-float">
            <AlertCircle className="w-12 h-12 text-[#4ecdc4] mx-auto mb-4" />
            <h1 className="text-2xl font-bold mb-2">No lobby selected</h1>
            <p className="text-sm text-[#8e8f94] mb-6">Choose a live lobby from Nexus to open the room panel.</p>
            <button
              onClick={onLeaveLobby}
              className="px-5 py-2.5 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg text-sm font-medium shadow-lg hover:shadow-xl glow-hover transition-all"
            >
              Back to Lobbies
            </button>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="h-full flex flex-col relative overflow-hidden">
      <div className="absolute inset-0 z-0">
        <AnimatedBackground />
      </div>

      {/* Compact Header */}
      <div className="px-6 py-4 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22]/95 to-[#161b22]/95 backdrop-blur-sm relative z-10">
        <div className="flex items-center justify-between">
          {/* Lobby Info */}
          <div className="flex items-center gap-4">
            <div>
              <div className="flex items-center gap-3">
                <h1 className="text-2xl font-bold">{lobbyId}</h1>
                <span className={`${statusConfig.color} text-[#14151a] px-3 py-1 rounded-full text-xs font-bold`}>
                  {statusConfig.label}
                </span>
              </div>
              <p className="text-sm text-[#8e8f94] mt-0.5">{effectiveLobbyName}</p>
            </div>
          </div>

          {/* Status Summary */}
          <div className="flex items-center gap-6 text-sm">
            <div className="text-right">
              <div className="text-[#8e8f94]">Owner</div>
              <div className="text-[#4ecdc4] font-medium">{effectiveOwner}</div>
            </div>
            <div className="text-right">
              <div className="text-[#8e8f94]">Created</div>
              <div className="text-white">{effectiveCreatedAt}</div>
            </div>
            <div className="text-right">
              <div className="text-[#8e8f94]">Players</div>
              <div className="text-white">{readyPlayers}/{players.length} Ready</div>
            </div>
          </div>

          {/* Primary Action - THE LAUNCH BUTTON */}
          {launchAvailable ? (
            <button
              onClick={handleLaunch}
              disabled={launchInFlight || Boolean(launchChoicePrompt)}
              className={`px-12 py-4 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold text-xl shadow-2xl hover:shadow-[#4ecdc4]/50 transition-all border-2 border-[#4ecdc4] ${
                launchInFlight || launchChoicePrompt ? 'opacity-75 cursor-wait' : 'animate-pulse'
              }`}
            >
              {launchInFlight ? 'LAUNCHING...' : launchChoicePrompt ? 'CHOOSE GAME...' : 'LAUNCH'}
            </button>
          ) : canGenerate ? (
            <button
              onClick={handleGenerate}
              className="px-10 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white rounded-lg font-bold shadow-2xl hover:shadow-[#ff6b35]/50 glow-hover transition-all animate-pulse border-2 border-[#ffb199]"
            >
              Generate Sync
            </button>
          ) : (
            <button className="px-10 py-3 bg-[#2a2b30] text-[#8e8f94] rounded-lg font-bold cursor-not-allowed">
              Waiting...
            </button>
          )}
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex overflow-hidden relative z-10">
        {/* Left Side - Players & Seeds */}
        <div className="flex-1 border-r-2 border-[#2a2b30] flex flex-col bg-[#0d1117]/95 backdrop-blur-sm shadow-[0_10px_40px_rgba(0,0,0,0.8),0_0_20px_rgba(0,0,0,0.5)]">
          <div className="p-4 border-b-2 border-[#2a2b30] bg-[#14151a]/90">
            <div className="flex items-center justify-between mb-2">
              <h2 className="font-bold text-sm text-[#e6edf3]">PLAYERS & SEED CONFIGS</h2>
              <div className="flex items-center gap-2 text-xs">
                <Users className="w-4 h-4 text-[#8e8f94]" />
                <span className="text-[#8e8f94]">{players.length} Players</span>
                <span className="text-[#8e8f94]">•</span>
                <span className="text-[#8e8f94]">{totalConfigs} Configs</span>
              </div>
            </div>
          </div>

          {/* Players List */}
          <div className="flex-1 overflow-auto p-4 space-y-3">
            {players.map((player) => {
              const readyConfig = getReadyStateConfig(player.readyState);
              return (
                <div
                  key={player.id}
                  className="p-4 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg hover:border-[#4ecdc4] transition-all"
                >
                  {/* Player Header */}
                  <div
                    className="flex items-center justify-between mb-3"
                    onContextMenu={(e) => {
                      e.preventDefault();
                      setContextMenu({ playerId: player.id, x: e.clientX, y: e.clientY });
                    }}
                  >
                    <div className="flex items-center gap-3">
                      <div className="relative">
                        <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center font-bold">
                          {player.username.charAt(0).toUpperCase()}
                        </div>
                        {player.isHost && (
                          <div className="absolute -top-1 -right-1 w-5 h-5 bg-[#f69d50] rounded-full flex items-center justify-center">
                            <Crown className="w-3 h-3 text-[#14151a]" />
                          </div>
                        )}
                      </div>
                      <div>
                        <div className="font-bold flex items-center gap-2">
                          {player.username}
                          {player.isHost && (
                            <span className="px-2 py-0.5 bg-[#f69d50]/20 text-[#f69d50] rounded text-xs font-bold">
                              HOST
                            </span>
                          )}
                        </div>
                        <div className={`flex items-center gap-1.5 text-xs ${readyConfig.color}`}>
                          {readyConfig.icon}
                          <span>{readyConfig.label}</span>
                        </div>
                      </div>
                    </div>

                    {/* Ready Toggle (only for current user) */}
                    {player.id === currentUser.id && (
                      <button
                        onClick={toggleCurrentReady}
                        className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                          player.readyState === 'ready'
                            ? 'bg-[#2a2b30] text-white hover:bg-[#3a3b40]'
                            : 'bg-[#4ecdc4] text-[#14151a] hover:bg-[#95e1d3]'
                        }`}
                      >
                        {player.readyState === 'ready' ? 'Unready' : 'Ready'}
                      </button>
                    )}
                  </div>

                  {/* Seed Configs */}
                  <div className="space-y-2">
                    {player.seedConfigs.length > 0 ? (
                      <>
                        {/* Compact Seed Display */}
                        <div className="flex flex-wrap gap-2">
                          {(expandedPlayers.has(player.id) ? player.seedConfigs : player.seedConfigs.slice(0, 2)).map((config) => (
                            <div
                              key={config.id}
                              className="group relative flex items-center gap-2 pl-3 pr-2 py-1.5 bg-[#0d1117] border border-[#2a2b30] rounded-lg hover:border-[#4ecdc4] transition-all"
                            >
                              <div className="flex items-center gap-1.5">
                                <span className="font-medium text-xs">{config.game}</span>
                                {config.instance && (
                                  <span className="text-[10px] text-[#8e8f94]">{`{${config.instance}}`}</span>
                                )}
                                <span className={`px-1.5 py-0.5 rounded text-[9px] font-bold ${
                                  config.source === 'easy' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' : 'bg-[#aa96da]/20 text-[#aa96da]'
                                }`}>
                                  {config.source === 'easy' ? 'E' : 'A'}
                                </span>
                                {config.trackerVariantName || config.trackerVariant ? (
                                  <span className="px-1.5 py-0.5 rounded text-[9px] font-bold bg-[#4ecdc4]/15 text-[#4ecdc4]">
                                    {config.trackerVariantName || config.trackerVariant}
                                  </span>
                                ) : null}
                              </div>

                              {/* Tooltip on Hover */}
                              <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-3 py-2 bg-[#1c1d22] border-2 border-[#4ecdc4] rounded-lg shadow-2xl opacity-0 group-hover:opacity-100 pointer-events-none transition-opacity z-50 whitespace-nowrap">
                                <div className="text-xs font-bold text-white mb-1">{config.game}</div>
                                <div className="text-xs text-[#8e8f94] mb-1">{config.configName}</div>
                                {(config.trackerVariantName || config.trackerVariant) && (
                                  <div className="text-xs text-[#4ecdc4] mb-1">Tracker: {config.trackerVariantName || config.trackerVariant}</div>
                                )}
                                <div className="flex items-center gap-2">
                                  <span className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                                    config.source === 'easy' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' : 'bg-[#aa96da]/20 text-[#aa96da]'
                                  }`}>
                                    {config.source.toUpperCase()}
                                  </span>
                                  <span className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                                    config.status === 'valid' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' :
                                    config.status === 'draft' ? 'bg-[#f69d50]/20 text-[#f69d50]' :
                                    'bg-[#f85149]/20 text-[#f85149]'
                                  }`}>
                                    {config.status.toUpperCase()}
                                  </span>
                                </div>
                              </div>

                              {player.id === currentUser.id && (
                                <button
                                  onClick={() => handleRemoveSeed(config.id)}
                                  className="w-5 h-5 rounded bg-transparent hover:bg-[#f85149] transition-colors flex items-center justify-center opacity-0 group-hover:opacity-100"
                                >
                                  <X className="w-3 h-3 text-[#8e8f94] hover:text-white" />
                                </button>
                              )}
                            </div>
                          ))}

                          {/* Show More/Less Button */}
                          {player.seedConfigs.length > 2 && (
                            <button
                              onClick={() => {
                                const newExpanded = new Set(expandedPlayers);
                                if (expandedPlayers.has(player.id)) {
                                  newExpanded.delete(player.id);
                                } else {
                                  newExpanded.add(player.id);
                                }
                                setExpandedPlayers(newExpanded);
                              }}
                              className="px-3 py-1.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-xs font-medium text-[#4ecdc4] transition-all flex items-center gap-1"
                            >
                              {expandedPlayers.has(player.id) ? (
                                <>Show Less<ChevronDown className="w-3 h-3 rotate-180" /></>
                              ) : (
                                <>+{player.seedConfigs.length - 2} more<ChevronDown className="w-3 h-3" /></>
                              )}
                            </button>
                          )}
                        </div>
                      </>
                    ) : (
                      <div className="p-2 bg-[#0d1117] border border-[#2a2b30] rounded-lg text-center">
                        <p className="text-xs text-[#8e8f94]">No seed config selected</p>
                      </div>
                    )}

                    {/* Add Seed Button (only for current user) */}
                    {player.id === currentUser.id && (
                      <button
                        onClick={() => setShowSeedSelector(true)}
                        className="w-full py-2 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg text-xs font-bold shadow-lg hover:shadow-xl transition-all flex items-center justify-center gap-2"
                      >
                        <Plus className="w-3 h-3" />
                        Add Seed Config
                      </button>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Right Side - Chat & Actions */}
        <div className="w-[400px] flex flex-col bg-[#0d1117]/95 backdrop-blur-sm shadow-[0_10px_40px_rgba(0,0,0,0.8),0_0_20px_rgba(0,0,0,0.5)] border-l border-[#2a2b30]">
          {/* Status Dashboard */}
          <div className="p-4 border-b-2 border-[#2a2b30] bg-[#14151a]/90">
            <h3 className="text-xs font-bold text-[#e6edf3] mb-3">LOBBY STATUS</h3>
            <div className="grid grid-cols-2 gap-3">
              <div className="p-3 bg-[#1c1d22] rounded-lg border border-[#2a2b30]">
                <div className="text-xs text-[#8e8f94] mb-1">Ready Players</div>
                <div className="text-xl font-bold text-[#4ecdc4]">{readyPlayers}/{players.length}</div>
              </div>
              <div className="p-3 bg-[#1c1d22] rounded-lg border border-[#2a2b30]">
                <div className="text-xs text-[#8e8f94] mb-1">Total Configs</div>
                <div className="text-xl font-bold text-white">{totalConfigs}</div>
              </div>
            </div>
            {missingConfigs > 0 && (
              <div className="mt-3 p-2 bg-[#ff6b35]/10 border-l-2 border-[#ff6b35] rounded text-xs text-[#ff6b35]">
                {missingConfigs} player{missingConfigs > 1 ? 's' : ''} missing seed config
              </div>
            )}
          </div>

          {/* Chat */}
          <div className="flex-1 flex flex-col">
            <div className="p-4 border-b-2 border-[#2a2b30]">
              <h2 className="font-bold text-sm text-[#e6edf3]">LOBBY CHAT</h2>
            </div>

            <div className="flex-1 overflow-auto p-4 space-y-3">
              <div className="text-xs text-center py-2 px-3 bg-[#1c1d22] rounded-lg text-[#8e8f94]">
                <span className="font-medium text-[#4ecdc4]">{effectiveOwner}</span> created the lobby
              </div>
              {chatMessages.length ? chatMessages.map((entry) => (
                <div key={entry.id} className={entry.kind === 'system' ? 'text-xs text-center py-2 px-3 bg-[#1c1d22] rounded-lg text-[#8e8f94]' : 'text-sm'}>
                  {entry.kind === 'system' ? (
                    <EmoteText text={entry.content} />
                  ) : (
                    <div className="p-3 bg-[#1c1d22] border border-[#2a2b30] rounded-lg">
                      <div className="text-xs text-[#4ecdc4] font-bold mb-1">{entry.author}</div>
                      <div className="text-[#e6edf3]"><EmoteText text={entry.content} /></div>
                    </div>
                  )}
                </div>
              )) : (
                <div className="text-sm text-[#8e8f94] text-center py-8">
                  No messages yet
                </div>
              )}
            </div>

            <div className="p-4 border-t-2 border-[#2a2b30] bg-[#14151a]/90">
              <div className="flex gap-2">
                <input
                  type="text"
                  value={message}
                  onChange={(e) => setMessage(e.target.value)}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') void sendLobbyMessage();
                  }}
                  placeholder="Type a message..."
                  className="flex-1 px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none transition-colors text-sm"
                />
                <button
                  onClick={() => void sendLobbyMessage()}
                  className="px-4 py-2.5 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all"
                >
                  <Send className="w-4 h-4" />
                </button>
              </div>
            </div>
          </div>

          {/* Actions */}
          <div className="p-4 border-t-2 border-[#2a2b30] bg-[#14151a]/90 space-y-2">
            <button
              onClick={() => setShowLobbySettings(true)}
              className="w-full flex items-center justify-center gap-2 px-4 py-2.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors"
            >
              <Settings className="w-4 h-4" />
              Lobby Settings
            </button>
            <button
              onClick={() => void handleLeaveLobby()}
              className="w-full px-4 py-2.5 bg-[#f85149]/10 hover:bg-[#f85149]/20 text-[#f85149] rounded-lg text-sm font-medium transition-colors border border-[#f85149]/30"
            >
              Leave Lobby
            </button>
          </div>
        </div>
      </div>

      {/* Context Menu */}
      {contextMenu && (
        <>
          {/* Backdrop */}
          <div
            className="fixed inset-0 z-40"
            onClick={() => setContextMenu(null)}
          />

          {/* Context Menu */}
          <div
            className="fixed bg-[#1c1d22] rounded-lg shadow-2xl z-50 card-float border-2 border-[#2a2b30] overflow-hidden min-w-[200px]"
            style={{
              left: `${contextMenu.x}px`,
              top: `${contextMenu.y}px`,
            }}
          >
            <div className="py-2">
              {/* User Profile */}
              <button
                onClick={() => {
                  console.log('View profile:', contextMenu.playerId);
                  setContextMenu(null);
                }}
                className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#4ecdc4]/10 hover:to-transparent transition-all text-left"
              >
                <UserCircle className="w-4 h-4 text-[#4ecdc4]" />
                <span className="text-sm font-medium">User Profile</span>
              </button>

              {/* Friend Request */}
              <button
                onClick={() => {
                  console.log('Send friend request:', contextMenu.playerId);
                  setContextMenu(null);
                }}
                className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#aa96da]/10 hover:to-transparent transition-all text-left"
              >
                <UserPlus className="w-4 h-4 text-[#aa96da]" />
                <span className="text-sm font-medium">Friend Request</span>
              </button>

              {/* Mute */}
              <button
                onClick={() => {
                  console.log('Mute user:', contextMenu.playerId);
                  setContextMenu(null);
                }}
                className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#f69d50]/10 hover:to-transparent transition-all text-left"
              >
                <Volume2 className="w-4 h-4 text-[#f69d50]" />
                <span className="text-sm font-medium">Mute</span>
              </button>

              {/* Add to Blacklist */}
              <button
                onClick={() => {
                  console.log('Blacklist user:', contextMenu.playerId);
                  setContextMenu(null);
                }}
                className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#f85149]/10 hover:to-transparent transition-all text-left"
              >
                <Ban className="w-4 h-4 text-[#f85149]" />
                <span className="text-sm font-medium">Add to Blacklist</span>
              </button>

              {/* Kick User (Host only) */}
              {currentUser.isHost && contextMenu.playerId !== currentUser.id && (
                <>
                  <div className="h-px bg-[#2a2b30] my-2" />
                  <button
                    onClick={() => void handleKickPlayer(contextMenu.playerId)}
                    className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#f85149]/20 hover:to-transparent transition-all text-left"
                  >
                    <LogOut className="w-4 h-4 text-[#f85149]" />
                    <span className="text-sm font-medium text-[#f85149]">Kick User</span>
                  </button>
                </>
              )}
            </div>
          </div>
        </>
      )}

      {/* Seed Selector Modal */}
      {showSeedSelector && (
        <div className="fixed inset-0 bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
          <div className="w-full max-w-3xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4ecdc4] card-float overflow-hidden">
            {/* Modal Header */}
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#4ecdc4] to-[#95e1d3] flex items-center justify-center">
                  <Tag className="w-5 h-5 text-[#14151a]" />
                </div>
                <div>
                  <h2 className="text-2xl font-bold">Select Seed Config</h2>
                  <p className="text-sm text-[#8e8f94]">Choose from your library</p>
                </div>
              </div>
              <button
                onClick={() => setShowSeedSelector(false)}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            {/* Search */}
            <div className="p-4 border-b-2 border-[#2a2b30]">
              <div className="relative">
                <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-[#8e8f94]" />
                <input
                  type="text"
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  placeholder="Search games or configs..."
                  className="w-full pl-10 pr-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#4ecdc4] outline-none transition-colors"
                />
              </div>
            </div>

            {/* Seed Configs List */}
            <div className="p-4 max-h-[500px] overflow-y-auto space-y-2">
              {filteredConfigs.map((config) => (
                <button
                  key={config.id}
                  onClick={() => void handleAddSeed(config)}
                  disabled={Boolean(addingSeedName)}
                  className={`w-full p-4 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg hover:border-[#4ecdc4] transition-all text-left group ${
                    addingSeedName ? 'opacity-70 cursor-wait' : ''
                  }`}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex-1">
                      <div className="flex items-center gap-2 mb-1">
                        <span className="font-bold">{config.game}</span>
                        <span className={`px-2 py-0.5 rounded text-xs font-bold ${
                          config.source === 'easy' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' : 'bg-[#aa96da]/20 text-[#aa96da]'
                        }`}>
                          {config.source.toUpperCase()}
                        </span>
                        <span className={`px-2 py-0.5 rounded text-xs font-bold ${
                          config.status === 'valid' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' :
                          config.status === 'draft' ? 'bg-[#f69d50]/20 text-[#f69d50]' :
                          'bg-[#f85149]/20 text-[#f85149]'
                        }`}>
                          {config.status.toUpperCase()}
                        </span>
                      </div>
                      <div className="text-sm text-[#8e8f94]">{config.configName}</div>
                    </div>
                    <Plus className="w-5 h-5 text-[#8e8f94] group-hover:text-[#4ecdc4] transition-colors" />
                  </div>
                </button>
              ))}
              {filteredConfigs.length === 0 && (
                <div className="text-center py-12 text-[#8e8f94]">
                  No seed configs found
                </div>
              )}
            </div>

            {/* Footer */}
            <div className="p-4 border-t-2 border-[#2a2b30] bg-[#14151a]/90">
              <button
                onClick={() => setShowSeedSelector(false)}
                className="w-full py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
              >
                Close
              </button>
            </div>
          </div>
        </div>
      )}

      {launchChoicePrompt && (
        <div className="fixed inset-0 bg-black/85 backdrop-blur-sm flex items-center justify-center z-[70] p-8">
          <div className="w-full max-w-2xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4ecdc4] card-float overflow-hidden">
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div>
                <h2 className="text-2xl font-bold">Choose Game to Launch</h2>
                <p className="text-sm text-[#8e8f94] mt-1">This lobby has multiple configs for your account.</p>
              </div>
              <button
                onClick={() => {
                  launchChoicePrompt.resolve(null);
                  setLaunchChoicePrompt(null);
                }}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            <div className="p-5 space-y-3 max-h-[520px] overflow-y-auto">
              {launchChoicePrompt.choices.map((choice) => {
                const selected = launchChoicePrompt.selectedId === choice.id;
                const slotName = choice.selfLaunch.playerName || currentUser.username;
                const gameName = choice.selfLaunch.apGameName || choice.config.game || 'Unknown game';
                return (
                  <button
                    key={choice.id}
                    onClick={() => setLaunchChoicePrompt((current) => current ? { ...current, selectedId: choice.id } : current)}
                    className={`w-full p-4 rounded-lg border-2 text-left transition-all ${
                      selected
                        ? 'border-[#4ecdc4] bg-[#4ecdc4]/10 shadow-[0_0_18px_rgba(78,205,196,0.18)]'
                        : 'border-[#2a2b30] bg-[#1c1d22] hover:border-[#4ecdc4]/70'
                    }`}
                  >
                    <div className="flex items-start justify-between gap-4">
                      <div className="min-w-0">
                        <div className="flex flex-wrap items-center gap-2 mb-1">
                          <span className="font-bold text-white">{gameName}</span>
                          <span className={`px-2 py-0.5 rounded text-[10px] font-bold ${
                            choice.config.source === 'easy' ? 'bg-[#4ecdc4]/20 text-[#4ecdc4]' : 'bg-[#aa96da]/20 text-[#aa96da]'
                          }`}>
                            {choice.config.source.toUpperCase()}
                          </span>
                          {choice.config.trackerVariantName || choice.config.trackerVariant ? (
                            <span className="px-2 py-0.5 rounded text-[10px] font-bold bg-[#4ecdc4]/15 text-[#4ecdc4]">
                              {choice.config.trackerVariantName || choice.config.trackerVariant}
                            </span>
                          ) : null}
                        </div>
                        <div className="text-sm text-[#d7dde5] truncate">{choice.config.configName}</div>
                        <div className="mt-2 grid grid-cols-1 sm:grid-cols-3 gap-2 text-xs text-[#8e8f94]">
                          <span>Slot: <b className="text-[#e6edf3]">{slotName || '-'}</b></span>
                          <span>Downloads: <b className="text-[#e6edf3]">{choice.downloadCount}</b></span>
                          <span>Config: <b className="text-[#e6edf3]">{choice.config.id}</b></span>
                        </div>
                      </div>
                      {selected && <Check className="w-5 h-5 text-[#4ecdc4] shrink-0 mt-1" />}
                    </div>
                  </button>
                );
              })}
            </div>

            <div className="p-4 border-t-2 border-[#2a2b30] bg-[#14151a]/90 flex gap-3">
              <button
                onClick={() => {
                  launchChoicePrompt.resolve(null);
                  setLaunchChoicePrompt(null);
                }}
                className="flex-1 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={() => {
                  const selected = launchChoicePrompt.choices.find((choice) => choice.id === launchChoicePrompt.selectedId) || launchChoicePrompt.choices[0] || null;
                  launchChoicePrompt.resolve(selected);
                  setLaunchChoicePrompt(null);
                }}
                className="flex-1 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white rounded-lg font-bold shadow-lg hover:shadow-xl transition-all"
              >
                Launch Selected
              </button>
            </div>
          </div>
        </div>
      )}

      {trackerVariantPrompt && (
        <div className="fixed inset-0 bg-black/85 backdrop-blur-sm flex items-center justify-center z-[70] p-8">
          <div className="w-full max-w-xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4ecdc4] card-float overflow-hidden">
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div>
                <h2 className="text-2xl font-bold">Choose PopTracker Layout</h2>
                <p className="text-sm text-[#8e8f94] mt-1">{trackerVariantPrompt.gameLabel} • {trackerVariantPrompt.configName}</p>
              </div>
              <button
                onClick={() => {
                  trackerVariantPrompt.resolve(null);
                  setTrackerVariantPrompt(null);
                }}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            <div className="p-5 space-y-3 max-h-[440px] overflow-y-auto">
              {trackerVariantPrompt.variants.map((variant) => (
                <button
                  key={variant.id}
                  onClick={() => setTrackerVariantPrompt((current) => current ? { ...current, selected: variant.id } : current)}
                  className={`w-full p-4 rounded-lg border-2 text-left transition-all ${
                    trackerVariantPrompt.selected === variant.id
                      ? 'border-[#4ecdc4] bg-[#4ecdc4]/10 shadow-[0_0_18px_rgba(78,205,196,0.18)]'
                      : 'border-[#2a2b30] bg-[#1c1d22] hover:border-[#4ecdc4]/70'
                  }`}
                >
                  <div className="flex items-center justify-between gap-4">
                    <div>
                      <div className="font-bold text-white">{variant.name}</div>
                      <div className="text-xs text-[#8e8f94] mt-1">{variant.id}</div>
                    </div>
                    {trackerVariantPrompt.selected === variant.id && (
                      <Check className="w-5 h-5 text-[#4ecdc4]" />
                    )}
                  </div>
                </button>
              ))}
            </div>

            <div className="p-4 border-t-2 border-[#2a2b30] bg-[#14151a]/90 flex gap-3">
              <button
                onClick={() => {
                  trackerVariantPrompt.resolve(null);
                  setTrackerVariantPrompt(null);
                }}
                className="flex-1 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={() => {
                  const selected = trackerVariantPrompt.variants.find((variant) => variant.id === trackerVariantPrompt.selected) || trackerVariantPrompt.variants[0] || null;
                  trackerVariantPrompt.resolve(selected);
                  setTrackerVariantPrompt(null);
                }}
                className="flex-1 py-3 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all"
              >
                Use Layout
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Lobby Settings Modal */}
      {showLobbySettings && (
        <LobbySettingsModal
          onClose={() => setShowLobbySettings(false)}
          onSave={async (settings) => {
            if (!lobbyId) return;
            const response = await apiFetch(`/api/lobbies/${encodeURIComponent(lobbyId)}/settings`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({
                server_password: settings.password,
                password: settings.password,
                item_cheat: !settings.disableItemCheat,
                disable_item_cheat: settings.disableItemCheat,
                location_check_points: settings.locationCheckPoints,
                hint_cost: settings.hintCost,
                release_mode: settings.releaseMode,
                collect_mode: settings.collectMode,
                remaining_mode: settings.remainingMode,
                hint_mode: settings.hintMode,
                spoiler: settings.spoiler,
              }),
            });
            const data = await response.json().catch(() => ({}));
            if (!response.ok) {
              const message = String((data as any)?.error || 'Unable to save lobby settings.');
              setError(message);
              throw new Error(message);
            }
            await loadLobbyState('action');
          }}
        />
      )}

      {((lobbyStatus === 'generating' || lobbyStatus === 'error' || generationRunning(generation) || generationFailed(generation) || generationPhase === 'error' || canLaunch)) && !generationModalDismissed && (
        <GenerationProgressModal
          generation={generation}
          status={lobbyStatus}
          phase={generationPhase}
          canLaunch={canLaunch}
          onLaunch={handleLaunch}
          onReset={handleResetGeneration}
          resetInFlight={generationResetInFlight}
          onClose={() => setGenerationModalDismissed(true)}
        />
      )}

      {addingSeedName && (
        <LoadingModal
          title={`Adding ${addingSeedName}...`}
          message="Adding this config to the lobby..."
        />
      )}

      {launchInFlight && (
        <LoadingModal
          title="Launching game"
          message="Launching game, brace yourself..."
        />
      )}

      {error && (
        <ErrorModal
          title="Lobby action failed"
          message={error}
          code="LOBBY_ACTION_FAILED"
          onClose={() => setError('')}
        />
      )}

      {/* Floating Help Button */}
      <HelpButton />
    </div>
  );
}

function GenerationProgressModal({
  generation,
  status,
  phase,
  canLaunch,
  onLaunch,
  onReset,
  resetInFlight,
  onClose,
}: {
  generation: LobbyGenerationStatus | null;
  status: LobbyStatus;
  phase: GenerationPhase;
  canLaunch: boolean;
  onLaunch: () => void | Promise<void>;
  onReset: () => void | Promise<void>;
  resetInFlight: boolean;
  onClose: () => void;
}) {
  const ready = canLaunch || generationReady(generation);
  const failed = phase === 'error' || status === 'error' || generationFailed(generation);
  const failureSummary = generationErrorText(generation) || 'Worlds could not finish this sync.';
  const failureDetails = generationErrorDetails(generation);
  const running = status === 'generating' || generationRunning(generation);
  const statusPhase = generationPhaseFromStatus(generation);
  const effectivePhase: GenerationPhase = failed ? 'error' : ready ? 'ready' : statusPhase || (running && phase === 'idle' ? 'generating' : phase);
  const activeIndex = effectivePhase === 'validating'
    ? 0
    : effectivePhase === 'sending'
      ? 1
      : effectivePhase === 'generating'
        ? 2
        : effectivePhase === 'preparing'
          ? 3
          : -1;
  const steps = [
    { label: 'Validating players', complete: generationPhaseRank[effectivePhase] >= generationPhaseRank.sending },
    { label: 'Sending configs to Worlds', complete: generationPhaseRank[effectivePhase] >= generationPhaseRank.generating },
    { label: 'Generating Sync', complete: generationPhaseRank[effectivePhase] >= generationPhaseRank.preparing || ready },
    { label: 'Preparing room server', complete: ready },
    { label: 'Ready to launch', complete: ready },
  ];

  return (
    <div className="fixed inset-0 z-[70] flex items-center justify-center bg-black/75 backdrop-blur-sm p-8">
      <div className="w-full max-w-xl bg-[#161b22] border-2 border-[#4ecdc4] rounded-xl shadow-2xl card-float overflow-hidden">
        <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22]">
          <div className="flex items-start justify-between gap-4">
            <div className="flex items-center gap-4">
              <div className="w-14 h-14 rounded-xl bg-[#4ecdc4]/10 border border-[#4ecdc4]/40 flex items-center justify-center">
                {failed ? <AlertCircle className="w-7 h-7 text-[#ff6b6b]" /> : ready ? <Check className="w-7 h-7 text-[#4ecdc4]" /> : <Loader className="w-7 h-7 text-[#4ecdc4] animate-spin" />}
              </div>
              <div>
                <h2 className="text-2xl font-bold">{failed ? 'Generation failed' : ready ? 'Sync ready' : 'Generating Sync'}</h2>
                <p className="text-sm text-[#8e8f94] mt-1">
                  {failed ? failureSummary : ready ? 'The room package is ready for this lobby.' : 'Everyone in the lobby is waiting on the same generation job.'}
                </p>
              </div>
            </div>
            <button
              type="button"
              onClick={onClose}
              aria-label="Close Sync ready dialog"
              className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] text-[#8e8f94] hover:text-white transition-colors flex items-center justify-center"
            >
              <X className="w-5 h-5" />
            </button>
          </div>
        </div>

        <div className="p-6 space-y-3">
          {steps.map((step, index) => (
            <div key={step.label} className="flex items-center gap-3 p-3 bg-[#0d1117] border border-[#2a2b30] rounded-lg">
              <div className={`w-7 h-7 rounded-full flex items-center justify-center text-xs font-bold ${
                step.complete ? 'bg-[#4ecdc4] text-[#14151a]' : 'bg-[#2a2b30] text-[#8e8f94]'
              }`}>
                {step.complete ? <Check className="w-4 h-4" /> : activeIndex === index ? <Loader className="w-4 h-4 animate-spin" /> : index + 1}
              </div>
              <span className={step.complete ? 'text-white' : 'text-[#8e8f94]'}>{step.label}</span>
            </div>
          ))}

          {failed && failureDetails && failureDetails !== failureSummary && (
            <pre className="max-h-48 overflow-auto whitespace-pre-wrap rounded-lg border border-[#ff6b6b]/35 bg-[#0d1117] p-3 text-xs leading-relaxed text-[#f5c2c7]">
              {failureDetails}
            </pre>
          )}

          {failed ? (
            <div className="mt-4 grid grid-cols-1 sm:grid-cols-2 gap-3">
              <button
                onClick={() => void onReset()}
                disabled={resetInFlight}
                className="py-4 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-black hover:shadow-[#4ecdc4]/30 disabled:opacity-60 disabled:cursor-not-allowed transition-all"
              >
                {resetInFlight ? 'Resetting...' : 'Reset room'}
              </button>
              <button
                onClick={onClose}
                disabled={resetInFlight}
                className="py-4 bg-[#2a2b30] text-white rounded-lg font-bold hover:bg-[#3a3b40] disabled:opacity-60 disabled:cursor-not-allowed transition-all"
              >
                Close
              </button>
            </div>
          ) : ready ? (
            <button
              onClick={() => void onLaunch()}
              className="mt-4 w-full py-5 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg font-black text-2xl shadow-2xl hover:shadow-[#4ecdc4]/50 transition-all border-2 border-[#4ecdc4]"
            >
              LAUNCH
            </button>
          ) : (
            <div className="mt-4 text-center text-sm text-[#8e8f94]">
              Generation is running. This modal will update automatically.
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
