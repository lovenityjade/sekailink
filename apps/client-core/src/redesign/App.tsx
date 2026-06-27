import { Suspense, lazy, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Home, Library, Users, Settings, Bell, Play, Star, Clock, ChevronLeft, ChevronRight, X, UserPlus, Send, Search, Sparkles, ExternalLink, MessageCircle, Megaphone, Smile } from 'lucide-react';
import packageJson from '../../package.json';
import TitleBar from './components/TitleBar';
import AnimatedBackground from './components/AnimatedBackground';
import CreateLobbyModal from './components/CreateLobbyModal';
import HelpButton from './components/HelpButton';
import { appendEmote, STANDARD_EMOTES } from './components/EmoteText';
import {
  ChatMessage,
  FriendCard,
  FriendRequestNotification,
  LobbyCard,
  NavItem,
  NotificationItem,
  ProfileAvatar,
  StatsCard,
  StatusMenuItem,
} from './components/AppWidgets';
import { ErrorModal, LoadingModal } from './components/FeedbackModal';
import { apiCurrentUser, getCachedCurrentUser, type CurrentUser } from '../services/api';
import { joinLobby, listLobbies, type LobbySummary } from '../services/lobbyClient';
import { ALTTP_SHOWCASE_GAME, type SeedGameEntry } from '../services/seedConfig';
import { runtime } from '../services/runtime';
import { APP_TOAST_EVENT, SOCIAL_OPEN_DM_EVENT, UPDATE_AVAILABLE_EVENT, type AppToastPayload } from '../services/toast';
import {
  listSocialSnapshot,
  isPresenceOnline,
  loadDirectMessages,
  markDirectMessagesRead,
  normalizePresenceStatus,
  presenceStatusLabel,
  respondSocialFriendRequest,
  searchSocialUsers,
  sendDirectMessage,
  sendSocialFriendRequest,
  updateSocialPresence,
  type SocialDirectMessage,
  type SocialProfile,
  type SocialRequest,
} from '../services/socialClient';
import { trace, traceError } from '../services/trace';
import { useSfx } from '../hooks/useSfx';
import { isRuntimeLabSession } from '../services/runtimeLab';

const NEWS_ITEMS = [
  {
    title: "BETA-3 Showcase",
    description: "A Link to the Past is the current live showcase for lobbies, seed configs, and Sekaiemu launch.",
    tag: "SHOWCASE",
    image: "/assets/img/banners/banner-1.png",
    detailUrl: "https://sekailink.com/showcase"
  },
  {
    title: "Live Lobby Runtime",
    description: "Lobbies, friends, room chat, and seed selections now come from Nexus.",
    tag: "LIVE",
    image: "/assets/img/banners/banner-3.png",
    detailUrl: "https://sekailink.com/lobbies"
  },
  {
    title: "Pulse Seed Setup",
    description: "Use Easy Mode for guided configs or Advanced Mode for full APWorld options.",
    tag: "PULSE",
    image: "/assets/img/banners/banner-2.png",
    detailUrl: "https://pulse.sekailink.com"
  }
];

const APP_VERSION = String((packageJson as { version?: string }).version || 'dev');

const ChatPage = lazy(() => import('./components/ChatPage'));
const EasyConfigPage = lazy(() => import('./components/EasyConfigPage'));
const LibraryPage = lazy(() => import('./components/LibraryPage'));
const LobbiesPage = lazy(() => import('./components/LobbiesPage'));
const LobbyRoomPage = lazy(() => import('./components/LobbyRoomPage'));
const PulsePage = lazy(() => import('./components/PulsePage'));
const SekaiemuRuntimeLabPage = lazy(() => import('./components/SekaiemuRuntimeLabPage'));
const SettingsPage = lazy(() => import('./components/SettingsPage'));

type UserStatus = 'online' | 'appear-offline' | 'busy' | 'afk';

const profileName = (profile: SocialProfile | null) =>
  profile?.display_name || profile?.name || profile?.username || profile?.user_id || 'Player';

const isProfileOnline = (profile: SocialProfile) => {
  return isPresenceOnline(profile.presence_status || profile.status);
};

const statusLabel = (status: string) => {
  return presenceStatusLabel(status);
};

const userStatusFromPresence = (status: string): UserStatus => {
  const normalized = normalizePresenceStatus(status, 'offline');
  if (normalized === 'dnd' || normalized === 'busy') return 'busy';
  if (normalized === 'afk') return 'afk';
  return normalized === 'online' ? 'online' : 'appear-offline';
};

const presenceFromUserStatus = (status: UserStatus) => {
  if (status === 'appear-offline') return 'offline';
  if (status === 'busy') return 'dnd';
  return status;
};

const maxPlayersForLobby = (lobby: LobbySummary) => {
  const max = Number(lobby.max_players || 50);
  return Number.isFinite(max) && max > 0 ? max : 50;
};

const formatMessageTime = (value: string) => {
  const timestamp = Date.parse(value);
  if (!Number.isFinite(timestamp)) return '';
  return new Date(timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
};

function PageLoader() {
  return (
    <div className="relative z-10 flex min-h-[320px] items-center justify-center rounded-xl border border-[#2a2b30] bg-[#14151a]/70 text-sm font-medium text-[#8e8f94]">
      Loading...
    </div>
  );
}

function RedesignShell() {
  const { play } = useSfx();
  const [activeNav, setActiveNav] = useState('home');
  const [currentNews, setCurrentNews] = useState(0);
  const [showNotifications, setShowNotifications] = useState(false);
  const [showDiscordModal, setShowDiscordModal] = useState(false);
  const [showBannerModal, setShowBannerModal] = useState(false);
  const [showUpdateInstallModal, setShowUpdateInstallModal] = useState(false);
  const [updateNoticeVersion, setUpdateNoticeVersion] = useState('');
  const [toasts, setToasts] = useState<Array<AppToastPayload & { id: string }>>([]);
  const [systemNotifications, setSystemNotifications] = useState<Array<{
    id: string;
    title: string;
    message: string;
    type: 'info' | 'update';
    read: boolean;
  }>>([]);
  const [selectedFriend, setSelectedFriend] = useState<SocialProfile | null>(null);
  const [userStatus, setUserStatus] = useState<UserStatus>('online');
  const [showStatusMenu, setShowStatusMenu] = useState(false);
  const [showCreateLobby, setShowCreateLobby] = useState(false);
  const [showAddFriends, setShowAddFriends] = useState(false);
  const [friendSearchQuery, setFriendSearchQuery] = useState('');
  const [currentUser, setCurrentUser] = useState<CurrentUser | null>(() => getCachedCurrentUser());
  const [selectedLobbyId, setSelectedLobbyId] = useState('');
  const [easyConfigGame, setEasyConfigGame] = useState<SeedGameEntry>(ALTTP_SHOWCASE_GAME);
  const [liveLobbies, setLiveLobbies] = useState<LobbySummary[]>([]);
  const [lobbiesLoading, setLobbiesLoading] = useState(true);
  const [joiningLobbyId, setJoiningLobbyId] = useState('');
  const [friends, setFriends] = useState<SocialProfile[]>([]);
  const [incomingRequests, setIncomingRequests] = useState<SocialRequest[]>([]);
  const [outgoingRequests, setOutgoingRequests] = useState<SocialRequest[]>([]);
  const [unreadDmCount, setUnreadDmCount] = useState(0);
  const [unreadDmByUser, setUnreadDmByUser] = useState<Record<string, number>>({});
  const [socialLoading, setSocialLoading] = useState(true);
  const [friendSearchResults, setFriendSearchResults] = useState<SocialProfile[]>([]);
  const [friendSearchBusy, setFriendSearchBusy] = useState(false);
  const [friendActionBusyId, setFriendActionBusyId] = useState('');
  const [dmMessages, setDmMessages] = useState<SocialDirectMessage[]>([]);
  const [dmDraft, setDmDraft] = useState('');
  const [dmLoading, setDmLoading] = useState(false);
  const [dmSending, setDmSending] = useState(false);
  const [showDmEmotes, setShowDmEmotes] = useState(false);
  const [uiError, setUiError] = useState('');
  const [lobbyTransition, setLobbyTransition] = useState<{
    lobbyId: string;
    label: string;
    phase: 'joining' | 'loading';
  } | null>(null);
  const lobbiesRefreshInFlight = useRef(false);
  const socialRefreshInFlight = useRef(false);
  const previousSocialCounts = useRef({ friendsOnline: 0, incoming: 0, unread: 0 });

  useEffect(() => {
    trace('redesign-app', 'mounted');
    return () => trace('redesign-app', 'unmounted');
  }, []);

  const pushToast = useCallback((payload: AppToastPayload) => {
    const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
    setToasts((prev) => [...prev.slice(-3), { ...payload, id }]);
    const duration = payload.sticky ? 9000 : payload.durationMs || 4200;
    window.setTimeout(() => {
      setToasts((prev) => prev.filter((toast) => toast.id !== id));
    }, duration);
  }, []);

  const addSystemNotification = useCallback((notification: Omit<typeof systemNotifications[number], 'id' | 'read'>) => {
    setSystemNotifications((prev) => [
      {
        ...notification,
        id: `${notification.type}-${Date.now()}-${Math.random().toString(16).slice(2)}`,
        read: false,
      },
      ...prev,
    ].slice(0, 20));
    play(notification.type === 'update' ? 'sfx_update_notification' : 'sfx_global_notification', 0.85);
    pushToast({ message: notification.message, kind: notification.type === 'update' ? 'success' : 'info' });
  }, [play, pushToast]);

  useEffect(() => {
    const handleToast = (event: Event) => {
      const detail = (event as CustomEvent<AppToastPayload>).detail;
      if (!detail?.message) return;
      pushToast(detail);
      play(detail.kind === 'error' ? 'sfx_generation_error' : 'sfx_global_notification', 0.75);
    };
    const handleOpenDm = (event: Event) => {
      const detail = (event as CustomEvent<{ userId?: string; name?: string }>).detail;
      const userId = String(detail?.userId || '').trim();
      if (!userId) return;
      const friend = friends.find((entry) => entry.user_id === userId);
      setSelectedFriend(friend || {
        user_id: userId,
        username: detail?.name || userId,
        display_name: detail?.name || userId,
        name: detail?.name || userId,
        avatar_url: '',
        status: 'offline',
        presence_status: 'offline',
      });
      setShowNotifications(false);
    };
    const handleUpdateAvailable = (event: Event) => {
      const detail = (event as CustomEvent<{ version?: string; message?: string }>).detail || {};
      const version = String(detail.version || '').trim();
      const message = String(detail.message || (version ? `A new version is available ${version}!` : 'A new version is available!'));
      if (version) setUpdateNoticeVersion(version);
      addSystemNotification({
        title: 'SekaiLink update',
        message,
        type: 'update',
      });
    };
    window.addEventListener(APP_TOAST_EVENT, handleToast);
    window.addEventListener(SOCIAL_OPEN_DM_EVENT, handleOpenDm);
    window.addEventListener(UPDATE_AVAILABLE_EVENT, handleUpdateAvailable);
    return () => {
      window.removeEventListener(APP_TOAST_EVENT, handleToast);
      window.removeEventListener(SOCIAL_OPEN_DM_EVENT, handleOpenDm);
      window.removeEventListener(UPDATE_AVAILABLE_EVENT, handleUpdateAvailable);
    };
  }, [addSystemNotification, friends, play, pushToast]);

  useEffect(() => {
    const unsubscribe = runtime.onUpdaterEvent?.((raw) => {
      const event = raw && typeof raw === 'object' ? raw as Record<string, unknown> : {};
      const type = String(event.type || event.event || event.status || '').toLowerCase();
      if (!type.includes('available') && !type.includes('ready') && !type.includes('update')) return;
      const version = String(event.version || event.latest || event.releaseVersion || '').trim();
      if (version) setUpdateNoticeVersion(version);
      addSystemNotification({
        title: 'SekaiLink update',
        message: version ? `A new version is available ${version}!` : 'A new version is available!',
        type: 'update',
      });
    });
    return () => {
      if (typeof unsubscribe === 'function') unsubscribe();
    };
  }, [addSystemNotification]);

  useEffect(() => {
    trace('redesign-app', 'navigation_changed', { activeNav, selectedLobbyId });
  }, [activeNav, selectedLobbyId]);

  const selectNews = useCallback((nextIndex: number) => {
    const normalized = (nextIndex + NEWS_ITEMS.length) % NEWS_ITEMS.length;
    setCurrentNews((current) => (normalized === current ? current : normalized));
  }, []);

  useEffect(() => {
    const interval = setInterval(() => {
      selectNews(currentNews + 1);
    }, 5000);

    return () => clearInterval(interval);
  }, [currentNews, selectNews]);

  useEffect(() => {
    let cancelled = false;
    trace('redesign-app', 'current_user_load_start');
    apiCurrentUser()
      .then((user) => {
        if (!cancelled) {
          trace('redesign-app', 'current_user_load_success', {
            userId: user.user_id,
            username: user.username,
            displayName: user.display_name,
          });
          setCurrentUser(user);
        }
      })
      .catch((error) => {
        if (!cancelled) {
          traceError('redesign-app', 'current_user_load_failed', error);
          setCurrentUser(getCachedCurrentUser());
        }
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const refreshLobbies = useCallback(async () => {
    if (lobbiesRefreshInFlight.current) {
      trace('redesign-app', 'live_lobbies_load_skipped_in_flight');
      return;
    }
    lobbiesRefreshInFlight.current = true;
    trace('redesign-app', 'live_lobbies_load_start');
    try {
      const next = await listLobbies();
      setLiveLobbies(next);
      setUiError('');
      trace('redesign-app', 'live_lobbies_load_success', { count: next.length });
    } catch (error) {
      traceError('redesign-app', 'live_lobbies_load_failed', error);
      setUiError(error instanceof Error ? error.message : 'Unable to load lobbies.');
    } finally {
      setLobbiesLoading(false);
      lobbiesRefreshInFlight.current = false;
    }
  }, []);

  useEffect(() => {
    void refreshLobbies();
    const intervalMs = activeNav === 'lobby-room' ? 60000 : 20000;
    const interval = window.setInterval(() => void refreshLobbies(), intervalMs);
    return () => window.clearInterval(interval);
  }, [activeNav, refreshLobbies]);

  const refreshSocial = useCallback(async () => {
    if (socialRefreshInFlight.current) {
      trace('redesign-app', 'social_load_skipped_in_flight');
      return;
    }
    socialRefreshInFlight.current = true;
    trace('redesign-app', 'social_load_start');
    try {
      const snapshot = await listSocialSnapshot();
      setFriends(snapshot.friends);
      setIncomingRequests(snapshot.incoming);
      setOutgoingRequests(snapshot.outgoing);
      setUnreadDmCount(snapshot.unreadCount);
      setUnreadDmByUser(snapshot.unreadByUser || {});
      setUserStatus(userStatusFromPresence(snapshot.presenceStatus));
      if (previousSocialCounts.current.friendsOnline && snapshot.friends.filter(isProfileOnline).length > previousSocialCounts.current.friendsOnline) {
        play('sfx_friend_online', 0.75);
      }
      if (snapshot.incoming.length > previousSocialCounts.current.incoming) {
        play('sfx_friend_request_notification', 0.85);
        pushToast({ message: 'New friend request received.', kind: 'info' });
      }
      if (snapshot.unreadCount > previousSocialCounts.current.unread) {
        play('sfx_chat_notification', 0.8);
        pushToast({ message: 'New unread message.', kind: 'info' });
      }
      previousSocialCounts.current = {
        friendsOnline: snapshot.friends.filter(isProfileOnline).length,
        incoming: snapshot.incoming.length,
        unread: snapshot.unreadCount,
      };
      trace('redesign-app', 'social_load_success', {
        friends: snapshot.friends.length,
        incoming: snapshot.incoming.length,
        outgoing: snapshot.outgoing.length,
        unreadDmCount: snapshot.unreadCount,
      });
    } catch (error) {
      traceError('redesign-app', 'social_load_failed', error);
      setFriends([]);
      setIncomingRequests([]);
      setOutgoingRequests([]);
      setUnreadDmCount(0);
      setUnreadDmByUser({});
    } finally {
      setSocialLoading(false);
      socialRefreshInFlight.current = false;
    }
  }, [play, pushToast]);

  useEffect(() => {
    void refreshSocial();
    const interval = window.setInterval(() => void refreshSocial(), 30000);
    return () => window.clearInterval(interval);
  }, [refreshSocial]);

  useEffect(() => {
    if (!showAddFriends) {
      setFriendSearchResults([]);
      setFriendSearchBusy(false);
      return;
    }
    const query = friendSearchQuery.trim();
    if (query.length < 2) {
      setFriendSearchResults([]);
      setFriendSearchBusy(false);
      return;
    }
    let cancelled = false;
    const timer = window.setTimeout(() => {
      setFriendSearchBusy(true);
      searchSocialUsers(query)
        .then((users) => {
          if (!cancelled) setFriendSearchResults(users);
        })
        .catch((error) => {
          if (!cancelled) {
            traceError('redesign-app', 'friend_search_failed', error, { length: query.length });
            setFriendSearchResults([]);
          }
        })
        .finally(() => {
          if (!cancelled) setFriendSearchBusy(false);
        });
    }, 250);
    return () => {
      cancelled = true;
      window.clearTimeout(timer);
    };
  }, [friendSearchQuery, showAddFriends]);

  useEffect(() => {
    if (!selectedFriend?.user_id) {
      setDmMessages([]);
      setDmDraft('');
      return;
    }
    let cancelled = false;
    const load = async () => {
      setDmLoading(true);
      try {
        const next = await loadDirectMessages(selectedFriend.user_id);
        await markDirectMessagesRead(selectedFriend.user_id).catch((error) => {
          traceError('redesign-app', 'dm_mark_read_failed', error, { userId: selectedFriend.user_id });
        });
        if (!cancelled) {
          setDmMessages(next);
          setUnreadDmByUser((prev) => {
            const nextUnread = { ...prev };
            delete nextUnread[selectedFriend.user_id];
            return nextUnread;
          });
          setUnreadDmCount((count) => Math.max(0, count - Number(unreadDmByUser[selectedFriend.user_id] || 0)));
        }
      } catch (error) {
        if (!cancelled) {
          traceError('redesign-app', 'dm_load_failed', error, { userId: selectedFriend.user_id });
          setDmMessages([]);
        }
      } finally {
        if (!cancelled) setDmLoading(false);
      }
    };
    void load();
    const interval = window.setInterval(() => void load(), 10000);
    return () => {
      cancelled = true;
      window.clearInterval(interval);
    };
  }, [selectedFriend?.user_id, unreadDmByUser]);

  const onlineFriends = useMemo(() => friends.filter(isProfileOnline), [friends]);
  const unreadSystemCount = systemNotifications.filter((notification) => !notification.read).length;
  const notificationCount = incomingRequests.length + unreadDmCount + unreadSystemCount;
  const activeHomeLobbies = useMemo(() => liveLobbies.filter((lobby) => !lobby.is_locked).slice(0, 9), [liveLobbies]);

  const handleSetUserStatus = async (status: UserStatus) => {
    setUserStatus(status);
    setShowStatusMenu(false);
    try {
      await updateSocialPresence(presenceFromUserStatus(status));
      void refreshSocial();
    } catch (error) {
      traceError('redesign-app', 'presence_update_failed', error, { status });
    }
  };

  const handleJoinLobby = async (lobby: LobbySummary) => {
    if (!lobby.id || joiningLobbyId) return;
    trace('redesign-app', 'home_lobby_join_start', { lobbyId: lobby.id, private: lobby.is_private === true });
    const password = lobby.is_private ? window.prompt('Lobby password') : '';
    if (password === null) return;
    setJoiningLobbyId(lobby.id);
    setLobbyTransition({
      lobbyId: lobby.id,
      label: lobby.name || lobby.id,
      phase: 'joining',
    });
    try {
      await joinLobby(lobby, password || undefined);
      setLobbyTransition({
        lobbyId: lobby.id,
        label: lobby.name || lobby.id,
        phase: 'loading',
      });
      setSelectedLobbyId(lobby.id);
      setActiveNav('lobby-room');
      trace('redesign-app', 'home_lobby_join_success', { lobbyId: lobby.id });
    } catch (error) {
      traceError('redesign-app', 'home_lobby_join_failed', error, { lobbyId: lobby.id });
      setUiError(error instanceof Error ? error.message : 'Unable to join lobby.');
      setLobbyTransition(null);
    } finally {
      setJoiningLobbyId('');
    }
  };

  const handleSendFriendRequest = async (profile: SocialProfile) => {
    if (!profile.user_id || friendActionBusyId) return;
    setFriendActionBusyId(profile.user_id);
    try {
      await sendSocialFriendRequest(profile.user_id);
      setFriendSearchQuery('');
      setFriendSearchResults([]);
      setShowAddFriends(false);
      await refreshSocial();
    } catch (error) {
      traceError('redesign-app', 'friend_request_failed', error, { userId: profile.user_id });
      setUiError(error instanceof Error ? error.message : 'Unable to send friend request.');
    } finally {
      setFriendActionBusyId('');
    }
  };

  const handleRespondFriendRequest = async (request: SocialRequest, action: 'accept' | 'decline') => {
    if (!request.id || friendActionBusyId) return;
    setFriendActionBusyId(String(request.id));
    try {
      await respondSocialFriendRequest(request.id, action);
      await refreshSocial();
    } catch (error) {
      traceError('redesign-app', 'friend_request_response_failed', error, { requestId: request.id, action });
      setUiError(error instanceof Error ? error.message : 'Unable to update friend request.');
    } finally {
      setFriendActionBusyId('');
    }
  };

  const handleSendDm = async () => {
    if (!selectedFriend?.user_id || !dmDraft.trim() || dmSending) return;
    const content = dmDraft;
    setDmDraft('');
    setDmSending(true);
    try {
      const message = await sendDirectMessage(selectedFriend.user_id, content);
      if (message) setDmMessages((prev) => [...prev, message]);
      void refreshSocial();
    } catch (error) {
      traceError('redesign-app', 'dm_send_failed', error, { userId: selectedFriend.user_id });
      setDmDraft(content);
    } finally {
      setDmSending(false);
    }
  };

  const handleLaunchUpdate = async () => {
    setShowNotifications(false);
    setShowUpdateInstallModal(true);
  };

  const confirmLaunchUpdate = async () => {
    setShowUpdateInstallModal(false);
    try {
      await runtime.updaterLaunchBootstrapperAndQuit?.();
    } catch (error) {
      traceError('redesign-app', 'update_bootloader_launch_failed', error);
      setUiError(error instanceof Error ? error.message : 'Unable to launch the bootloader.');
    }
  };

  const handleUnreadMessagesClick = () => {
    const unreadUserId = Object.entries(unreadDmByUser).sort((a, b) => b[1] - a[1])[0]?.[0] || '';
    const friend = friends.find((entry) => entry.user_id === unreadUserId) || friends[0];
    if (friend) setSelectedFriend(friend);
    setShowNotifications(false);
  };

  const handleLobbyInitialLoadComplete = useCallback((loadedLobbyId: string) => {
    setLobbyTransition((current) => (
      current?.lobbyId === loadedLobbyId ? null : current
    ));
  }, []);

  const displayName = currentUser?.display_name || currentUser?.username || 'Player';

  return (
    <div className="skl-redesign dark h-screen w-screen bg-[#14151a] text-white flex flex-col overflow-hidden">
      {/* Title Bar */}
      <TitleBar />

      {/* Main App Container */}
      <div className="flex-1 flex overflow-hidden">
      {/* Left Sidebar */}
      <aside className="w-64 bg-[#0e0f13]/95 backdrop-blur-sm flex flex-col relative z-20 border-r border-[#2a2b30]" style={{ boxShadow: '8px 0 20px rgba(0,0,0,0.4), 4px 0 10px rgba(0,0,0,0.3)' }}>
        {/* Logo */}
        <div className="p-6 border-b border-[#2a2b30]">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-[#161b22] flex items-center justify-center shadow-lg overflow-hidden border border-[#2a2b30]">
              <img src="/assets/img/sekailink-logo-image.png" alt="SekaiLink" className="w-8 h-8 object-contain" />
            </div>
            <div>
              <div className="font-bold text-lg">SekaiLink</div>
              <div className="text-xs text-[#8e8f94]">v{APP_VERSION}</div>
            </div>
          </div>
        </div>

        {/* Navigation */}
        <nav className="flex-1 p-4 space-y-1">
          <NavItem icon={<Home className="w-5 h-5" />} label="Home" active={activeNav === 'home'} onClick={() => setActiveNav('home')} />
          <NavItem icon={<Library className="w-5 h-5" />} label="Library" active={activeNav === 'library'} onClick={() => setActiveNav('library')} />
          <NavItem icon={<MessageCircle className="w-5 h-5" />} label="Chat" active={activeNav === 'chat'} onClick={() => setActiveNav('chat')} />
          <NavItem icon={<Users className="w-5 h-5" />} label="Lobbies" active={activeNav === 'lobbies'} onClick={() => setActiveNav('lobbies')} />
          <NavItem icon={<Sparkles className="w-5 h-5" />} label="Pulse" active={activeNav === 'pulse'} onClick={() => setActiveNav('pulse')} />
          <NavItem icon={<Settings className="w-5 h-5" />} label="Settings" active={activeNav === 'settings'} onClick={() => setActiveNav('settings')} />
        </nav>

        {/* User Profile */}
        <div className="p-4 border-t border-[#2a2b30] relative">
          <div
            onClick={() => setShowStatusMenu(!showStatusMenu)}
            className="flex items-center gap-3 p-3 rounded-lg bg-[#1c1d22] hover:bg-[#2a2b30] transition-colors cursor-pointer"
          >
            <div className="relative">
              <ProfileAvatar name={displayName} avatarUrl={currentUser?.avatar_url || ''} size="md" />
              {/* Status Indicator */}
              <div className={`absolute -bottom-0.5 -right-0.5 w-4 h-4 rounded-full border-2 border-[#0e0f13] ${
                userStatus === 'online' ? 'bg-[#4ecdc4]' :
                userStatus === 'busy' ? 'bg-[#ff6b35]' :
                userStatus === 'afk' ? 'bg-[#f69d50]' :
                'bg-[#8e8f94]'
              }`} />
            </div>
            <div className="flex-1 min-w-0">
              <div className="font-medium text-sm">{displayName}</div>
              <div className="text-xs text-[#8e8f94] capitalize">
                {userStatus === 'appear-offline' ? 'Appear Offline' : userStatus === 'afk' ? 'AFK' : userStatus}
              </div>
            </div>
          </div>

          {/* Status Menu */}
          {showStatusMenu && (
            <>
              {/* Backdrop */}
              <div
                className="fixed inset-0 z-40"
                onClick={() => setShowStatusMenu(false)}
              />

              {/* Menu */}
              <div className="absolute bottom-full left-4 right-4 mb-2 bg-[#1c1d22] rounded-lg shadow-2xl z-50 card-float border border-[#2a2b30] overflow-hidden">
                <div className="p-2 space-y-1">
                  <StatusMenuItem
                    status="online"
                    label="Online"
                    color="bg-[#4ecdc4]"
                    isActive={userStatus === 'online'}
                    onClick={() => void handleSetUserStatus('online')}
                  />
                  <StatusMenuItem
                    status="busy"
                    label="Busy"
                    color="bg-[#ff6b35]"
                    isActive={userStatus === 'busy'}
                    onClick={() => void handleSetUserStatus('busy')}
                  />
                  <StatusMenuItem
                    status="afk"
                    label="AFK"
                    color="bg-[#f69d50]"
                    isActive={userStatus === 'afk'}
                    onClick={() => void handleSetUserStatus('afk')}
                  />
                  <StatusMenuItem
                    status="appear-offline"
                    label="Appear Offline"
                    color="bg-[#8e8f94]"
                    isActive={userStatus === 'appear-offline'}
                    onClick={() => void handleSetUserStatus('appear-offline')}
                  />
                </div>
              </div>
            </>
          )}
        </div>
      </aside>

      {/* Main Content */}
      <main className="flex-1 overflow-auto relative">
        <AnimatedBackground />
        {activeNav === 'home' && (
        <div className="p-8 space-y-6 relative z-10">
          {/* Header */}
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-3xl font-bold mb-1">Dashboard</h1>
              <p className="text-[#8e8f94]">Welcome back to SekaiLink</p>
            </div>
            <button
              onClick={() => setActiveNav('library')}
              className="px-6 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg font-medium shadow-lg hover:shadow-xl glow-hover transition-all flex items-center gap-2"
            >
              <Play className="w-5 h-5" />
              Open Library
            </button>
          </div>

          {/* News Banner */}
          <div className="relative rounded-xl card-float overflow-hidden border border-[#2a2b30]">
            <button
              onClick={() => setShowBannerModal(true)}
              className="absolute inset-0 z-10"
              aria-label={`Open ${NEWS_ITEMS[currentNews].title}`}
            />
            {NEWS_ITEMS.map((item, index) => (
              <img
                key={item.image}
                src={item.image}
                alt={index === currentNews ? item.title : ''}
                aria-hidden={index === currentNews ? undefined : true}
                className={`absolute inset-0 w-full h-full object-cover transition-opacity duration-500 ease-in-out ${
                  index === currentNews ? 'opacity-100' : 'opacity-0'
                }`}
              />
            ))}
            <div className="absolute inset-0 bg-gradient-to-r from-[#0e0f13]/80 via-[#0e0f13]/35 to-[#0e0f13]/70" />
            <div className="relative flex items-center justify-between min-h-44 p-6">
              <button
                onClick={() => selectNews(currentNews - 1)}
                className="relative z-20 w-8 h-8 flex items-center justify-center rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] transition-colors"
              >
                <ChevronLeft className="w-5 h-5" />
              </button>

              <div className="flex-1 mx-6 text-center pointer-events-none">
                <div className="inline-block px-3 py-1 bg-[#ff6b35] rounded-full text-xs font-bold mb-2">
                  {NEWS_ITEMS[currentNews].tag}
                </div>
                <h3 className="text-xl font-bold mb-1">{NEWS_ITEMS[currentNews].title}</h3>
                <p className="text-sm text-[#8e8f94]">{NEWS_ITEMS[currentNews].description}</p>
              </div>

              <button
                onClick={() => selectNews(currentNews + 1)}
                className="relative z-20 w-8 h-8 flex items-center justify-center rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] transition-colors"
              >
                <ChevronRight className="w-5 h-5" />
              </button>
            </div>

            {/* Indicator dots */}
            <div className="flex justify-center gap-2 mt-4">
              {NEWS_ITEMS.map((_, index) => (
                <button
                  key={index}
                  onClick={() => selectNews(index)}
                  className={`w-2 h-2 rounded-full transition-all ${
                    index === currentNews ? 'bg-[#ff6b35] w-6' : 'bg-[#2a2b30]'
                  }`}
                />
              ))}
            </div>
          </div>

          {/* Stats Cards */}
          <div className="grid grid-cols-3 gap-6">
            <StatsCard
              icon={<Play className="w-6 h-6" />}
              label="Active Lobbies"
              value={String(liveLobbies.filter((lobby) => !lobby.is_locked).length)}
              change={lobbiesLoading ? 'Loading from Nexus' : `${liveLobbies.length} total listed`}
              color="from-[#ff6b35] to-[#f38181]"
              onClick={() => setActiveNav('lobbies')}
            />
            <StatsCard
              icon={<Users className="w-6 h-6" />}
              label="Friends Online"
              value={String(onlineFriends.length)}
              change={`${friends.length} friends on Nexus`}
              color="from-[#4ecdc4] to-[#95e1d3]"
              onClick={() => setShowDiscordModal(true)}
            />
            <StatsCard
              icon={<Star className="w-6 h-6" />}
              label="Online Players"
              value={String(onlineFriends.length)}
              change="Visible Nexus friends online"
              color="from-[#aa96da] to-[#f38181]"
            />
          </div>

          {/* Active Lobbies */}
          <div className="bg-gradient-card rounded-xl p-6 card-float">
            <div className="flex items-center justify-between mb-6">
              <div>
                <h2 className="text-xl font-bold mb-1">Active Lobbies</h2>
                <p className="text-sm text-[#8e8f94]">Join a lobby or create your own</p>
              </div>
              <button
                onClick={() => setShowCreateLobby(true)}
                className="px-5 py-2.5 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg text-sm font-medium shadow-lg hover:shadow-xl glow-hover transition-all"
              >
                Create Lobby
              </button>
            </div>

            {/* Lobbies List */}
            <div className="grid grid-cols-3 gap-3">
              {activeHomeLobbies.map((lobby) => (
                <LobbyCard
                  key={lobby.id}
                  name={lobby.name || lobby.id}
                  host={lobby.owner || 'Unknown'}
                  seedsInSync={Number(lobby.message_count || 0)}
                  players={Number(lobby.member_count || 0)}
                  maxPlayers={maxPlayersForLobby(lobby)}
                  status={lobby.is_locked ? 'in-progress' : 'waiting'}
                  joining={joiningLobbyId === lobby.id}
                  onJoin={() => void handleJoinLobby(lobby)}
                />
              ))}
              {!lobbiesLoading && activeHomeLobbies.length === 0 && (
                <div className="p-8 rounded-lg border-2 border-[#2a2b30] bg-[#1c1d22]/60 text-center text-sm text-[#8e8f94]">
                  No live lobbies from Nexus yet.
                </div>
              )}
              {lobbiesLoading && activeHomeLobbies.length === 0 && (
                <div className="p-8 rounded-lg border-2 border-[#2a2b30] bg-[#1c1d22]/60 text-center text-sm text-[#8e8f94]">
                  Loading lobbies from Nexus...
                </div>
              )}
            </div>
          </div>
        </div>
        )}

        <Suspense fallback={<PageLoader />}>
          {activeNav === 'library' && (
            <div className="relative z-10">
              <LibraryPage
                onNavigateToEasyConfig={(game) => {
                  setEasyConfigGame(game);
                  setActiveNav('easy-config');
                }}
              />
            </div>
          )}

          {activeNav === 'easy-config' && (
            <div className="relative z-10 h-full">
              <EasyConfigPage
                game={easyConfigGame}
                onBack={() => setActiveNav('library')}
                onComplete={(config) => {
                  trace('redesign-app', 'easy_config_completed', {
                    seedId: config.id,
                    title: config.title,
                    gameKey: config.game_key,
                  });
                  setActiveNav('library');
                }}
              />
            </div>
          )}

          {activeNav === 'chat' && (
            <div className="relative z-10 h-full">
              <ChatPage
                currentUser={currentUser}
                friends={friends}
                onOpenLobby={(lobbyId, lobbyName) => {
                  setLobbyTransition({
                    lobbyId,
                    label: lobbyName || lobbyId,
                    phase: 'loading',
                  });
                  setSelectedLobbyId(lobbyId);
                  setActiveNav('lobby-room');
                }}
              />
            </div>
          )}

          {activeNav === 'lobbies' && (
            <div className="relative z-10">
              <LobbiesPage
                onCreateLobby={() => setShowCreateLobby(true)}
                onJoinStart={(lobby) => {
                  if (!lobby.id) return;
                  setLobbyTransition({
                    lobbyId: lobby.id,
                    label: lobby.name || lobby.id,
                    phase: 'joining',
                  });
                }}
                onJoinError={(lobby) => {
                  setLobbyTransition((current) => (
                    current?.lobbyId === lobby.id ? null : current
                  ));
                }}
                onJoinLobby={(lobbyId, lobby) => {
                  setLobbyTransition({
                    lobbyId,
                    label: lobby?.name || lobbyId,
                    phase: 'loading',
                  });
                  setSelectedLobbyId(lobbyId);
                  setActiveNav('lobby-room');
                }}
              />
            </div>
          )}

          {activeNav === 'lobby-room' && (
            <div className="relative z-10 h-full">
              <LobbyRoomPage
                lobbyId={selectedLobbyId || undefined}
                onInitialLoadComplete={handleLobbyInitialLoadComplete}
                onLeaveLobby={() => {
                  setLobbyTransition(null);
                  setSelectedLobbyId('');
                  setActiveNav('lobbies');
                }}
              />
            </div>
          )}

          {activeNav === 'settings' && (
            <div className="relative z-10 h-full">
              <SettingsPage />
            </div>
          )}

          {activeNav === 'pulse' && (
            <div className="relative z-10 h-full">
              <PulsePage />
            </div>
          )}
        </Suspense>
      </main>

      {/* Right Sidebar */}
      <aside className="w-80 bg-[#0e0f13]/95 backdrop-blur-sm border-l border-[#2a2b30] flex flex-col relative z-20" style={{ boxShadow: '-8px 0 20px rgba(0,0,0,0.4), -4px 0 10px rgba(0,0,0,0.3)' }}>
        {/* Header */}
        <div className="p-6 border-b border-[#2a2b30]">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-bold">Friends</h2>
            <button
              onClick={() => {
                setShowNotifications(!showNotifications);
                setSystemNotifications((prev) => prev.map((notification) => ({ ...notification, read: true })));
              }}
              className="relative"
            >
              <Bell className="w-5 h-5 text-[#8e8f94] cursor-pointer hover:text-white transition-colors" />
              {notificationCount > 0 && (
                <div className="absolute -top-1 -right-1 min-w-4 h-4 px-1 bg-[#ff6b35] rounded-full text-[10px] font-bold flex items-center justify-center">
                  {notificationCount}
                </div>
              )}
            </button>
          </div>
          <div className="flex items-center gap-2 text-sm text-[#8e8f94]">
            <div className="w-2 h-2 rounded-full bg-[#4ecdc4]" />
            <span>{onlineFriends.length} online</span>
          </div>
        </div>

        {/* Add Friends Button */}
        <div className="px-4 pt-4 pb-2">
          <button
            onClick={() => setShowAddFriends(true)}
            className="w-full py-2.5 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg text-sm font-medium shadow-lg hover:shadow-xl glow-hover transition-all flex items-center justify-center gap-2"
          >
            <UserPlus className="w-4 h-4" />
            Add Friends
          </button>
        </div>

        {/* Friends List */}
        <div className="flex-1 overflow-auto px-4 pb-4 space-y-2">
          {friends.map((friend) => (
            <FriendCard
              key={friend.user_id}
              name={profileName(friend)}
              avatarUrl={friend.avatar_url}
              status={statusLabel(friend.presence_status || friend.status)}
              online={isProfileOnline(friend)}
              unreadCount={unreadDmByUser[friend.user_id] || 0}
              onClick={() => setSelectedFriend(friend)}
            />
          ))}
          {!socialLoading && friends.length === 0 && (
            <div className="p-6 rounded-lg border-2 border-[#2a2b30] bg-[#1c1d22]/60 text-center text-sm text-[#8e8f94]">
              No Nexus friends yet.
            </div>
          )}
          {socialLoading && friends.length === 0 && (
            <div className="p-6 rounded-lg border-2 border-[#2a2b30] bg-[#1c1d22]/60 text-center text-sm text-[#8e8f94]">
              Loading friends...
            </div>
          )}
        </div>

        {/* Notification Tooltip */}
        {showNotifications && (
          <>
            {/* Backdrop */}
            <div
              className="fixed inset-0 z-40"
              onClick={() => setShowNotifications(false)}
            />

            {/* Notification Dropdown */}
            <div className="absolute top-14 right-6 w-96 bg-[#1c1d22] rounded-xl shadow-2xl z-50 card-float border border-[#2a2b30] overflow-hidden">
              {/* Header */}
              <div className="p-4 border-b border-[#2a2b30] flex items-center justify-between">
                <div>
                  <h3 className="font-bold">Notifications</h3>
                  <p className="text-xs text-[#8e8f94]">
                    {notificationCount ? `${notificationCount} live Nexus update${notificationCount === 1 ? '' : 's'}` : 'No live notifications'}
                  </p>
                </div>
                <button
                  onClick={() => setShowNotifications(false)}
                  className="text-[#8e8f94] hover:text-white transition-colors"
                  aria-label="Close notifications"
                >
                  <X className="w-4 h-4" />
                </button>
              </div>

              {/* Notifications List */}
              <div className="max-h-96 overflow-auto">
                {systemNotifications.map((notification, index) => (
                  <NotificationItem
                    key={notification.id}
                    title={notification.title}
                    message={notification.message}
                    time="Now"
                    index={index}
                    onClick={notification.type === 'update' ? () => void handleLaunchUpdate() : undefined}
                  />
                ))}
                {incomingRequests.map((request, index) => (
                  <FriendRequestNotification
                    key={request.id || request.user_id}
                    request={request}
                    index={systemNotifications.length + index}
                    busy={friendActionBusyId === String(request.id)}
                    onAccept={() => void handleRespondFriendRequest(request, 'accept')}
                    onDecline={() => void handleRespondFriendRequest(request, 'decline')}
                  />
                ))}
                {unreadDmCount > 0 && (
                  <NotificationItem
                    title="Unread messages"
                    message={`${unreadDmCount} direct message${unreadDmCount === 1 ? '' : 's'} waiting in Nexus.`}
                    time="Now"
                    index={systemNotifications.length + incomingRequests.length}
                    onClick={handleUnreadMessagesClick}
                  />
                )}
                {notificationCount === 0 && (
                  <div className="p-8 text-center text-sm text-[#8e8f94]">
                    Nothing new from Nexus.
                  </div>
                )}
              </div>

              {/* Footer */}
              <div className="p-3 border-t border-[#2a2b30] bg-[#14151a]">
                <button
                  onClick={() => {
                    setShowNotifications(false);
                    void refreshSocial();
                  }}
                  className="w-full py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors"
                >
                  Refresh
                </button>
              </div>
            </div>
          </>
        )}

        {/* Chat Panel */}
        {selectedFriend && (
          <>
            {/* Backdrop */}
            <div
              className="fixed inset-0 bg-black/60 z-50 backdrop-blur-sm"
              onClick={() => {
                setSelectedFriend(null);
                setShowDmEmotes(false);
              }}
            />

            {/* Chat Panel */}
            <div className="fixed right-0 top-0 bottom-0 w-96 bg-[#0e0f13] shadow-2xl z-50 flex flex-col animate-slide-in-right border-l border-[#2a2b30]">
              {/* Header */}
              <div className="p-6 border-b border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#14151a]">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-3">
                    <div className="relative">
                      <ProfileAvatar name={profileName(selectedFriend)} avatarUrl={selectedFriend.avatar_url} size="lg" />
                      {isProfileOnline(selectedFriend) && (
                        <div className="absolute -bottom-0.5 -right-0.5 w-4 h-4 rounded-full border-2 border-[#0e0f13] bg-[#4ecdc4]" />
                      )}
                    </div>
                    <div>
                      <h3 className="font-bold text-lg">{profileName(selectedFriend)}</h3>
                      <p className="text-sm text-[#8e8f94]">
                        {statusLabel(selectedFriend.presence_status || selectedFriend.status)}
                      </p>
                    </div>
                  </div>
                  <button
                    onClick={() => {
                      setSelectedFriend(null);
                      setShowDmEmotes(false);
                    }}
                    className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
                  >
                    <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
                  </button>
                </div>

                {/* Quick Actions */}
                <div className="flex gap-2">
                  <div className="flex-1 py-2 px-3 bg-[#2a2b30] rounded-lg text-xs font-medium text-center text-[#8e8f94]">
                    Nexus profile
                  </div>
                  <div className="flex-1 py-2 px-3 bg-[#2a2b30] rounded-lg text-xs font-medium text-center text-[#8e8f94]">
                    {selectedFriend.user_id}
                  </div>
                </div>
              </div>

              {/* Chat Messages */}
              <div className="flex-1 overflow-auto p-4 space-y-4">
                {dmMessages.map((message) => (
                  <ChatMessage
                    key={message.id}
                    message={message.content}
                    time={formatMessageTime(message.created_at)}
                    sent={message.from_id === currentUser?.user_id}
                    lobbyLinks={liveLobbies.filter((lobby) => !lobby.is_locked).map((lobby) => ({ id: lobby.id, name: lobby.name || lobby.id }))}
                    onLobbyLinkClick={(lobby) => {
                      setSelectedFriend(null);
                      setShowDmEmotes(false);
                      setLobbyTransition({
                        lobbyId: lobby.id,
                        label: lobby.name || lobby.id,
                        phase: 'loading',
                      });
                      setSelectedLobbyId(lobby.id);
                      setActiveNav('lobby-room');
                    }}
                  />
                ))}
                {!dmLoading && dmMessages.length === 0 && (
                  <div className="text-center py-12 text-sm text-[#8e8f94]">
                    No direct messages yet.
                  </div>
                )}
                {dmLoading && dmMessages.length === 0 && (
                  <div className="text-center py-12 text-sm text-[#8e8f94]">
                    Loading messages...
                  </div>
                )}
              </div>

              {/* Input Area */}
              <div className="p-4 border-t border-[#2a2b30] bg-[#14151a]">
                <div className="relative flex gap-2">
                  <button
                    type="button"
                    onClick={() => setShowDmEmotes((value) => !value)}
                    className="w-12 h-12 rounded-lg border border-[#2a2b30] bg-[#1c1d22] text-[#8e8f94] transition-colors hover:border-[#ff6b35] hover:text-white"
                    aria-label="Show direct message emotes"
                  >
                    <Smile className="mx-auto h-5 w-5" />
                  </button>
                  {showDmEmotes && (
                    <div className="absolute bottom-[58px] left-0 z-30 grid w-72 grid-cols-4 gap-2 rounded-lg border border-[#2a2b30] bg-[#1c1d22] p-3 shadow-2xl">
                      {STANDARD_EMOTES.map((emote) => (
                        <button
                          key={emote.code}
                          type="button"
                          onClick={() => {
                            setDmDraft((value) => appendEmote(value, emote.code));
                            setShowDmEmotes(false);
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
                  <input
                    type="text"
                    value={dmDraft}
                    onChange={(event) => setDmDraft(event.target.value)}
                    onKeyDown={(event) => {
                      if (event.key === 'Enter' && !event.shiftKey) {
                        event.preventDefault();
                        void handleSendDm();
                      }
                    }}
                    placeholder="Type a message..."
                    className="flex-1 px-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#ff6b35] outline-none transition-colors"
                  />
                  <button
                    onClick={() => void handleSendDm()}
                    disabled={dmSending || !dmDraft.trim()}
                    className="w-12 h-12 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg flex items-center justify-center shadow-lg hover:shadow-xl glow-hover transition-all disabled:opacity-50"
                  >
                    <Send className="w-5 h-5" />
                  </button>
                </div>
              </div>
            </div>
          </>
        )}

        {/* Add Friends Panel */}
        {showAddFriends && (
          <>
            {/* Backdrop */}
            <div
              className="fixed inset-0 bg-black/60 z-50 backdrop-blur-sm"
              onClick={() => setShowAddFriends(false)}
            />

            {/* Add Friends Panel */}
            <div className="fixed right-0 top-0 bottom-0 w-96 bg-[#0e0f13] shadow-2xl z-50 flex flex-col animate-slide-in-right border-l border-[#2a2b30]">
              {/* Header */}
              <div className="p-6 border-b border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#14151a]">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-3">
                    <div className="w-12 h-12 rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center">
                      <UserPlus className="w-6 h-6 text-white" />
                    </div>
                    <div>
                      <h3 className="font-bold text-lg">Add Friends</h3>
                      <p className="text-sm text-[#8e8f94]">Search for users</p>
                    </div>
                  </div>
                  <button
                    onClick={() => setShowAddFriends(false)}
                    className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
                  >
                    <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
                  </button>
                </div>

                {/* Search Bar */}
                <div className="relative">
                  <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-[#8e8f94]" />
                  <input
                    type="text"
                    value={friendSearchQuery}
                    onChange={(e) => setFriendSearchQuery(e.target.value)}
                    placeholder="Search username..."
                    className="w-full pl-10 pr-4 py-3 bg-[#1c1d22] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#ff6b35] outline-none transition-colors"
                  />
                </div>
              </div>

              {/* Search Results */}
              <div className="flex-1 overflow-auto p-4">
                {friendSearchQuery.trim() === '' ? (
                  <div className="text-center py-12">
                    <UserPlus className="w-12 h-12 text-[#8e8f94] mx-auto mb-3 opacity-50" />
                    <p className="text-sm text-[#8e8f94]">Start typing to search for users</p>
                  </div>
                ) : (
                  <div className="space-y-2">
                    {friendSearchBusy && (
                      <div className="text-center py-8 text-sm text-[#8e8f94]">
                        Searching Nexus...
                      </div>
                    )}
                    {!friendSearchBusy && friendSearchResults.map((profile) => (
                      <div
                        key={profile.user_id}
                        className="p-4 bg-[#1c1d22] rounded-lg border-2 border-[#2a2b30] hover:border-[#ff6b35] transition-all"
                      >
                        <div className="flex items-center justify-between">
                          <div className="flex items-center gap-3">
                            <ProfileAvatar name={profileName(profile)} avatarUrl={profile.avatar_url} size="md" />
                            <div>
                              <div className="font-medium">{profileName(profile)}</div>
                              <div className="text-xs text-[#8e8f94]">{profile.username || profile.user_id}</div>
                            </div>
                          </div>
                          <button
                            onClick={() => void handleSendFriendRequest(profile)}
                            disabled={friendActionBusyId === profile.user_id}
                            className="px-4 py-2 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg text-sm font-medium shadow-lg hover:shadow-xl transition-all disabled:opacity-50"
                          >
                            {friendActionBusyId === profile.user_id ? 'Sending...' : 'Add'}
                          </button>
                        </div>
                      </div>
                    ))}
                    {!friendSearchBusy && friendSearchResults.length === 0 && (
                      <div className="text-center py-12">
                        <p className="text-sm text-[#8e8f94]">No users found</p>
                      </div>
                    )}
                  </div>
                )}
              </div>
            </div>
          </>
        )}
      </aside>
      </div>

      {/* Create Lobby Modal */}
      {showCreateLobby && (
        <CreateLobbyModal
          onClose={() => setShowCreateLobby(false)}
          onCreateSuccess={(lobbyId, createdLobbyName) => {
            if (lobbyId) {
              setLobbyTransition({
                lobbyId,
                label: createdLobbyName || lobbyId,
                phase: 'loading',
              });
            }
            setSelectedLobbyId(lobbyId || '');
            setActiveNav('lobby-room');
          }}
        />
      )}

      {showUpdateInstallModal && (
        <div className="fixed inset-0 z-[75] flex items-center justify-center bg-black/70 backdrop-blur-sm p-6">
          <div className="w-full max-w-md rounded-xl border border-[#2a2b30] bg-[#161b22] p-6 shadow-2xl">
            <div className="mb-5 flex items-start justify-between gap-4">
              <div>
                <p className="mb-1 text-xs font-bold uppercase tracking-[0.16em] text-[#4ecdc4]">Update</p>
                <h3 className="text-xl font-bold text-white">
                  {updateNoticeVersion
                    ? `A new version is available ${updateNoticeVersion}!`
                    : 'A new version is available!'}
                </h3>
              </div>
              <button
                type="button"
                onClick={() => setShowUpdateInstallModal(false)}
                className="flex h-9 w-9 items-center justify-center rounded-lg bg-[#2a2b30] text-[#8e8f94] transition-colors hover:bg-[#3a3b40] hover:text-white"
                aria-label="Close update prompt"
              >
                <X className="h-4 w-4" />
              </button>
            </div>
            <p className="mb-6 text-sm leading-relaxed text-[#b9babf]">
              SekaiLink needs to close and relaunch the bootloader to install it.
            </p>
            <div className="flex flex-col gap-3 sm:flex-row">
              <button
                type="button"
                onClick={() => void confirmLaunchUpdate()}
                className="flex-1 rounded-lg bg-gradient-to-r from-[#ff6b35] to-[#f38181] px-4 py-3 text-sm font-bold text-white shadow-lg transition-transform hover:scale-[1.01]"
              >
                Restart & Install
              </button>
              <button
                type="button"
                onClick={() => setShowUpdateInstallModal(false)}
                className="flex-1 rounded-lg border border-[#2a2b30] bg-[#1c1d22] px-4 py-3 text-sm font-bold text-[#b9babf] transition-colors hover:border-[#4ecdc4] hover:text-white"
              >
                Later
              </button>
            </div>
          </div>
        </div>
      )}

      {uiError && (
        <ErrorModal
          title="SekaiLink client error"
          message={uiError}
          code="CLIENT_ACTION_FAILED"
          onClose={() => setUiError('')}
        />
      )}

      {lobbyTransition && (
        <LoadingModal
          title={lobbyTransition.phase === 'joining' ? 'Joining Lobby' : 'Loading Lobby'}
          message={
            lobbyTransition.phase === 'joining'
              ? `Joining ${lobbyTransition.label}...`
              : `Loading ${lobbyTransition.label} from Nexus...`
          }
        />
      )}

      {showDiscordModal && (
        <div className="fixed inset-0 z-[70] flex items-center justify-center bg-black/70 backdrop-blur-sm p-6">
          <div className="w-full max-w-md rounded-xl border border-[#2a2b30] bg-[#161b22] p-6 shadow-2xl">
            <div className="flex items-start justify-between gap-4 mb-4">
              <div className="flex items-center gap-3">
                <div className="w-11 h-11 rounded-lg bg-[#5865f2] flex items-center justify-center">
                  <MessageCircle className="w-6 h-6" />
                </div>
                <div>
                  <h3 className="text-xl font-bold">Join the SekaiLink Discord</h3>
                  <p className="text-sm text-[#8e8f94]">Find players, testers, streamers and support.</p>
                </div>
              </div>
              <button onClick={() => setShowDiscordModal(false)} className="w-9 h-9 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] flex items-center justify-center">
                <X className="w-4 h-4" />
              </button>
            </div>
            <button
              onClick={() => {
                runtime.openExternal?.('https://discord.gg/sekailink');
                setShowDiscordModal(false);
              }}
              className="w-full py-3 rounded-lg bg-gradient-to-r from-[#5865f2] to-[#aa96da] font-bold flex items-center justify-center gap-2"
            >
              <ExternalLink className="w-4 h-4" />
              Join Discord
            </button>
          </div>
        </div>
      )}

      {showBannerModal && (
        <div className="fixed inset-0 z-[70] flex items-center justify-center bg-black/75 backdrop-blur-sm p-6">
          <div className="w-full max-w-3xl rounded-xl border border-[#2a2b30] bg-[#161b22] shadow-2xl overflow-hidden">
            <div className="relative h-56">
              <img src={NEWS_ITEMS[currentNews].image} alt={NEWS_ITEMS[currentNews].title} className="absolute inset-0 w-full h-full object-cover" />
              <div className="absolute inset-0 bg-gradient-to-t from-[#161b22] to-transparent" />
              <button onClick={() => setShowBannerModal(false)} className="absolute right-4 top-4 w-9 h-9 rounded-lg bg-black/50 hover:bg-black/70 flex items-center justify-center">
                <X className="w-4 h-4" />
              </button>
              <div className="absolute left-6 bottom-5">
                <div className="text-xs font-bold text-[#ff6b35] mb-1">{NEWS_ITEMS[currentNews].tag}</div>
                <h3 className="text-2xl font-bold">{NEWS_ITEMS[currentNews].title}</h3>
              </div>
            </div>
            <div className="p-6 space-y-4">
              <p className="text-[#b9babf]">{NEWS_ITEMS[currentNews].description}</p>
              <button
                onClick={() => runtime.openExternal?.(NEWS_ITEMS[currentNews].detailUrl)}
                className="px-5 py-2.5 rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] text-sm font-medium flex items-center gap-2"
              >
                <ExternalLink className="w-4 h-4" />
                Open details
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="fixed right-5 bottom-5 z-[80] space-y-2 pointer-events-none">
        {toasts.map((toast) => (
          <button
            key={toast.id}
            onClick={() => {
              if (toast.action?.type === 'open_dm') {
                const friend = friends.find((entry) => entry.user_id === toast.action?.userId);
                if (friend) setSelectedFriend(friend);
              }
              setToasts((prev) => prev.filter((entry) => entry.id !== toast.id));
            }}
            className="pointer-events-auto block w-80 rounded-lg border border-[#2a2b30] bg-[#161b22]/95 px-4 py-3 text-left text-sm shadow-2xl hover:border-[#ff6b35] transition-colors"
          >
            <div className="flex items-start gap-3">
              <Megaphone className="mt-0.5 w-4 h-4 text-[#ff6b35] flex-shrink-0" />
              <span>{toast.message}</span>
            </div>
          </button>
        ))}
      </div>

      {/* Floating Help Button */}
      <HelpButton />
    </div>
  );
}

export default function App() {
  if (isRuntimeLabSession()) {
    return (
      <Suspense fallback={<PageLoader />}>
        <SekaiemuRuntimeLabPage />
      </Suspense>
    );
  }
  return <RedesignShell />;
}
