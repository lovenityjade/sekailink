import { Users, Zap, User, Clock, Calendar } from 'lucide-react';
import { useCallback, useEffect, useMemo, useState } from 'react';
import { joinLobby, listLobbies, type LobbySummary } from '../../services/lobbyClient';
import { trace, traceError } from '../../services/trace';
import { ErrorModal } from './FeedbackModal';

interface LobbiesPageProps {
  onCreateLobby?: () => void;
  onJoinStart?: (lobby: LobbySummary) => void;
  onJoinError?: (lobby: LobbySummary, error: unknown) => void;
  onJoinLobby?: (lobbyId: string, lobby: LobbySummary) => void;
}

const formatAge = (value?: string) => {
  if (!value) return 'recently';
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) return 'recently';
  const diff = Math.max(0, Date.now() - timestamp);
  const minutes = Math.floor(diff / 60000);
  if (minutes < 1) return 'just now';
  if (minutes < 60) return `${minutes}m ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 48) return `${hours}h ago`;
  return `${Math.floor(hours / 24)}d ago`;
};

const maxPlayersForLobby = (lobby: LobbySummary) => {
  const max = Number(lobby.max_players || 50);
  return Number.isFinite(max) && max > 0 ? max : 50;
};

export default function LobbiesPage({ onCreateLobby, onJoinStart, onJoinError, onJoinLobby }: LobbiesPageProps = {}) {
  const [lobbies, setLobbies] = useState<LobbySummary[]>([]);
  const [loading, setLoading] = useState(true);
  const [joiningId, setJoiningId] = useState('');
  const [error, setError] = useState('');

  const load = useCallback(async () => {
    trace('lobbies-page', 'load_start');
    try {
      const next = await listLobbies();
      setLobbies(next);
      setError('');
      trace('lobbies-page', 'load_success', { count: next.length });
    } catch (err) {
      traceError('lobbies-page', 'load_failed', err);
      setError(err instanceof Error ? err.message : 'Unable to load lobbies.');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void load();
    const interval = window.setInterval(() => void load(), 20000);
    return () => window.clearInterval(interval);
  }, [load]);

  const activeLobbies = useMemo(() => lobbies.filter((lobby) => !lobby.is_locked), [lobbies]);
  const completedLobbies = useMemo(() => lobbies.filter((lobby) => lobby.is_locked), [lobbies]);

  const handleJoin = async (lobby: LobbySummary) => {
    if (!lobby.id || joiningId) return;
    trace('lobbies-page', 'join_click', { lobbyId: lobby.id, private: lobby.is_private === true });
    const password = lobby.is_private ? window.prompt('Lobby password') : '';
    if (password === null) return;
    setJoiningId(lobby.id);
    setError('');
    onJoinStart?.(lobby);
    try {
      await joinLobby(lobby, password || undefined);
      trace('lobbies-page', 'join_success', { lobbyId: lobby.id });
      onJoinLobby?.(lobby.id, lobby);
    } catch (err) {
      traceError('lobbies-page', 'join_failed', err, { lobbyId: lobby.id });
      setError(err instanceof Error ? err.message : 'Unable to join lobby.');
      onJoinError?.(lobby, err);
    } finally {
      setJoiningId('');
    }
  };

  return (
    <div className="p-8 space-y-8">
      {/* Header */}
      <div>
        <h1 className="text-3xl font-bold mb-1">Lobbies</h1>
        <p className="text-[#8e8f94]">Manage your active and completed lobbies</p>
      </div>

      {/* Active Lobbies Section */}
      <div>
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <div className="w-1 h-6 bg-[#4ecdc4] rounded-full" />
            <h2 className="text-xl font-bold">Active Lobbies</h2>
            <span className="px-2 py-1 bg-[#4ecdc4]/20 text-[#4ecdc4] rounded-full text-xs font-bold">
              {activeLobbies.length} Active
            </span>
          </div>
          <button
            onClick={onCreateLobby}
            className="px-5 py-2.5 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg font-medium shadow-lg hover:shadow-xl glow-hover transition-all"
          >
            Create New Lobby
          </button>
        </div>

        <div className="grid grid-cols-1 gap-4">
          {activeLobbies.map((lobby) => (
            <ActiveLobbyCard
              key={lobby.id}
              name={lobby.name || lobby.id}
              host={lobby.owner || 'Unknown'}
              seedsInSync={Number(lobby.message_count || 0)}
              players={Number(lobby.member_count || 0)}
              maxPlayers={maxPlayersForLobby(lobby)}
              status={lobby.is_locked ? 'in-progress' : 'waiting'}
              createdAt={formatAge(lobby.last_activity)}
              joining={joiningId === lobby.id}
              onJoin={() => void handleJoin(lobby)}
            />
          ))}
          {!loading && activeLobbies.length === 0 && (
            <EmptyLobbyState label="No active lobbies yet." />
          )}
          {loading && activeLobbies.length === 0 && (
            <EmptyLobbyState label="Loading lobbies..." />
          )}
        </div>
      </div>

      {/* Completed Lobbies Section */}
      <div>
        <div className="flex items-center gap-3 mb-4">
          <div className="w-1 h-6 bg-[#8e8f94] rounded-full" />
          <h2 className="text-xl font-bold">Completed Lobbies</h2>
          <span className="px-2 py-1 bg-[#8e8f94]/20 text-[#8e8f94] rounded-full text-xs font-bold">
            {completedLobbies.length} Completed
          </span>
        </div>

        <div className="grid grid-cols-1 gap-4">
          {completedLobbies.map((lobby) => (
            <CompletedLobbyCard
              key={lobby.id}
              name={lobby.name || lobby.id}
              host={lobby.owner || 'Unknown'}
              seedsInSync={Number(lobby.message_count || 0)}
              players={Number(lobby.member_count || 0)}
              completedAt={formatAge(lobby.last_activity)}
              duration="Archived"
            />
          ))}
          {!loading && completedLobbies.length === 0 && (
            <EmptyLobbyState label="No completed lobbies in the current list." />
          )}
        </div>
      </div>
      {error && (
        <ErrorModal
          title="Lobby error"
          message={error}
          code="LOBBY_ACTION_FAILED"
          onClose={() => setError('')}
        />
      )}
    </div>
  );
}

function EmptyLobbyState({ label }: { label: string }) {
  return (
    <div className="p-8 bg-gradient-card rounded-lg border-2 border-[#2a2b30] text-center text-[#8e8f94]">
      {label}
    </div>
  );
}

function ActiveLobbyCard({ name, host, seedsInSync, players, maxPlayers, status, createdAt, joining, onJoin }: {
  name: string;
  host: string;
  seedsInSync: number;
  players: number;
  maxPlayers: number;
  status: 'waiting' | 'in-progress';
  createdAt: string;
  joining?: boolean;
  onJoin: () => void;
}) {
  const statusConfig = {
    waiting: {
      bg: 'bg-[#4ecdc4]',
      text: 'Waiting',
      gradient: 'from-[#4ecdc4]/10 to-transparent'
    },
    'in-progress': {
      bg: 'bg-[#ff6b35]',
      text: 'In Progress',
      gradient: 'from-[#ff6b35]/10 to-transparent'
    }
  }[status];

  return (
    <div className={`p-6 bg-gradient-to-r ${statusConfig.gradient} border-2 border-[#2a2b30] rounded-lg hover:border-[#ff6b35] transition-all cursor-pointer card-float-hover`}>
      <div className="flex items-start justify-between mb-4">
        <div className="flex-1">
          <div className="flex items-center gap-3 mb-3">
            <h3 className="text-xl font-bold">{name}</h3>
            <span className={`${statusConfig.bg} text-[#14151a] px-3 py-1 rounded-full text-xs font-bold`}>
              {statusConfig.text}
            </span>
          </div>
          <div className="flex items-center gap-6 text-sm text-[#8e8f94]">
            <div className="flex items-center gap-2">
              <User className="w-4 h-4" />
              <span>Host: <span className="text-white">{host}</span></span>
            </div>
            <div className="flex items-center gap-2">
              <Zap className="w-4 h-4" />
              <span>Activity: <span className="text-white">{seedsInSync}</span></span>
            </div>
            <div className="flex items-center gap-2">
              <Clock className="w-4 h-4" />
              <span>Updated {createdAt}</span>
            </div>
          </div>
        </div>
        <div className="flex gap-2">
          <button
            onClick={onJoin}
            disabled={joining}
            className="px-5 py-2.5 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors disabled:opacity-50"
          >
            {joining ? 'Joining...' : status === 'waiting' ? 'Join' : 'Spectate'}
          </button>
        </div>
      </div>

      {/* Players Bar */}
      <div className="flex items-center gap-3">
        <div className="flex-1">
          <div className="flex items-center gap-2 mb-2">
            <Users className="w-4 h-4 text-[#8e8f94]" />
            <span className="text-sm text-[#8e8f94]">{players}/{maxPlayers} Players</span>
          </div>
          <div className="h-2 bg-[#14151a] rounded-full overflow-hidden">
            <div
              className="h-full bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] rounded-full transition-all"
              style={{ width: `${Math.min(100, Math.max(0, (players / maxPlayers) * 100))}%` }}
            />
          </div>
        </div>
      </div>
    </div>
  );
}

function CompletedLobbyCard({ name, host, seedsInSync, players, completedAt, duration }: {
  name: string;
  host: string;
  seedsInSync: number;
  players: number;
  completedAt: string;
  duration: string;
}) {
  return (
    <div className="p-6 bg-gradient-to-r from-[#8e8f94]/5 to-transparent border-2 border-[#2a2b30] rounded-lg hover:border-[#8e8f94] transition-all card-float-hover">
      <div className="flex items-start justify-between">
        <div className="flex-1">
          <div className="flex items-center gap-3 mb-3">
            <h3 className="text-lg font-bold text-[#e6edf3]">{name}</h3>
            <span className="bg-[#8e8f94]/20 text-[#8e8f94] px-3 py-1 rounded-full text-xs font-bold">
              Completed
            </span>
          </div>

          <div className="grid grid-cols-2 gap-4 mb-4">
            <div className="flex items-center gap-2 text-sm">
              <User className="w-4 h-4 text-[#8e8f94]" />
              <span className="text-[#8e8f94]">Host: <span className="text-white">{host}</span></span>
            </div>
            <div className="flex items-center gap-2 text-sm">
              <Users className="w-4 h-4 text-[#8e8f94]" />
              <span className="text-[#8e8f94]">Players: <span className="text-white">{players}</span></span>
            </div>
            <div className="flex items-center gap-2 text-sm">
              <Zap className="w-4 h-4 text-[#8e8f94]" />
              <span className="text-[#8e8f94]">Activity: <span className="text-white">{seedsInSync}</span></span>
            </div>
            <div className="flex items-center gap-2 text-sm">
              <Clock className="w-4 h-4 text-[#8e8f94]" />
              <span className="text-[#8e8f94]">Duration: <span className="text-white">{duration}</span></span>
            </div>
            <div className="flex items-center gap-2 text-sm">
              <Calendar className="w-4 h-4 text-[#8e8f94]" />
              <span className="text-[#8e8f94]">{completedAt}</span>
            </div>
          </div>
        </div>

        <button className="px-5 py-2.5 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#14151a] rounded-lg text-sm font-bold shadow-lg hover:shadow-xl transition-all">
          View Results
        </button>
      </div>
    </div>
  );
}
