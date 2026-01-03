import os
import sys
import uuid
import logging
import json
import yaml
import hashlib
from datetime import datetime
from urllib.parse import quote
from flask import Flask, session, request, jsonify, redirect, send_file
from flask_cors import CORS
from flask_socketio import SocketIO, emit, join_room, leave_room, rooms
import requests
from werkzeug.utils import secure_filename
from tasks import run_generator

# Import models from models package
from models import (
    db, User, YamlFile, RomFile, Lobby, LobbySettings, LobbyPlayer, ChatMessage,
    Game, FavoriteGame, Friend, Ban, Warning, UserRating, UserReview,
    ServerRating, TwitchConnection, CustomWorld,
    init_db, calculate_server_rating, generate_lobby_slug
)
from models.choices import LobbyStates, PlayerStates, LobbyVisibility

# --- ENVIRONMENT VALIDATION ---
class ConfigurationError(Exception):
    """Raised when required environment variables are missing"""
    pass

def validate_environment():
    """
    Validate that all required environment variables are set.
    Fails fast with clear error messages if any are missing.
    """
    # Required variables that MUST be set
    required_vars = {
        'FLASK_SECRET_KEY': 'Flask secret key for session encryption',
        'DATABASE_URL': 'PostgreSQL database connection URL',
        'POSTGRES_USER': 'PostgreSQL username',
        'POSTGRES_PASSWORD': 'PostgreSQL password',
        'POSTGRES_DB': 'PostgreSQL database name',
        'REDIS_URL': 'Redis connection URL',
        'DISCORD_CLIENT_ID': 'Discord OAuth2 client ID',
        'DISCORD_CLIENT_SECRET': 'Discord OAuth2 client secret',
        'DISCORD_REDIRECT_URI': 'Discord OAuth2 redirect URI',
    }

    missing_vars = []
    empty_vars = []

    for var_name, description in required_vars.items():
        value = os.getenv(var_name)
        if value is None:
            missing_vars.append(f"  - {var_name}: {description}")
        elif value.strip() == '':
            empty_vars.append(f"  - {var_name}: {description}")

    if missing_vars or empty_vars:
        error_msg = "\n" + "="*70 + "\n"
        error_msg += "❌ CONFIGURATION ERROR: Missing required environment variables\n"
        error_msg += "="*70 + "\n\n"

        if missing_vars:
            error_msg += "Missing variables (not set in .env):\n"
            error_msg += "\n".join(missing_vars) + "\n\n"

        if empty_vars:
            error_msg += "Empty variables (set but blank in .env):\n"
            error_msg += "\n".join(empty_vars) + "\n\n"

        error_msg += "="*70 + "\n"
        error_msg += "How to fix:\n"
        error_msg += "  1. Copy .env.template to .env:\n"
        error_msg += "     cp .env.template .env\n\n"
        error_msg += "  2. Edit .env and fill in all required values\n\n"
        error_msg += "  3. See ENV_SETUP.md for detailed setup instructions\n"
        error_msg += "="*70 + "\n"

        raise ConfigurationError(error_msg)

    # Warn about optional but recommended variables
    optional_vars = {
        'TWITCH_CLIENT_ID': 'Twitch OAuth (for broadcasting feature)',
        'SMTP_HOST': 'Email notifications',
        'LOG_LEVEL': 'Logging verbosity',
    }

    missing_optional = []
    for var_name, description in optional_vars.items():
        if not os.getenv(var_name):
            missing_optional.append(f"  - {var_name}: {description}")

    if missing_optional:
        logger.warning("⚠️  Optional environment variables not set:")
        for var in missing_optional:
            logger.warning(var)
        logger.warning("These features will be disabled. See .env.template for details.\n")

# Validate environment before initializing Flask
try:
    validate_environment()
except ConfigurationError as e:
    print(str(e), file=sys.stderr)
    sys.exit(1)

# --- CONFIGURATION & LOGGING ---
log_level = os.getenv('LOG_LEVEL', 'INFO').upper()
logging.basicConfig(
    level=getattr(logging, log_level),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("SekaiLink")

logger.info("✅ Environment validation passed")
logger.info(f"🚀 Starting SekaiLink API (Environment: {os.getenv('FLASK_ENV', 'production')})")

app = Flask(__name__)
app.config['SECRET_KEY'] = os.getenv('FLASK_SECRET_KEY')
app.config['SQLALCHEMY_DATABASE_URI'] = os.getenv('DATABASE_URL')
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['MAX_CONTENT_LENGTH'] = int(os.getenv('MAX_UPLOAD_SIZE', 52428800))  # Default 50MB

# Initialize database from models package
db.init_app(app)
CORS(app, supports_credentials=True)

# --- SOCKETIO INITIALIZATION ---
socketio = SocketIO(
    app,
    cors_allowed_origins=os.getenv('ALLOWED_ORIGINS', '*').split(','),
    async_mode='eventlet',
    logger=True,
    engineio_logger=True,
    ping_timeout=60,
    ping_interval=25
)

logger.info("✅ SocketIO initialized for real-time communications")

# --- CONFIGURATION DES FICHIERS ---
ALLOWED_EXTENSIONS = {'sfc', 'gbc', 'gba', 'n64', 'iso'}

# Temporary ROM storage (per user)
# Note: ROMs are NOT stored permanently - they're in /tmp and cleared monthly
ROM_TEMP_BASE = '/tmp/generation/roms'
os.makedirs(ROM_TEMP_BASE, exist_ok=True)

KNOWN_ROMS = {
    "f3ae08818175831599faef9e236b28096f600f94": "Pokémon Emerald (USA)",
    "e26ee8a442910395379059f1479fa6972ef784d8": "Pokémon FireRed (USA v1.0)",
    "03a63b3f0e989a518464999597196034c3116bc2": "Zelda: A Link to the Past (USA v1.0)",
    "d63d2c86b4479a381484c47bc4795b54a2221351": "Super Metroid (Japan/USA)",
}

# Discord OAuth2 - Already validated, safe to use
DISCORD_CLIENT_ID = os.getenv('DISCORD_CLIENT_ID')
DISCORD_CLIENT_SECRET = os.getenv('DISCORD_CLIENT_SECRET')
DISCORD_REDIRECT_URI = os.getenv('DISCORD_REDIRECT_URI')

# --- DATABASE INITIALIZATION ---
# Models are imported from models package above
# Initialize database tables
with app.app_context():
    db.create_all()
    logger.info("✅ Database tables initialized")

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

# --- ROUTES D'AUTHENTIFICATION ---
@app.route('/api/auth/login')
def login():
    url = f"https://discord.com/api/oauth2/authorize?client_id={DISCORD_CLIENT_ID}&redirect_uri={quote(DISCORD_REDIRECT_URI, safe='')}&response_type=code&scope=identify+email"
    return redirect(url)

@app.route('/api/auth/callback')
def callback():
    code = request.args.get('code')
    data = {'client_id': DISCORD_CLIENT_ID, 'client_secret': DISCORD_CLIENT_SECRET, 'grant_type': 'authorization_code', 'code': code, 'redirect_uri': DISCORD_REDIRECT_URI}
    r = requests.post('https://discord.com/api/oauth2/token', data=data).json()
    token = r.get('access_token')
    u = requests.get('https://discord.com/api/users/@me', headers={'Authorization': f'Bearer {token}'}).json()
    user = User.query.filter_by(discord_id=u['id']).first()
    if not user:
        user = User(id=str(uuid.uuid4()), discord_id=u['id'], username=u['username'], avatar_url=f"https://cdn.discordapp.com/avatars/{u['id']}/{u['avatar']}.png", email=u.get('email'))
        db.session.add(user)
    else:
        user.username = u['username']
        user.avatar_url = f"https://cdn.discordapp.com/avatars/{u['id']}/{u['avatar']}.png"
    db.session.commit()
    session['user_id'] = user.id
    return redirect("/dashboard.html")

@app.route('/api/auth/logout')
def logout():
    session.clear()
    return redirect("/")

# --- ROUTES API ---
@app.route('/api/me', methods=['GET', 'POST'])
def me():
    uid = session.get('user_id')
    if not uid: return jsonify({"logged_in": False}), 401
    user = User.query.get(uid)
    if request.method == 'POST':
        data = request.json
        user.bio = data.get('bio', user.bio)
        user.pronouns = data.get('pronouns', user.pronouns)
        db.session.commit()
        return jsonify({"status": "updated"})
    return jsonify({"logged_in": True, "username": user.username, "avatar": user.avatar_url, "bio": user.bio, "pronouns": user.pronouns})

@app.route('/api/yamls', methods=['GET', 'POST'])
def handle_yamls():
    uid = session.get('user_id')
    if not uid: return jsonify([]), 401
    if request.method == 'GET':
        files = YamlFile.query.filter_by(user_id=uid).all()
        return jsonify([{"id": f.id, "filename": f.filename, "game": f.game, "content": f.content} for f in files])
    data = request.json
    try:
        y_data = yaml.safe_load(data['content'])
        game_name = next((k for k in y_data if k not in ['name', 'description', 'requires']), "Unknown")
        new_f = YamlFile(user_id=uid, filename=data['filename'], content=data['content'], game=game_name)
        db.session.add(new_f)
        db.session.commit()
        return jsonify({"status": "saved"})
    except: return jsonify({"error": "invalid yaml"}), 400

@app.route('/api/roms', methods=['GET'])
def list_roms():
    uid = session.get('user_id')
    if not uid: return jsonify([]), 401
    roms = RomFile.query.filter_by(user_id=uid).all()
    return jsonify([{"id": r.id, "name": r.filename, "sha": r.sha1, "game": r.game_detected, "status": r.status} for r in roms])

@app.route('/api/roms/upload', methods=['POST'])
def upload_rom():
    """
    Upload ROM to temporary storage with SHA-1 validation.
    ROMs are stored in /tmp/generation/roms/{user_id}/ and cleared monthly.
    """
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    file = request.files.get('file')
    if not file or not allowed_file(file.filename):
        return jsonify({"error": "invalid file type"}), 400

    filename = secure_filename(file.filename)

    # Create user-specific temporary directory
    user_rom_dir = os.path.join(ROM_TEMP_BASE, uid)
    os.makedirs(user_rom_dir, exist_ok=True)

    # Save to temporary location
    temp_path = os.path.join(user_rom_dir, filename)

    # If file already exists, remove it first
    if os.path.exists(temp_path):
        os.remove(temp_path)

    file.save(temp_path)

    # Calculate SHA-1 hash for validation
    sha1 = hashlib.sha1()
    try:
        with open(temp_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                sha1.update(chunk)
        f_sha = sha1.hexdigest()
    except Exception as e:
        logger.error(f"Failed to calculate SHA-1 for {filename}: {e}")
        os.remove(temp_path)
        return jsonify({"error": "failed to process file"}), 500

    # Validate against known ROMs
    game = KNOWN_ROMS.get(f_sha, "Unknown")
    status = "verified" if f_sha in KNOWN_ROMS else "unverified"

    # Check if ROM already exists for this user
    existing_rom = RomFile.query.filter_by(user_id=uid, sha1=f_sha).first()
    if existing_rom:
        # Update existing record
        existing_rom.filename = filename
        existing_rom.file_path = temp_path
        existing_rom.game_detected = game
        existing_rom.status = status
        existing_rom.uploaded_at = datetime.utcnow()
        db.session.commit()

        logger.info(f"ROM updated for user {uid}: {game} (SHA-1: {f_sha})")
        return jsonify({
            "status": status,
            "game": game,
            "sha1": f_sha,
            "message": "ROM updated"
        })

    # Create new ROM record
    new_r = RomFile(
        user_id=uid,
        filename=filename,
        file_path=temp_path,
        sha1=f_sha,
        game_detected=game,
        status=status
    )
    db.session.add(new_r)
    db.session.commit()

    logger.info(f"ROM uploaded for user {uid}: {game} (SHA-1: {f_sha}, Status: {status})")

    return jsonify({
        "status": status,
        "game": game,
        "sha1": f_sha,
        "message": "ROM uploaded successfully" if status == "verified" else "ROM uploaded but not verified"
    })

@app.route('/api/roms/<int:rom_id>', methods=['DELETE'])
def delete_rom(rom_id):
    """Delete a ROM from temporary storage"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    rom = RomFile.query.get(rom_id)
    if not rom:
        return jsonify({"error": "ROM not found"}), 404

    if rom.user_id != uid:
        return jsonify({"error": "unauthorized"}), 403

    # Delete physical file
    if rom.file_path and os.path.exists(rom.file_path):
        try:
            os.remove(rom.file_path)
            logger.info(f"Deleted ROM file: {rom.file_path}")
        except Exception as e:
            logger.error(f"Failed to delete ROM file {rom.file_path}: {e}")

    # Delete database record
    db.session.delete(rom)
    db.session.commit()

    logger.info(f"User {uid} deleted ROM: {rom.game_detected}")
    return jsonify({"message": "ROM deleted successfully"})

@app.route('/api/lobbies', methods=['GET'])
def get_lobbies():
    """List all open lobbies or user's lobbies"""
    uid = session.get('user_id')
    if not uid: return jsonify([]), 401

    # Get all open lobbies
    lobbies = Lobby.query.filter(
        Lobby.visibility.in_(['open', 'private']),
        Lobby.status.in_(['pending', 'ready', 'active'])
    ).order_by(Lobby.created_at.desc()).all()

    result = []
    for l in lobbies:
        # Get player count
        player_count = LobbyPlayer.query.filter_by(lobby_id=l.id).count()
        settings = LobbySettings.query.filter_by(lobby_id=l.id).first()
        max_players = settings.max_players if settings else 10

        result.append({
            "id": l.id,
            "name": l.name,
            "status": l.status,
            "visibility": l.visibility,
            "host_id": l.host_id,
            "players": f"{player_count}/{max_players}",
            "created_at": l.created_at.strftime("%Y-%m-%d %H:%M")
        })

    return jsonify(result)

@app.route('/api/lobbies/create', methods=['POST'])
def create_lobby():
    """Create a new lobby with settings"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    data = request.json
    lobby_name = data.get('name', 'New Lobby').strip()

    if not lobby_name or len(lobby_name) > 100:
        return jsonify({"error": "invalid lobby name"}), 400

    # Create lobby
    new_lobby = Lobby(
        host_id=uid,
        name=lobby_name,
        status='pending',
        visibility=data.get('visibility', 'open')
    )
    db.session.add(new_lobby)
    db.session.flush()  # Get lobby ID

    # Create settings
    settings = LobbySettings(
        lobby_id=new_lobby.id,
        max_players=data.get('max_players', 10),
        time_limit_hours=data.get('time_limit_hours'),
        sync_rules=data.get('sync_rules', ''),
        allow_multigame=data.get('allow_multigame', True),
        allow_broadcast=data.get('allow_broadcast', True)
    )
    db.session.add(settings)

    # Host automatically joins
    host_player = LobbyPlayer(
        lobby_id=new_lobby.id,
        user_id=uid,
        game='',  # Host selects game later
        yaml_file_id=None
    )
    db.session.add(host_player)

    db.session.commit()

    logger.info(f"User {uid} created lobby {new_lobby.id}: {lobby_name}")

    return jsonify({
        "lobby_id": new_lobby.id,
        "message": "Lobby created successfully"
    }), 201

@app.route('/api/lobbies/<int:lobby_id>', methods=['GET'])
def get_lobby_details(lobby_id):
    """Get full lobby details"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "lobby not found"}), 404

    # Get settings
    settings = LobbySettings.query.filter_by(lobby_id=lobby_id).first()

    # Get players
    players_query = db.session.query(LobbyPlayer, User).join(
        User, LobbyPlayer.user_id == User.id
    ).filter(LobbyPlayer.lobby_id == lobby_id).all()

    players = []
    for lp, user in players_query:
        players.append({
            "user_id": user.id,
            "username": user.username,
            "avatar": user.avatar_url,
            "pronouns": user.pronouns,
            "game": lp.game,
            "is_ready": lp.is_ready,
            "status": lp.status,
            "patch_url": lp.patch_url,
            "joined_at": lp.joined_at.isoformat()
        })

    # Get recent chat messages
    messages_query = db.session.query(ChatMessage, User).outerjoin(
        User, ChatMessage.user_id == User.id
    ).filter(ChatMessage.lobby_id == lobby_id).order_by(
        ChatMessage.created_at.desc()
    ).limit(50).all()

    messages = []
    for msg, user in reversed(messages_query):
        messages.append({
            "id": msg.id,
            "username": user.username if user else "System",
            "avatar": user.avatar_url if user else None,
            "message": msg.message,
            "type": msg.message_type,
            "created_at": msg.created_at.isoformat()
        })

    return jsonify({
        "id": lobby.id,
        "name": lobby.name,
        "host_id": lobby.host_id,
        "status": lobby.status,
        "visibility": lobby.visibility,
        "created_at": lobby.created_at.isoformat(),
        "started_at": lobby.started_at.isoformat() if lobby.started_at else None,
        "server_port": lobby.server_port,
        "seed_url": lobby.seed_url,
        "settings": {
            "max_players": settings.max_players if settings else 10,
            "time_limit_hours": settings.time_limit_hours if settings else None,
            "sync_rules": settings.sync_rules if settings else '',
            "allow_multigame": settings.allow_multigame if settings else True,
            "allow_broadcast": settings.allow_broadcast if settings else True
        },
        "players": players,
        "messages": messages
    })

@app.route('/api/lobbies/<int:lobby_id>/join', methods=['POST'])
def join_lobby(lobby_id):
    """Join a lobby"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "lobby not found"}), 404

    if lobby.visibility == 'closed':
        return jsonify({"error": "lobby is closed"}), 403

    # Check if already joined
    existing = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=uid).first()
    if existing:
        return jsonify({"message": "already in lobby"}), 200

    # Check player limit
    settings = LobbySettings.query.filter_by(lobby_id=lobby_id).first()
    max_players = settings.max_players if settings else 10
    current_players = LobbyPlayer.query.filter_by(lobby_id=lobby_id).count()

    if current_players >= max_players:
        return jsonify({"error": "lobby is full"}), 403

    # Add player
    new_player = LobbyPlayer(
        lobby_id=lobby_id,
        user_id=uid,
        game='',
        yaml_file_id=None
    )
    db.session.add(new_player)

    # Add system message
    user = User.query.get(uid)
    sys_msg = ChatMessage(
        lobby_id=lobby_id,
        user_id=None,
        message=f"{user.username} joined the lobby",
        message_type='system'
    )
    db.session.add(sys_msg)

    db.session.commit()

    logger.info(f"User {uid} joined lobby {lobby_id}")

    return jsonify({"message": "joined successfully"}), 200

@app.route('/api/lobbies/<int:lobby_id>/leave', methods=['POST'])
def leave_lobby(lobby_id):
    """Leave a lobby"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "lobby not found"}), 404

    player = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=uid).first()
    if not player:
        return jsonify({"error": "not in lobby"}), 404

    user = User.query.get(uid)

    # If host is leaving, transfer host
    if lobby.host_id == uid:
        # Find next player
        next_player = LobbyPlayer.query.filter(
            LobbyPlayer.lobby_id == lobby_id,
            LobbyPlayer.user_id != uid
        ).first()

        if next_player:
            lobby.host_id = next_player.user_id
            logger.info(f"Host transferred from {uid} to {next_player.user_id}")

            # Add system message
            new_host = User.query.get(next_player.user_id)
            sys_msg = ChatMessage(
                lobby_id=lobby_id,
                user_id=None,
                message=f"{new_host.username} is now the host",
                message_type='system'
            )
            db.session.add(sys_msg)

    # Remove player
    db.session.delete(player)

    # Add system message
    sys_msg = ChatMessage(
        lobby_id=lobby_id,
        user_id=None,
        message=f"{user.username} left the lobby",
        message_type='system'
    )
    db.session.add(sys_msg)

    db.session.commit()

    logger.info(f"User {uid} left lobby {lobby_id}")

    return jsonify({"message": "left successfully"}), 200

@app.route('/api/lobbies/<int:lobby_id>/ready', methods=['POST'])
def toggle_ready(lobby_id):
    """Toggle ready status"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    player = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=uid).first()
    if not player:
        return jsonify({"error": "not in lobby"}), 404

    data = request.json
    yaml_id = data.get('yaml_id')

    if not yaml_id:
        return jsonify({"error": "yaml_id required"}), 400

    # Validate YAML belongs to user
    yaml_file = YamlFile.query.filter_by(id=yaml_id, user_id=uid).first()
    if not yaml_file:
        return jsonify({"error": "yaml not found"}), 404

    # Update player
    player.yaml_file_id = yaml_id
    player.game = yaml_file.game
    player.is_ready = not player.is_ready

    db.session.commit()

    logger.info(f"User {uid} ready status: {player.is_ready} in lobby {lobby_id}")

    return jsonify({
        "is_ready": player.is_ready,
        "game": player.game
    }), 200

@app.route('/api/lobbies/<int:lobby_id>/kick', methods=['POST'])
def kick_player(lobby_id):
    """Kick a player (host only)"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "lobby not found"}), 404

    if lobby.host_id != uid:
        return jsonify({"error": "only host can kick"}), 403

    data = request.json
    target_user_id = data.get('user_id')

    if not target_user_id or target_user_id == uid:
        return jsonify({"error": "invalid user"}), 400

    player = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=target_user_id).first()
    if not player:
        return jsonify({"error": "player not in lobby"}), 404

    kicked_user = User.query.get(target_user_id)
    db.session.delete(player)

    # Add system message
    sys_msg = ChatMessage(
        lobby_id=lobby_id,
        user_id=None,
        message=f"{kicked_user.username} was kicked from the lobby",
        message_type='system'
    )
    db.session.add(sys_msg)

    db.session.commit()

    logger.info(f"Host {uid} kicked user {target_user_id} from lobby {lobby_id}")

    return jsonify({"message": "player kicked"}), 200

@app.route('/api/generate', methods=['POST'])
def generate_seed():
    """Start seed generation for a lobby"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    data = request.json
    lobby_id = data.get('lobby_id')

    if not lobby_id:
        return jsonify({"error": "lobby_id required"}), 400

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "lobby not found"}), 404

    # Only host can start generation
    if lobby.host_id != uid:
        return jsonify({"error": "only host can start generation"}), 403

    # Check all players are ready
    players = LobbyPlayer.query.filter_by(lobby_id=lobby_id).all()
    if not all(p.is_ready for p in players):
        return jsonify({"error": "not all players are ready"}), 400

    # Create temporary generation directory
    temp_dir = f"/tmp/generation/{lobby_id}"
    os.makedirs(temp_dir, exist_ok=True)

    # Copy all player YAMLs
    for player in players:
        if player.yaml_file_id:
            yaml_file = YamlFile.query.get(player.yaml_file_id)
            if yaml_file:
                # Use player's username as filename to avoid conflicts
                user = User.query.get(player.user_id)
                filename = f"{user.username}_{yaml_file.filename}"
                filepath = os.path.join(temp_dir, filename)

                with open(filepath, "w") as f:
                    f.write(yaml_file.content)

                logger.info(f"Copied YAML for {user.username}: {filename}")

    # Update lobby status
    lobby.status = 'generating'
    db.session.commit()

    # Start generation task
    run_generator.delay(lobby_id, temp_dir, lobby.name)

    logger.info(f"Started generation for lobby {lobby_id}")

    return jsonify({
        "status": "started",
        "lobby_id": lobby_id,
        "message": "Generation started"
    })

@app.route('/api/lobbies/<int:lobby_id>/patches/<path:filename>', methods=['GET'])
def download_patch(lobby_id, filename):
    """Download a patch file for a lobby"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    # Verify user is in the lobby
    player = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=uid).first()
    if not player:
        # Also allow if user was in lobby (for cleanup grace period)
        lobby = Lobby.query.get(lobby_id)
        if not lobby:
            return jsonify({"error": "lobby not found"}), 404

    # Patch directory
    patches_dir = f"/tmp/generation/{lobby_id}/patches"
    filepath = os.path.join(patches_dir, secure_filename(filename))

    if not os.path.exists(filepath):
        logger.error(f"Patch file not found: {filepath}")
        return jsonify({"error": "patch file not found"}), 404

    logger.info(f"User {uid} downloading patch: {filename} from lobby {lobby_id}")

    return send_file(
        filepath,
        as_attachment=True,
        download_name=filename,
        mimetype='application/octet-stream'
    )

# ==========================================
# WEBSOCKET EVENTS FOR REAL-TIME LOBBY UPDATES
# ==========================================

# --- CONNECTION EVENTS ---
@socketio.on('connect')
def handle_connect():
    """Handle client connection"""
    uid = session.get('user_id')
    if not uid:
        logger.warning("Unauthorized WebSocket connection attempt")
        return False  # Reject connection

    user = User.query.get(uid)
    if user:
        logger.info(f"✅ User {user.username} connected via WebSocket")
        emit('connected', {'status': 'success', 'message': 'Connected to SekaiLink'})
    else:
        return False

@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection"""
    uid = session.get('user_id')
    if uid:
        user = User.query.get(uid)
        if user:
            logger.info(f"User {user.username} disconnected from WebSocket")
            # Notify lobbies the user was in
            user_rooms = [room for room in rooms() if room != request.sid]
            for room in user_rooms:
                emit('user_left', {
                    'user_id': uid,
                    'username': user.username,
                    'timestamp': datetime.utcnow().isoformat()
                }, room=room)

# --- LOBBY EVENTS ---
@socketio.on('join_lobby')
def handle_join_lobby(data):
    """Join a lobby room for real-time updates"""
    uid = session.get('user_id')
    if not uid:
        emit('error', {'message': 'Not authenticated'})
        return

    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('error', {'message': 'Missing lobby_id'})
        return

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        emit('error', {'message': 'Lobby not found'})
        return

    user = User.query.get(uid)
    room = f"lobby_{lobby_id}"
    join_room(room)

    logger.info(f"User {user.username} joined lobby {lobby.name} (room: {room})")

    # Notify others in the lobby
    emit('user_joined', {
        'user_id': uid,
        'username': user.username,
        'avatar': user.avatar_url,
        'pronouns': user.pronouns,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room, include_self=False)

    # Send current lobby state to the joining user
    emit('lobby_state', {
        'lobby_id': lobby_id,
        'name': lobby.name,
        'status': lobby.status,
        'host_id': lobby.host_id,
        'created_at': lobby.created_at.isoformat(),
        'seed_url': lobby.seed_url
    })

@socketio.on('leave_lobby')
def handle_leave_lobby(data):
    """Leave a lobby room"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    if not lobby_id:
        return

    user = User.query.get(uid)
    room = f"lobby_{lobby_id}"
    leave_room(room)

    logger.info(f"User {user.username} left lobby {lobby_id}")

    # Notify others
    emit('user_left', {
        'user_id': uid,
        'username': user.username,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

# --- PLAYER STATUS EVENTS ---
@socketio.on('player_ready')
def handle_player_ready(data):
    """Mark player as ready"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    ready = data.get('ready', True)

    user = User.query.get(uid)
    room = f"lobby_{lobby_id}"

    logger.info(f"User {user.username} marked as {'ready' if ready else 'not ready'} in lobby {lobby_id}")

    # Broadcast to lobby
    emit('player_status_changed', {
        'user_id': uid,
        'username': user.username,
        'ready': ready,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

@socketio.on('player_finished')
def handle_player_finished(data):
    """Mark player as finished"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    status = data.get('status', 'finished')  # 'finished' or 'dnf'

    user = User.query.get(uid)
    room = f"lobby_{lobby_id}"

    logger.info(f"User {user.username} marked as {status} in lobby {lobby_id}")

    # Broadcast to lobby
    emit('player_finished', {
        'user_id': uid,
        'username': user.username,
        'status': status,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

# --- LOBBY CONTROL EVENTS (Host only) ---
@socketio.on('start_sync')
def handle_start_sync(data):
    """Start the sync (host only)"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    lobby = Lobby.query.get(lobby_id)

    if not lobby or lobby.host_id != uid:
        emit('error', {'message': 'Only the host can start the sync'})
        return

    lobby.status = 'active'
    db.session.commit()

    room = f"lobby_{lobby_id}"
    user = User.query.get(uid)

    logger.info(f"Host {user.username} started sync for lobby {lobby_id}")

    # Broadcast to everyone in the lobby
    emit('sync_started', {
        'lobby_id': lobby_id,
        'started_by': user.username,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

@socketio.on('stop_sync')
def handle_stop_sync(data):
    """Stop the sync (host only)"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    lobby = Lobby.query.get(lobby_id)

    if not lobby or lobby.host_id != uid:
        emit('error', {'message': 'Only the host can stop the sync'})
        return

    lobby.status = 'completed'
    db.session.commit()

    room = f"lobby_{lobby_id}"
    user = User.query.get(uid)

    logger.info(f"Host {user.username} stopped sync for lobby {lobby_id}")

    # Broadcast to everyone
    emit('sync_stopped', {
        'lobby_id': lobby_id,
        'stopped_by': user.username,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

# --- CHAT EVENTS ---
@socketio.on('chat_message')
def handle_chat_message(data):
    """Handle chat messages in lobby"""
    uid = session.get('user_id')
    if not uid:
        return

    lobby_id = data.get('lobby_id')
    message = data.get('message', '').strip()

    if not message or len(message) > 500:
        emit('error', {'message': 'Invalid message'})
        return

    user = User.query.get(uid)
    room = f"lobby_{lobby_id}"

    # Save to database
    chat_msg = ChatMessage(
        lobby_id=lobby_id,
        user_id=uid,
        message=message,
        message_type='user'
    )
    db.session.add(chat_msg)
    db.session.commit()

    logger.info(f"Chat message in lobby {lobby_id} from {user.username}: {message}")

    # Broadcast to lobby
    emit('chat_message', {
        'user_id': uid,
        'username': user.username,
        'avatar': user.avatar_url,
        'message': message,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

# --- GENERATION PROGRESS EVENTS ---
@socketio.on('generation_progress')
def handle_generation_progress(data):
    """Update generation progress (called by Celery task)"""
    lobby_id = data.get('lobby_id')
    progress = data.get('progress', 0)
    message = data.get('message', '')

    room = f"lobby_{lobby_id}"

    logger.info(f"Generation progress for lobby {lobby_id}: {progress}% - {message}")

    # Broadcast progress to lobby
    emit('generation_update', {
        'lobby_id': lobby_id,
        'progress': progress,
        'message': message,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

@socketio.on('generation_complete')
def handle_generation_complete(data):
    """Notify when generation is complete"""
    lobby_id = data.get('lobby_id')
    success = data.get('success', False)
    seed_url = data.get('seed_url', '')
    error = data.get('error')

    lobby = Lobby.query.get(lobby_id)
    if lobby:
        if success:
            lobby.status = 'ready'
            lobby.seed_url = seed_url
        else:
            lobby.status = 'failed'
        db.session.commit()

    room = f"lobby_{lobby_id}"

    logger.info(f"Generation {'completed' if success else 'failed'} for lobby {lobby_id}")

    # Broadcast result to lobby
    emit('generation_complete', {
        'lobby_id': lobby_id,
        'success': success,
        'seed_url': seed_url,
        'error': error,
        'timestamp': datetime.utcnow().isoformat()
    }, room=room)

if __name__ == '__main__':
    # Use socketio.run() instead of app.run() for WebSocket support
    socketio.run(app, host='0.0.0.0', port=7000, debug=os.getenv('FLASK_DEBUG', 'False') == 'True')