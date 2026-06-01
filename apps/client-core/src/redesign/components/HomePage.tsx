import { Plus, Zap, Clock, Users, Play } from 'lucide-react';

interface HomePageProps {
  onNavigate: (view: 'home' | 'library' | 'lobbies' | 'settings') => void;
}

export default function HomePage({ onNavigate }: HomePageProps) {
  return (
    <div className="h-full p-8 space-y-6 overflow-auto">
      {/* Welcome Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-semibold text-[#e6edf3] mb-1">Welcome back, Player</h1>
          <p className="text-sm text-[#8b949e]">Ready for your next randomizer adventure?</p>
        </div>
        <div className="flex gap-3">
          <ActionButton icon={<Plus className="w-5 h-5" />} primary>
            Create Lobby
          </ActionButton>
          <ActionButton icon={<Users className="w-5 h-5" />}>
            Join Lobby
          </ActionButton>
        </div>
      </div>

      {/* Active Sync Status */}
      <div className="border-2 border-[#30363d] bg-[#161b22] p-6 relative overflow-hidden">
        <div className="absolute top-0 right-0 w-32 h-32 bg-gradient-to-br from-[#4dffd2]/10 to-transparent rounded-full blur-3xl" />
        <div className="relative z-10">
          <div className="flex items-center gap-2 mb-4">
            <div className="w-2 h-2 bg-[#8b949e] rounded-full" />
            <span className="text-sm font-medium text-[#8b949e]">CURRENT SYNC</span>
          </div>
          <div className="text-[#8b949e]">
            No active Sync session
          </div>
          <p className="text-sm text-[#8b949e] mt-2">
            Create or join a lobby to start a multiplayer randomizer session
          </p>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-6">
        {/* Quick Select Games */}
        <div>
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-lg font-semibold text-[#e6edf3]">Quick Select</h2>
            <button
              onClick={() => onNavigate('library')}
              className="text-sm text-[#4dffd2] hover:underline"
            >
              View All
            </button>
          </div>
          <div className="space-y-3">
            <GameQuickCard
              name="A Link to the Past"
              platform="SNES"
              configs={3}
              onClick={() => onNavigate('library')}
            />
            <GameQuickCard
              name="Super Metroid"
              platform="SNES"
              configs={1}
              disabled
            />
            <GameQuickCard
              name="Ocarina of Time"
              platform="N64"
              configs={0}
              disabled
            />
          </div>
        </div>

        {/* Recent Activity */}
        <div>
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-lg font-semibold text-[#e6edf3]">Recent Lobbies</h2>
          </div>
          <div className="space-y-3">
            <LobbyCard
              name="Weekend Randomizer"
              game="A Link to the Past"
              players={3}
              status="completed"
              time="2h ago"
            />
            <LobbyCard
              name="Quick Sync"
              game="A Link to the Past"
              players={2}
              status="completed"
              time="1d ago"
            />
          </div>
        </div>
      </div>

      {/* Recent Configs */}
      <div>
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-lg font-semibold text-[#e6edf3]">Recent Seed Configs</h2>
        </div>
        <div className="grid grid-cols-3 gap-4">
          <ConfigCard
            name="Standard Run"
            game="A Link to the Past"
            source="Easy"
            updated="2d ago"
          />
          <ConfigCard
            name="Hard Mode Expert"
            game="A Link to the Past"
            source="Advanced"
            updated="1w ago"
          />
          <ConfigCard
            name="Beginner Friendly"
            game="A Link to the Past"
            source="Pulse"
            updated="2w ago"
          />
        </div>
      </div>
    </div>
  );
}

function ActionButton({ icon, children, primary }: { icon: React.ReactNode; children: React.ReactNode; primary?: boolean }) {
  return (
    <button
      className={`
        px-6 py-3 border-2 font-medium transition-all flex items-center gap-2
        ${primary
          ? 'bg-[#4dffd2] border-[#4dffd2] text-[#0d1117] hover:shadow-[0_0_20px_rgba(77,255,210,0.3)]'
          : 'bg-[#161b22] border-[#30363d] text-[#e6edf3] hover:border-[#4dffd2]'
        }
      `}
    >
      {icon}
      <span>{children}</span>
    </button>
  );
}

function GameQuickCard({ name, platform, configs, disabled, onClick }: {
  name: string;
  platform: string;
  configs: number;
  disabled?: boolean;
  onClick?: () => void;
}) {
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      className={`
        w-full p-4 border-2 text-left transition-all
        ${disabled
          ? 'bg-[#161b22] border-[#30363d] opacity-50 cursor-not-allowed'
          : 'bg-[#161b22] border-[#30363d] hover:border-[#4dffd2] hover:bg-[#21262d]'
        }
      `}
    >
      <div className="flex items-start justify-between mb-2">
        <div>
          <h3 className="font-medium text-[#e6edf3]">{name}</h3>
          <p className="text-xs text-[#8b949e] mt-1">{platform}</p>
        </div>
        <div className="px-2 py-1 bg-[#21262d] border border-[#30363d] text-xs text-[#8b949e]">
          {configs} config{configs !== 1 ? 's' : ''}
        </div>
      </div>
      {!disabled && (
        <div className="flex items-center gap-2 text-sm text-[#4dffd2]">
          <Play className="w-4 h-4" />
          <span>Select</span>
        </div>
      )}
    </button>
  );
}

function LobbyCard({ name, game, players, status, time }: {
  name: string;
  game: string;
  players: number;
  status: string;
  time: string;
}) {
  return (
    <div className="p-4 border-2 border-[#30363d] bg-[#161b22]">
      <div className="flex items-start justify-between mb-2">
        <div>
          <h3 className="font-medium text-[#e6edf3]">{name}</h3>
          <p className="text-xs text-[#8b949e] mt-1">{game}</p>
        </div>
        <div className="px-2 py-1 bg-[#21262d] border border-[#30363d] text-xs text-[#8b949e]">
          {status}
        </div>
      </div>
      <div className="flex items-center gap-4 text-xs text-[#8b949e]">
        <div className="flex items-center gap-1">
          <Users className="w-3 h-3" />
          <span>{players} players</span>
        </div>
        <div className="flex items-center gap-1">
          <Clock className="w-3 h-3" />
          <span>{time}</span>
        </div>
      </div>
    </div>
  );
}

function ConfigCard({ name, game, source, updated }: {
  name: string;
  game: string;
  source: string;
  updated: string;
}) {
  return (
    <div className="p-4 border-2 border-[#30363d] bg-[#161b22] hover:border-[#4dffd2] transition-all cursor-pointer">
      <div className="mb-3">
        <h3 className="font-medium text-[#e6edf3] mb-1">{name}</h3>
        <p className="text-xs text-[#8b949e]">{game}</p>
      </div>
      <div className="flex items-center justify-between text-xs">
        <div className="px-2 py-1 bg-[#21262d] border border-[#30363d] text-[#4dffd2]">
          {source}
        </div>
        <span className="text-[#8b949e]">{updated}</span>
      </div>
    </div>
  );
}
