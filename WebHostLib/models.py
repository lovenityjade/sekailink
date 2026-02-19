from datetime import datetime
from uuid import UUID, uuid4
from pony.orm import Database, PrimaryKey, Required, Set, Optional, buffer, LongStr, composite_key

db = Database()

STATE_QUEUED = 0
STATE_STARTED = 1
STATE_ERROR = -1


class Slot(db.Entity):
    id = PrimaryKey(int, auto=True)
    player_id = Required(int)
    player_name = Required(str)
    data = Optional(bytes, lazy=True)
    seed = Optional('Seed')
    game = Required(str)


class Room(db.Entity):
    id = PrimaryKey(UUID, default=uuid4)
    last_activity = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    creation_time = Required(datetime, default=lambda: datetime.utcnow(), index=True)  # index used by landing page
    owner = Required(UUID, index=True)
    commands = Set('Command')
    seed = Required('Seed', index=True)
    multisave = Optional(buffer, lazy=True)
    show_spoiler = Required(int, default=0)  # 0 -> never, 1 -> after completion, -> 2 always
    timeout = Required(int, default=lambda: 2 * 60 * 60)  # seconds since last activity to shutdown
    tracker = Optional(UUID, index=True)
    # Port special value -1 means the server errored out. Another attempt can be made with a page refresh
    last_port = Optional(int, default=lambda: 0)


class Seed(db.Entity):
    id = PrimaryKey(UUID, default=uuid4)
    rooms = Set(Room)
    multidata = Required(bytes, lazy=True)
    owner = Required(UUID, index=True)
    creation_time = Required(datetime, default=lambda: datetime.utcnow(), index=True)  # index used by landing page
    slots = Set(Slot)
    spoiler = Optional(LongStr, lazy=True)
    meta = Required(LongStr, default=lambda: "{\"race\": false}")  # additional meta information/tags


class Command(db.Entity):
    id = PrimaryKey(int, auto=True)
    room = Required(Room)
    commandtext = Required(str)


class Generation(db.Entity):
    id = PrimaryKey(UUID, default=uuid4)
    owner = Required(UUID)
    options = Required(buffer, lazy=True)
    meta = Required(LongStr, default=lambda: "{\"race\": false}")
    state = Required(int, default=0, index=True)


class GameDataPackage(db.Entity):
    checksum = PrimaryKey(str)
    data = Required(bytes)


class User(db.Entity):
    discord_id = PrimaryKey(str)
    user_uuid = Optional(str, unique=True)
    username = Optional(str)
    global_name = Optional(str)
    avatar = Optional(str)
    email = Optional(str)
    role = Required(str, default="player")
    banned = Required(bool, default=False)
    ban_reason = Optional(str)
    patreon_id = Optional(str)
    patreon_member_id = Optional(str)
    patreon_email = Optional(str)
    patreon_status = Optional(str)
    patreon_tier = Optional(str)
    patreon_is_supporter = Required(bool, default=False)
    patreon_last_sync = Optional(datetime, index=True)
    patreon_raw = Optional(LongStr)
    presence_status = Required(str, default="online")
    dm_policy = Required(str, default="friends")
    is_online = Required(bool, default=False)
    last_seen_at = Optional(datetime, index=True)
    presence_updated_at = Optional(datetime, index=True)
    raw_profile = Optional(LongStr)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    last_login = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    terms_accepted = Required(bool, default=False)
    terms_accepted_at = Optional(datetime, index=True)
    terms_version = Optional(str)
    terms_accepted_ip = Optional(str)
    terms_accepted_user_agent = Optional(LongStr)
    bio = Optional(LongStr)
    badges_json = Optional(LongStr)
    custom_avatar_url = Optional(str)
    password_updated_at = Optional(datetime, index=True)
    tutorial = Required(bool, default=True)
    tutorial_completed_at = Optional(datetime, index=True)
    lobbies_owned = Set('Lobby')
    lobby_memberships = Set('LobbyMember')
    lobby_messages = Set('LobbyMessage')
    lobby_generations = Set('LobbyGeneration')
    lobby_bans = Set('LobbyBan', reverse='user')
    lobby_bans_created = Set('LobbyBan', reverse='created_by')
    lobby_host_votes = Set('LobbyHostVote')
    support_tickets = Set('SupportTicket')
    friend_requests_sent = Set('FriendRequest', reverse='from_user')
    friend_requests_received = Set('FriendRequest', reverse='to_user')
    friendships = Set('Friendship', reverse='user')
    friend_of = Set('Friendship', reverse='friend')
    direct_messages_sent = Set('DirectMessage', reverse='sender')
    direct_messages_received = Set('DirectMessage', reverse='recipient')
    yamls = Set('UserYaml')
    desktop_tokens = Set('DesktopToken')
    identities = Set('AuthIdentity')


class DesktopToken(db.Entity):
    token = PrimaryKey(str)
    user = Required(User)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    expires_at = Required(datetime, index=True)
    last_used_at = Optional(datetime, index=True)


class DesktopOAuthState(db.Entity):
    state = PrimaryKey(str)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    expires_at = Required(datetime, index=True)


class AuthIdentity(db.Entity):
    id = PrimaryKey(int, auto=True)
    user = Required(User)
    provider = Required(str, default="discord", index=True)
    provider_user_id = Optional(str, unique=True)
    email = Optional(str, unique=True)
    password_hash = Optional(str)
    email_verified = Required(bool, default=False)
    verification_token_hash = Optional(str, index=True)
    verification_sent_at = Optional(datetime, index=True)
    preferred_locale = Required(str, default="en")
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    last_login = Optional(datetime, index=True)


class Lobby(db.Entity):
    id = PrimaryKey(UUID, default=uuid4)
    name = Required(str)
    description = Optional(str)
    owner = Required(User)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    last_activity = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    server_password = Optional(str)
    release_mode = Required(str, default="enabled")
    collect_mode = Required(str, default="goal")
    remaining_mode = Required(str, default="enabled")
    countdown_mode = Required(str, default="auto")
    item_cheat = Required(bool, default=False)
    spoiler = Required(int, default=0)
    hint_cost = Required(int, default=5)
    plando_items = Required(bool, default=True)
    plando_bosses = Required(bool, default=True)
    plando_connections = Required(bool, default=True)
    plando_texts = Required(bool, default=True)
    allow_custom_yamls = Required(bool, default=True)
    members = Set('LobbyMember')
    messages = Set('LobbyMessage')
    generations = Set('LobbyGeneration')
    bans = Set('LobbyBan')
    host_votes = Set('LobbyHostVote')


class LobbyMember(db.Entity):
    id = PrimaryKey(int, auto=True)
    lobby = Required(Lobby)
    user = Required(User)
    joined_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    last_seen = Optional(datetime, index=True)
    active_yaml_id = Optional(str)
    active_yaml_title = Optional(str)
    active_yaml_game = Optional(str)
    active_yaml_player = Optional(str)
    yamls = Set('LobbyMemberYaml')
    ready = Required(bool, default=False, index=True)
    ready_at = Optional(datetime, index=True)


class LobbyMemberYaml(db.Entity):
    id = PrimaryKey(int, auto=True)
    member = Required(LobbyMember)
    yaml_id = Required(str)
    title = Optional(str)
    game = Optional(str)
    player_name = Optional(str)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class UserYaml(db.Entity):
    id = PrimaryKey(int, auto=True)
    user = Required(User)
    yaml_id = Required(str)
    custom = Required(bool, default=False)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    updated_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class LobbyMessage(db.Entity):
    id = PrimaryKey(int, auto=True)
    lobby = Required(Lobby)
    user = Optional(User)
    author_name = Required(str)
    content = Required(LongStr)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class LobbyGeneration(db.Entity):
    id = PrimaryKey(int, auto=True)
    lobby = Required(Lobby)
    generation_id = Required(UUID, unique=True)
    owner_uuid = Required(UUID)
    owner_user = Optional(User)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    completed_at = Optional(datetime, index=True)
    status = Required(str, default="queued", index=True)
    seed_id = Optional(UUID)
    room_id = Optional(UUID)
    error = Optional(LongStr)


class LobbyBan(db.Entity):
    id = PrimaryKey(int, auto=True)
    lobby = Required(Lobby)
    user = Required(User, reverse='lobby_bans')
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    created_by = Optional(User, reverse='lobby_bans_created')
    expires_at = Optional(datetime, index=True)
    reason = Optional(str)


class LobbyHostVote(db.Entity):
    id = PrimaryKey(int, auto=True)
    lobby = Required(Lobby)
    user = Required(User)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class SupportTicket(db.Entity):
    id = PrimaryKey(int, auto=True)
    user = Required(User)
    category = Required(str, default="ban_appeal")
    status = Required(str, default="open", index=True)
    subject = Optional(str)
    message = Required(LongStr)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class FriendRequest(db.Entity):
    id = PrimaryKey(int, auto=True)
    from_user = Required(User, reverse='friend_requests_sent')
    to_user = Required(User, reverse='friend_requests_received')
    status = Required(str, default="pending", index=True)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class Friendship(db.Entity):
    id = PrimaryKey(int, auto=True)
    user = Required(User, reverse='friendships')
    friend = Required(User)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)


class DirectMessage(db.Entity):
    id = PrimaryKey(int, auto=True)
    sender = Required(User, reverse='direct_messages_sent')
    recipient = Required(User, reverse='direct_messages_received')
    content = Required(LongStr)
    created_at = Required(datetime, default=lambda: datetime.utcnow(), index=True)
    read_at = Optional(datetime, index=True)
