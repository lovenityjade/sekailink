import { useState, type ReactNode } from 'react';
import { Ban, ExternalLink, MessageCircle, Play, User, UserCircle, UserMinus, Users, Zap } from 'lucide-react';
import type { SocialRequest } from '../../services/socialClient';
import { EmoteText } from './EmoteText';

const formatMessageTime = (value: string) => {
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) return '';
  return new Date(timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
};

const profileName = (profile: SocialRequest | null) =>
  profile?.display_name || profile?.name || profile?.username || profile?.user_id || 'Player';

export function NavItem({ icon, label, active, onClick, disabled = false, badge }: {
  icon: ReactNode;
  label: string;
  active: boolean;
  onClick: () => void;
  disabled?: boolean;
  badge?: string;
}) {
  return (
    <button
      onClick={disabled ? undefined : onClick}
      disabled={disabled}
      title={disabled ? `${label} is in learning` : undefined}
      className={`
        w-full flex items-center gap-3 px-4 py-3 rounded-lg transition-all
        ${disabled ? 'cursor-not-allowed opacity-55' : ''}
        ${active
          ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white shadow-lg'
          : disabled
            ? 'text-[#8e8f94]'
            : 'text-[#8e8f94] hover:bg-[#1c1d22] hover:text-white'
        }
      `}
    >
      {icon}
      <span className="font-medium">{label}</span>
      {badge && <span className="ml-auto rounded-full border border-[#38f3dd]/25 bg-[#10201f] px-2 py-0.5 text-[10px] font-bold text-[#8cf5e8]">{badge}</span>}
    </button>
  );
}

export function ProfileAvatar({ name, avatarUrl, size = 'md' }: { name: string; avatarUrl?: string; size?: 'sm' | 'md' | 'lg' }) {
  const sizeClass = size === 'lg' ? 'w-12 h-12 text-lg' : size === 'sm' ? 'w-8 h-8 text-xs' : 'w-10 h-10 text-sm';
  const cleanUrl = String(avatarUrl || '').trim();
  return (
    <div className={`${sizeClass} rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center font-bold overflow-hidden`}>
      {cleanUrl ? (
        <img src={cleanUrl} alt={name} className="w-full h-full object-cover" />
      ) : (
        <span>{(name || 'S').charAt(0).toUpperCase()}</span>
      )}
    </div>
  );
}

export function StatsCard({ icon, label, value, change, color, onClick }: {
  icon: ReactNode;
  label: string;
  value: string;
  change: string;
  color: string;
  onClick?: () => void;
}) {
  return (
    <button
      onClick={onClick}
      disabled={!onClick}
      className="bg-gradient-card rounded-xl p-6 card-float card-float-hover text-left disabled:cursor-default"
    >
      <div className={`w-12 h-12 rounded-lg bg-gradient-to-br ${color} flex items-center justify-center mb-4 shadow-lg`}>
        <div className="text-white">{icon}</div>
      </div>
      <div className="text-3xl font-bold mb-1">{value}</div>
      <div className="text-sm text-white mb-1">{label}</div>
      <div className="text-xs text-[#8e8f94]">{change}</div>
    </button>
  );
}

export function GameCard({ title, platform, status, image, disabled }: {
  title: string;
  platform: string;
  status: string;
  image: string;
  disabled?: boolean;
}) {
  return (
    <div className={`
      bg-gradient-card rounded-xl overflow-hidden card-float transition-all
      ${disabled ? 'opacity-50 cursor-not-allowed' : 'card-float-hover cursor-pointer'}
    `}>
      <div className="h-48 bg-gradient-to-br from-[#2a2b30] to-[#1c1d22] flex items-center justify-center relative overflow-hidden">
        <div className="absolute inset-0 bg-gradient-to-t from-[#1c1d22] via-transparent to-transparent" />
        <div className="text-6xl font-bold text-[#2a2b30] z-10">{image}</div>
        {!disabled && (
          <div className="absolute top-4 right-4 px-3 py-1 bg-[#ff6b35] rounded-full text-xs font-medium shadow-lg">
            {platform}
          </div>
        )}
      </div>

      <div className="p-6">
        <h3 className="text-lg font-bold mb-2">{title}</h3>
        <p className="text-sm text-[#8e8f94] mb-4">{status}</p>
        {!disabled && (
          <button className="w-full py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors flex items-center justify-center gap-2">
            <Play className="w-4 h-4" />
            Select
          </button>
        )}
      </div>
    </div>
  );
}

export function FriendCard({ name, avatarUrl, status, lobbyId, online, inGame, unreadCount = 0, onClick, onViewProfile, onOpenPublicProfile, onRemove, onBlock }: {
  name: string;
  avatarUrl?: string;
  status: string;
  lobbyId?: string;
  online?: boolean;
  inGame?: boolean;
  unreadCount?: number;
  onClick?: () => void;
  onViewProfile?: () => void;
  onOpenPublicProfile?: () => void;
  onRemove?: () => void;
  onBlock?: () => void;
}) {
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number } | null>(null);
  const runAction = (action?: () => void) => {
    setContextMenu(null);
    action?.();
  };
  return (
    <>
    <div
      onClick={onClick}
      onContextMenu={(event) => {
        event.preventDefault();
        event.stopPropagation();
        setContextMenu({
          x: Math.min(event.clientX, window.innerWidth - 224),
          y: Math.min(event.clientY, window.innerHeight - 244),
        });
      }}
      className="p-3 rounded-lg bg-[#1c1d22] hover:bg-[#2a2b30] transition-colors cursor-pointer"
      title="Right-click for friend actions"
    >
      <div className="flex items-center gap-3">
        <div className="relative">
          <ProfileAvatar name={name} avatarUrl={avatarUrl} size="md" />
          {online && (
            <div className={`absolute -bottom-0.5 -right-0.5 w-3 h-3 rounded-full border-2 border-[#1c1d22] ${
              inGame ? 'bg-[#ff6b35]' : 'bg-[#4ecdc4]'
            }`} />
          )}
          {unreadCount > 0 && (
            <div className="absolute -top-1 -right-1 min-w-5 h-5 px-1 rounded-full bg-[#ff6b35] text-[10px] font-bold flex items-center justify-center">
              {unreadCount > 9 ? '9+' : unreadCount}
            </div>
          )}
        </div>
        <div className="flex-1 min-w-0">
          <div className="font-medium text-sm">{name}</div>
          <div className="text-xs text-[#8e8f94] truncate">
            {lobbyId ? `${status} • ${lobbyId}` : status}
          </div>
        </div>
      </div>
    </div>
    {contextMenu && (
      <>
        <button
          type="button"
          aria-label="Close friend menu"
          className="fixed inset-0 z-[70] cursor-default"
          onClick={() => setContextMenu(null)}
          onContextMenu={(event) => {
            event.preventDefault();
            setContextMenu(null);
          }}
        />
        <div
          className="fixed z-[71] w-52 overflow-hidden rounded-lg border border-[#34353b] bg-[#17191f] p-1.5 shadow-2xl"
          style={{ left: contextMenu.x, top: contextMenu.y }}
          role="menu"
        >
          <FriendContextAction icon={<UserCircle />} label="View profile" onClick={() => runAction(onViewProfile || onClick)} />
          <FriendContextAction icon={<MessageCircle />} label="Message" onClick={() => runAction(onClick)} />
          <FriendContextAction icon={<ExternalLink />} label="Open public profile" onClick={() => runAction(onOpenPublicProfile)} />
          <div className="my-1.5 border-t border-[#34353b]" />
          <FriendContextAction icon={<UserMinus />} label="Remove friend" danger onClick={() => runAction(onRemove)} />
          <FriendContextAction icon={<Ban />} label="Block" danger onClick={() => runAction(onBlock)} />
        </div>
      </>
    )}
    </>
  );
}

function FriendContextAction({ icon, label, danger = false, onClick }: {
  icon: ReactNode;
  label: string;
  danger?: boolean;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      role="menuitem"
      onClick={onClick}
      className={`flex w-full items-center gap-3 rounded-md px-3 py-2.5 text-left text-sm transition-colors ${
        danger ? 'text-[#f38181] hover:bg-[#f85149]/10' : 'text-[#d7dde5] hover:bg-[#2a2b30]'
      }`}
    >
      <span className="[&>svg]:h-4 [&>svg]:w-4">{icon}</span>
      <span>{label}</span>
    </button>
  );
}

export function NotificationItem({ title, message, time, index, onClick }: {
  title: string;
  message: string;
  time: string;
  index: number;
  onClick?: () => void;
}) {
  const gradients = [
    'from-[#ff6b35]/10 to-transparent',
    'from-[#4ecdc4]/10 to-transparent',
    'from-[#aa96da]/10 to-transparent',
    'from-[#f38181]/10 to-transparent',
    'from-[#95e1d3]/10 to-transparent',
  ];
  const accentColors = [
    'bg-[#ff6b35]',
    'bg-[#4ecdc4]',
    'bg-[#aa96da]',
    'bg-[#f38181]',
    'bg-[#95e1d3]',
  ];
  const gradient = gradients[index % gradients.length];
  const accent = accentColors[index % accentColors.length];

  return (
    <button
      onClick={onClick}
      className={`w-full text-left p-4 border-l-2 ${accent.replace('bg-', 'border-')} bg-gradient-to-r ${gradient} hover:bg-[#2a2b30]/30 transition-colors cursor-pointer relative overflow-hidden`}
    >
      <div className={`absolute left-0 top-0 bottom-0 w-1 ${accent}`} />
      <div className="ml-2">
        <div className="flex items-start justify-between gap-2 mb-1">
          <h4 className="font-semibold text-sm">{title}</h4>
          <span className="text-[10px] text-[#8e8f94] whitespace-nowrap">{time}</span>
        </div>
        <p className="text-xs text-[#8e8f94] leading-relaxed">{message}</p>
      </div>
    </button>
  );
}

export function FriendRequestNotification({ request, index, busy, onAccept, onDecline }: {
  request: SocialRequest;
  index: number;
  busy?: boolean;
  onAccept: () => void;
  onDecline: () => void;
}) {
  return (
    <div className="p-4 border-l-2 border-[#4ecdc4] bg-gradient-to-r from-[#4ecdc4]/10 to-transparent relative overflow-hidden">
      <div className="absolute left-0 top-0 bottom-0 w-1 bg-[#4ecdc4]" />
      <div className="ml-2">
        <div className="flex items-start justify-between gap-2 mb-1">
          <h4 className="font-semibold text-sm">Friend Request</h4>
          <span className="text-[10px] text-[#8e8f94] whitespace-nowrap">
            {request.created_at ? formatMessageTime(request.created_at) : `#${index + 1}`}
          </span>
        </div>
        <p className="text-xs text-[#8e8f94] leading-relaxed mb-3">
          {request.from_name || profileName(request)} wants to connect on Nexus.
        </p>
        <div className="flex gap-2">
          <button
            onClick={onAccept}
            disabled={busy}
            className="flex-1 py-1.5 rounded-lg bg-[#4ecdc4] text-[#14151a] text-xs font-bold disabled:opacity-50"
          >
            Accept
          </button>
          <button
            onClick={onDecline}
            disabled={busy}
            className="flex-1 py-1.5 rounded-lg bg-[#2a2b30] text-xs font-bold disabled:opacity-50"
          >
            Decline
          </button>
        </div>
      </div>
    </div>
  );
}

export function LobbyCard({ name, host, seedsInSync, players, maxPlayers, status, joining, onJoin }: {
  name: string;
  host: string;
  seedsInSync: number;
  players: number;
  maxPlayers: number;
  status: 'waiting' | 'in-progress';
  joining?: boolean;
  onJoin?: () => void;
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
    <div className={`min-h-[164px] p-5 bg-gradient-to-r ${statusConfig.gradient} border-2 border-[#2a2b30] rounded-lg hover:border-[#ff6b35] transition-all cursor-pointer card-float-hover flex flex-col`}>
      <div className="flex items-start justify-between gap-3 mb-4">
        <div className="flex-1 min-w-0">
          <div className="grid grid-cols-[minmax(0,1fr)_auto] items-start gap-2 mb-3">
            <h3 className="text-lg font-bold leading-snug line-clamp-2 min-h-[2.75rem]">{name}</h3>
            <span className={`${statusConfig.bg} text-[#14151a] px-3 py-1 rounded-full text-xs font-bold whitespace-nowrap`}>
              {statusConfig.text}
            </span>
          </div>
          <div className="grid grid-cols-2 gap-3 text-sm text-[#8e8f94]">
            <div className="flex items-center gap-1.5 min-w-0">
              <User className="w-4 h-4 shrink-0" />
              <span className="truncate">Host: {host}</span>
            </div>
            <div className="flex items-center gap-1.5 min-w-0">
              <Zap className="w-4 h-4 shrink-0" />
              <span className="truncate">Seeds in Sync: {seedsInSync}</span>
            </div>
          </div>
        </div>
        <button
          onClick={onJoin}
          disabled={joining}
          className="shrink-0 px-5 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors disabled:opacity-50"
        >
          {joining ? 'Joining...' : 'Join'}
        </button>
      </div>

      <div className="flex items-center gap-3 mt-auto">
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

export function ChatMessage({ message, time, sent, lobbyLinks, onLobbyLinkClick }: {
  message: string;
  time: string;
  sent: boolean;
  lobbyLinks?: Array<{ id: string; name: string }>;
  onLobbyLinkClick?: (lobby: { id: string; name: string }) => void;
}) {
  return (
    <div className={`flex ${sent ? 'justify-end' : 'justify-start'}`}>
      <div className={`max-w-[75%] ${sent ? 'order-1' : 'order-2'}`}>
        <div
          className={`px-4 py-3 rounded-2xl shadow-lg ${
            sent
              ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white rounded-br-sm'
              : 'bg-[#1c1d22] text-white rounded-bl-sm'
          }`}
        >
          <p className="text-sm leading-relaxed">
            <EmoteText text={message} lobbyLinks={lobbyLinks || []} onLobbyLinkClick={onLobbyLinkClick} />
          </p>
        </div>
        <div className={`mt-1 px-2 text-[10px] text-[#8e8f94] ${sent ? 'text-right' : 'text-left'}`}>
          {time}
        </div>
      </div>
    </div>
  );
}

export function StatusMenuItem({ label, color, isActive, onClick }: {
  status: string;
  label: string;
  color: string;
  isActive: boolean;
  onClick: () => void;
}) {
  const gradients = {
    'bg-[#4ecdc4]': 'from-[#4ecdc4]/10 to-transparent',
    'bg-[#ff6b35]': 'from-[#ff6b35]/10 to-transparent',
    'bg-[#f69d50]': 'from-[#f69d50]/10 to-transparent',
    'bg-[#8e8f94]': 'from-[#8e8f94]/10 to-transparent',
  };

  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-lg transition-all ${
        isActive
          ? `bg-gradient-to-r ${gradients[color as keyof typeof gradients]} border border-[#2a2b30]`
          : 'hover:bg-[#2a2b30]'
      }`}
    >
      <div className={`w-3 h-3 rounded-full ${color}`} />
      <span className="text-sm font-medium text-white">{label}</span>
      {isActive && (
        <div className="ml-auto w-1.5 h-1.5 rounded-full bg-white" />
      )}
    </button>
  );
}
