import React, { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import { apiCurrentUser, apiErrorMessage, apiFetch, apiJson } from "@/services/api";
import { chatService, type ChatChannelRef } from "@/services/chatService";
import { emitToast } from "@/services/toast";
import RoomListPanel, { type RoomListRoom } from "../components/RoomListPanel";
import ChatroomPanel, { type ChatMessage } from "../components/ChatroomPanel";
import UserListPanel from "../components/UserListPanel";
import CreateRoomModal from "../components/CreateRoomModal";
import ChamferedPanel from "@/components/ChamferedPanel";
import { isSoloLobby } from "@/utils/soloLobby";
import SeedConfigModal from "@/components/SeedConfigModal";
import {
  addActiveSeed,
  ALTTP_SHOWCASE_GAME,
  listSeedGames,
  loadActiveSeeds,
  type SeedEntry,
  type SeedGameEntry,
} from "@/services/seedConfig";

type OnlineUser = { user_id: string; name: string; avatar_url?: string; status?: string; last_seen?: string };
type MeResponse = { username?: string; global_name?: string; avatar_url?: string };
const GLOBAL_CHAT_CHANNEL: ChatChannelRef = { kind: "global", locale: "fr" };

const userFacingError = (error: unknown, fallback: string) => {
  const raw = error instanceof Error ? error.message : String(error || "");
  return apiErrorMessage(raw, fallback);
};

const normalizeRoomList = (payload: any): RoomListRoom[] => {
  const rawRooms = Array.isArray(payload?.lobbies)
    ? payload.lobbies
    : Array.isArray(payload?.rooms)
      ? payload.rooms
      : [];
  return rawRooms
    .map((room: any) => {
      const id = String(room?.id || room?.room_id || "").trim();
      if (!id) return null;
      const game = String(room?.game || "").trim();
      const roomType = String(room?.room_type || "").trim();
      const slot = String(room?.slot_alias || room?.slot_name || "").trim();
      return {
        id,
        name: String(room?.name || room?.room_name || id).trim(),
        description: String(room?.description || [game, roomType].filter(Boolean).join(" · ")).trim(),
        owner: String(room?.owner || room?.owner_username || slot || "SekaiLink").trim(),
        is_private: Boolean(room?.is_private || room?.visibility === "private"),
        member_count: Number(room?.member_count || room?.player_count || 1),
        max_players: Number(room?.max_players || 50),
        last_activity: String(room?.last_activity || room?.updated_at || room?.generated_at || "").trim(),
      };
    })
    .filter(Boolean) as RoomListRoom[];
};

const loadRoomList = async (): Promise<RoomListRoom[]> => {
  return normalizeRoomList(await apiJson<any>("/api/lobbies"));
};

const DashboardPage: React.FC = () => {
  const navigate = useNavigate();
  const [rooms, setRooms] = useState<RoomListRoom[]>([]);
  const [roomsLoadError, setRoomsLoadError] = useState("");
  const [search, setSearch] = useState("");
  const [filter, setFilter] = useState<"all" | "open">("all");
  const [selectedRoomId, setSelectedRoomId] = useState<string>("");
  const selectedRoomIdRef = useRef("");
  const [onlineUsers, setOnlineUsers] = useState<OnlineUser[]>([]);
  const [createOpen, setCreateOpen] = useState(false);
  const [joinRoom, setJoinRoom] = useState<RoomListRoom | null>(null);
  const [joinPassword, setJoinPassword] = useState("");
  const [joinError, setJoinError] = useState("");
  const [joiningRoomId, setJoiningRoomId] = useState<string>("");
  const [joinConflict, setJoinConflict] = useState<{ room: RoomListRoom; extra: Record<string, unknown>; message: string } | null>(null);
  const [me, setMe] = useState<MeResponse | null>(null);
  const [quickGames, setQuickGames] = useState<SeedGameEntry[]>([ALTTP_SHOWCASE_GAME]);
  const [activeSeeds, setActiveSeeds] = useState<SeedEntry[]>(() => loadActiveSeeds());
  const [seedModal, setSeedModal] = useState<{ open: boolean; mode: "add" | "manage" | "select"; game: SeedGameEntry | null }>({
    open: false,
    mode: "select",
    game: null,
  });
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>(() => [
    { id: "sys-1", author: "System", content: "Welcome to SekaiLink.", created_at: new Date().toISOString(), kind: "system" },
  ]);

  useEffect(() => {
    selectedRoomIdRef.current = selectedRoomId;
  }, [selectedRoomId]);

  useEffect(() => {
    let cancelled = false;
    const loadRooms = async () => {
      try {
        const list = (await loadRoomList()).filter((room) => !isSoloLobby(room));
        if (cancelled) return;
        setRooms(list);
        setRoomsLoadError("");
        const currentSelectedRoomId = selectedRoomIdRef.current;
        if (!currentSelectedRoomId && list[0]?.id) setSelectedRoomId(list[0].id);
        if (currentSelectedRoomId && !list.some((room) => room.id === currentSelectedRoomId)) {
          setSelectedRoomId(list[0]?.id || "");
        }
      } catch (err) {
        if (cancelled) return;
        setRooms([]);
        setSelectedRoomId("");
        setRoomsLoadError(userFacingError(err, "Unable to load rooms."));
      }
    };
    void loadRooms();
    const roomTimer = window.setInterval(loadRooms, 5000);
    return () => { cancelled = true; window.clearInterval(roomTimer); };
  }, []);

  useEffect(() => {
    apiCurrentUser().then((data) => setMe(data)).catch(() => setMe(null));
  }, []);

  useEffect(() => {
    let cancelled = false;
    const loadGames = async () => {
      const games = await listSeedGames();
      if (!cancelled) setQuickGames(games.length ? games : [ALTTP_SHOWCASE_GAME]);
    };
    void loadGames();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    let cancelled = false;
    const loadGlobalPresence = async () => {
      try {
        await chatService.touchPresence(GLOBAL_CHAT_CHANNEL);
        const users = await chatService.listPresence(GLOBAL_CHAT_CHANNEL);
        if (!cancelled) {
          setOnlineUsers(users.map((u) => ({
            user_id: u.user_id,
            name: u.name || u.display_name || u.username || "SekaiLink",
            avatar_url: u.avatar_url || "",
            status: u.status || "online",
            last_seen: u.last_seen || "",
          })));
        }
      } catch { if (!cancelled) setOnlineUsers([]); }
    };
    void loadGlobalPresence();
    const t = window.setInterval(loadGlobalPresence, 15000);
    return () => { cancelled = true; window.clearInterval(t); };
  }, []);

  useEffect(() => {
    let cancelled = false;
    const loadGlobalMessages = async () => {
      try {
        const list = await chatService.listMessages(GLOBAL_CHAT_CHANNEL);
        if (cancelled) return;
        setChatMessages(list);
      } catch (err) {
        if (cancelled) return;
        setChatMessages([
          {
            id: "sys-chat-error",
            author: "System",
            content: userFacingError(err, "Unable to load global chat."),
            created_at: new Date().toISOString(),
            kind: "system",
          },
        ]);
      }
    };
    void loadGlobalMessages();
    const timer = window.setInterval(loadGlobalMessages, 4000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, []);

  const userAvatarByName = React.useMemo(() => {
    const out: Record<string, string> = {};
    for (const u of onlineUsers) { if (u.name && u.avatar_url) out[u.name] = u.avatar_url; }
    if (me?.global_name && me?.avatar_url) out[me.global_name] = me.avatar_url;
    if (me?.username && me?.avatar_url) out[me.username] = me.avatar_url;
    return out;
  }, [onlineUsers, me]);

  const selectedRoom = React.useMemo(
    () => rooms.find((room) => room.id === selectedRoomId) || null,
    [rooms, selectedRoomId]
  );

  const flowStep = React.useMemo(() => {
    if (roomsLoadError) {
      return {
        title: "Room service needs attention.",
        detail: roomsLoadError,
      };
    }
    if (!selectedRoom) {
      return {
        title: "Create or choose a room.",
        detail: "Game and Config Selection now happen inside the room, per player, until Ready -> Generation locks the room.",
      };
    }
    return {
      title: "Join the room, then choose games/configs.",
      detail: `Room "${selectedRoom.name}" is selected. Config Selection is room-local and will stay editable until Ready -> Generation.`,
    };
  }, [roomsLoadError, selectedRoom]);

  const requestJoinRoom = async (room: RoomListRoom) => {
    if (joiningRoomId) return;
    if (room.is_private) { setJoinRoom(room); setJoinPassword(""); setJoinError(""); return; }
    try { await joinWithPayload(room, {}); }
    catch (err) { emitToast({ message: err instanceof Error ? err.message : "Unable to join room.", kind: "error", durationMs: 9000 }); }
  };

  const joinWithPayload = async (room: RoomListRoom, extra: Record<string, unknown>) => {
    setJoiningRoomId(room.id);
    try {
      const response = await apiFetch(`/api/lobbies/${room.id}/join`, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(extra) });
      const data: any = await response.json().catch(() => ({}));
      if (!response.ok) {
        const reason = String(data?.reason || "");
        const msg = String(data?.error || "Unable to join room.");
        if (reason === "active_game_conflict") { setJoinConflict({ room, extra, message: `${msg}\n\nContinue and slow-release the previous room?` }); return; }
        throw new Error(msg);
      }
      navigate(`/lobby/${room.id}`);
    } finally { setJoiningRoomId(""); }
  };

  const confirmPrivateJoin = async () => {
    if (!joinRoom) return;
    setJoinError("");
    try { await joinWithPayload(joinRoom, { password: joinPassword }); setJoinRoom(null); }
    catch (err) { const msg = err instanceof Error ? err.message : "Unable to join room."; setJoinError(msg); emitToast({ message: msg, kind: "error", durationMs: 9000 }); }
  };

  return (
    <div className="h-full flex gap-3 p-3 overflow-hidden">
      {/* Center column */}
      <div className="flex-1 flex flex-col gap-3 min-w-0">
        <RoomListPanel
          rooms={rooms} search={search} setSearch={setSearch} filter={filter} setFilter={setFilter}
          onCreateRoom={() => setCreateOpen(true)} joinPendingRoomId={joiningRoomId}
          onJoinRoom={(room) => { void requestJoinRoom(room); }}
          selectedRoomId={selectedRoomId} setSelectedRoomId={setSelectedRoomId}
        />
        {roomsLoadError && (
          <div className="skl-app-panel" style={{ padding: "10px 14px" }}>
            <div className="text-xs font-code text-magenta/70">{roomsLoadError}</div>
          </div>
        )}
        <ChatroomPanel
          messages={chatMessages} setMessages={setChatMessages}
          currentUserName={me?.global_name || me?.username || "You"}
          currentUserAvatarUrl={me?.avatar_url} userAvatarByName={userAvatarByName}
          channel={GLOBAL_CHAT_CHANNEL}
          title="GLOBAL CHAT"
          presenceCountOverride={onlineUsers.length}
        />
      </div>

      {/* Right column */}
      <div className="w-[310px] shrink-0 flex flex-col gap-3 overflow-hidden">
        <ChamferedPanel title="QUICK SELECT" delay={0.25} className="shrink-0">
          <div className="space-y-3">
            <div className="text-[11px] font-code text-phosphor/55">
              Pick a showcase game, then add one of your seeds to the active list.
            </div>

            <div className="grid gap-2">
              {quickGames.map((game) => (
                <button
                  key={game.game_key}
                  type="button"
                  className="panel-chamfer-sm border border-teal/15 bg-black/20 px-3 py-2 text-left text-[10px] font-header tracking-widest text-phosphor/60 hover:text-teal hover:border-teal/30"
                  onClick={() => setSeedModal({ open: true, mode: "select", game })}
                >
                  {game.display_name}
                  <div className="text-[9px] font-code text-phosphor/25 mt-1">Open Select Seed</div>
                </button>
              ))}
            </div>

            <div className="flex gap-2">
              <button
                type="button"
                className="flex-1 h-8 px-3 panel-chamfer-sm bg-black/20 border border-teal/10 text-phosphor/40 text-[10px] font-header tracking-widest hover:text-phosphor/60"
                onClick={() => setCreateOpen(true)}
              >
                CREATE SYNC
              </button>
              <button
                type="button"
                className="flex-1 h-8 px-3 panel-chamfer-sm bg-teal/10 border border-teal/20 text-teal text-[10px] font-header tracking-widest disabled:opacity-40"
                disabled={!selectedRoom}
                onClick={() => {
                  if (!selectedRoom) return;
                  void requestJoinRoom(selectedRoom);
                }}
              >
                JOIN SYNC
              </button>
            </div>
          </div>
        </ChamferedPanel>
        <ChamferedPanel title="ACTIVE SEEDS" delay={0.3} className="shrink-0">
          <div className="skl-active-seeds">
            {activeSeeds.map((seed) => (
              <div className="skl-active-seed-card" key={seed.id}>
                <strong>{seed.title}</strong>
                <span>{seed.game}</span>
              </div>
            ))}
            {!activeSeeds.length && (
              <div className="panel-chamfer-sm border border-teal/10 bg-black/20 px-3 py-2.5 text-[11px] font-code text-phosphor/45">
                No active seeds yet.
              </div>
            )}
          </div>
        </ChamferedPanel>
        <UserListPanel
          users={onlineUsers.map((u) => ({ user_id: u.user_id, name: u.name, avatar_url: u.avatar_url, status: u.status, last_seen: u.last_seen, ready: false }))}
          onUserBlocked={(userId) => { setOnlineUsers((prev) => prev.filter((u) => u.user_id !== userId)); }}
        />
      </div>

      <CreateRoomModal open={createOpen} onClose={() => setCreateOpen(false)} onCreated={() => {
        loadRoomList().then((list) => {
          setRooms(list.filter((room) => !isSoloLobby(room)));
        }).catch(() => undefined);
      }} />

      <SeedConfigModal
        open={seedModal.open}
        mode={seedModal.mode}
        game={seedModal.game}
        onClose={() => setSeedModal((prev) => ({ ...prev, open: false }))}
        onSeedSelected={(seed) => {
          setActiveSeeds(addActiveSeed(seed));
        }}
        onSeedsChanged={() => setActiveSeeds(loadActiveSeeds())}
      />

      {/* Private room password modal */}
      <div className={`skl-modal${joinRoom ? " open" : ""}`} aria-hidden={!joinRoom}>
        <div className="skl-modal-backdrop" onClick={() => setJoinRoom(null)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true">
          <div className="skl-modal-header"><h3>Private Room</h3><button className="skl-modal-close" type="button" onClick={() => setJoinRoom(null)}>Close</button></div>
          <div className="skl-lobby-form">
            <label>Room Password<input type="password" className="input" autoFocus value={joinPassword} onChange={(e) => setJoinPassword(e.target.value)} onKeyDown={(e) => { if (e.key === "Enter") { e.preventDefault(); void confirmPrivateJoin(); } }} /></label>
            {joinError && <div className="skl-lobby-error show">{joinError}</div>}
            <div className="skl-modal-actions"><button className="skl-btn ghost" type="button" onClick={() => setJoinRoom(null)}>Cancel</button><button className="skl-btn primary" type="button" onClick={() => { void confirmPrivateJoin(); }}>Join</button></div>
          </div>
        </div>
      </div>

      {/* Join conflict modal */}
      <div className={`skl-modal${joinConflict ? " open" : ""}`} aria-hidden={!joinConflict}>
        <div className="skl-modal-backdrop" onClick={() => setJoinConflict(null)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true">
          <div className="skl-modal-header"><h3>Active Room Detected</h3><button className="skl-modal-close" type="button" onClick={() => setJoinConflict(null)}>Close</button></div>
          <div className="skl-modal-body" style={{ whiteSpace: "pre-line" }}>{joinConflict?.message || ""}</div>
          <div className="skl-modal-actions">
            <button className="skl-btn ghost" type="button" onClick={() => setJoinConflict(null)}>Cancel</button>
            <button className="skl-btn primary" type="button" onClick={() => {
              if (!joinConflict) return;
              const payload = { ...joinConflict.extra, force_abandon: true };
              const room = joinConflict.room;
              setJoinConflict(null);
              void joinWithPayload(room, payload).catch((err) => { emitToast({ message: err instanceof Error ? err.message : "Unable to join room.", kind: "error", durationMs: 9000 }); });
            }}>Continue</button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DashboardPage;
