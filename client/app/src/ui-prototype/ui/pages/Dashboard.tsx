import React, { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { apiFetch, apiJson } from "@/services/api";
import { emitToast } from "@/services/toast";
import RoomListPanel, { type RoomListRoom } from "../components/RoomListPanel";
import ChatroomPanel, { type ChatMessage } from "../components/ChatroomPanel";
import GameManagerPanel from "../components/GameManagerPanel";
import UserListPanel from "../components/UserListPanel";
import CreateRoomModal from "../components/CreateRoomModal";
import { isSoloLobby } from "@/utils/soloLobby";

type LobbyListResponse = { lobbies: RoomListRoom[] };

type OnlineUser = {
  name: string;
  avatar_url?: string;
  status?: string;
};

type MeResponse = {
  username?: string;
  global_name?: string;
  avatar_url?: string;
};

const DashboardPage: React.FC = () => {
  const navigate = useNavigate();
  const [rooms, setRooms] = useState<RoomListRoom[]>([]);
  const [search, setSearch] = useState("");
  const [filter, setFilter] = useState<"all" | "open" | "fn">("all");
  const [selectedRoomId, setSelectedRoomId] = useState<string>("");
  const [onlineUsers, setOnlineUsers] = useState<OnlineUser[]>([]);
  const [createOpen, setCreateOpen] = useState(false);
  const [joinRoom, setJoinRoom] = useState<RoomListRoom | null>(null);
  const [joinPassword, setJoinPassword] = useState("");
  const [joinError, setJoinError] = useState("");
  const [me, setMe] = useState<MeResponse | null>(null);
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>(() => [
    { id: "sys-1", author: "System", content: "Welcome to SekaiLink.", created_at: new Date().toISOString(), kind: "system" },
  ]);

  useEffect(() => {
    let cancelled = false;
    apiJson<LobbyListResponse>("/api/lobbies")
      .then((data) => {
        if (cancelled) return;
        const list = (Array.isArray(data?.lobbies) ? data.lobbies : []).filter((room) => !isSoloLobby(room));
        setRooms(list);
        if (!selectedRoomId && list[0]?.id) setSelectedRoomId(list[0].id);
      })
      .catch(() => {
        if (cancelled) return;
        // Offline / no-backend fallback
        const now = new Date().toISOString();
        const fallback: RoomListRoom[] = [
          { id: "mock-1", name: "ZXCCZZ", owner: "TheLovelyJade", is_private: false, member_count: 1, last_activity: now },
          { id: "mock-2", name: "SNES Test", owner: "TheLovelyJade", is_private: false, member_count: 0, last_activity: now },
        ];
        setRooms(fallback);
        if (!selectedRoomId) setSelectedRoomId("mock-1");
      });
    return () => {
      cancelled = true;
    };
  }, [selectedRoomId]);

  // Prove autoscroll behavior with fake incoming messages.
  useEffect(() => {
    // TODO: replace with real global chat (Redis-backed) via /api/global/messages + socket event.
    // For now: do not spam fake messages.
    return undefined;
  }, []);

  useEffect(() => {
    apiJson<MeResponse>("/api/me")
      .then((data) => setMe(data))
      .catch(() => setMe(null));
  }, []);

  useEffect(() => {
    let cancelled = false;

    const loadOnline = async () => {
      try {
        const data: any = await apiJson("/api/online-users");
        const list = Array.isArray(data?.users) ? data.users : [];
        const normalized = list
          .map((u: any) => ({
            name: String(u?.display_name || u?.name || u?.username || "").trim(),
            avatar_url: typeof u?.avatar_url === "string" ? u.avatar_url : "",
            status: typeof u?.status === "string" ? u.status : (u?.presence_status ? String(u.presence_status) : "online"),
          }))
          .filter((u: any) => u.name);
        if (!cancelled) setOnlineUsers(normalized);
      } catch {
        if (!cancelled) setOnlineUsers([]);
      }
    };

    void loadOnline();
    const t = window.setInterval(loadOnline, 15000);
    return () => {
      cancelled = true;
      window.clearInterval(t);
    };
  }, []);

  const userAvatarByName = React.useMemo(() => {
    const out: Record<string, string> = {};
    for (const u of onlineUsers) {
      if (u.name && u.avatar_url) out[u.name] = u.avatar_url;
    }
    if (me?.global_name && me?.avatar_url) out[me.global_name] = me.avatar_url;
    if (me?.username && me?.avatar_url) out[me.username] = me.avatar_url;
    return out;
  }, [onlineUsers, me]);

  const requestJoinRoom = async (room: RoomListRoom) => {
    if (room.is_private) {
      setJoinRoom(room);
      setJoinPassword("");
      setJoinError("");
      return;
    }
    await joinWithPayload(room, {});
  };

  const joinWithPayload = async (room: RoomListRoom, extra: Record<string, unknown>) => {
    const response = await apiFetch(`/api/lobbies/${room.id}/join`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(extra),
    });
    const data: any = await response.json().catch(() => ({}));
    if (!response.ok) {
      const reason = String(data?.reason || "");
      const msg = String(data?.error || "Unable to join room.");
      if (reason === "active_game_conflict") {
        const ok = window.confirm(`${msg}\n\nContinue and slow-release the previous room?`);
        if (!ok) return;
        await joinWithPayload(room, { ...extra, force_abandon: true });
        return;
      }
      throw new Error(msg);
    }
    navigate(`/lobby/${room.id}`);
  };

  const confirmPrivateJoin = async () => {
    if (!joinRoom) return;
    setJoinError("");
    try {
      await joinWithPayload(joinRoom, { password: joinPassword });
      setJoinRoom(null);
    } catch (err) {
      const msg = err instanceof Error ? err.message : "Unable to join room.";
      setJoinError(msg);
      emitToast({ message: msg, kind: "error", durationMs: 9000 });
    }
  };

  return (
    <div className="sklp-dashboard">
      <div className="sklp-main">
        <RoomListPanel
          rooms={rooms}
          search={search}
          setSearch={setSearch}
          filter={filter}
          setFilter={setFilter}
          onCreateRoom={() => setCreateOpen(true)}
          onJoinRoom={(room) => { void requestJoinRoom(room); }}
          selectedRoomId={selectedRoomId}
          setSelectedRoomId={setSelectedRoomId}
        />

        <ChatroomPanel
          messages={chatMessages}
          setMessages={setChatMessages}
          currentUserName={me?.global_name || me?.username || "You"}
          currentUserAvatarUrl={me?.avatar_url}
          userAvatarByName={userAvatarByName}
        />
      </div>

      <aside className="sklp-right">
        <GameManagerPanel
          games={[
            { name: "ALTTP - Unconfigured", sub: "Team: 1 - Main: Standard" },
            { name: "Emerald - Unconfigured", sub: "ROM: ? - Tracker: ?seed" },
            { name: "FireRed - Kanto Only - Normal", sub: "Team: 1 - Main: Normal" },
            { name: "SNES Test", sub: "Game: smw.com:6416" },
          ]}
          onSelectGame={(name) => {
            setChatMessages((prev) => [
              ...prev,
              { id: `gm-${Date.now()}`, author: "System", content: `Active game set to ${name}`, created_at: new Date().toISOString(), kind: "system" },
            ]);
            emitToast({ message: `Active game set to ${name}`, kind: "success", durationMs: 9000 });
          }}
        />

        <UserListPanel
          users={
            onlineUsers.map((u) => ({
              name: u.name,
              avatar_url: u.avatar_url,
              status: u.status,
              ready: false,
            }))
          }
        />
      </aside>

      <CreateRoomModal
        open={createOpen}
        onClose={() => setCreateOpen(false)}
        onCreated={() => {
          // Best-effort refresh for the room list panel.
          apiJson<LobbyListResponse>("/api/lobbies")
            .then((data) => {
              const list = (Array.isArray(data?.lobbies) ? data.lobbies : []).filter((room) => !isSoloLobby(room));
              setRooms(list);
            })
            .catch(() => undefined);
        }}
      />

      <div className={`skl-modal${joinRoom ? " open" : ""}`} aria-hidden={!joinRoom}>
        <div className="skl-modal-backdrop" onClick={() => setJoinRoom(null)}></div>
        <div className="skl-modal-panel" role="dialog" aria-modal="true" aria-labelledby="join-private-room-title">
          <div className="skl-modal-header">
            <h3 id="join-private-room-title">Private Room</h3>
            <button className="skl-modal-close" type="button" onClick={() => setJoinRoom(null)}>Close</button>
          </div>
          <div className="skl-lobby-form">
            <label>
              Room Password
              <input
                type="password"
                className="input"
                autoFocus
                value={joinPassword}
                onChange={(e) => setJoinPassword(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    e.preventDefault();
                    void confirmPrivateJoin();
                  }
                }}
              />
            </label>
            {joinError && <div className="skl-lobby-error show">{joinError}</div>}
            <div className="skl-modal-actions">
              <button className="skl-btn ghost" type="button" onClick={() => setJoinRoom(null)}>Cancel</button>
              <button className="skl-btn primary" type="button" onClick={() => { void confirmPrivateJoin(); }}>Join</button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DashboardPage;
