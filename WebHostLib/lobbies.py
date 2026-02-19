import datetime
import json
import os
import tempfile
import zipfile
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

import redis
import yaml
from flask import abort, jsonify, request, session, url_for, render_template
from pony.orm import db_session, count, select, desc

from NetUtils import ClientStatus
from Utils import restricted_dumps, get_file_safe_name
from WebHostLib import app, to_url, socketio, cache
from WebHostLib.tracker import TrackerData
from .generate import get_meta, gen_game
from .check import roll_options
from .misc import _get_user_yaml_dir, _get_bearer_token, _desktop_token_auth_context
from .models import Lobby, LobbyMember, LobbyMemberYaml, LobbyMessage, LobbyGeneration, LobbyBan, LobbyHostVote, User, UserYaml, Generation, STATE_QUEUED, Seed, Room, Command, uuid4

REDIS_URL = os.environ.get("REDIS_URL", "redis://127.0.0.1:6379/0")
_redis = redis.Redis.from_url(REDIS_URL, decode_responses=True)
_RELEASED_KEY_PREFIX = "room:released:"
_SLOW_RELEASE_TIMEOUT_KEY_PREFIX = "lobby:slow_release_timeout:"
_SLOW_RELEASE_TIMEOUT_DEFAULT = 1800
_SLOW_RELEASE_TIMEOUT_ALLOWED = {900, 1800, 3600, 5400, 7200}
_MAX_PLAYERS_KEY_PREFIX = "lobby:max_players:"
_MAX_PLAYERS_DEFAULT = 50


_BOT_API_URL = os.environ.get("SEKAILINK_BOT_CONTROL_API_URL", "http://127.0.0.1:8095").rstrip("/")
_BOT_API_KEY = os.environ.get("SEKAILINK_BOT_CONTROL_API_KEY", "")
_LOBBY_VOICE_URL_KEY_PREFIX = "lobby:discord_voice_url:"


def _bot_api_request(path: str, payload: dict[str, Any] | None = None, method: str = "POST") -> dict[str, Any] | None:
    if not _BOT_API_URL or not _BOT_API_KEY:
        return None
    url = f"{_BOT_API_URL}{path}"
    body = json.dumps(payload or {}).encode("utf-8") if payload is not None else None
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("X-SekaiLink-Bot-Key", _BOT_API_KEY)
    if payload is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=6) as response:
            raw = response.read().decode("utf-8", errors="ignore")
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError:
        return None
    except Exception:
        return None


def _ensure_lobby_voice_url(lobby: Lobby) -> str:
    key = f"{_LOBBY_VOICE_URL_KEY_PREFIX}{to_url(lobby.id)}"
    try:
        cached = _redis.get(key)
        if cached:
            return cached
    except Exception:
        pass

    data = _bot_api_request(
        "/discord/lobby-voice/create",
        payload={"lobby_id": to_url(lobby.id), "lobby_name": lobby.name},
        method="POST",
    )
    url = ""
    if isinstance(data, dict):
        url = str(data.get("channel_url") or "")
    if url:
        try:
            _redis.setex(key, 7 * 24 * 60 * 60, url)
        except Exception:
            pass
    return url


def _get_lobby_voice_url(lobby: Lobby) -> str:
    key = f"{_LOBBY_VOICE_URL_KEY_PREFIX}{to_url(lobby.id)}"
    try:
        cached = _redis.get(key)
        if cached:
            return cached
    except Exception:
        pass

    data = _bot_api_request(f"/discord/lobby-voice/{urllib.parse.quote(to_url(lobby.id))}", payload=None, method="GET")
    url = ""
    if isinstance(data, dict):
        url = str(data.get("channel_url") or "")
    if url:
        try:
            _redis.setex(key, 7 * 24 * 60 * 60, url)
        except Exception:
            pass
    return url


def _delete_lobby_voice(lobby: Lobby) -> None:
    key = f"{_LOBBY_VOICE_URL_KEY_PREFIX}{to_url(lobby.id)}"
    _bot_api_request(
        "/discord/lobby-voice/delete",
        payload={"lobby_id": to_url(lobby.id)},
        method="POST",
    )
    try:
        _redis.delete(key)
    except Exception:
        pass


def _get_session_user() -> User | None:
    discord = session.get("discord_user")
    discord_id = None
    if discord:
        discord_id = discord.get("id")
    if not discord_id:
        token = _get_bearer_token()
        if token:
            context = _desktop_token_auth_context(token)
            if context:
                discord_id = context.get("id")
    if not discord_id:
        return None
    user = User.get(discord_id=discord_id)
    if user and user.banned:
        abort(403)
    return user


def _display_name_for(user: User | None) -> str:
    if not user:
        return "Unknown"
    return user.global_name or user.username or "Unknown"


def _member_yaml_list(member: LobbyMember) -> list[dict[str, Any]]:
    yamls = select(y for y in LobbyMemberYaml if y.member == member).order_by(LobbyMemberYaml.created_at)[:50]
    try:
        custom_map = {y.yaml_id: y.custom for y in select(y for y in UserYaml if y.user == member.user)}
    except Exception:
        custom_map = {}
    entries = [
        {
            "id": y.yaml_id,
            "title": y.title,
            "game": y.game,
            "player_name": y.player_name,
            "custom": bool(custom_map.get(y.yaml_id)),
        }
        for y in yamls
    ]
    if entries:
        return entries
    if member.active_yaml_id:
        return [{
            "id": member.active_yaml_id,
            "title": member.active_yaml_title,
            "game": member.active_yaml_game,
            "player_name": member.active_yaml_player,
            "custom": bool(custom_map.get(member.active_yaml_id)),
        }]
    return []


def _user_room(discord_id: str) -> str:
    return f"user:{discord_id}"


def _avatar_url_for(user: User | None) -> str | None:
    if not user or not user.discord_id:
        return None
    if user.avatar:
        return f"https://cdn.discordapp.com/avatars/{user.discord_id}/{user.avatar}.png?size=64"
    try:
        idx = int(user.discord_id) % 5
    except ValueError:
        idx = 0
    return f"https://cdn.discordapp.com/embed/avatars/{idx}.png"


def _get_active_ban(lobby: Lobby, user: User) -> LobbyBan | None:
    now = datetime.datetime.utcnow()
    return select(
        b for b in LobbyBan
        if b.lobby == lobby
        and b.user == user
        and (b.expires_at is None or b.expires_at > now)
    ).order_by(desc(LobbyBan.created_at)).first()


def _get_host_member(lobby: Lobby) -> LobbyMember | None:
    return LobbyMember.get(lobby=lobby, user=lobby.owner)


def _get_candidate_host(lobby: Lobby) -> LobbyMember | None:
    return select(
        m for m in LobbyMember if m.lobby == lobby and m.user != lobby.owner
    ).order_by(desc(LobbyMember.joined_at)).first()


def _clear_host_votes(lobby: Lobby) -> None:
    select(v for v in LobbyHostVote if v.lobby == lobby).delete(bulk=True)


def _normalize_player_name(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip().lower()


def _latest_generated_room(lobby: Lobby) -> Room | None:
    latest = select(
        lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None
    ).order_by(desc(LobbyGeneration.created_at)).first()
    if not latest or not latest.room_id:
        return None
    return Room.get(id=latest.room_id)


def _member_player_aliases(member: LobbyMember) -> dict[str, str]:
    aliases: dict[str, str] = {}
    for y in select(y for y in LobbyMemberYaml if y.member == member).order_by(LobbyMemberYaml.created_at):
        raw = str(y.player_name or "").strip()
        key = _normalize_player_name(raw)
        if key and key not in aliases:
            aliases[key] = raw
    raw_active = str(member.active_yaml_player or "").strip()
    key_active = _normalize_player_name(raw_active)
    if key_active and key_active not in aliases:
        aliases[key_active] = raw_active
    return aliases


def _resolve_member_tracker_slot(member: LobbyMember, room: Room) -> tuple[int, str] | None:
    try:
        tracker_data = TrackerData(room)
        players = tracker_data.get_all_players().get(0, [])
    except Exception:
        return None
    aliases = _member_player_aliases(member)
    if not aliases:
        return None
    for slot in players:
        player_name = str(tracker_data.get_player_name(slot) or "").strip()
        key = _normalize_player_name(player_name)
        if key and key in aliases:
            return slot, player_name
    return None


def _room_player_state(room: Room) -> tuple[set[str], set[str]]:
    all_players: set[str] = set()
    completed_players: set[str] = set()
    try:
        tracker_data = TrackerData(room)
        players = tracker_data.get_all_players().get(0, [])
        for player in players:
            player_name = _normalize_player_name(tracker_data.get_player_name(player))
            if not player_name:
                continue
            all_players.add(player_name)
            if tracker_data.get_player_client_status(0, player) == ClientStatus.CLIENT_GOAL:
                completed_players.add(player_name)
    except Exception:
        pass

    try:
        released_key = f"{_RELEASED_KEY_PREFIX}{room.id}"
        released_players = {_normalize_player_name(name) for name in (_redis.smembers(released_key) or [])}
        released_players.discard("")
        completed_players |= released_players
    except Exception:
        pass

    return all_players, completed_players


def _member_is_active_in_room(member: LobbyMember, room: Room, assume_active_if_unknown: bool = False) -> bool:
    aliases = _member_player_aliases(member)
    if not aliases:
        return False
    all_players, completed_players = _room_player_state(room)
    if not all_players:
        return assume_active_if_unknown
    active_players = all_players - completed_players
    return bool(set(aliases.keys()) & active_players)


def _find_other_active_membership(user: User, current_lobby: Lobby) -> tuple[Lobby, LobbyMember, Room] | None:
    memberships = select(m for m in LobbyMember if m.user == user and m.lobby.id != current_lobby.id)[:100]
    for membership in memberships:
        room = _latest_generated_room(membership.lobby)
        if not room:
            continue
        if _member_is_active_in_room(membership, room, assume_active_if_unknown=False):
            return membership.lobby, membership, room
    return None


def _request_slow_release(lobby: Lobby, member: LobbyMember, room: Room, initiator: User) -> None:
    aliases = _member_player_aliases(member)
    if not aliases:
        return
    _, completed_players = _room_player_state(room)
    pending = [(key, raw) for key, raw in aliases.items() if key not in completed_players]
    if not pending:
        return
    for _, player_name in pending:
        Command(room=room, commandtext=f"/allow_release {player_name}")
    player_list = ", ".join(raw for _, raw in pending)
    _add_system_message(
        lobby,
        f"[Lobby] {_display_name_for(initiator)} switched lobbies. Slow release enabled for: {player_list}.",
    )


def _parse_slow_release_timeout(value: Any, default: int = _SLOW_RELEASE_TIMEOUT_DEFAULT) -> int:
    parsed = _parse_int(value, default, min_value=60, max_value=24 * 60 * 60)
    if parsed in _SLOW_RELEASE_TIMEOUT_ALLOWED:
        return parsed
    return default


def _max_players_key(lobby_id: Any) -> str:
    return f"{_MAX_PLAYERS_KEY_PREFIX}{lobby_id}"


def _get_max_players(lobby: Lobby) -> int:
    try:
        raw = _redis.get(_max_players_key(lobby.id))
        if raw is None:
            return _MAX_PLAYERS_DEFAULT
        return _parse_int(raw, _MAX_PLAYERS_DEFAULT, min_value=1, max_value=50)
    except Exception:
        return _MAX_PLAYERS_DEFAULT


def _set_max_players(lobby: Lobby, value: int) -> None:
    try:
        _redis.set(_max_players_key(lobby.id), str(_parse_int(value, _MAX_PLAYERS_DEFAULT, min_value=1, max_value=50)), ex=60 * 60 * 24 * 30)
    except Exception:
        pass


def _slow_release_timeout_key(lobby_id: Any) -> str:
    return f"{_SLOW_RELEASE_TIMEOUT_KEY_PREFIX}{lobby_id}"


def _get_slow_release_timeout(lobby: Lobby) -> int:
    try:
        raw = _redis.get(_slow_release_timeout_key(lobby.id))
        if raw is None:
            return _SLOW_RELEASE_TIMEOUT_DEFAULT
        return _parse_slow_release_timeout(raw, _SLOW_RELEASE_TIMEOUT_DEFAULT)
    except Exception:
        return _SLOW_RELEASE_TIMEOUT_DEFAULT


def _set_slow_release_timeout(lobby: Lobby, timeout_seconds: int) -> None:
    try:
        _redis.set(_slow_release_timeout_key(lobby.id), str(timeout_seconds), ex=60 * 60 * 24 * 30)
    except Exception:
        pass


def _ban_notice(ban: LobbyBan) -> str:
    if ban.expires_at:
        until = ban.expires_at.strftime("%b %d, %Y %H:%M UTC")
        return f"You are banned from this lobby until {until}."
    return "You are banned from this lobby."


def _parse_bool(value: Any) -> bool | None:
    if isinstance(value, bool):
        return value
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return bool(value)
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered in {"1", "true", "yes", "on"}:
            return True
        if lowered in {"0", "false", "no", "off"}:
            return False
    return None


def _parse_int(value: Any, default: int, min_value: int | None = None, max_value: int | None = None) -> int:
    try:
        parsed = int(float(value))
    except (TypeError, ValueError):
        parsed = default
    if min_value is not None:
        parsed = max(min_value, parsed)
    if max_value is not None:
        parsed = min(max_value, parsed)
    return parsed


def _parse_mode(value: Any, allowed: set[str], default: str) -> str:
    if isinstance(value, str):
        candidate = value.strip().lower()
        if candidate in allowed:
            return candidate
    return default


def _add_system_message(lobby: Lobby, content: str) -> None:
    message = LobbyMessage(
        lobby=lobby,
        user=None,
        author_name="SekaiLink",
        content=content,
        created_at=datetime.datetime.utcnow(),
    )
    socketio.emit(
        "lobby_message",
        {
            "id": message.id,
            "author": message.author_name,
            "avatar_url": None,
            "content": message.content,
            "created_at": message.created_at.isoformat(),
        },
        to=f"lobby:{to_url(lobby.id)}",
    )


def _emit_members_update(lobby: Lobby) -> None:
    members = select(m for m in LobbyMember if m.lobby == lobby).order_by(LobbyMember.joined_at)[:200]
    now = datetime.datetime.utcnow()
    payload = [
        {
            "name": _display_name_for(member.user),
            "discord_id": member.user.discord_id,
            "active_yaml_id": member.active_yaml_id,
            "active_yaml_title": member.active_yaml_title,
            "active_yaml_game": member.active_yaml_game,
            "active_yaml_player": member.active_yaml_player,
            "active_yamls": _member_yaml_list(member),
            "ready": member.ready,
            "online": bool(member.last_seen and now - member.last_seen < datetime.timedelta(minutes=3)),
            "is_host": member.user == lobby.owner,
        }
        for member in members
    ]
    socketio.emit("members_update", {"members": payload}, to=f"lobby:{to_url(lobby.id)}")


def _emit_generation_update(lobby: Lobby) -> None:
    latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
    if not latest:
        socketio.emit("generation_update", {"status": "none"}, to=f"lobby:{to_url(lobby.id)}")
        return
    payload = {"status": latest.status, "error": latest.error or ""}
    if latest.seed_id:
        payload["seed_url"] = f"/seed/{to_url(latest.seed_id)}"
    if latest.room_id:
        payload["room_url"] = f"/room/{to_url(latest.room_id)}"
    if latest.completed_at:
        payload["completed_at"] = latest.completed_at.isoformat()
    socketio.emit("generation_update", payload, to=f"lobby:{to_url(lobby.id)}")


def _list_user_yamls(user: User) -> list[dict[str, Any]]:
    yaml_entries: list[dict[str, Any]] = []
    user_dir = _get_user_yaml_dir(user)
    for filename in os.listdir(user_dir):
        if not filename.endswith(".yaml"):
            continue
        yaml_id = filename[:-5]
        path = os.path.join(user_dir, filename)
        title = filename
        game = ""
        player_name = ""
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = yaml.safe_load(f)
            player_name = (content or {}).get("name", "")
            game = (content or {}).get("game", "")
            title = ((content or {}).get("sekailink") or {}).get("title") or title
        except Exception:
            pass
        custom_flag = False
        try:
            custom_entry = UserYaml.get(user=user, yaml_id=yaml_id)
            custom_flag = bool(custom_entry.custom) if custom_entry else False
        except Exception:
            custom_flag = False
        yaml_entries.append({
            "id": yaml_id,
            "title": title,
            "game": game,
            "player_name": player_name,
            "custom": custom_flag,
        })
    return sorted(yaml_entries, key=lambda entry: entry["title"].lower())


def _load_yaml_entry(user: User, yaml_id: str) -> dict[str, str] | None:
    user_dir = _get_user_yaml_dir(user)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if not os.path.exists(yaml_path):
        return None
    title = yaml_id
    game = ""
    player_name = ""
    try:
        with open(yaml_path, "r", encoding="utf-8") as f:
            content = yaml.safe_load(f) or {}
        player_name = content.get("name", "")
        game = content.get("game", "")
        title = (content.get("sekailink") or {}).get("title") or title
    except Exception:
        pass
    custom_flag = False
    try:
        custom_entry = UserYaml.get(user=user, yaml_id=yaml_id)
        custom_flag = bool(custom_entry.custom) if custom_entry else False
    except Exception:
        custom_flag = False
    return {
        "id": yaml_id,
        "title": title,
        "game": game,
        "player_name": player_name,
        "custom": custom_flag,
    }


@app.route("/lobby/<suuid:lobby_id>")
def lobby_view(lobby_id):
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        user = _get_session_user()
        yaml_entries = _list_user_yamls(user) if user else []
        active_yaml_id = None
        is_ready = False
        is_owner = False
        is_member = False
        if user:
            membership = LobbyMember.get(lobby=lobby, user=user)
            if membership:
                is_member = True
                active_yaml_id = membership.active_yaml_id
                is_ready = membership.ready
            is_owner = lobby.owner == user
        return render_template(
            "lobby.html",
            lobby=lobby,
            lobby_owner=_display_name_for(lobby.owner),
            yaml_entries=yaml_entries,
            active_yaml_id=active_yaml_id,
            is_ready=is_ready,
            is_owner=is_owner,
            is_member=is_member,
            is_private=bool(lobby.server_password),
            allow_custom_yamls=bool(lobby.allow_custom_yamls),
            current_user_name=_display_name_for(user) if user else "",
        )


@app.route("/api/lobbies", methods=["GET", "POST"])
def lobbies_api():
    if request.method == "POST":
        user = _get_session_user()
        if not user:
            return abort(401)
        payload = request.get_json(silent=True) or {}
        name = (payload.get("name") or "").strip()
        description = (payload.get("description") or "").strip()
        server_password = (payload.get("server_password") or "").strip()
        if len(name) < 3 or len(name) > 60:
            return jsonify({"error": "Lobby name must be 3-60 characters."}), 400
        if len(description) > 180:
            return jsonify({"error": "Description is too long."}), 400
        if server_password and len(server_password) > 64:
            return jsonify({"error": "Server password is too long."}), 400
        release_mode = _parse_mode(payload.get("release_mode"), {"enabled", "disabled", "auto", "auto-enabled", "goal"}, "goal")
        collect_mode = _parse_mode(payload.get("collect_mode"), {"enabled", "disabled", "auto", "auto-enabled", "goal"}, "goal")
        remaining_mode = _parse_mode(payload.get("remaining_mode"), {"enabled", "disabled", "goal"}, "enabled")
        countdown_mode = _parse_mode(payload.get("countdown_mode"), {"enabled", "disabled", "auto"}, "auto")
        item_cheat = _parse_bool(payload.get("item_cheat"))
        if item_cheat is None:
            item_cheat = False
        spoiler = _parse_int(payload.get("spoiler"), 0, 0, 3)
        hint_cost = _parse_int(payload.get("hint_cost"), 5, 0, 100)
        max_players = _parse_int(payload.get("max_players"), _MAX_PLAYERS_DEFAULT, 1, 50)
        slow_release_timeout = _parse_slow_release_timeout(payload.get("slow_release_timeout"), _SLOW_RELEASE_TIMEOUT_DEFAULT)
        plando_items = _parse_bool(payload.get("plando_items"))
        plando_bosses = _parse_bool(payload.get("plando_bosses"))
        plando_connections = _parse_bool(payload.get("plando_connections"))
        plando_texts = _parse_bool(payload.get("plando_texts"))
        allow_custom_yamls = _parse_bool(payload.get("allow_custom_yamls"))
        if plando_items is None:
            plando_items = True
        if plando_bosses is None:
            plando_bosses = True
        if plando_connections is None:
            plando_connections = True
        if plando_texts is None:
            plando_texts = True
        if allow_custom_yamls is None:
            allow_custom_yamls = True
        with db_session:
            lobby = Lobby(
                name=name,
                description=description or None,
                owner=user,
                server_password=server_password,
                release_mode=release_mode,
                collect_mode=collect_mode,
                remaining_mode=remaining_mode,
                countdown_mode=countdown_mode,
                item_cheat=item_cheat,
                spoiler=spoiler,
                hint_cost=hint_cost,
                plando_items=plando_items,
                plando_bosses=plando_bosses,
                plando_connections=plando_connections,
                plando_texts=plando_texts,
                allow_custom_yamls=allow_custom_yamls,
            )
            LobbyMember(lobby=lobby, user=user)
            _set_slow_release_timeout(lobby, slow_release_timeout)
            _set_max_players(lobby, max_players)
            voice_url = _ensure_lobby_voice_url(lobby)
            cache.delete("lobbies_list_v1")
            return jsonify({"url": url_for("lobby_view", lobby_id=lobby.id), "discord_voice_url": voice_url}), 201

    cached = cache.get("lobbies_list_v1")
    if cached is not None:
        return jsonify(cached)
    with db_session:
        lobby_rows = select(l for l in Lobby).order_by(desc(Lobby.last_activity))[:50]
        data = []
        for lobby in lobby_rows:
            generated_room = _latest_generated_room(lobby)
            slow_release_timeout = _get_slow_release_timeout(lobby)
            max_players = _get_max_players(lobby)
            data.append({
                "id": to_url(lobby.id),
                "url": f"/lobby/{to_url(lobby.id)}",
                "name": lobby.name,
                "description": lobby.description or "",
                "owner": _display_name_for(lobby.owner),
                "created_at": lobby.created_at.isoformat(),
                "last_activity": lobby.last_activity.isoformat(),
                "member_count": count(m for m in LobbyMember if m.lobby == lobby),
                "max_players": max_players,
                "message_count": count(m for m in LobbyMessage if m.lobby == lobby),
                "is_private": bool(lobby.server_password),
                "release_mode": lobby.release_mode,
                "collect_mode": lobby.collect_mode,
                "remaining_mode": lobby.remaining_mode,
                "countdown_mode": lobby.countdown_mode,
                "item_cheat": bool(lobby.item_cheat),
                "spoiler": lobby.spoiler,
                "hint_cost": lobby.hint_cost,
                "plando_items": bool(lobby.plando_items),
                "plando_bosses": bool(lobby.plando_bosses),
                "plando_connections": bool(lobby.plando_connections),
                "plando_texts": bool(lobby.plando_texts),
                "allow_custom_yamls": bool(lobby.allow_custom_yamls),
                "is_locked": bool(generated_room),
                "slow_release_timeout": slow_release_timeout,
                "discord_voice_url": _get_lobby_voice_url(lobby),
            })
        payload = {"lobbies": data}
        cache.set("lobbies_list_v1", payload, timeout=5)
        return jsonify(payload)


@app.route("/api/lobbies/<suuid:lobby_id>/join", methods=["POST"])
def lobby_join(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    provided_password = (payload.get("password") or "").strip()
    force_abandon = _parse_bool(payload.get("force_abandon")) is True
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        ban = _get_active_ban(lobby, user)
        if ban:
            return jsonify({"error": _ban_notice(ban), "reason": "ban"}), 403
        membership = LobbyMember.get(lobby=lobby, user=user)
        if lobby.server_password and not membership:
            if not provided_password:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
            if provided_password != lobby.server_password:
                return jsonify({"error": "Invalid lobby password.", "reason": "password"}), 403
        latest_room = _latest_generated_room(lobby)
        if latest_room:
            if not membership:
                return jsonify({
                    "error": "This room is locked after generation. Only active players from this seed can rejoin.",
                    "reason": "room_locked",
                }), 403
            if not _member_is_active_in_room(membership, latest_room, assume_active_if_unknown=True):
                return jsonify({
                    "error": "This room is locked. Your run is already completed/released for this seed.",
                    "reason": "room_locked",
                }), 403
        conflict = _find_other_active_membership(user, lobby)
        if conflict:
            conflict_lobby, conflict_member, conflict_room = conflict
            if not force_abandon:
                return jsonify({
                    "error": "You are still active in another room. Continuing will abandon that run and start a slow release there.",
                    "reason": "active_game_conflict",
                    "previous_lobby_id": to_url(conflict_lobby.id),
                    "previous_lobby_name": conflict_lobby.name,
                    "requires_confirmation": True,
                }), 409
            _request_slow_release(conflict_lobby, conflict_member, conflict_room, user)

        if not membership:
            membership = LobbyMember(lobby=lobby, user=user)
        membership.last_seen = datetime.datetime.utcnow()
        lobby.last_activity = datetime.datetime.utcnow()
        _emit_members_update(lobby)
        return jsonify({"status": "ok"})


@app.route("/api/lobbies/<suuid:lobby_id>/messages", methods=["GET", "POST"])
def lobby_messages(lobby_id):
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if request.method == "GET" and lobby.server_password:
            user = _get_session_user()
            if not user:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
            membership = LobbyMember.get(lobby=lobby, user=user)
            if not membership:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403

        if request.method == "POST":
            user = _get_session_user()
            if not user:
                return abort(401)
            ban = _get_active_ban(lobby, user)
            if ban:
                return jsonify({"error": _ban_notice(ban), "reason": "ban"}), 403
            payload = request.get_json(silent=True) or {}
            content = (payload.get("content") or "").strip()
            if not content:
                return jsonify({"error": "Message is empty."}), 400
            if len(content) > 800:
                return jsonify({"error": "Message is too long."}), 400
            membership = LobbyMember.get(lobby=lobby, user=user)
            if lobby.server_password and not membership:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
            if not membership:
                membership = LobbyMember(lobby=lobby, user=user)
            membership.last_seen = datetime.datetime.utcnow()
            lobby.last_activity = datetime.datetime.utcnow()
            message = LobbyMessage(
                lobby=lobby,
                user=user,
                author_name=_display_name_for(user),
                content=content,
            )
            socketio.emit(
                "lobby_message",
                {
                    "id": message.id,
                    "author": message.author_name,
                    "avatar_url": _avatar_url_for(user),
                    "content": message.content,
                    "created_at": message.created_at.isoformat(),
                },
                to=f"lobby:{to_url(lobby.id)}",
            )
            return jsonify({
                "id": message.id,
                "author": message.author_name,
                "avatar_url": _avatar_url_for(user),
                "content": message.content,
                "created_at": message.created_at.isoformat(),
            }), 201

        after_id = request.args.get("after_id", type=int)
        if after_id:
            messages = select(
                m for m in LobbyMessage
                if m.lobby == lobby and m.id > after_id
            ).order_by(LobbyMessage.id)[:100]
        else:
            recent = select(
                m for m in LobbyMessage
                if m.lobby == lobby
            ).order_by(desc(LobbyMessage.id))[:50]
            messages = list(reversed(list(recent)))
        return jsonify({
            "messages": [
                {
                    "id": message.id,
                    "author": message.author_name,
                    "avatar_url": _avatar_url_for(message.user) if message.user else None,
                    "content": message.content,
                    "created_at": message.created_at.isoformat(),
                }
                for message in messages
            ]
        })


@app.route("/api/lobbies/<suuid:lobby_id>/members", methods=["GET"])
def lobby_members(lobby_id):
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.server_password:
            user = _get_session_user()
            if not user:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
            membership = LobbyMember.get(lobby=lobby, user=user)
            if not membership:
                return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
        now = datetime.datetime.utcnow()
        members = select(m for m in LobbyMember if m.lobby == lobby).order_by(LobbyMember.joined_at)[:200]
        return jsonify({
            "members": [
                {
                    "name": _display_name_for(member.user),
                    "discord_id": member.user.discord_id,
                    "active_yaml_id": member.active_yaml_id,
                    "active_yaml_title": member.active_yaml_title,
                    "active_yaml_game": member.active_yaml_game,
                    "active_yaml_player": member.active_yaml_player,
                    "active_yamls": _member_yaml_list(member),
                    "ready": member.ready,
                    "online": bool(member.last_seen and now - member.last_seen < datetime.timedelta(minutes=3)),
                    "is_host": member.user == lobby.owner,
                }
                for member in members
            ]
        })


@app.route("/api/lobbies/<suuid:lobby_id>/hint-catalog", methods=["GET"])
def lobby_hint_catalog(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        membership = LobbyMember.get(lobby=lobby, user=user)
        if lobby.server_password and not membership:
            return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
        if not membership:
            return abort(403)
        room = _latest_generated_room(lobby)
        if not room:
            return jsonify({"items": [], "locations": [], "game": "", "player_name": ""})
        resolved = _resolve_member_tracker_slot(membership, room)
        if not resolved:
            return jsonify({"items": [], "locations": [], "game": "", "player_name": ""})
        slot, player_name = resolved
        try:
            tracker_data = TrackerData(room)
            game_name = tracker_data.get_player_game(slot)
            item_map = tracker_data.item_name_to_id.get(game_name, {}) or {}
            location_map = tracker_data.location_name_to_id.get(game_name, {}) or {}
            items = sorted([str(name) for name in item_map.keys() if str(name).strip()], key=lambda x: x.lower())
            locations = sorted([str(name) for name in location_map.keys() if str(name).strip()], key=lambda x: x.lower())
            return jsonify({
                "game": game_name,
                "player_name": player_name,
                "items": items,
                "locations": locations,
            })
        except Exception:
            return jsonify({"items": [], "locations": [], "game": "", "player_name": ""})


@app.route("/api/lobbies/<suuid:lobby_id>/active-yaml", methods=["POST"])
def lobby_active_yaml(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    yaml_id = (payload.get("yaml_id") or "").strip()
    action = (payload.get("action") or "set").strip().lower()
    if action not in {"set", "add", "remove", "clear"}:
        return jsonify({"error": "Invalid action."}), 400
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        ban = _get_active_ban(lobby, user)
        if ban:
            return jsonify({"error": _ban_notice(ban), "reason": "ban"}), 403
        membership = LobbyMember.get(lobby=lobby, user=user)
        if lobby.server_password and not membership:
            return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
        if not membership:
            membership = LobbyMember(lobby=lobby, user=user)
        if action in {"add", "set"} and not yaml_id:
            return jsonify({"error": "Missing YAML id."}), 400

        def sync_primary_yaml() -> None:
            entries = select(y for y in LobbyMemberYaml if y.member == membership).order_by(LobbyMemberYaml.created_at)[:1]
            primary = entries[0] if entries else None
            if primary:
                membership.active_yaml_id = primary.yaml_id
                membership.active_yaml_title = primary.title
                membership.active_yaml_game = primary.game
                membership.active_yaml_player = primary.player_name
            else:
                membership.active_yaml_id = ""
                membership.active_yaml_title = ""
                membership.active_yaml_game = ""
                membership.active_yaml_player = ""
                membership.ready = False
                membership.ready_at = None

        if action == "clear":
            select(y for y in LobbyMemberYaml if y.member == membership).delete()
            sync_primary_yaml()
            membership.last_seen = datetime.datetime.utcnow()
            lobby.last_activity = datetime.datetime.utcnow()
            _emit_members_update(lobby)
            return jsonify({"status": "cleared", "active_yamls": _member_yaml_list(membership)})

        if action == "remove":
            if not yaml_id:
                return jsonify({"error": "Missing YAML id."}), 400
            select(y for y in LobbyMemberYaml if y.member == membership and y.yaml_id == yaml_id).delete()
            sync_primary_yaml()
            membership.last_seen = datetime.datetime.utcnow()
            lobby.last_activity = datetime.datetime.utcnow()
            _emit_members_update(lobby)
            return jsonify({"status": "removed", "active_yamls": _member_yaml_list(membership)})

        entry = _load_yaml_entry(user, yaml_id)
        if not entry:
            return jsonify({"error": "YAML not found."}), 404
        if entry.get("custom") and not lobby.allow_custom_yamls:
            return jsonify({"error": "Custom YAMLs are disabled in this lobby.", "reason": "custom"}), 403
        if entry.get("player_name"):
            existing_players = {
                (y.player_name or "").strip().lower()
                for y in select(y for y in LobbyMemberYaml if y.member == membership)
                if y.yaml_id != entry["id"]
            }
            if (entry["player_name"] or "").strip().lower() in existing_players:
                return jsonify({"error": "Duplicate player name in active YAMLs."}), 400
        if action == "set":
            select(y for y in LobbyMemberYaml if y.member == membership).delete()
        existing = LobbyMemberYaml.get(member=membership, yaml_id=entry["id"])
        if not existing:
            LobbyMemberYaml(
                member=membership,
                yaml_id=entry["id"],
                title=entry.get("title"),
                game=entry.get("game"),
                player_name=entry.get("player_name"),
            )
        sync_primary_yaml()
        membership.last_seen = datetime.datetime.utcnow()
        lobby.last_activity = datetime.datetime.utcnow()
        _emit_members_update(lobby)
        return jsonify({"status": "ok", "yaml": entry, "active_yamls": _member_yaml_list(membership)})


@app.route("/api/lobbies/<suuid:lobby_id>/ready", methods=["POST"])
def lobby_ready(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    ready_flag = payload.get("ready")
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        ban = _get_active_ban(lobby, user)
        if ban:
            return jsonify({"error": _ban_notice(ban), "reason": "ban"}), 403
        membership = LobbyMember.get(lobby=lobby, user=user)
        if lobby.server_password and not membership:
            return jsonify({"error": "Lobby password required.", "reason": "password"}), 403
        if not membership:
            membership = LobbyMember(lobby=lobby, user=user)
        if ready_flag not in (True, False):
            ready_flag = not membership.ready
        has_yaml = bool(select(y for y in LobbyMemberYaml if y.member == membership).first()) or bool(membership.active_yaml_id)
        if ready_flag and not has_yaml:
            return jsonify({"error": "Select an active YAML before readying up."}), 400
        membership.ready = bool(ready_flag)
        membership.ready_at = datetime.datetime.utcnow() if membership.ready else None
        membership.last_seen = datetime.datetime.utcnow()
        lobby.last_activity = datetime.datetime.utcnow()
        _emit_members_update(lobby)
        return jsonify({"status": "ok", "ready": membership.ready})


@app.route("/api/lobbies/<suuid:lobby_id>/generate", methods=["POST"])
def lobby_generate(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner != user:
            return abort(403)

        existing = LobbyGeneration.get(lobby=lobby, status="queued")
        if existing:
            return jsonify({"error": "A generation is already queued for this lobby."}), 400
        latest = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
        if latest and latest.status == "done":
            payload = {"error": "Generation already completed for this lobby."}
            if latest.seed_id:
                payload["seed_url"] = f"/seed/{to_url(latest.seed_id)}"
            if latest.room_id:
                payload["room_url"] = f"/room/{to_url(latest.room_id)}"
            return jsonify(payload), 400

        members = list(select(m for m in LobbyMember if m.lobby == lobby))
        if not members:
            return jsonify({"error": "No members in lobby."}), 400
        not_ready = [m for m in members if not m.ready]
        if not_ready:
            return jsonify({"error": "All members must be Ready before generating."}), 400
        def member_yaml_entries(member: LobbyMember) -> list[dict[str, Any]]:
            entries = [
                {
                    "id": y.yaml_id,
                    "title": y.title,
                    "game": y.game,
                    "player_name": y.player_name,
                }
                for y in select(y for y in LobbyMemberYaml if y.member == member).order_by(LobbyMemberYaml.created_at)
            ]
            if entries:
                return entries
            if member.active_yaml_id:
                return [{
                    "id": member.active_yaml_id,
                    "title": member.active_yaml_title,
                    "game": member.active_yaml_game,
                    "player_name": member.active_yaml_player,
                }]
            return []

        missing_yaml = [m for m in members if not member_yaml_entries(m)]
        if missing_yaml:
            return jsonify({"error": "All members must select an active YAML."}), 400

        yaml_files: dict[str, bytes] = {}
        filename_counts: dict[str, int] = {}
        for member in members:
            for entry in member_yaml_entries(member):
                yaml_id = entry.get("id")
                if not yaml_id:
                    continue
                user_dir = _get_user_yaml_dir(member.user)
                yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
                if not os.path.exists(yaml_path):
                    return jsonify({"error": f"Missing YAML for {member.user.username or 'user'}."}), 400
                with open(yaml_path, "rb") as f:
                    content = f.read()
                base_name = get_file_safe_name(entry.get("title") or yaml_id)
                filename = f"{base_name or yaml_id}.yaml"
                count = filename_counts.get(filename, 0)
                if count:
                    filename = f"{base_name or yaml_id}-{count + 1}.yaml"
                filename_counts[filename] = count + 1
                yaml_files[filename] = content

        if not yaml_files:
            return jsonify({"error": "No YAML files found to generate."}), 400

        with tempfile.TemporaryDirectory() as tmpdir:
            zip_path = os.path.join(tmpdir, "lobby-yamls.zip")
            with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
                for filename, content in yaml_files.items():
                    zf.writestr(filename, content)

        options_source = {
            "hint_cost": lobby.hint_cost,
            "release_mode": lobby.release_mode,
            "remaining_mode": lobby.remaining_mode,
            "collect_mode": lobby.collect_mode,
            "countdown_mode": lobby.countdown_mode,
            "item_cheat": int(lobby.item_cheat),
            "server_password": lobby.server_password or "",
            "spoiler": lobby.spoiler,
            "plando_items": lobby.plando_items,
            "plando_bosses": lobby.plando_bosses,
            "plando_connections": lobby.plando_connections,
            "plando_texts": lobby.plando_texts,
        }
        meta = get_meta(options_source, False)
        results, gen_options = roll_options(yaml_files, set(meta["plando_options"]))
        if any(type(result) == str for result in results.values()):
            error_lines = [f"{name}: {result}" for name, result in results.items() if type(result) == str]
            return jsonify({"error": "YAML error: " + "; ".join(error_lines)}), 400

        if len(gen_options) > app.config["MAX_ROLL"]:
            return jsonify({"error": "Too many players for server generation."}), 400

        requested_by = _display_name_for(user)
        summary_lines = []
        for member in members:
            entries = member_yaml_entries(member)
            if not entries:
                continue
            titles = ", ".join([e.get("title") or e.get("id") or "YAML" for e in entries])
            summary_lines.append(f"{_display_name_for(member.user)} → {titles}")
        _add_system_message(lobby, f"Generation requested by {requested_by}. Players: {len(members)}.")
        for line in summary_lines:
            _add_system_message(lobby, line)

        if len(gen_options) >= app.config["JOB_THRESHOLD"]:
            gen = Generation(
                options=restricted_dumps({name: vars(options) for name, options in gen_options.items()}),
                meta=json.dumps(meta),
                state=STATE_QUEUED,
                owner=session["_id"],
            )
            lobby_gen = LobbyGeneration(
                lobby=lobby,
                generation_id=gen.id,
                owner_uuid=session["_id"],
                owner_user=user,
                status="queued",
            )
            _add_system_message(lobby, f"Generation queued. ID: {gen.id}.")
            _emit_generation_update(lobby)
            lobby.last_activity = datetime.datetime.utcnow()
            return jsonify({"status": "queued", "generation_id": str(gen.id)})

        seed_id = gen_game(
            {name: vars(options) for name, options in gen_options.items()},
            meta=meta,
            owner=session["_id"].int,
            timeout=app.config["JOB_TIME"],
        )
        seed = Seed.get(id=seed_id)
        if not seed:
            return jsonify({"error": "Generation failed."}), 500
        room = Room(seed=seed, owner=session["_id"], tracker=uuid4())
        lobby_gen = LobbyGeneration(
            lobby=lobby,
            generation_id=seed_id,
            owner_uuid=session["_id"],
            owner_user=user,
            status="done",
            seed_id=seed_id,
            room_id=room.id,
            completed_at=datetime.datetime.utcnow(),
        )
        seed_url = f"/seed/{to_url(seed_id)}"
        room_url = f"/room/{to_url(room.id)}"
        _add_system_message(lobby, f"Generation complete. Seed: {seed_url} Room: {room_url}")
        _emit_generation_update(lobby)
        lobby.last_activity = datetime.datetime.utcnow()
        return jsonify({"status": "done", "seed_url": seed_url, "room_url": room_url})


@app.route("/api/lobbies/<suuid:lobby_id>/kick", methods=["POST"])
def lobby_kick(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    target_name = (payload.get("name") or "").strip()
    action = (payload.get("action") or "kick").strip().lower()
    if action not in {"kick", "ban"}:
        return jsonify({"error": "Invalid moderation action."}), 400
    if not target_name:
        return jsonify({"error": "Missing user name."}), 400
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner != user:
            return abort(403)
        members = select(m for m in LobbyMember if m.lobby == lobby)
        target_member = None
        for member in members:
            if _display_name_for(member.user) == target_name:
                target_member = member
                break
        if not target_member:
            return jsonify({"error": "User not found in lobby."}), 404
        if target_member.user == user:
            return jsonify({"error": "Cannot kick yourself."}), 400
        target_user = target_member.user
        target_member.delete()

        ban = None
        if action == "ban":
            duration_minutes = payload.get("duration_minutes")
            duration_hours = payload.get("duration_hours")
            ban_expires_at = None
            if duration_minutes is None and duration_hours is None:
                duration_minutes = 0
            if duration_minutes is not None:
                try:
                    minutes = int(float(duration_minutes))
                except (TypeError, ValueError):
                    minutes = 0
            else:
                try:
                    minutes = int(float(duration_hours) * 60)
                except (TypeError, ValueError):
                    minutes = 0
            if minutes <= 0:
                ban_expires_at = None
            else:
                minutes = min(max(minutes, 5), 60 * 24 * 30)
                ban_expires_at = datetime.datetime.utcnow() + datetime.timedelta(minutes=minutes)
            ban = LobbyBan(
                lobby=lobby,
                user=target_user,
                created_by=user,
                reason="Banned by lobby owner.",
                expires_at=ban_expires_at,
            )
            _add_system_message(lobby, f"{target_name} was banned from the lobby.")
        else:
            _add_system_message(lobby, f"{target_name} was kicked from the lobby.")

        if target_user.discord_id:
            if action == "ban":
                if ban and ban.expires_at:
                    until = ban.expires_at.strftime("%b %d, %Y %H:%M UTC")
                    user_message = f"You were banned from the lobby by the owner. Ban ends {until}."
                else:
                    user_message = "You were banned from the lobby by the owner."
            else:
                user_message = "You were kicked from the lobby by the owner."
            socketio.emit(
                "lobby_kicked",
                {
                    "lobby_id": str(lobby.id),
                    "message": user_message,
                    "redirect_url": "/",
                },
                to=_user_room(target_user.discord_id),
            )
        _emit_members_update(lobby)
        lobby.last_activity = datetime.datetime.utcnow()
        return jsonify({"status": "ok", "action": action})


@app.route("/api/lobbies/<suuid:lobby_id>/generation")
def lobby_generation_status(lobby_id):
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        lobby_gen = select(lg for lg in LobbyGeneration if lg.lobby == lobby).order_by(desc(LobbyGeneration.created_at)).first()
        if not lobby_gen:
            return jsonify({"status": "none"})
        payload = {
            "status": lobby_gen.status,
            "error": lobby_gen.error or "",
        }
        if lobby_gen.seed_id:
            payload["seed_url"] = f"/seed/{to_url(lobby_gen.seed_id)}"
        if lobby_gen.room_id:
            payload["room_url"] = f"/room/{to_url(lobby_gen.room_id)}"
        if lobby_gen.completed_at:
            payload["completed_at"] = lobby_gen.completed_at.isoformat()
        return jsonify(payload)


@app.route("/api/lobbies/<suuid:lobby_id>/settings", methods=["POST"])
def lobby_settings(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner != user:
            return abort(403)
        server_password = (payload.get("server_password") or "").strip()
        clear_password = _parse_bool(payload.get("clear_password")) is True
        if server_password and len(server_password) > 64:
            return jsonify({"error": "Server password is too long."}), 400
        release_mode = _parse_mode(payload.get("release_mode"), {"enabled", "disabled", "auto", "auto-enabled", "goal"}, lobby.release_mode)
        collect_mode = _parse_mode(payload.get("collect_mode"), {"enabled", "disabled", "auto", "auto-enabled", "goal"}, lobby.collect_mode)
        remaining_mode = _parse_mode(payload.get("remaining_mode"), {"enabled", "disabled", "goal"}, lobby.remaining_mode)
        countdown_mode = _parse_mode(payload.get("countdown_mode"), {"enabled", "disabled", "auto"}, lobby.countdown_mode)
        item_cheat = _parse_bool(payload.get("item_cheat"))
        spoiler = _parse_int(payload.get("spoiler"), lobby.spoiler, 0, 3)
        hint_cost = _parse_int(payload.get("hint_cost"), lobby.hint_cost, 0, 100)
        max_players = _parse_int(payload.get("max_players"), _get_max_players(lobby), 1, 50)
        slow_release_timeout = _parse_slow_release_timeout(
            payload.get("slow_release_timeout"),
            _get_slow_release_timeout(lobby),
        )
        plando_items = _parse_bool(payload.get("plando_items"))
        plando_bosses = _parse_bool(payload.get("plando_bosses"))
        plando_connections = _parse_bool(payload.get("plando_connections"))
        plando_texts = _parse_bool(payload.get("plando_texts"))
        allow_custom_yamls = _parse_bool(payload.get("allow_custom_yamls"))

        lobby.release_mode = release_mode
        lobby.collect_mode = collect_mode
        lobby.remaining_mode = remaining_mode
        lobby.countdown_mode = countdown_mode
        if item_cheat is not None:
            lobby.item_cheat = item_cheat
        lobby.spoiler = spoiler
        lobby.hint_cost = hint_cost
        if plando_items is not None:
            lobby.plando_items = plando_items
        if plando_bosses is not None:
            lobby.plando_bosses = plando_bosses
        if plando_connections is not None:
            lobby.plando_connections = plando_connections
        if plando_texts is not None:
            lobby.plando_texts = plando_texts
        if allow_custom_yamls is not None:
            lobby.allow_custom_yamls = allow_custom_yamls
        if clear_password:
            lobby.server_password = ""
        elif server_password:
            lobby.server_password = server_password
        _set_slow_release_timeout(lobby, slow_release_timeout)
        _set_max_players(lobby, max_players)

        lobby.last_activity = datetime.datetime.utcnow()
        settings_payload = {
            "release_mode": lobby.release_mode,
            "collect_mode": lobby.collect_mode,
            "remaining_mode": lobby.remaining_mode,
            "countdown_mode": lobby.countdown_mode,
            "item_cheat": lobby.item_cheat,
            "spoiler": lobby.spoiler,
            "hint_cost": lobby.hint_cost,
            "max_players": max_players,
            "plando_items": lobby.plando_items,
            "plando_bosses": lobby.plando_bosses,
            "plando_connections": lobby.plando_connections,
            "plando_texts": lobby.plando_texts,
            "allow_custom_yamls": lobby.allow_custom_yamls,
            "is_private": bool(lobby.server_password),
            "slow_release_timeout": slow_release_timeout,
        }
        socketio.emit("lobby_settings", settings_payload, to=f"lobby:{to_url(lobby.id)}")
        return jsonify({"status": "ok", "settings": settings_payload})

def _shutdown_lobby_room(lobby: Lobby) -> None:
    latest = select(
        lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None
    ).order_by(desc(LobbyGeneration.created_at)).first()
    if not latest or not latest.room_id:
        return
    room = Room.get(id=latest.room_id)
    if not room:
        return

    # Ask the room server to stop if it is alive.
    Command(room=room, commandtext="/exit")

    # Also force stale state in DB so autohost does not keep respawning it.
    now = datetime.datetime.utcnow()
    room.last_port = 0
    room.last_activity = now - datetime.timedelta(seconds=(room.timeout or 0) + 60)


@app.route("/api/lobbies/<suuid:lobby_id>/close", methods=["POST"])
def lobby_close(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner != user:
            return abort(403)
        socketio.emit(
            "lobby_closed",
            {"message": "Lobby closed by the host.", "redirect_url": "/"},
            to=f"lobby:{to_url(lobby.id)}",
        )
        _delete_lobby_voice(lobby)
        _shutdown_lobby_room(lobby)
        lobby.delete()
        return jsonify({"status": "ok"})


@app.route("/api/lobbies/<suuid:lobby_id>/transfer-host", methods=["POST"])
def lobby_transfer_host(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    target_name = (payload.get("name") or "").strip()
    if not target_name:
        return jsonify({"error": "Missing user name."}), 400
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner != user:
            return abort(403)
        target_member = select(
            m for m in LobbyMember if m.lobby == lobby and _display_name_for(m.user) == target_name
        ).first()
        if not target_member:
            return jsonify({"error": "User not found in lobby."}), 404
        if target_member.user == user:
            return jsonify({"error": "You are already the host."}), 400
        lobby.owner = target_member.user
        _clear_host_votes(lobby)
        _add_system_message(lobby, f"{_display_name_for(user)} assigned host to {target_name}.")
        _emit_members_update(lobby)
        socketio.emit(
            "lobby_host_changed",
            {"host_name": target_name},
            to=f"lobby:{to_url(lobby.id)}",
        )
        return jsonify({"status": "ok", "host_name": target_name})


@app.route("/api/lobbies/<suuid:lobby_id>/host-status")
def lobby_host_status(lobby_id):
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        host_member = _get_host_member(lobby)
        now = datetime.datetime.utcnow()
        host_last_seen = host_member.last_seen if host_member else None
        host_absent_seconds = (now - host_last_seen).total_seconds() if host_last_seen else None
        host_absent = True if not host_member else bool(host_absent_seconds and host_absent_seconds >= 15 * 60)
        if host_absent and host_absent_seconds is None:
            host_absent_seconds = 15 * 60
        candidate = _get_candidate_host(lobby)
        active_members = [
            m for m in select(m for m in LobbyMember if m.lobby == lobby)
            if m.last_seen and now - m.last_seen < datetime.timedelta(minutes=3)
        ]
        active_count = max(0, len(active_members) - 1)  # exclude host for voting
        vote_count = count(v for v in LobbyHostVote if v.lobby == lobby)
        vote_needed = max(1, (active_count // 2) + (active_count % 2))
        return jsonify({
            "host_name": _display_name_for(lobby.owner),
            "host_absent": host_absent,
            "host_absent_seconds": int(host_absent_seconds) if host_absent_seconds else 0,
            "candidate_name": _display_name_for(candidate.user) if candidate else "",
            "vote_count": int(vote_count),
            "vote_needed": int(vote_needed),
            "active_voters": int(active_count),
        })


@app.route("/api/lobbies/<suuid:lobby_id>/vote-host", methods=["POST"])
def lobby_vote_host(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        if lobby.owner == user:
            return jsonify({"error": "Host cannot vote."}), 400
        host_member = _get_host_member(lobby)
        now = datetime.datetime.utcnow()
        if not host_member or not host_member.last_seen:
            host_absent = True
        else:
            host_absent = (now - host_member.last_seen) >= datetime.timedelta(minutes=15)
        if not host_absent:
            return jsonify({"error": "Host is still active."}), 400
        candidate = _get_candidate_host(lobby)
        if not candidate:
            socketio.emit(
                "lobby_closed",
                {"message": "Lobby closed due to inactivity.", "redirect_url": "/"},
                to=f"lobby:{to_url(lobby.id)}",
            )
            _shutdown_lobby_room(lobby)
            lobby.delete()
            return jsonify({"status": "closed"})
        existing_vote = LobbyHostVote.get(lobby=lobby, user=user)
        if existing_vote:
            return jsonify({"status": "ok"})
        LobbyHostVote(lobby=lobby, user=user)
        active_members = [
            m for m in select(m for m in LobbyMember if m.lobby == lobby)
            if m.last_seen and now - m.last_seen < datetime.timedelta(minutes=3)
        ]
        active_count = max(0, len(active_members) - 1)
        vote_count = count(v for v in LobbyHostVote if v.lobby == lobby)
        vote_needed = max(1, (active_count // 2) + (active_count % 2))
        if vote_count >= vote_needed:
            old_host = _display_name_for(lobby.owner)
            lobby.owner = candidate.user
            _clear_host_votes(lobby)
            _add_system_message(lobby, f"{_display_name_for(candidate.user)} is now the host.")
            _emit_members_update(lobby)
            socketio.emit(
                "lobby_host_changed",
                {"host_name": _display_name_for(candidate.user)},
                to=f"lobby:{to_url(lobby.id)}",
            )
        return jsonify({"status": "ok", "vote_count": int(vote_count), "vote_needed": int(vote_needed)})


@app.route("/api/lobbies/<suuid:lobby_id>/release", methods=["POST"])
def lobby_release(lobby_id):
    user = _get_session_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    player_name = (payload.get("player_name") or "").strip()
    mode = (payload.get("mode") or "release").strip().lower()
    if not player_name:
        return jsonify({"error": "Missing player name."}), 400
    if mode not in {"release", "slow"}:
        return jsonify({"error": "Invalid release mode."}), 400
    with db_session:
        lobby = Lobby.get(id=lobby_id)
        if not lobby:
            return abort(404)
        ban = _get_active_ban(lobby, user)
        if ban:
            return jsonify({"error": _ban_notice(ban)}), 403
        target_member = select(
            m for m in LobbyMember if m.lobby == lobby and m.active_yaml_player == player_name
        ).first()
        if not target_member:
            return jsonify({"error": "Player not found in lobby."}), 404
        if lobby.owner != user and target_member.user != user:
            return abort(403)
        latest = select(
            lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None
        ).order_by(desc(LobbyGeneration.created_at)).first()
        if not latest or not latest.room_id:
            return jsonify({"error": "Room not ready yet."}), 400
        room = Room.get(id=latest.room_id)
        if not room:
            return jsonify({"error": "Room not ready yet."}), 400
        initiator = _display_name_for(user)
        if mode == "release":
            if target_member.user == user:
                notice = f"[Lobby] {initiator} released themselves."
            elif lobby.owner == user:
                notice = f"[Lobby] {initiator} released {player_name}."
            else:
                notice = f"[Lobby] {initiator} requested release for {player_name}."
            _add_system_message(lobby, notice)
            Command(room=room, commandtext=notice)
            Command(room=room, commandtext=f"/release {player_name}")
            try:
                _redis.sadd(f"{_RELEASED_KEY_PREFIX}{room.id}", player_name.strip().lower())
                _redis.expire(f"{_RELEASED_KEY_PREFIX}{room.id}", 7 * 24 * 60 * 60)
            except Exception:
                pass
        else:
            if target_member.user == user:
                notice = f"[Lobby] {initiator} enabled slow release for themselves."
            elif lobby.owner == user:
                notice = f"[Lobby] {initiator} enabled slow release for {player_name}."
            else:
                notice = f"[Lobby] {initiator} requested slow release for {player_name}."
            _add_system_message(lobby, notice)
            Command(room=room, commandtext=notice)
            Command(room=room, commandtext=f"/allow_release {player_name}")
        room.last_activity = datetime.datetime.utcnow()
        return jsonify({"status": "ok"})
