import datetime
import hmac
import json
import os
import time
import uuid
from typing import Any

import redis
from flask import session, request
from flask_socketio import emit, join_room, leave_room
from pony.orm import db_session, select, desc

from NetUtils import ClientStatus
from WebHostLib import app, socketio, to_python, to_url
from WebHostLib.misc import _desktop_token_auth_context
from WebHostLib.tracker import TrackerData
from .lobbies import (
    _display_name_for,
    _avatar_url_for,
    _ban_notice,
    _add_system_message,
    _latest_generated_room,
    _member_is_active_in_room,
    _member_player_aliases,
    _get_slow_release_timeout,
)
from .models import Lobby, LobbyGeneration, LobbyMember, LobbyMemberYaml, LobbyMessage, LobbyBan, Room, User, Command, Friendship


REDIS_URL = os.environ.get("REDIS_URL", "redis://127.0.0.1:6379/0")
_redis = redis.Redis.from_url(REDIS_URL, decode_responses=True)

_lobby_clients: dict[str, int] = {}
_lobby_user_counts: dict[tuple[str, str], int] = {}
_sid_user: dict[str, str] = {}
_room_watchers: dict[str, int] = {}
_tracker_watchers: dict[str, int] = {}
_tailer_tasks: dict[str, Any] = {}
_tracker_tasks: dict[str, Any] = {}
_sid_lobby: dict[str, str] = {}
_user_connections: dict[str, int] = {}
_user_active_room: dict[str, str | None] = {}
_presence_event_at: dict[tuple[str, str, str], float] = {}
_INACTIVITY_STATE_PREFIX = "room:inactivity:state:"
_INACTIVITY_GRACE_SECONDS = 90
_PRESENCE_EVENT_DEBOUNCE_SECONDS = 20


def _normalize_player_name(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip().lower()


def _parse_room_uuid(value: Any) -> uuid.UUID | None:
    raw = str(value or "").strip()
    if not raw:
        return None
    try:
        return to_python(raw)
    except Exception:
        pass
    try:
        return uuid.UUID(raw)
    except Exception:
        return None


def _inactivity_state_key(room_id: str, player_name: str) -> str:
    return f"{_INACTIVITY_STATE_PREFIX}{room_id}:{_normalize_player_name(player_name)}"


def _load_inactivity_state(room_id: str, player_name: str) -> dict[str, Any]:
    try:
        raw = _redis.get(_inactivity_state_key(room_id, player_name))
        if not raw:
            return {}
        return json.loads(raw)
    except Exception:
        return {}


def _save_inactivity_state(room_id: str, player_name: str, state: dict[str, Any]) -> None:
    try:
        _redis.set(_inactivity_state_key(room_id, player_name), json.dumps(state), ex=14 * 24 * 60 * 60)
    except Exception:
        pass


def _clear_inactivity_state(room_id: str, player_name: str) -> None:
    try:
        _redis.delete(_inactivity_state_key(room_id, player_name))
    except Exception:
        pass


def _seconds_since(ts: float | int | None) -> float:
    if not ts:
        return 0.0
    return max(0.0, time.time() - float(ts))


def _should_emit_presence_event(lobby_id: str, user_id: str, event: str) -> bool:
    key = (lobby_id, user_id, event)
    now = time.time()
    last = _presence_event_at.get(key, 0.0)
    if now - last < _PRESENCE_EVENT_DEBOUNCE_SECONDS:
        return False
    _presence_event_at[key] = now
    return True


def _get_bearer_token_from_request() -> str | None:
    auth = request.headers.get("Authorization", "")
    token = None
    if auth:
        parts = auth.split(None, 1)
        if len(parts) == 2 and parts[0].lower() == "bearer":
            token = parts[1].strip() or None
    if not token:
        token = (request.args.get("token") or request.args.get("desktop_token") or "").strip() or None
    return token


def _looks_like_desktop_runtime_request() -> bool:
    ua = (request.headers.get("User-Agent") or "").lower()
    origin = (request.headers.get("Origin") or "").lower()
    is_electron = "electron/" in ua
    origin_ok = (not origin) or origin == "null" or origin.startswith("file://") or origin.startswith("app://")
    return is_electron and origin_ok


def _has_valid_sekailink_client_key() -> bool:
    expected = str(app.config.get("SEKAILINK_CLIENT_API_KEY", "") or "").strip()
    if not expected:
        return False
    provided = (request.headers.get("X-SekaiLink-Client-Key") or "").strip()
    if not provided:
        return False
    return hmac.compare_digest(expected, provided)


def _is_authorized_sekailink_client_request() -> bool:
    token = _get_bearer_token_from_request()
    if not token:
        return False
    if not _desktop_token_auth_context(token):
        return False

    client_hint = (request.headers.get("X-SekaiLink-Client") or "").strip().lower()
    if client_hint in {"desktop", "sekailink-client"}:
        return True
    if _has_valid_sekailink_client_key():
        return True
    return _looks_like_desktop_runtime_request()


def _lobby_room(lobby_id: str) -> str:
    return f"lobby:{lobby_id}"


def _user_room(discord_id: str) -> str:
    return f"user:{discord_id}"


def _get_session_user() -> User | None:
    discord = session.get("discord_user")
    if discord:
        discord_id = discord.get("id")
        if not discord_id:
            return None
        user = User.get(discord_id=discord_id)
        if user and user.banned:
            return None
        return user
    token = _get_bearer_token_from_request()
    if not token:
        return None
    context = _desktop_token_auth_context(token)
    if not context:
        return None
    discord_id = context.get("id")
    if not discord_id:
        return None
    user = User.get(discord_id=discord_id)
    if user and user.banned:
        return None
    return user



def _get_active_ban(lobby: Lobby, user: User) -> LobbyBan | None:
    now = datetime.datetime.utcnow()
    return select(
        b for b in LobbyBan
        if b.lobby == lobby
        and b.user == user
        and (b.expires_at is None or b.expires_at > now)
    ).order_by(desc(LobbyBan.created_at)).first()


def _emit_members_update(lobby: Lobby) -> None:
    now = datetime.datetime.utcnow()
    members = select(m for m in LobbyMember if m.lobby == lobby).order_by(LobbyMember.joined_at)[:200]
    payload = [
        {
            "name": _display_name_for(member.user),
            "discord_id": member.user.discord_id,
            "active_yaml_id": member.active_yaml_id,
            "active_yaml_title": member.active_yaml_title,
            "active_yaml_game": member.active_yaml_game,
            "active_yaml_player": member.active_yaml_player,
            "active_yamls": (
                [
                    {
                        "id": y.yaml_id,
                        "title": y.title,
                        "game": y.game,
                        "player_name": y.player_name,
                    }
                    for y in select(y for y in LobbyMemberYaml if y.member == member).order_by(LobbyMemberYaml.created_at)
                ]
                or (
                    [{
                        "id": member.active_yaml_id,
                        "title": member.active_yaml_title,
                        "game": member.active_yaml_game,
                        "player_name": member.active_yaml_player,
                    }]
                    if member.active_yaml_id
                    else []
                )
            ),
            "ready": member.ready,
            "online": bool(member.last_seen and now - member.last_seen < datetime.timedelta(minutes=3)),
            "is_host": member.user == lobby.owner,
        }
        for member in members
    ]
    socketio.emit("members_update", {"members": payload}, to=_lobby_room(to_url(lobby.id)))


def _effective_presence(user: User) -> str:
    if user.presence_status == "offline":
        return "offline"
    if user.presence_status == "dnd":
        return "dnd" if user.is_online else "offline"
    return "online" if user.is_online else "offline"


def _emit_friend_presence(user: User) -> None:
    status = _effective_presence(user)
    friends = select(f.friend for f in Friendship if f.user == user)
    payload = {
        "user_id": user.discord_id,
        "display_name": _display_name_for(user),
        "status": status,
        "is_online": status == "online",
    }
    for friend in friends:
        socketio.emit("friend_presence", payload, to=_user_room(friend.discord_id))


def _emit_friend_activity(user: User, event_type: str, payload: dict) -> None:
    friends = select(f.friend for f in Friendship if f.user == user)
    base = {
        "user_id": user.discord_id,
        "display_name": _display_name_for(user),
        "type": event_type,
    }
    base.update(payload)
    for friend in friends:
        socketio.emit("friend_activity", base, to=_user_room(friend.discord_id))


def _emit_generation_update(lobby: Lobby) -> None:
    latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
    if not latest:
        socketio.emit("generation_update", {"status": "none"}, to=_lobby_room(to_url(lobby.id)))
        return
    payload = {"status": latest.status, "error": latest.error or ""}
    if latest.seed_id:
        payload["seed_url"] = f"/seed/{to_url(latest.seed_id)}"
    if latest.room_id:
        payload["room_url"] = f"/room/{to_url(latest.room_id)}"
    if latest.completed_at:
        payload["completed_at"] = latest.completed_at.isoformat()
    socketio.emit("generation_update", payload, to=_lobby_room(to_url(lobby.id)))


def _send_existing_log(room_id: str, sid: str) -> None:
    log_path = os.path.join("logs", f"{room_id}.txt")
    if not os.path.exists(log_path):
        return
    try:
        with open(log_path, "rb") as f:
            content = f.read()
        if content:
            socketio.emit("terminal_output",
                          {"text": content.decode("utf-8", errors="ignore")},
                          to=sid)
    except Exception:
        pass


@socketio.on("connect")
def connect_socket():
    with db_session:
        user = _get_session_user()
        if not user:
            return
        join_room(_user_room(user.discord_id))
        _sid_user[request.sid] = user.discord_id
        _user_connections[user.discord_id] = _user_connections.get(user.discord_id, 0) + 1
        if _user_connections[user.discord_id] == 1:
            user.is_online = True
            user.last_seen_at = datetime.datetime.utcnow()
            _emit_friend_presence(user)


def _ensure_room_watchers(lobby: Lobby) -> str | None:
    latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None)\
        .order_by(desc(LobbyGeneration.created_at)).first()
    if not latest or not latest.room_id:
        return None
    room_id = str(latest.room_id)
    room_key = room_id
    _room_watchers[room_key] = _room_watchers.get(room_key, 0) + 1
    if room_key not in _tailer_tasks:
        _tailer_tasks[room_key] = socketio.start_background_task(_tail_room_log, lobby.id, room_id)
    tracker = Room.get(id=latest.room_id).tracker if Room.get(id=latest.room_id) else None
    if tracker:
        tracker_key = str(tracker)
        _tracker_watchers[tracker_key] = _tracker_watchers.get(tracker_key, 0) + 1
        if tracker_key not in _tracker_tasks:
            _tracker_tasks[tracker_key] = socketio.start_background_task(_track_room_stats, lobby.id, tracker_key)
    return room_id


def _ensure_room_watchers_for_room(lobby: Lobby, room_uuid: uuid.UUID) -> None:
    room = Room.get(id=room_uuid)
    if not room:
        return
    room_key = str(room.id)
    _room_watchers[room_key] = _room_watchers.get(room_key, 0) + 1
    if room_key not in _tailer_tasks:
        _tailer_tasks[room_key] = socketio.start_background_task(_tail_room_log, lobby.id, room_key)
    if room.tracker:
        tracker_key = str(room.tracker)
        _tracker_watchers[tracker_key] = _tracker_watchers.get(tracker_key, 0) + 1
        if tracker_key not in _tracker_tasks:
            _tracker_tasks[tracker_key] = socketio.start_background_task(_track_room_stats, lobby.id, tracker_key)


def _release_room_watchers(lobby: Lobby) -> None:
    latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None)\
        .order_by(desc(LobbyGeneration.created_at)).first()
    if not latest or not latest.room_id:
        return
    room_key = str(latest.room_id)
    if room_key in _room_watchers:
        _room_watchers[room_key] = max(0, _room_watchers.get(room_key, 0) - 1)
        if _room_watchers[room_key] == 0:
            _room_watchers.pop(room_key, None)
    tracker = Room.get(id=latest.room_id).tracker if Room.get(id=latest.room_id) else None
    if tracker:
        tracker_key = str(tracker)
        if tracker_key in _tracker_watchers:
            _tracker_watchers[tracker_key] = max(0, _tracker_watchers.get(tracker_key, 0) - 1)
            if _tracker_watchers[tracker_key] == 0:
                _tracker_watchers.pop(tracker_key, None)


def _tail_room_log(lobby_id: uuid.UUID, room_id: str) -> None:
    lock_key = f"lobby:terminal_lock:{room_id}"
    offset_key = f"lobby:terminal_offset:{room_id}"
    lock_id = str(uuid.uuid4())
    while _room_watchers.get(room_id, 0) > 0:
        got_lock = _redis.set(lock_key, lock_id, nx=True, ex=5)
        if not got_lock:
            time.sleep(0.5)
            continue
        try:
            offset_raw = _redis.get(offset_key)
            offset = int(offset_raw) if offset_raw else 0
            log_path = os.path.join("logs", f"{room_id}.txt")
            if not os.path.exists(log_path):
                time.sleep(0.5)
                continue
            with open(log_path, "rb") as log_file:
                log_file.seek(offset)
                chunk = log_file.read()
                if chunk:
                    offset += len(chunk)
                    _redis.set(offset_key, str(offset), ex=3600)
                    socketio.emit("terminal_output",
                                  {"text": chunk.decode("utf-8", errors="ignore")},
                                  to=_lobby_room(to_url(lobby_id)))
            _redis.expire(lock_key, 5)
        finally:
            time.sleep(0.4)
    _room_watchers.pop(room_id, None)
    _tailer_tasks.pop(room_id, None)


def _track_room_stats(lobby_id: uuid.UUID, tracker_id: str) -> None:
    while _tracker_watchers.get(tracker_id, 0) > 0:
        try:
            with db_session:
                lobby = Lobby.get(id=lobby_id)
                if not lobby:
                    time.sleep(2)
                    continue
                room = Room.get(tracker=uuid.UUID(tracker_id))
                if not room:
                    time.sleep(2)
                    continue
                tracker_data = TrackerData(room)
                players = tracker_data.get_all_players().get(0, [])
                total_locations = sum(len(tracker_data.get_player_locations(player)) for player in players)
                checks_done = tracker_data.get_team_locations_checked_count().get(0, 0)
                player_names = {player: (tracker_data.get_player_name(player) or "").lower() for player in players}
                player_checks = {
                    player: len(tracker_data.get_player_checked_locations(0, player))
                    for player in players
                }
                goal_players = {
                    name for player, name in player_names.items()
                    if tracker_data.get_player_client_status(0, player) == ClientStatus.CLIENT_GOAL
                }
                released_key = f"room:released:{room.id}"
                try:
                    released_players = {name.lower() for name in (_redis.smembers(released_key) or [])}
                except Exception:
                    released_players = set()
                completed = len(goal_players | (released_players & set(player_names.values())))

                timeout_seconds = _get_slow_release_timeout(lobby)
                room_id_str = str(room.id)
                member_by_player: dict[str, tuple[LobbyMember, str]] = {}
                for member in select(m for m in LobbyMember if m.lobby == lobby):
                    aliases = _member_player_aliases(member)
                    for key, raw_name in aliases.items():
                        if key and key not in member_by_player:
                            member_by_player[key] = (member, raw_name)

                for player in players:
                    player_name_raw = tracker_data.get_player_name(player) or ""
                    player_key = _normalize_player_name(player_name_raw)
                    if not player_key:
                        continue
                    if player_key in goal_players or player_key in released_players:
                        _clear_inactivity_state(room_id_str, player_key)
                        continue

                    checked_count = int(player_checks.get(player, 0))
                    state = _load_inactivity_state(room_id_str, player_key)
                    prev_checks = int(state.get("checks", checked_count))
                    last_progress_at = float(state.get("last_progress_at", time.time()))
                    challenged_at = float(state.get("challenged_at", 0) or 0)
                    slow_released = bool(state.get("slow_released"))

                    if checked_count > prev_checks:
                        state = {
                            "checks": checked_count,
                            "last_progress_at": time.time(),
                            "challenged_at": 0,
                            "slow_released": False,
                        }
                        _save_inactivity_state(room_id_str, player_key, state)
                        continue

                    inactive_seconds = _seconds_since(last_progress_at)
                    if inactive_seconds < timeout_seconds:
                        if prev_checks != checked_count or challenged_at or slow_released:
                            state = {
                                "checks": checked_count,
                                "last_progress_at": last_progress_at,
                                "challenged_at": 0,
                                "slow_released": False,
                            }
                            _save_inactivity_state(room_id_str, player_key, state)
                        continue

                    member_info = member_by_player.get(player_key)
                    if not member_info:
                        continue
                    member, alias_name = member_info
                    user_id = member.user.discord_id if member.user else None
                    if not user_id:
                        continue

                    if challenged_at <= 0:
                        _add_system_message(
                            lobby,
                            f"[Lobby] Inactivity check: {alias_name} has no checks for {int(timeout_seconds // 60)} minutes.",
                        )
                        socketio.emit(
                            "inactivity_probe",
                            {
                                "player_name": alias_name,
                                "timeout_seconds": int(timeout_seconds),
                                "grace_seconds": _INACTIVITY_GRACE_SECONDS,
                            },
                            to=_user_room(user_id),
                        )
                        state = {
                            "checks": checked_count,
                            "last_progress_at": last_progress_at,
                            "challenged_at": time.time(),
                            "slow_released": False,
                        }
                        _save_inactivity_state(room_id_str, player_key, state)
                        continue

                    if _seconds_since(challenged_at) < _INACTIVITY_GRACE_SECONDS:
                        continue
                    if slow_released:
                        continue

                    Command(room=room, commandtext=f"/allow_release {alias_name}")
                    _add_system_message(
                        lobby,
                        f"[Lobby] {alias_name} was slow released for inactivity ({int(timeout_seconds // 60)}m without checks).",
                    )
                    socketio.emit(
                        "inactivity_slow_released",
                        {
                            "player_name": alias_name,
                            "timeout_seconds": int(timeout_seconds),
                        },
                        to=_user_room(user_id),
                    )
                    state = {
                        "checks": checked_count,
                        "last_progress_at": last_progress_at,
                        "challenged_at": challenged_at,
                        "slow_released": True,
                        "released_at": time.time(),
                    }
                    _save_inactivity_state(room_id_str, player_key, state)

                payload = {
                    "players_total": len(players),
                    "checks_done": checks_done,
                    "total_locations": total_locations,
                    "completed_players": completed,
                }
            socketio.emit("room_stats", payload, to=_lobby_room(to_url(lobby_id)))
        except Exception:
            pass
        time.sleep(5)
    _tracker_watchers.pop(tracker_id, None)
    _tracker_tasks.pop(tracker_id, None)


@socketio.on("join_lobby")
def join_lobby_socket(data: dict):
    lobby_id = data.get("lobby_id")
    if not lobby_id:
        return
    with db_session:
        user = _get_session_user()
        if not user:
            socketio.emit(
                "lobby_auth_required",
                {
                "message": "Connect with Discord to join this lobby. It is free and uses your Discord account.",
                    "redirect_url": "/api/auth/login",
                },
                to=request.sid,
            )
            return
        lobby = Lobby.get(id=to_python(lobby_id))
        if not lobby:
            return
        ban = _get_active_ban(lobby, user)
        if ban:
            socketio.emit(
                "lobby_kicked",
                {
                    "lobby_id": str(lobby.id),
                    "message": _ban_notice(ban) if ban else "You are banned from this lobby.",
                    "redirect_url": "/",
                },
                to=request.sid,
            )
            return
        if lobby.server_password:
            if not user:
                socketio.emit(
                    "lobby_password_required",
                    {
                        "lobby_id": str(lobby.id),
                        "message": "Connect with Discord and enter the lobby password to join.",
                    },
                    to=request.sid,
                )
                return
            membership = LobbyMember.get(lobby=lobby, user=user)
            if not membership:
                socketio.emit(
                    "lobby_password_required",
                    {
                        "lobby_id": str(lobby.id),
                        "message": "Lobby password required.",
                    },
                    to=request.sid,
                )
                return
        if user:
            membership = LobbyMember.get(lobby=lobby, user=user)
            latest_room = _latest_generated_room(lobby)
            if latest_room:
                if not membership:
                    socketio.emit(
                        "lobby_kicked",
                        {
                            "lobby_id": str(lobby.id),
                            "message": "This room is locked after generation. Only active players from this seed can rejoin.",
                            "redirect_url": "/",
                        },
                        to=request.sid,
                    )
                    return
                if not _member_is_active_in_room(membership, latest_room, assume_active_if_unknown=True):
                    socketio.emit(
                        "lobby_kicked",
                        {
                            "lobby_id": str(lobby.id),
                            "message": "This room is locked. Your run is already completed/released for this seed.",
                            "redirect_url": "/",
                        },
                        to=request.sid,
                    )
                    return
            if not membership:
                membership = LobbyMember(lobby=lobby, user=user)
            latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None)\
                .order_by(desc(LobbyGeneration.created_at)).first()
            room_id = str(latest.room_id) if latest and latest.room_id else None
            _user_active_room[user.discord_id] = room_id
        join_room(_lobby_room(lobby_id))
        _sid_lobby[request.sid] = lobby_id
        _lobby_clients[lobby_id] = _lobby_clients.get(lobby_id, 0) + 1
        if user and user.discord_id:
            _sid_user[request.sid] = user.discord_id
            key = (lobby_id, user.discord_id)
            _lobby_user_counts[key] = _lobby_user_counts.get(key, 0) + 1
            if _lobby_user_counts[key] == 1:
                if _should_emit_presence_event(lobby_id, user.discord_id, "join") and _user_active_room.get(user.discord_id):
                    _emit_friend_activity(user, "room_join", {"room_id": _user_active_room[user.discord_id]})
            join_room(_user_room(user.discord_id))
        _emit_members_update(lobby)
        _emit_generation_update(lobby)
        watched_room_id = _ensure_room_watchers(lobby)
        if watched_room_id:
            _send_existing_log(watched_room_id, request.sid)


@socketio.on("leave_lobby")
def leave_lobby_socket(data: dict):
    lobby_id = data.get("lobby_id")
    if not lobby_id:
        return
    leave_room(_lobby_room(lobby_id))
    _lobby_clients[lobby_id] = max(0, _lobby_clients.get(lobby_id, 1) - 1)
    _sid_lobby.pop(request.sid, None)
    with db_session:
        lobby = Lobby.get(id=to_python(lobby_id))
        if lobby:
            user = _get_session_user()
            if user and user.discord_id:
                key = (lobby_id, user.discord_id)
                if key in _lobby_user_counts:
                    _lobby_user_counts[key] = max(0, _lobby_user_counts[key] - 1)
                    if _lobby_user_counts[key] == 0:
                        _lobby_user_counts.pop(key, None)
                        if _should_emit_presence_event(lobby_id, user.discord_id, "leave"):
                            _emit_friend_activity(user, "lobby_leave", {"lobby_id": lobby_id, "lobby_name": lobby.name})
                        if _user_active_room.get(user.discord_id):
                            _emit_friend_activity(user, "room_leave", {
                                "room_id": _user_active_room[user.discord_id],
                            })
                            _user_active_room[user.discord_id] = None
            _release_room_watchers(lobby)
            _emit_members_update(lobby)


@socketio.on("disconnect")
def disconnect_socket():
    lobby_id = _sid_lobby.pop(request.sid, None)
    if lobby_id:
        _lobby_clients[lobby_id] = max(0, _lobby_clients.get(lobby_id, 1) - 1)
    user_id = _sid_user.pop(request.sid, None)
    if user_id:
        _user_connections[user_id] = max(0, _user_connections.get(user_id, 1) - 1)
    with db_session:
        user = None
        if user_id:
            user = User.get(discord_id=user_id)
            if user and _user_connections.get(user_id, 0) == 0:
                user.is_online = False
                user.last_seen_at = datetime.datetime.utcnow()
                _emit_friend_presence(user)
        if lobby_id:
            lobby = Lobby.get(id=to_python(lobby_id))
            if lobby and user_id:
                key = (lobby_id, user_id)
                if key in _lobby_user_counts:
                    _lobby_user_counts[key] = max(0, _lobby_user_counts.get(key, 1) - 1)
                    if _lobby_user_counts[key] == 0:
                        _lobby_user_counts.pop(key, None)
                        if user:
                            if _should_emit_presence_event(lobby_id, user.discord_id, "leave"):
                                _emit_friend_activity(user, "lobby_leave", {"lobby_id": lobby_id, "lobby_name": lobby.name})
                            if _user_active_room.get(user_id):
                                _emit_friend_activity(user, "room_leave", {
                                    "room_id": _user_active_room[user_id],
                                })
                                _user_active_room[user_id] = None
                _release_room_watchers(lobby)
                _emit_members_update(lobby)


@socketio.on("inactivity_pong")
def inactivity_pong(data: dict):
    lobby_id = data.get("lobby_id")
    if not lobby_id:
        return
    with db_session:
        user = _get_session_user()
        if not user:
            return
        lobby = Lobby.get(id=to_python(lobby_id))
        if not lobby:
            return
        member = LobbyMember.get(lobby=lobby, user=user)
        if not member:
            return
        room = _latest_generated_room(lobby)
        if not room:
            return
        now_ts = time.time()
        room_id_str = str(room.id)
        aliases = _member_player_aliases(member)
        for key in aliases.keys():
            if not key:
                continue
            state = _load_inactivity_state(room_id_str, key)
            checks = int(state.get("checks", 0))
            _save_inactivity_state(
                room_id_str,
                key,
                {
                    "checks": checks,
                    "last_progress_at": now_ts,
                    "challenged_at": 0,
                    "slow_released": False,
                },
            )


@socketio.on("chat_send")
def chat_send(data: dict):
    lobby_id = data.get("lobby_id")
    content = (data.get("content") or "").strip()
    if not lobby_id or not content:
        return
    with db_session:
        user = _get_session_user()
        if not user:
            return
        lobby = Lobby.get(id=to_python(lobby_id))
        if not lobby:
            return
        ban = _get_active_ban(lobby, user)
        if ban:
            socketio.emit(
                "lobby_kicked",
                {
                    "lobby_id": str(lobby.id),
                    "message": _ban_notice(ban),
                    "redirect_url": "/",
                },
                to=request.sid,
            )
            return
        message = LobbyMessage(
            lobby=lobby,
            user=user,
            author_name=_display_name_for(user),
            content=content,
            created_at=datetime.datetime.utcnow(),
        )
        lobby.last_activity = datetime.datetime.utcnow()
        socketio.emit(
            "lobby_message",
            {
                "id": message.id,
                "author": message.author_name,
                "avatar_url": _avatar_url_for(user),
                "content": message.content,
                "created_at": message.created_at.isoformat(),
            },
            to=_lobby_room(lobby_id),
        )


@socketio.on("terminal_command")
def terminal_command(data: dict):
    lobby_id = data.get("lobby_id")
    room_id = data.get("room_id")
    cmd = (data.get("cmd") or "").strip()
    if not lobby_id or not room_id or not cmd:
        return
    with db_session:
        user = _get_session_user()
        if not user:
            return
        client_authorized = _is_authorized_sekailink_client_request()
        session_authorized = bool(session.get("discord_user"))
        if not (client_authorized or session_authorized):
            emit("terminal_ack", {"status": "denied", "error": "client_auth_required"}, to=request.sid)
            return
        room_uuid = _parse_room_uuid(room_id)
        if not room_uuid:
            return
        room = Room.get(id=room_uuid)
        if not room:
            return
        owner_ok = False
        try:
            owner_ok = room.owner == session["_id"]
        except Exception:
            owner_ok = False
        if not owner_ok:
            lobby_uuid = _parse_room_uuid(lobby_id)
            lobby = Lobby.get(id=lobby_uuid) if lobby_uuid else None
            owner_ok = bool(lobby and lobby.owner == user)
        if not owner_ok:
            emit("terminal_ack", {"status": "denied", "error": "not_room_owner"}, to=request.sid)
            return
        Command(room=room, commandtext=cmd)
        room.last_activity = datetime.datetime.utcnow()
        socketio.emit("terminal_ack", {"status": "sent"}, to=_lobby_room(lobby_id))


@socketio.on("watch_room")
def watch_room(data: dict):
    lobby_id = data.get("lobby_id")
    room_id = data.get("room_id")
    if not lobby_id or not room_id:
        return
    with db_session:
        lobby = Lobby.get(id=to_python(lobby_id))
        if not lobby:
            return
        room_uuid = _parse_room_uuid(room_id)
        if not room_uuid:
            return
        _ensure_room_watchers_for_room(lobby, room_uuid)
        _send_existing_log(str(room_uuid), request.sid)
