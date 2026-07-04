import { useState } from 'react';
import { Send, Crown, Check, X, Loader, Settings } from 'lucide-react';

export default function LobbyPage() {
  const [message, setMessage] = useState('');
  const [lobbyStatus, setLobbyStatus] = useState<'waiting' | 'ready' | 'generating'>('waiting');

  return (
    <div className="h-full flex flex-col">
      {/* Lobby Header */}
      <div className="border-b-2 border-[#30363d] bg-[#161b22] px-8 py-6">
        <div className="flex items-start justify-between">
          <div>
            <div className="flex items-center gap-3 mb-2">
              <h1 className="text-2xl font-semibold text-[#e6edf3]">Weekend Randomizer</h1>
              <StatusBadge status={lobbyStatus} />
            </div>
            <p className="text-sm text-[#8b949e]">
              Casual ALTTP run with entrance shuffle • Hosted by Jade
            </p>
          </div>
          <div className="flex gap-3">
            {lobbyStatus === 'ready' && (
              <button
                onClick={() => setLobbyStatus('generating')}
                className="px-6 py-3 bg-[#4dffd2] border-2 border-[#4dffd2] text-[#0d1117] font-medium hover:shadow-[0_0_20px_rgba(77,255,210,0.3)] transition-all"
              >
                Generate & Launch
              </button>
            )}
            <button className="px-4 py-3 bg-[#161b22] border-2 border-[#30363d] text-[#e6edf3] hover:border-[#4dffd2] transition-all">
              <Settings className="w-5 h-5" />
            </button>
          </div>
        </div>
      </div>

      <div className="flex-1 flex overflow-hidden">
        {/* Main Lobby Area */}
        <div className="flex-1 flex flex-col">
          {/* Chat */}
          <div className="flex-1 overflow-auto p-6 space-y-3">
            <SystemMessage>Lobby created by Jade</SystemMessage>
            <ChatMessage user="Jade" time="10:42" userColor="#4dffd2">
              Hey everyone! Standard settings with entrance shuffle enabled
            </ChatMessage>
            <ChatMessage user="Raifu" time="10:43" userColor="#bc8cff">
              Sounds good! Ready when you are
            </ChatMessage>
            <ItemMessage
              from="Jade"
              fromGame="A Link to the Past {1}"
              to="Raifu"
              toGame="Twilight Princess"
              item="Boomerang"
            />
            <ChatMessage user="Phoenix" time="10:45" userColor="#58a6ff">
              Just need a minute to check my config
            </ChatMessage>
            <SystemMessage>Phoenix is now ready</SystemMessage>
          </div>

          {/* Chat Input */}
          <div className="border-t-2 border-[#30363d] bg-[#161b22] p-4">
            <div className="flex gap-3">
              <input
                type="text"
                value={message}
                onChange={(e) => setMessage(e.target.value)}
                placeholder="Type a message..."
                className="flex-1 px-4 py-3 bg-[#0d1117] border-2 border-[#30363d] text-[#e6edf3] placeholder:text-[#8b949e] focus:border-[#4dffd2] outline-none transition-colors"
              />
              <button className="px-6 py-3 bg-[#4dffd2] border-2 border-[#4dffd2] text-[#0d1117] hover:shadow-[0_0_20px_rgba(77,255,210,0.3)] transition-all">
                <Send className="w-5 h-5" />
              </button>
            </div>
          </div>
        </div>

        {/* Player List Sidebar */}
        <aside className="w-80 border-l-2 border-[#30363d] bg-[#161b22] flex flex-col">
          <div className="border-b-2 border-[#30363d] p-4">
            <h3 className="text-sm font-semibold text-[#e6edf3]">PLAYERS (3/4)</h3>
          </div>

          <div className="flex-1 overflow-auto p-4 space-y-3">
            <PlayerCard
              name="Jade"
              game="A Link to the Past"
              config="Standard Run"
              isHost
              isReady
              color="#4dffd2"
            />
            <PlayerCard
              name="Raifu"
              game="Twilight Princess"
              config="Hero Mode"
              isReady
              color="#bc8cff"
            />
            <PlayerCard
              name="Phoenix"
              game="A Link to the Past"
              config="Hard Mode Expert"
              isReady
              color="#58a6ff"
            />
          </div>

          <div className="border-t-2 border-[#30363d] p-4">
            <div className="text-xs text-[#8b949e] space-y-2">
              <div className="flex justify-between">
                <span>Ready</span>
                <span className="text-[#4dffd2]">3/3</span>
              </div>
              <div className="flex justify-between">
                <span>Total Slots</span>
                <span>3/4</span>
              </div>
            </div>
          </div>
        </aside>
      </div>
    </div>
  );
}

function StatusBadge({ status }: { status: string }) {
  const config = {
    waiting: { bg: 'bg-[#f69d50]', text: 'Waiting', icon: null },
    ready: { bg: 'bg-[#4dffd2]', text: 'Ready', icon: <Check className="w-3 h-3" /> },
    generating: { bg: 'bg-[#58a6ff]', text: 'Generating...', icon: <Loader className="w-3 h-3 animate-spin" /> }
  }[status] || { bg: 'bg-[#8b949e]', text: status, icon: null };

  return (
    <div className={`${config.bg} text-[#0d1117] px-3 py-1 text-xs font-semibold flex items-center gap-1.5`}>
      {config.icon}
      <span>{config.text}</span>
    </div>
  );
}

function PlayerCard({ name, game, config, isHost, isReady, color }: {
  name: string;
  game: string;
  config: string;
  isHost?: boolean;
  isReady?: boolean;
  color: string;
}) {
  return (
    <div className="p-4 border-2 border-[#30363d] bg-[#0d1117]">
      <div className="flex items-start justify-between mb-3">
        <div className="flex items-center gap-2">
          <div
            className="w-10 h-10 clip-hex flex items-center justify-center text-white font-bold"
            style={{ backgroundColor: color }}
          >
            {name.charAt(0)}
          </div>
          <div>
            <div className="flex items-center gap-2">
              <span className="font-medium text-[#e6edf3]">{name}</span>
              {isHost && <Crown className="w-3 h-3 text-[#f69d50]" />}
            </div>
            <div className="text-xs text-[#8b949e]">{game}</div>
          </div>
        </div>
        {isReady ? (
          <div className="w-6 h-6 bg-[#4dffd2] flex items-center justify-center">
            <Check className="w-4 h-4 text-[#0d1117]" />
          </div>
        ) : (
          <div className="w-6 h-6 bg-[#8b949e] flex items-center justify-center">
            <X className="w-4 h-4 text-[#0d1117]" />
          </div>
        )}
      </div>
      <div className="text-xs text-[#8b949e] border-t border-[#30363d] pt-2 mt-2">
        Config: <span className="text-[#4dffd2]">{config}</span>
      </div>
    </div>
  );
}

function ChatMessage({ user, time, userColor, children }: {
  user: string;
  time: string;
  userColor: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex gap-3">
      <div
        className="w-8 h-8 clip-hex flex items-center justify-center text-white text-sm font-bold flex-shrink-0"
        style={{ backgroundColor: userColor }}
      >
        {user.charAt(0)}
      </div>
      <div className="flex-1">
        <div className="flex items-baseline gap-2 mb-1">
          <span className="font-medium text-[#e6edf3]" style={{ color: userColor }}>{user}</span>
          <span className="text-xs text-[#8b949e]">{time}</span>
        </div>
        <p className="text-sm text-[#e6edf3]">{children}</p>
      </div>
    </div>
  );
}

function SystemMessage({ children }: { children: React.ReactNode }) {
  return (
    <div className="flex items-center gap-2 text-xs text-[#8b949e] py-1">
      <div className="flex-1 h-px bg-[#30363d]" />
      <span>{children}</span>
      <div className="flex-1 h-px bg-[#30363d]" />
    </div>
  );
}

function ItemMessage({ from, fromGame, to, toGame, item }: {
  from: string;
  fromGame: string;
  to: string;
  toGame: string;
  item: string;
}) {
  return (
    <div className="p-3 border-l-2 border-[#4dffd2] bg-[#4dffd2]/5 text-sm">
      <span className="text-[#4dffd2] font-medium">{from}</span>
      <span className="text-[#8b949e]"> ({fromGame}) sends </span>
      <span className="text-[#e6edf3] font-medium">{item}</span>
      <span className="text-[#8b949e]"> to </span>
      <span className="text-[#bc8cff] font-medium">{to}</span>
      <span className="text-[#8b949e]"> ({toGame})</span>
    </div>
  );
}
