import {
  Circle,
  Ban,
  Hash,
  MessageCircle,
  MoreHorizontal,
  Search,
  Send,
  Shield,
  Smile,
  UserCircle,
  UserMinus,
  UserPlus,
  Users,
  Volume2,
  VolumeX,
  X,
} from 'lucide-react';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { chatChannelId, chatService, type ChatChannelRef, type SekaiChatMessage, type SekaiChatPresenceUser } from '../../services/chatService';
import type { CurrentUser } from '../../services/api';
import { listLobbies, type LobbySummary } from '../../services/lobbyClient';
import { blockSocialUser, removeSocialFriend, sendSocialFriendRequest, type SocialProfile } from '../../services/socialClient';
import { trace, traceError } from '../../services/trace';
import { appendEmote, EmoteText, STANDARD_EMOTES, type LobbyTextLink } from './EmoteText';

type ChannelOption = {
  id: string;
  label: string;
  description: string;
  channel: ChatChannelRef;
  accent: string;
};

type Member = {
  user_id: string;
  name: string;
  username?: string;
  avatar_url?: string;
  status?: string;
  role?: string;
  permissions?: string[];
  patreon_tier?: string;
  patreon_is_supporter?: boolean;
  irc_op?: boolean;
  irc_voice?: boolean;
  source: 'presence' | 'friend' | 'self';
};

type ContextMenuState = {
  member: Member;
  x: number;
  y: number;
} | null;

interface ChatPageProps {
  currentUser?: CurrentUser | null;
  friends?: SocialProfile[];
  onOpenLobby?: (lobbyId: string, lobbyName?: string) => void;
}

const CHANNELS: ChannelOption[] = [
  {
    id: 'welcome',
    label: '# welcome',
    description: 'MOTD, annonces courtes et repères communautaires',
    channel: { kind: 'global', locale: 'welcome' },
    accent: '#ff6b35',
  },
  {
    id: 'global-fr',
    label: '# global-fr',
    description: 'Salon principal francophone',
    channel: { kind: 'global', locale: 'fr' },
    accent: '#4ecdc4',
  },
  {
    id: 'global-en',
    label: '# global-en',
    description: 'English community channel',
    channel: { kind: 'global', locale: 'en' },
    accent: '#aa96da',
  },
  {
    id: 'support',
    label: '# support',
    description: 'Aide rapide et diagnostics',
    channel: { kind: 'global', locale: 'support' },
    accent: '#ff6b35',
  },
  {
    id: 'event',
    label: '# event',
    description: 'Stream, races, showcase et événements spéciaux',
    channel: { kind: 'global', locale: 'event' },
    accent: '#f69d50',
  },
];

const WELCOME_MOTD: SekaiChatMessage[] = [
  {
    id: 'welcome-motd-1',
    author: 'SekaiLink',
    content: 'Bienvenue dans SekaiLink. Ce salon affiche les annonces importantes, les règles de base et les liens utiles pour commencer.',
    created_at: new Date(0).toISOString(),
    kind: 'system',
    channel: 'global:welcome',
  },
  {
    id: 'welcome-motd-2',
    author: 'SekaiLink',
    content: 'Respecte les autres joueurs, évite les spoilers sans consentement, et utilise #support si tu as besoin d’aide avec une seed, un tracker ou un lobby.',
    created_at: new Date(0).toISOString(),
    kind: 'system',
    channel: 'global:welcome',
  },
];

const memberName = (member: Member) =>
  member.name || member.username || member.user_id || 'SekaiLink';

const profileName = (profile: SocialProfile) =>
  profile.display_name || profile.name || profile.username || profile.user_id || 'Player';

const currentUserName = (user?: CurrentUser | null) =>
  user?.display_name || user?.username || user?.user_id || 'You';

const currentUserNames = (user?: CurrentUser | null) =>
  new Set([user?.display_name, user?.username, user?.global_name, user?.user_id, user?.discord_id, 'You']
    .filter(Boolean)
    .map((value) => String(value).toLowerCase()));

const messageTime = (value: string) => {
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) return '';
  return new Date(timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
};

const initial = (name: string) => (name || 'S').trim().charAt(0).toUpperCase();

const normalizeStatus = (status?: string) => {
  const clean = String(status || '').toLowerCase();
  if (clean === 'dnd' || clean === 'busy') return 'busy';
  if (clean === 'afk' || clean === 'away') return 'away';
  if (clean === 'offline') return 'offline';
  return 'online';
};

const statusColor = (status?: string) => {
  const normalized = normalizeStatus(status);
  if (normalized === 'busy') return 'text-[#ff6b35]';
  if (normalized === 'away') return 'text-[#f69d50]';
  if (normalized === 'offline') return 'text-[#8e8f94]';
  return 'text-[#4ecdc4]';
};

const isAdminOrModerator = (value?: string, permissions: string[] = []) => {
  const clean = String(value || '').toLowerCase();
  if (/(admin|moderator|modo|mod|service|staff)/.test(clean)) return true;
  return permissions.some((permission) => /(admin|moderator|mod|service|staff)/i.test(permission));
};

const isPatreonVip = (tier?: string, supporter?: boolean) => {
  if (!supporter && !tier) return false;
  const clean = String(tier || '').toLowerCase();
  if (/vip|tier\s*[123]|tier[ _-]*[123]|t[123]\b/.test(clean)) return true;
  return Boolean(supporter);
};

const memberIsOp = (member: Member) =>
  Boolean(member.irc_op) || isAdminOrModerator(member.role, member.permissions);

const memberIsVip = (member: Member) =>
  !memberIsOp(member) && (Boolean(member.irc_voice) || isPatreonVip(member.patreon_tier, member.patreon_is_supporter));

const memberBadge = (member: Member) => {
  if (memberIsOp(member)) return { label: 'OP', className: 'border-[#f69d50]/40 bg-[#f69d50]/15 text-[#f69d50]' };
  if (memberIsVip(member)) return { label: 'VIP', className: 'border-[#aa96da]/40 bg-[#aa96da]/15 text-[#d9d2ff]' };
  return null;
};

const roleForPresence = (user?: CurrentUser | null) => {
  const permissions = Array.isArray(user?.permissions) ? user?.permissions || [] : [];
  if (isAdminOrModerator(user?.role, permissions)) return 'op';
  const raw = user as any;
  if (isPatreonVip(raw?.patreon_tier, raw?.patreon_is_supporter)) return 'vip';
  return 'client-core';
};

const emptyMessagesForChannel = (channel: ChannelOption) =>
  channel.id === 'welcome' ? WELCOME_MOTD : [];

const isWaitingLobby = (lobby: LobbySummary) => {
  if (lobby.is_locked) return false;
  const status = String((lobby as any).status || (lobby as any).state || '').toLowerCase();
  return !status || status === 'waiting' || status === 'open' || status === 'created';
};

export default function ChatPage({ currentUser, friends = [], onOpenLobby }: ChatPageProps) {
  const [selectedChannelId, setSelectedChannelId] = useState(CHANNELS[0].id);
  const [messages, setMessages] = useState<SekaiChatMessage[]>([]);
  const [presence, setPresence] = useState<SekaiChatPresenceUser[]>([]);
  const [draft, setDraft] = useState('');
  const [loading, setLoading] = useState(true);
  const [sending, setSending] = useState(false);
  const [error, setError] = useState('');
  const [composerNotice, setComposerNotice] = useState('');
  const [canTalk, setCanTalk] = useState(true);
  const [showEmotes, setShowEmotes] = useState(false);
  const [showLobbyPicker, setShowLobbyPicker] = useState(false);
  const [lobbyPickerLoading, setLobbyPickerLoading] = useState(false);
  const [linkableLobbies, setLinkableLobbies] = useState<LobbySummary[]>([]);
  const [linkBracketStart, setLinkBracketStart] = useState<number | null>(null);
  const [memberQuery, setMemberQuery] = useState('');
  const [mutedUsers, setMutedUsers] = useState<Set<string>>(() => new Set());
  const [contextMenu, setContextMenu] = useState<ContextMenuState>(null);
  const [profileModal, setProfileModal] = useState<Member | null>(null);
  const composerRef = useRef<HTMLTextAreaElement | null>(null);
  const scrollAnchorRef = useRef<HTMLDivElement | null>(null);

  const selectedChannel = useMemo(
    () => CHANNELS.find((channel) => channel.id === selectedChannelId) || CHANNELS[0],
    [selectedChannelId]
  );

  const knownMembersById = useMemo(() => {
    const byId = new Map<string, Member>();
    const rawUser = currentUser as any;
    const selfId = currentUser?.user_id || currentUser?.discord_id || currentUser?.username || 'self';
    byId.set(String(selfId), {
      user_id: String(selfId),
      name: currentUserName(currentUser),
      username: currentUser?.username,
      avatar_url: currentUser?.avatar_url || '',
      status: 'online',
      role: currentUser?.role || '',
      permissions: currentUser?.permissions || [],
      patreon_tier: rawUser?.patreon_tier || '',
      patreon_is_supporter: Boolean(rawUser?.patreon_is_supporter),
      irc_op: isAdminOrModerator(currentUser?.role, currentUser?.permissions || []),
      irc_voice: isPatreonVip(rawUser?.patreon_tier, rawUser?.patreon_is_supporter),
      source: 'self',
    });
    for (const friend of friends) {
      if (!friend.user_id) continue;
      byId.set(friend.user_id, {
        user_id: friend.user_id,
        name: profileName(friend),
        username: friend.username,
        avatar_url: friend.avatar_url,
        status: friend.presence_status || friend.status,
        role: friend.role || '',
        permissions: friend.permissions || [],
        patreon_tier: friend.patreon_tier || '',
        patreon_is_supporter: Boolean(friend.patreon_is_supporter),
        irc_op: isAdminOrModerator(friend.role, friend.permissions || []),
        irc_voice: isPatreonVip(friend.patreon_tier, friend.patreon_is_supporter),
        source: 'friend' as const,
      });
    }
    return byId;
  }, [currentUser, friends]);

  const members = useMemo<Member[]>(() => {
    const byId = new Map<string, Member>();
    for (const user of presence) {
      const rawUser = user as any;
      const known = knownMembersById.get(user.user_id);
      const status = user.status || known?.status || 'online';
      if (normalizeStatus(status) === 'offline') continue;
      const member: Member = {
        user_id: user.user_id,
        name: user.display_name || user.name || user.username || known?.name || user.user_id,
        username: user.username || known?.username,
        avatar_url: user.avatar_url || known?.avatar_url,
        status,
        role: user.role || known?.role,
        permissions: Array.isArray(rawUser?.permissions) ? rawUser.permissions.map((entry: unknown) => String(entry)) : known?.permissions || [],
        patreon_tier: typeof rawUser?.patreon_tier === 'string' ? rawUser.patreon_tier : known?.patreon_tier || '',
        patreon_is_supporter: Boolean(rawUser?.patreon_is_supporter || known?.patreon_is_supporter),
        irc_op: Boolean(user.irc_op || known?.irc_op),
        irc_voice: Boolean(user.irc_voice || known?.irc_voice),
        source: known?.source === 'self' ? 'self' : 'presence',
      };
      byId.set(member.user_id, member);
    }
    const query = memberQuery.trim().toLowerCase();
    return Array.from(byId.values())
      .filter((member) => !query || [member.name, member.username, member.user_id].join(' ').toLowerCase().includes(query))
      .sort((a, b) => {
        if (memberIsOp(a) !== memberIsOp(b)) return memberIsOp(a) ? -1 : 1;
        if (memberIsVip(a) !== memberIsVip(b)) return memberIsVip(a) ? -1 : 1;
        if (a.source !== b.source && a.source === 'self') return -1;
        if (a.source !== b.source && b.source === 'self') return 1;
        return memberName(a).localeCompare(memberName(b));
      });
  }, [knownMembersById, memberQuery, presence]);

  const visibleMessages = useMemo(() => {
    if (!mutedUsers.size) return messages;
    const mutedNames = new Set(
      members
        .filter((member) => mutedUsers.has(member.user_id))
        .flatMap((member) => [member.name, member.username, member.user_id].filter(Boolean).map((value) => String(value).toLowerCase()))
    );
    return messages.filter((message) => !mutedNames.has(String(message.author || '').toLowerCase()));
  }, [members, messages, mutedUsers]);

  const friendIds = useMemo(() => new Set(friends.map((friend) => friend.user_id).filter(Boolean)), [friends]);
  const ownNames = useMemo(() => currentUserNames(currentUser), [currentUser]);
  const lobbyLinks = useMemo<LobbyTextLink[]>(
    () => linkableLobbies.filter(isWaitingLobby).map((lobby) => ({ id: lobby.id, name: lobby.name || lobby.id })),
    [linkableLobbies]
  );

  const loadChannel = useCallback(async (channel: ChannelOption, background = false) => {
    if (!background) setLoading(true);
    setError('');
    setComposerNotice('');
    setCanTalk(true);
    setShowEmotes(false);
    setShowLobbyPicker(false);
    trace('chat-page', 'load_start', { channel: channel.id });
    try {
      await chatService.touchPresence(channel.channel, { role: roleForPresence(currentUser) }).catch((err) => {
        traceError('chat-page', 'presence_touch_failed', err, { channel: channel.id });
      });
      const [nextMessages, nextPresence] = await Promise.all([
        chatService.listMessages(channel.channel).catch((err) => {
          traceError('chat-page', 'messages_load_failed', err, { channel: channel.id });
          if (!background) setError(err instanceof Error ? err.message : 'Chat gateway unavailable.');
          return emptyMessagesForChannel(channel);
        }),
        chatService.listPresence(channel.channel).catch((err) => {
          traceError('chat-page', 'presence_load_failed', err, { channel: channel.id });
          return [];
        }),
      ]);
      setMessages(nextMessages.length ? nextMessages : emptyMessagesForChannel(channel));
      setPresence(nextPresence);
      trace('chat-page', 'load_success', { channel: channel.id, messages: nextMessages.length, presence: nextPresence.length });
    } finally {
      if (!background) setLoading(false);
    }
  }, [currentUser]);

  useEffect(() => {
    void loadChannel(selectedChannel);
    const timer = window.setInterval(() => void loadChannel(selectedChannel, true), 8000);
    return () => window.clearInterval(timer);
  }, [loadChannel, selectedChannel]);

  useEffect(() => {
    scrollAnchorRef.current?.scrollIntoView({ block: 'end' });
  }, [visibleMessages, selectedChannelId, loading]);

  useEffect(() => {
    const close = () => setContextMenu(null);
    window.addEventListener('click', close);
    window.addEventListener('keydown', close);
    return () => {
      window.removeEventListener('click', close);
      window.removeEventListener('keydown', close);
    };
  }, []);

  const loadLobbyPicker = useCallback(async () => {
    setLobbyPickerLoading(true);
    try {
      const lobbies = await listLobbies();
      setLinkableLobbies(lobbies.filter(isWaitingLobby));
    } catch (err) {
      traceError('chat-page', 'lobby_picker_load_failed', err);
      setLinkableLobbies([]);
    } finally {
      setLobbyPickerLoading(false);
    }
  }, []);

  useEffect(() => {
    void loadLobbyPicker();
  }, [loadLobbyPicker]);

  const insertLobbyLinkShell = () => {
    const textarea = composerRef.current;
    if (!textarea) return;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const before = draft.slice(0, start);
    const selected = draft.slice(start, end);
    const after = draft.slice(end);
    const next = `${before}[${selected}]${after}`;
    setDraft(next);
    setLinkBracketStart(start);
    setShowLobbyPicker(true);
    setShowEmotes(false);
    void loadLobbyPicker();
    window.requestAnimationFrame(() => {
      textarea.focus();
      textarea.setSelectionRange(start + 1, start + 1 + selected.length);
    });
  };

  const chooseLobbyLink = (lobby: LobbySummary) => {
    const textarea = composerRef.current;
    const name = lobby.name || lobby.id;
    const start = linkBracketStart ?? textarea?.selectionStart ?? draft.length;
    const closing = draft.indexOf(']', start);
    const end = closing >= start ? closing + 1 : start;
    const next = `${draft.slice(0, start)}[${name}]${draft.slice(end)}`;
    const cursor = start + name.length + 2;
    setDraft(next);
    setShowLobbyPicker(false);
    setLinkBracketStart(null);
    window.requestAnimationFrame(() => {
      textarea?.focus();
      textarea?.setSelectionRange(cursor, cursor);
    });
  };

  const sendMessage = async () => {
    const text = draft.trim();
    if (!text || sending) return;
    if (!canTalk) {
      setComposerNotice('You are not authorized to talk in that channel.');
      return;
    }
    setSending(true);
    setDraft('');
    setError('');
    const optimistic: SekaiChatMessage = {
      id: `local-${Date.now()}`,
      author: currentUserName(currentUser),
      content: text,
      created_at: new Date().toISOString(),
      kind: 'user',
      channel: chatChannelId(selectedChannel.channel),
      avatar_url: currentUser?.avatar_url || '',
    };
    setMessages((prev) => [...prev, optimistic]);
    try {
      const saved = await chatService.sendMessage(selectedChannel.channel, text);
      setMessages((prev) => prev.map((message) => (message.id === optimistic.id ? saved : message)));
    } catch (err) {
      traceError('chat-page', 'send_failed', err, { channel: selectedChannel.id });
      const message = err instanceof Error ? err.message : 'Unable to send message.';
      if (/channel_forbidden|forbidden|not authorized|unauthorized/i.test(message)) {
        setCanTalk(false);
        setComposerNotice('You are not authorized to talk in that channel.');
        setMessages((prev) => prev.filter((entry) => entry.id !== optimistic.id));
      } else {
        setError(message);
        setMessages((prev) => prev.map((entry) => (
          entry.id === optimistic.id ? { ...entry, kind: 'system', author: 'System', content: `Message non envoyé: ${text}` } : entry
        )));
      }
    } finally {
      setSending(false);
    }
  };

  const toggleMute = (member: Member) => {
    setMutedUsers((current) => {
      const next = new Set(current);
      if (next.has(member.user_id)) next.delete(member.user_id);
      else next.add(member.user_id);
      return next;
    });
    setContextMenu(null);
  };

  const runMemberAction = async (member: Member, action: 'add' | 'remove' | 'block') => {
    if (!member.user_id || member.source === 'self') return;
    const label = memberName(member);
    trace('chat-page', 'member_action_start', { action, userId: member.user_id });
    try {
      if (action === 'add') await sendSocialFriendRequest(member.user_id);
      if (action === 'remove') await removeSocialFriend(member.user_id);
      if (action === 'block') await blockSocialUser(member.user_id);
      setError('');
      trace('chat-page', 'member_action_success', { action, userId: member.user_id });
      if (action === 'add') setProfileModal({ ...member, source: 'friend' });
    } catch (err) {
      traceError('chat-page', 'member_action_failed', err, { action, userId: member.user_id });
      setError(err instanceof Error ? err.message : `Action impossible pour ${label}.`);
    } finally {
      setContextMenu(null);
    }
  };

  return (
    <div className="h-full p-8 flex flex-col gap-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold mb-1">Chat</h1>
          <p className="text-[#8e8f94]">Salons communautaires SekaiLink, style IRC moderne.</p>
        </div>
        <div className="flex items-center gap-3 rounded-xl border border-[#2a2b30] bg-[#0e0f13]/80 px-4 py-3">
          <MessageCircle className="w-5 h-5 text-[#4ecdc4]" />
          <span className="text-sm text-[#c9c9d1]">{members.length} noms visibles</span>
        </div>
      </div>

      <div className="min-h-0 flex-1 grid grid-cols-[240px_minmax(0,1fr)_260px] overflow-hidden rounded-xl border border-[#2a2b30] bg-[#0e0f13]/90 shadow-2xl">
        <aside className="border-r border-[#2a2b30] bg-[#101116]/95 p-4 flex flex-col">
          <div className="mb-4 flex items-center gap-2 text-sm font-bold text-white">
            <Hash className="w-4 h-4 text-[#4ecdc4]" />
            Salons
          </div>
          <div className="space-y-2">
            {CHANNELS.map((channel) => (
              <button
                key={channel.id}
                onClick={() => setSelectedChannelId(channel.id)}
                className={`w-full rounded-lg border p-3 text-left transition-colors ${
                  selectedChannelId === channel.id
                    ? 'border-[#4ecdc4]/60 bg-[#4ecdc4]/10'
                    : 'border-transparent bg-[#1c1d22]/70 hover:border-[#2a2b30] hover:bg-[#24262d]'
                }`}
              >
                <div className="flex items-center justify-between gap-3">
                  <span className="font-bold text-sm">{channel.label}</span>
                  <span className="w-2 h-2 rounded-full" style={{ backgroundColor: channel.accent }} />
                </div>
                <div className="mt-1 text-xs text-[#8e8f94]">{channel.description}</div>
              </button>
            ))}
          </div>

          <div className="mt-auto rounded-lg border border-[#2a2b30] bg-[#14151a] p-3 text-xs leading-relaxed text-[#8e8f94]">
            OP est réservé aux admins/modos. VIP reflète les supporters Patreon actifs quand Nexus expose le tier.
          </div>
        </aside>

        <section className="min-w-0 min-h-0 flex flex-col">
          <div className="border-b border-[#2a2b30] bg-[#14151a]/85 px-5 py-4 flex items-center justify-between">
            <div>
              <div className="flex items-center gap-2">
                <Hash className="w-5 h-5" style={{ color: selectedChannel.accent }} />
                <h2 className="font-bold text-lg">{selectedChannel.label.replace('# ', '')}</h2>
              </div>
              <p className="text-sm text-[#8e8f94]">{selectedChannel.description}</p>
            </div>
            <div className="flex items-center gap-2 text-xs text-[#8e8f94]">
              <Circle className={`w-3 h-3 fill-current ${error ? 'text-[#f69d50]' : 'text-[#4ecdc4]'}`} />
              {error ? 'Mode dégradé' : 'Connecté'}
            </div>
          </div>

          <div className="min-h-0 flex-1 overflow-auto p-5 space-y-4">
            {loading && (
              <div className="rounded-lg border border-[#2a2b30] bg-[#1c1d22]/70 p-4 text-sm text-[#8e8f94]">
                Chargement du salon...
              </div>
            )}
            {!loading && visibleMessages.map((message) => (
              <ChatMessageRow
                key={message.id}
                message={message}
                mine={ownNames.has(String(message.author || '').toLowerCase())}
                lobbyLinks={lobbyLinks}
                onLobbyLinkClick={(lobby) => onOpenLobby?.(lobby.id, lobby.name)}
              />
            ))}
            {!loading && visibleMessages.length === 0 && (
              <div className="rounded-lg border border-dashed border-[#2a2b30] bg-[#1c1d22]/40 p-5 text-center text-sm text-[#8e8f94]">
                Aucun message dans {selectedChannel.label} pour le moment.
              </div>
            )}
            <div ref={scrollAnchorRef} />
          </div>

          {error && (
            <div className="mx-5 mb-3 rounded-lg border border-[#f69d50]/40 bg-[#f69d50]/10 px-3 py-2 text-xs text-[#ffd6a8]">
              {error}
            </div>
          )}

          <div className="border-t border-[#2a2b30] bg-[#14151a] p-4">
            <div className="relative flex items-end gap-3">
              <button
                type="button"
                onClick={() => setShowEmotes((value) => !value)}
                className="h-[52px] w-12 rounded-lg border border-[#2a2b30] bg-[#1c1d22] text-[#8e8f94] transition-colors hover:border-[#4ecdc4] hover:text-white"
                aria-label="Show emotes"
              >
                <Smile className="mx-auto h-5 w-5" />
              </button>
              {showEmotes && (
                <div className="absolute bottom-[60px] left-0 z-30 grid w-72 grid-cols-4 gap-2 rounded-lg border border-[#2a2b30] bg-[#1c1d22] p-3 shadow-2xl">
                  {STANDARD_EMOTES.map((emote) => (
                    <button
                      key={emote.code}
                      type="button"
                      onClick={() => {
                        setDraft((value) => appendEmote(value, emote.code));
                        setShowEmotes(false);
                      }}
                      className="rounded-lg bg-[#14151a] px-2 py-2 text-left text-sm transition-colors hover:bg-[#2a2b30]"
                      title={`${emote.label} ${emote.code}`}
                    >
                      <span className="mr-1 text-base">{emote.emoji}</span>
                      <span className="text-xs text-[#8e8f94]">{emote.code}</span>
                    </button>
                  ))}
                </div>
              )}
              <div className="flex-1">
                <textarea
                  value={draft}
                  onChange={(event) => setDraft(event.target.value)}
                  onKeyDown={(event) => {
                    if (event.key === '[') {
                      event.preventDefault();
                      insertLobbyLinkShell();
                      return;
                    }
                    if (event.key === 'Enter' && !event.shiftKey) {
                      event.preventDefault();
                      void sendMessage();
                    }
                  }}
                  disabled={!canTalk}
                  ref={composerRef}
                  rows={2}
                  placeholder={canTalk ? `Message ${selectedChannel.label}...` : 'You are not authorized to talk in that channel.'}
                  className={`min-h-[52px] w-full resize-none rounded-lg border-2 px-4 py-3 text-sm outline-none transition-colors placeholder:text-[#8e8f94] ${
                    canTalk
                      ? 'border-[#2a2b30] bg-[#1c1d22] text-white focus:border-[#4ecdc4]'
                      : 'border-[#2a2b30] bg-[#1c1d22]/60 text-[#8e8f94] cursor-not-allowed'
                  }`}
                />
                {composerNotice && (
                  <div className="mt-2 text-xs text-[#8e8f94]">{composerNotice}</div>
                )}
                {showLobbyPicker && (
                  <div className="absolute bottom-[60px] left-[60px] z-30 w-80 overflow-hidden rounded-lg border border-[#2a2b30] bg-[#1c1d22] shadow-2xl">
                    <div className="border-b border-[#2a2b30] px-3 py-2 text-xs font-bold text-[#8e8f94]">
                      Lobbies waiting
                    </div>
                    <div className="max-h-64 overflow-auto p-2">
                      {lobbyPickerLoading && (
                        <div className="px-3 py-3 text-sm text-[#8e8f94]">Loading lobbies...</div>
                      )}
                      {!lobbyPickerLoading && linkableLobbies.length === 0 && (
                        <div className="px-3 py-3 text-sm text-[#8e8f94]">No waiting lobby available.</div>
                      )}
                      {!lobbyPickerLoading && linkableLobbies.map((lobby) => (
                        <button
                          key={lobby.id}
                          type="button"
                          onClick={() => chooseLobbyLink(lobby)}
                          className="w-full rounded-lg px-3 py-2 text-left transition-colors hover:bg-[#2a2b30]"
                        >
                          <div className="text-sm font-bold text-white">{lobby.name || lobby.id}</div>
                          <div className="text-xs text-[#8e8f94]">{lobby.owner || 'Unknown host'} • {lobby.member_count || 0} players</div>
                        </button>
                      ))}
                    </div>
                  </div>
                )}
              </div>
              <button
                onClick={() => void sendMessage()}
                disabled={sending || !draft.trim() || !canTalk}
                className="h-[52px] w-14 rounded-lg bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3] text-[#0e0f13] flex items-center justify-center shadow-lg transition-all disabled:opacity-50"
                aria-label="Send chat message"
              >
                <Send className="w-5 h-5" />
              </button>
            </div>
          </div>
        </section>

        <aside className="border-l border-[#2a2b30] bg-[#101116]/95 flex flex-col">
          <div className="border-b border-[#2a2b30] p-4">
            <div className="mb-3 flex items-center justify-between">
              <div className="flex items-center gap-2 font-bold">
                <Users className="w-4 h-4 text-[#aa96da]" />
                Noms
              </div>
              <span className="rounded-full bg-[#2a2b30] px-2 py-1 text-xs text-[#8e8f94]">{members.length}</span>
            </div>
            <div className="relative">
              <Search className="absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-[#8e8f94]" />
              <input
                value={memberQuery}
                onChange={(event) => setMemberQuery(event.target.value)}
                placeholder="Filtrer..."
                className="w-full rounded-lg border border-[#2a2b30] bg-[#14151a] py-2 pl-9 pr-3 text-sm outline-none placeholder:text-[#8e8f94] focus:border-[#aa96da]"
              />
            </div>
          </div>

          <div className="min-h-0 flex-1 overflow-auto p-3 space-y-1">
            {members.map((member) => (
              <button
                key={member.user_id}
                onContextMenu={(event) => {
                  event.preventDefault();
                  setContextMenu({ member, x: event.clientX, y: event.clientY });
                }}
                onClick={() => setProfileModal(member)}
                className="w-full rounded-lg px-2 py-2 text-left transition-colors hover:bg-[#1c1d22]"
                title="Right-click for member actions"
              >
                <div className="flex items-center gap-3">
                  <Avatar name={memberName(member)} avatarUrl={member.avatar_url} />
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-1.5">
                      {member.irc_op && <Shield className="w-3.5 h-3.5 text-[#f69d50]" />}
                      <span className="truncate text-sm font-medium">{memberName(member)}</span>
                      {memberBadge(member) && (
                        <span className={`rounded border px-1.5 py-0.5 text-[9px] font-bold ${memberBadge(member)?.className}`}>
                          {memberBadge(member)?.label}
                        </span>
                      )}
                      {mutedUsers.has(member.user_id) && <VolumeX className="w-3.5 h-3.5 text-[#8e8f94]" />}
                    </div>
                    <div className={`flex items-center gap-1 text-xs ${statusColor(member.status)}`}>
                      <Circle className="h-2 w-2 fill-current" />
                      <span className="truncate capitalize">{normalizeStatus(member.status)}</span>
                    </div>
                  </div>
                  <MoreHorizontal className="w-4 h-4 text-[#8e8f94]" />
                </div>
              </button>
            ))}
          </div>
        </aside>
      </div>

      {contextMenu && (
        <div
          className="fixed z-50 min-w-52 overflow-hidden rounded-lg border border-[#2a2b30] bg-[#1c1d22] shadow-2xl"
          style={{ left: contextMenu.x, top: contextMenu.y }}
          onClick={(event) => event.stopPropagation()}
        >
          <button
            onClick={() => {
              setProfileModal(contextMenu.member);
              setContextMenu(null);
            }}
            className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm hover:bg-[#2a2b30]"
          >
            <UserCircle className="w-4 h-4 text-[#4ecdc4]" />
            Voir le profil
          </button>
          {contextMenu.member.source !== 'self' && !friendIds.has(contextMenu.member.user_id) && (
            <button
              onClick={() => void runMemberAction(contextMenu.member, 'add')}
              className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm hover:bg-[#2a2b30]"
            >
              <UserPlus className="w-4 h-4 text-[#4ecdc4]" />
              Ajouter ami
            </button>
          )}
          {contextMenu.member.source !== 'self' && friendIds.has(contextMenu.member.user_id) && (
            <button
              onClick={() => void runMemberAction(contextMenu.member, 'remove')}
              className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm hover:bg-[#2a2b30]"
            >
              <UserMinus className="w-4 h-4 text-[#f69d50]" />
              Retirer ami
            </button>
          )}
          <button
            onClick={() => toggleMute(contextMenu.member)}
            className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm hover:bg-[#2a2b30]"
          >
            {mutedUsers.has(contextMenu.member.user_id) ? (
              <Volume2 className="w-4 h-4 text-[#4ecdc4]" />
            ) : (
              <VolumeX className="w-4 h-4 text-[#f69d50]" />
            )}
            {mutedUsers.has(contextMenu.member.user_id) ? 'Réactiver' : 'Muter'}
          </button>
          {contextMenu.member.source !== 'self' && (
            <button
              onClick={() => void runMemberAction(contextMenu.member, 'block')}
              className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm text-[#ffb4aa] hover:bg-[#f85149]/10"
            >
              <Ban className="w-4 h-4" />
              Bloquer
            </button>
          )}
        </div>
      )}

      {profileModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-6" onClick={() => setProfileModal(null)}>
          <div className="w-full max-w-md rounded-xl border border-[#2a2b30] bg-[#14151a] shadow-2xl" onClick={(event) => event.stopPropagation()}>
            <div className="flex items-center justify-between border-b border-[#2a2b30] p-5">
              <div className="flex items-center gap-3">
                <Avatar name={memberName(profileModal)} avatarUrl={profileModal.avatar_url} size="lg" />
                <div>
                  <div className="flex items-center gap-2">
                    <h3 className="text-lg font-bold">{memberName(profileModal)}</h3>
                    {memberBadge(profileModal) && (
                      <span className={`rounded border px-2 py-0.5 text-[10px] font-bold ${memberBadge(profileModal)?.className}`}>
                        {memberBadge(profileModal)?.label}
                      </span>
                    )}
                  </div>
                  <p className="text-sm text-[#8e8f94]">{profileModal.username || profileModal.user_id}</p>
                </div>
              </div>
              <button
                onClick={() => setProfileModal(null)}
                className="rounded-lg bg-[#2a2b30] p-2 text-[#8e8f94] transition-colors hover:text-white"
                aria-label="Close profile"
              >
                <X className="w-5 h-5" />
              </button>
            </div>
            <div className="space-y-3 p-5 text-sm">
              <InfoRow label="Status" value={normalizeStatus(profileModal.status)} />
              <InfoRow label="Role" value={memberIsOp(profileModal) ? 'Operator' : memberIsVip(profileModal) ? 'VIP' : profileModal.role || profileModal.source} />
              <InfoRow label="Channel" value={selectedChannel.label} />
              <button
                onClick={() => toggleMute(profileModal)}
                className={`mt-4 flex w-full items-center justify-center gap-2 rounded-lg px-4 py-3 font-medium transition-colors ${
                  mutedUsers.has(profileModal.user_id)
                    ? 'bg-[#4ecdc4]/15 text-[#4ecdc4] hover:bg-[#4ecdc4]/25'
                    : 'bg-[#2a2b30] text-white hover:bg-[#3a3b40]'
                }`}
              >
                {mutedUsers.has(profileModal.user_id) ? <Volume2 className="w-4 h-4" /> : <VolumeX className="w-4 h-4" />}
                {mutedUsers.has(profileModal.user_id) ? 'Réactiver cette personne' : 'Muter cette personne'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

function ChatMessageRow({
  message,
  mine,
  lobbyLinks,
  onLobbyLinkClick,
}: {
  message: SekaiChatMessage;
  mine?: boolean;
  lobbyLinks: LobbyTextLink[];
  onLobbyLinkClick?: (lobby: LobbyTextLink) => void;
}) {
  const isSystem = message.kind === 'system' || message.kind === 'join' || message.kind === 'leave';
  if (isSystem) {
    return (
      <div className="rounded-lg border border-[#2a2b30] bg-[#1c1d22]/60 px-4 py-3 text-sm text-[#8e8f94]">
        <span className="font-medium text-[#c9c9d1]">{message.author}</span>
        <span className="mx-2 text-[#4a4c55]">•</span>
        <EmoteText text={message.content} lobbyLinks={lobbyLinks} onLobbyLinkClick={onLobbyLinkClick} />
      </div>
    );
  }
  return (
    <div className={`flex gap-3 ${mine ? 'flex-row-reverse' : ''}`}>
      <Avatar name={message.author} avatarUrl={message.avatar_url} />
      <div className={`min-w-0 max-w-[78%] ${mine ? 'text-right' : ''}`}>
        <div className={`mb-1 flex items-baseline gap-2 ${mine ? 'justify-end' : ''}`}>
          <span className="font-bold text-sm">{message.author}</span>
          <span className="text-xs text-[#8e8f94]">{messageTime(message.created_at)}</span>
        </div>
        <div className={`whitespace-pre-wrap break-words rounded-lg px-4 py-3 text-sm leading-relaxed ${
          mine
            ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white rounded-br-sm'
            : 'bg-[#1c1d22] text-[#ececf1] rounded-bl-sm'
        }`}>
          <EmoteText text={message.content} lobbyLinks={lobbyLinks} onLobbyLinkClick={onLobbyLinkClick} />
        </div>
      </div>
    </div>
  );
}

function Avatar({ name, avatarUrl, size = 'md' }: { name: string; avatarUrl?: string; size?: 'md' | 'lg' }) {
  const sizeClass = size === 'lg' ? 'h-12 w-12 text-lg' : 'h-9 w-9 text-sm';
  const cleanUrl = String(avatarUrl || '').trim();
  return (
    <div className={`${sizeClass} flex flex-shrink-0 items-center justify-center overflow-hidden rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] font-bold text-white`}>
      {cleanUrl ? <img src={cleanUrl} alt={name} className="h-full w-full object-cover" /> : <span>{initial(name)}</span>}
    </div>
  );
}

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center justify-between rounded-lg bg-[#1c1d22] px-4 py-3">
      <span className="text-[#8e8f94]">{label}</span>
      <span className="font-medium capitalize">{value || '-'}</span>
    </div>
  );
}
