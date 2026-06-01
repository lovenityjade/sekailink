import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Home, Library, Users, Settings, User, Bell, Play, Star, Clock, ChevronLeft, ChevronRight, X, Zap, UserPlus, Send, Search } from 'lucide-react';
import TitleBar from './components/TitleBar';
import AnimatedBackground from './components/AnimatedBackground';
import LobbiesPage from './components/LobbiesPage';
import CreateLobbyModal from './components/CreateLobbyModal';
import LobbyRoomPage from './components/LobbyRoomPage';
import HelpButton from './components/HelpButton';
import LibraryPage from './components/LibraryPage';
import EasyConfigPage from './components/EasyConfigPage';
import SettingsPage from './components/SettingsPage';
import { ErrorModal, LoadingModal } from './components/FeedbackModal';
import { apiCurrentUser, type CurrentUser } from '../services/api';
import { joinLobby, listLobbies, type LobbySummary } from '../services/lobbyClient';
import { ALTTP_SHOWCASE_GAME, type SeedGameEntry } from '../services/seedConfig';
import {
  listSocialSnapshot,
  loadDirectMessages,
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

const NEWS_ITEMS = [
  {
    title: "BETA-3 Showcase",
    description: "A Link to the Past is the current live showcase for lobbies, seed configs, and Sekaiemu launch.",
    tag: "SHOWCASE"
  },
  {
    title: "Live Lobby Runtime",
    description: "Lobbies, friends, room chat, and seed selections now come from Nexus.",
    tag: "LIVE"
  },
  {
    title: "Pulse Seed Setup",
    description: "Use Easy Mode for guided configs or Advanced Mode for full APWorld options.",
    tag: "PULSE"
  }
];

type UserStatus = 'online' | 'appear-offline' | 'busy' | 'afk';

const profileName = (profile: SocialProfile | null) =>
  profile?.display_name || profile?.name || profile?.username || profile?.user_id || 'Player';

const isProfileOnline = (profile: SocialProfile) => {
  const status = String(profile.presence_status || profile.status || '').toLowerCase();
  return status !== 'offline' && status !== 'appear-offline';
};

const statusLabel = (status: string) => {
  const normalized = String(status || '').toLowerCase();
  if (normalized === 'dnd' || normalized === 'busy') return 'Busy';
  if (normalized === 'afk') return 'AFK';
  if (normalized === 'offline' || normalized === 'appear-offline') return 'Offline';
  return 'Online';
};

const userStatusFromPresence = (status: string): UserStatus => {
  const normalized = String(status || '').toLowerCase();
  if (normalized === 'dnd' || normalized === 'busy') return 'busy';
  if (normalized === 'afk') return 'afk';
  if (normalized === 'offline' || normalized === 'appear-offline') return 'appear-offline';
  return 'online';
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

export default function App() {
  const [activeNav, setActiveNav] = useState('home');
  const [currentNews, setCurrentNews] = useState(0);
  const [showNotifications, setShowNotifications] = useState(false);
  const [selectedFriend, setSelectedFriend] = useState<SocialProfile | null>(null);
  const [userStatus, setUserStatus] = useState<UserStatus>('online');
  const [showStatusMenu, setShowStatusMenu] = useState(false);
  const [showCreateLobby, setShowCreateLobby] = useState(false);
  const [showAddFriends, setShowAddFriends] = useState(false);
  const [friendSearchQuery, setFriendSearchQuery] = useState('');
  const [currentUser, setCurrentUser] = useState<CurrentUser | null>(null);
  const [selectedLobbyId, setSelectedLobbyId] = useState('');
  const [easyConfigGame, setEasyConfigGame] = useState<SeedGameEntry>(ALTTP_SHOWCASE_GAME);
  const [liveLobbies, setLiveLobbies] = useState<LobbySummary[]>([]);
  const [lobbiesLoading, setLobbiesLoading] = useState(true);
  const [joiningLobbyId, setJoiningLobbyId] = useState('');
  const [friends, setFriends] = useState<SocialProfile[]>([]);
  const [incomingRequests, setIncomingRequests] = useState<SocialRequest[]>([]);
  const [outgoingRequests, setOutgoingRequests] = useState<SocialRequest[]>([]);
  const [unreadDmCount, setUnreadDmCount] = useState(0);
  const [socialLoading, setSocialLoading] = useState(true);
  const [friendSearchResults, setFriendSearchResults] = useState<SocialProfile[]>([]);
  const [friendSearchBusy, setFriendSearchBusy] = useState(false);
  const [friendActionBusyId, setFriendActionBusyId] = useState('');
  const [dmMessages, setDmMessages] = useState<SocialDirectMessage[]>([]);
  const [dmDraft, setDmDraft] = useState('');
  const [dmLoading, setDmLoading] = useState(false);
  const [dmSending, setDmSending] = useState(false);
  const [uiError, setUiError] = useState('');
  const [lobbyTransition, setLobbyTransition] = useState<{
    lobbyId: string;
    label: string;
    phase: 'joining' | 'loading';
  } | null>(null);
  const lobbiesRefreshInFlight = useRef(false);
  const socialRefreshInFlight = useRef(false);

  useEffect(() => {
    trace('redesign-app', 'mounted');
    return () => trace('redesign-app', 'unmounted');
  }, []);

  useEffect(() => {
    trace('redesign-app', 'navigation_changed', { activeNav, selectedLobbyId });
  }, [activeNav, selectedLobbyId]);

  useEffect(() => {
    const interval = setInterval(() => {
      setCurrentNews((prev) => (prev + 1) % NEWS_ITEMS.length);
    }, 5000); // Change news every 5 seconds

    return () => clearInterval(interval);
  }, []);

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
          setCurrentUser(null);
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
      setUserStatus(userStatusFromPresence(snapshot.presenceStatus));
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
    } finally {
      setSocialLoading(false);
      socialRefreshInFlight.current = false;
    }
  }, []);

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
        if (!cancelled) setDmMessages(next);
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
  }, [selectedFriend?.user_id]);

  const onlineFriends = useMemo(() => friends.filter(isProfileOnline), [friends]);
  const notificationCount = incomingRequests.length + unreadDmCount;
  const activeHomeLobbies = useMemo(() => liveLobbies.filter((lobby) => !lobby.is_locked).slice(0, 3), [liveLobbies]);

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
            <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center shadow-lg">
              <span className="text-white font-bold text-xl">S</span>
            </div>
            <div>
              <div className="font-bold text-lg">SekaiLink</div>
              <div className="text-xs text-[#8e8f94]">Client Core</div>
            </div>
          </div>
        </div>

        {/* Navigation */}
        <nav className="flex-1 p-4 space-y-1">
          <NavItem icon={<Home className="w-5 h-5" />} label="Home" active={activeNav === 'home'} onClick={() => setActiveNav('home')} />
          <NavItem icon={<Library className="w-5 h-5" />} label="Library" active={activeNav === 'library'} onClick={() => setActiveNav('library')} />
          <NavItem icon={<Users className="w-5 h-5" />} label="Lobbies" active={activeNav === 'lobbies'} onClick={() => setActiveNav('lobbies')} />
          <NavItem icon={<Settings className="w-5 h-5" />} label="Settings" active={activeNav === 'settings'} onClick={() => setActiveNav('settings')} />
        </nav>

        {/* User Profile */}
        <div className="p-4 border-t border-[#2a2b30] relative">
          <div
            onClick={() => setShowStatusMenu(!showStatusMenu)}
            className="flex items-center gap-3 p-3 rounded-lg bg-[#1c1d22] hover:bg-[#2a2b30] transition-colors cursor-pointer"
          >
            <div className="relative">
              <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#4ecdc4] to-[#95e1d3] flex items-center justify-center">
                <User className="w-5 h-5 text-white" />
              </div>
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
          <div className="relative bg-gradient-to-r from-[#1c1d22] via-[#2a2b30] to-[#1c1d22] rounded-xl p-6 card-float overflow-hidden">
            <div className="absolute inset-0 bg-gradient-to-r from-[#ff6b35]/10 to-transparent" />
            <div className="relative flex items-center justify-between">
              <button
                onClick={() => setCurrentNews((prev) => (prev - 1 + NEWS_ITEMS.length) % NEWS_ITEMS.length)}
                className="w-8 h-8 flex items-center justify-center rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] transition-colors"
              >
                <ChevronLeft className="w-5 h-5" />
              </button>

              <div className="flex-1 mx-6 text-center">
                <div className="inline-block px-3 py-1 bg-[#ff6b35] rounded-full text-xs font-bold mb-2">
                  {NEWS_ITEMS[currentNews].tag}
                </div>
                <h3 className="text-xl font-bold mb-1">{NEWS_ITEMS[currentNews].title}</h3>
                <p className="text-sm text-[#8e8f94]">{NEWS_ITEMS[currentNews].description}</p>
              </div>

              <button
                onClick={() => setCurrentNews((prev) => (prev + 1) % NEWS_ITEMS.length)}
                className="w-8 h-8 flex items-center justify-center rounded-lg bg-[#2a2b30] hover:bg-[#3a3b40] transition-colors"
              >
                <ChevronRight className="w-5 h-5" />
              </button>
            </div>

            {/* Indicator dots */}
            <div className="flex justify-center gap-2 mt-4">
              {NEWS_ITEMS.map((_, index) => (
                <button
                  key={index}
                  onClick={() => setCurrentNews(index)}
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
            />
            <StatsCard
              icon={<Users className="w-6 h-6" />}
              label="Friends Online"
              value={String(onlineFriends.length)}
              change={`${friends.length} friends on Nexus`}
              color="from-[#4ecdc4] to-[#95e1d3]"
            />
            <StatsCard
              icon={<Star className="w-6 h-6" />}
              label="Pending Requests"
              value={String(incomingRequests.length)}
              change={outgoingRequests.length ? `${outgoingRequests.length} sent` : 'No outgoing requests'}
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
            <div className="space-y-3">
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
      </main>

      {/* Right Sidebar */}
      <aside className="w-80 bg-[#0e0f13]/95 backdrop-blur-sm border-l border-[#2a2b30] flex flex-col relative z-20" style={{ boxShadow: '-8px 0 20px rgba(0,0,0,0.4), -4px 0 10px rgba(0,0,0,0.3)' }}>
        {/* Header */}
        <div className="p-6 border-b border-[#2a2b30]">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-bold">Friends</h2>
            <button
              onClick={() => setShowNotifications(!showNotifications)}
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
              status={statusLabel(friend.presence_status || friend.status)}
              online={isProfileOnline(friend)}
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
                {incomingRequests.map((request, index) => (
                  <FriendRequestNotification
                    key={request.id || request.user_id}
                    request={request}
                    index={index}
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
                    index={incomingRequests.length}
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
              onClick={() => setSelectedFriend(null)}
            />

            {/* Chat Panel */}
            <div className="fixed right-0 top-0 bottom-0 w-96 bg-[#0e0f13] shadow-2xl z-50 flex flex-col animate-slide-in-right border-l border-[#2a2b30]">
              {/* Header */}
              <div className="p-6 border-b border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#14151a]">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-3">
                    <div className="relative">
                      <div className="w-12 h-12 rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center font-bold text-lg">
                        {profileName(selectedFriend).charAt(0)}
                      </div>
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
                    onClick={() => setSelectedFriend(null)}
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
                <div className="flex gap-2">
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
                            <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#4ecdc4] to-[#95e1d3] flex items-center justify-center font-bold text-sm">
                              {profileName(profile).charAt(0)}
                            </div>
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

      {/* Floating Help Button */}
      <HelpButton />
    </div>
  );
}

function NavItem({ icon, label, active, onClick }: {
  icon: React.ReactNode;
  label: string;
  active: boolean;
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={`
        w-full flex items-center gap-3 px-4 py-3 rounded-lg transition-all
        ${active
          ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white shadow-lg'
          : 'text-[#8e8f94] hover:bg-[#1c1d22] hover:text-white'
        }
      `}
    >
      {icon}
      <span className="font-medium">{label}</span>
    </button>
  );
}

function StatsCard({ icon, label, value, change, color }: {
  icon: React.ReactNode;
  label: string;
  value: string;
  change: string;
  color: string;
}) {
  return (
    <div className="bg-gradient-card rounded-xl p-6 card-float card-float-hover">
      <div className={`w-12 h-12 rounded-lg bg-gradient-to-br ${color} flex items-center justify-center mb-4 shadow-lg`}>
        <div className="text-white">{icon}</div>
      </div>
      <div className="text-3xl font-bold mb-1">{value}</div>
      <div className="text-sm text-white mb-1">{label}</div>
      <div className="text-xs text-[#8e8f94]">{change}</div>
    </div>
  );
}

function GameCard({ title, platform, status, image, disabled }: {
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
      {/* Image Area */}
      <div className="h-48 bg-gradient-to-br from-[#2a2b30] to-[#1c1d22] flex items-center justify-center relative overflow-hidden">
        <div className="absolute inset-0 bg-gradient-to-t from-[#1c1d22] via-transparent to-transparent" />
        <div className="text-6xl font-bold text-[#2a2b30] z-10">{image}</div>
        {!disabled && (
          <div className="absolute top-4 right-4 px-3 py-1 bg-[#ff6b35] rounded-full text-xs font-medium shadow-lg">
            {platform}
          </div>
        )}
      </div>

      {/* Content */}
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

function FriendCard({ name, status, lobbyId, online, inGame, onClick }: {
  name: string;
  status: string;
  lobbyId?: string;
  online?: boolean;
  inGame?: boolean;
  onClick?: () => void;
}) {
  return (
    <div onClick={onClick} className="p-3 rounded-lg bg-[#1c1d22] hover:bg-[#2a2b30] transition-colors cursor-pointer">
      <div className="flex items-center gap-3">
        <div className="relative">
          <div className="w-10 h-10 rounded-full bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center text-sm font-bold">
            {name.charAt(0)}
          </div>
          {online && (
            <div className={`absolute -bottom-0.5 -right-0.5 w-3 h-3 rounded-full border-2 border-[#1c1d22] ${
              inGame ? 'bg-[#ff6b35]' : 'bg-[#4ecdc4]'
            }`} />
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
  );
}

function NotificationItem({ title, message, time, index }: {
  title: string;
  message: string;
  time: string;
  index: number;
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
    <div className={`p-4 border-l-2 ${accent.replace('bg-', 'border-')} bg-gradient-to-r ${gradient} hover:bg-[#2a2b30]/30 transition-colors cursor-pointer relative overflow-hidden`}>
      <div className={`absolute left-0 top-0 bottom-0 w-1 ${accent}`} />
      <div className="ml-2">
        <div className="flex items-start justify-between gap-2 mb-1">
          <h4 className="font-semibold text-sm">{title}</h4>
          <span className="text-[10px] text-[#8e8f94] whitespace-nowrap">{time}</span>
        </div>
        <p className="text-xs text-[#8e8f94] leading-relaxed">{message}</p>
      </div>
    </div>
  );
}

function FriendRequestNotification({ request, index, busy, onAccept, onDecline }: {
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

function LobbyCard({ name, host, seedsInSync, players, maxPlayers, status, joining, onJoin }: {
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
    <div className={`p-5 bg-gradient-to-r ${statusConfig.gradient} border-2 border-[#2a2b30] rounded-lg hover:border-[#ff6b35] transition-all cursor-pointer card-float-hover`}>
      <div className="flex items-start justify-between mb-4">
        <div className="flex-1">
          <div className="flex items-center gap-3 mb-2">
            <h3 className="text-lg font-bold">{name}</h3>
            <span className={`${statusConfig.bg} text-[#14151a] px-3 py-1 rounded-full text-xs font-bold`}>
              {statusConfig.text}
            </span>
          </div>
          <div className="flex items-center gap-4 text-sm text-[#8e8f94]">
            <div className="flex items-center gap-1.5">
              <User className="w-4 h-4" />
              <span>Host: {host}</span>
            </div>
            <div className="flex items-center gap-1.5">
              <Zap className="w-4 h-4" />
              <span>Seeds in Sync: {seedsInSync}</span>
            </div>
          </div>
        </div>
        <button
          onClick={onJoin}
          disabled={joining}
          className="px-5 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg text-sm font-medium transition-colors disabled:opacity-50"
        >
          {joining ? 'Joining...' : 'Join'}
        </button>
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

function ChatMessage({ message, time, sent }: {
  message: string;
  time: string;
  sent: boolean;
}) {
  return (
    <div className={`flex ${sent ? "justify-end" : "justify-start"}`}>
      <div className={`max-w-[75%] ${sent ? "order-1" : "order-2"}`}>
        <div
          className={`px-4 py-3 rounded-2xl shadow-lg ${
            sent
              ? "bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white rounded-br-sm"
              : "bg-[#1c1d22] text-white rounded-bl-sm"
          }`}
        >
          <p className="text-sm leading-relaxed">{message}</p>
        </div>
        <div className={`mt-1 px-2 text-[10px] text-[#8e8f94] ${sent ? "text-right" : "text-left"}`}>
          {time}
        </div>
      </div>
    </div>
  );
}

function StatusMenuItem({ status, label, color, isActive, onClick }: {
  status: string;
  label: string;
  color: string;
  isActive: boolean;
  onClick: () => void;
}) {
  const gradients = {
    "bg-[#4ecdc4]": "from-[#4ecdc4]/10 to-transparent",
    "bg-[#ff6b35]": "from-[#ff6b35]/10 to-transparent",
    "bg-[#f69d50]": "from-[#f69d50]/10 to-transparent",
    "bg-[#8e8f94]": "from-[#8e8f94]/10 to-transparent"
  };

  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-lg transition-all ${
        isActive
          ? `bg-gradient-to-r ${gradients[color as keyof typeof gradients]} border border-[#2a2b30]`
          : "hover:bg-[#2a2b30]"
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
