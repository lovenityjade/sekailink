import os
import sys
import uuid
import logging
import json
import yaml
import hashlib
from datetime import datetime
from urllib.parse import quote
from flask import Flask, session, request, jsonify, redirect, send_file, render_template
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

# Configure Flask to serve frontend templates and static files
# In Docker: /frontend is mounted from host ./frontend directory
# In development: use relative path
if os.path.exists('/frontend'):
    # Docker environment
    frontend_dir = '/frontend'
else:
    # Local development
    frontend_dir = os.path.join(os.path.dirname(__file__), '..', 'frontend')

app = Flask(__name__,
            template_folder=os.path.join(frontend_dir, 'src'),
            static_folder=os.path.join(frontend_dir, 'static'),
            static_url_path='/static')
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

    # Error handling for OAuth
    if not code:
        logger.error("OAuth callback: No code provided")
        return "OAuth Error: No authorization code provided", 400

    # Exchange code for token
    data = {
        'client_id': DISCORD_CLIENT_ID,
        'client_secret': DISCORD_CLIENT_SECRET,
        'grant_type': 'authorization_code',
        'code': code,
        'redirect_uri': DISCORD_REDIRECT_URI
    }

    try:
        token_response = requests.post('https://discord.com/api/oauth2/token', data=data)
        token_response.raise_for_status()
        r = token_response.json()

        if 'access_token' not in r:
            logger.error(f"OAuth token exchange failed: {r}")
            return f"OAuth Error: Failed to get access token - {r.get('error', 'Unknown error')}", 400

        token = r['access_token']

        # Get user info from Discord
        user_response = requests.get('https://discord.com/api/users/@me', headers={'Authorization': f'Bearer {token}'})
        user_response.raise_for_status()
        u = user_response.json()

        if 'id' not in u:
            logger.error(f"Discord user API returned invalid data: {u}")
            return "OAuth Error: Failed to get user information from Discord", 400

        # Create or update user
        user = User.query.filter_by(discord_id=u['id']).first()
        if not user:
            user = User(
                id=str(uuid.uuid4()),
                discord_id=u['id'],
                username=u['username'],
                avatar_url=f"https://cdn.discordapp.com/avatars/{u['id']}/{u.get('avatar', 'default')}.png",
                email=u.get('email')
            )
            db.session.add(user)
            logger.info(f"Created new user: {user.username} ({user.discord_id})")
        else:
            user.username = u['username']
            user.avatar_url = f"https://cdn.discordapp.com/avatars/{u['id']}/{u.get('avatar', 'default')}.png"
            logger.info(f"Updated existing user: {user.username} ({user.discord_id})")

        db.session.commit()

        # Store user object in session (not just ID)
        session['user_id'] = user.id
        session['user'] = {
            'id': user.id,
            'username': user.username,
            'avatar': user.avatar_url,
            'email': user.email,
            'discord_id': user.discord_id,
            'role': user.role,
            'bio': user.bio,
            'pronouns': user.pronouns
        }

        logger.info(f"User {user.username} logged in successfully")
        return redirect("/dashboard.html")

    except requests.exceptions.RequestException as e:
        logger.error(f"OAuth request failed: {str(e)}")
        return f"OAuth Error: Failed to communicate with Discord - {str(e)}", 500
    except Exception as e:
        logger.error(f"OAuth callback error: {str(e)}")
        return f"OAuth Error: {str(e)}", 500

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

@app.route('/api/me/favorites', methods=['GET'])
def get_user_favorites():
    """Get user's favorite games"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    favorites = FavoriteGame.query.filter_by(user_id=uid).all()
    games = []
    for fav in favorites:
        game = Game.query.get(fav.game_id)
        if game:
            games.append({
                'id': game.id,
                'name': game.name,
                'slug': game.slug,
                'boxart_url': game.boxart_url or '/static/boxarts/default.png',
                'requires_rom': game.requires_rom,
                'sync_count': game.sync_count,
                'favorited_at': fav.created_at.isoformat()
            })

    return jsonify(games)

@app.route('/api/friends', methods=['GET', 'POST'])
def manage_friends():
    """Get friends list or add a friend"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    if request.method == 'GET':
        # Get friends and blacklist
        friends = Friend.query.filter_by(user_id=uid).all()
        result = {'friends': [], 'blacklist': []}

        for friend in friends:
            friend_user = User.query.get(friend.friend_id)
            if friend_user:
                friend_data = {
                    'id': friend_user.id,
                    'username': friend_user.username,
                    'avatar': friend_user.avatar_url,
                    'is_online': (datetime.utcnow() - friend_user.last_seen).seconds < 300 if friend_user.last_seen else False,
                    'added_at': friend.created_at.isoformat()
                }
                if friend.is_blacklisted:
                    result['blacklist'].append(friend_data)
                else:
                    result['friends'].append(friend_data)

        return jsonify(result)

    # POST - Add friend or blacklist
    data = request.json
    friend_username = data.get('username')
    is_blacklist = data.get('blacklist', False)

    if not friend_username:
        return jsonify({"error": "Username required"}), 400

    friend_user = User.query.filter_by(username=friend_username).first()
    if not friend_user:
        return jsonify({"error": "User not found"}), 404

    if friend_user.id == uid:
        return jsonify({"error": "Cannot add yourself"}), 400

    # Check if already exists
    existing = Friend.query.filter_by(user_id=uid, friend_id=friend_user.id).first()
    if existing:
        # Update blacklist status
        existing.is_blacklisted = is_blacklist
        db.session.commit()
        return jsonify({"status": "updated", "blacklisted": is_blacklist})

    # Create new friend entry
    new_friend = Friend(user_id=uid, friend_id=friend_user.id, is_blacklisted=is_blacklist)
    db.session.add(new_friend)
    db.session.commit()

    return jsonify({"status": "added", "blacklisted": is_blacklist})

@app.route('/api/friends/<friend_id>', methods=['DELETE'])
def remove_friend(friend_id):
    """Remove a friend or unblock from blacklist"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    friend = Friend.query.filter_by(user_id=uid, friend_id=friend_id).first()
    if not friend:
        return jsonify({"error": "Friend not found"}), 404

    db.session.delete(friend)
    db.session.commit()

    return jsonify({"status": "removed"})

# --- GAMES API ---
@app.route('/api/games', methods=['GET'])
def list_games():
    """
    Get all games with optional filtering.

    Query params:
    - search: Search by name (case-insensitive)
    - rom: 'required' or 'not_required'
    - world_type: 'official' or 'custom'
    - is_active: 'true' or 'false'
    - sort: 'name', 'sync_count', 'created_at' (default: name)
    - order: 'asc' or 'desc' (default: asc for name, desc for sync_count)
    """
    query = Game.query

    # Search filter
    search = request.args.get('search', '').strip()
    if search:
        query = query.filter(Game.name.ilike(f'%{search}%'))

    # ROM requirement filter
    rom_filter = request.args.get('rom')
    if rom_filter == 'required':
        query = query.filter(Game.requires_rom == True)
    elif rom_filter == 'not_required':
        query = query.filter(Game.requires_rom == False)

    # World type filter
    world_type = request.args.get('world_type')
    if world_type in ['official', 'custom']:
        query = query.filter(Game.world_type == world_type)

    # Active status filter
    is_active = request.args.get('is_active')
    if is_active == 'true':
        query = query.filter(Game.is_active == True)
    elif is_active == 'false':
        query = query.filter(Game.is_active == False)

    # Sorting
    sort_by = request.args.get('sort', 'name')
    order = request.args.get('order', 'asc' if sort_by == 'name' else 'desc')

    if sort_by == 'sync_count':
        query = query.order_by(Game.sync_count.desc() if order == 'desc' else Game.sync_count.asc())
    elif sort_by == 'created_at':
        query = query.order_by(Game.created_at.desc() if order == 'desc' else Game.created_at.asc())
    else:  # Default to name
        query = query.order_by(Game.name.asc() if order == 'asc' else Game.name.desc())

    games = query.all()

    # Get user favorites if logged in
    user_favorites = set()
    uid = session.get('user_id')
    if uid:
        favorites = FavoriteGame.query.filter_by(user_id=uid).all()
        user_favorites = {f.game_id for f in favorites}

    return jsonify([{
        'id': g.id,
        'name': g.name,
        'slug': g.slug,
        'description': g.description,
        'boxart_url': g.boxart_url or '/static/boxarts/default.png',
        'requires_rom': g.requires_rom,
        'world_type': g.world_type,
        'sync_count': g.sync_count,
        'is_active': g.is_active,
        'is_favorited': g.id in user_favorites
    } for g in games])

@app.route('/api/games/<slug>', methods=['GET'])
def get_game(slug):
    """Get detailed information about a specific game."""
    game = Game.query.filter_by(slug=slug).first()
    if not game:
        return jsonify({'error': 'Game not found'}), 404

    # Get user favorite status if logged in
    is_favorited = False
    uid = session.get('user_id')
    if uid:
        favorite = FavoriteGame.query.filter_by(user_id=uid, game_id=game.id).first()
        is_favorited = favorite is not None

    # Get active lobbies for this game
    active_lobbies = []
    lobbies = Lobby.query.filter(
        Lobby.status.in_(['open', 'pending', 'ready', 'active'])
    ).all()

    # Filter lobbies that have players with this game
    for lobby in lobbies:
        # Check if any player in this lobby is playing this game
        players = LobbyPlayer.query.filter_by(lobby_id=lobby.id).all()
        for player in players:
            if player.game == game.name:
                active_lobbies.append({
                    'id': lobby.id,
                    'name': lobby.name,
                    'slug': lobby.slug,
                    'host_name': User.query.get(lobby.host_id).username if lobby.host_id else 'Unknown',
                    'player_count': len(players),
                    'max_players': lobby.max_players,
                    'status': lobby.status,
                    'created_at': lobby.created_at.isoformat()
                })
                break

    return jsonify({
        'id': game.id,
        'name': game.name,
        'slug': game.slug,
        'description': game.description,
        'boxart_url': game.boxart_url or '/static/boxarts/default.png',
        'requires_rom': game.requires_rom,
        'world_type': game.world_type,
        'sync_count': game.sync_count,
        'is_active': game.is_active,
        'is_favorited': is_favorited,
        'active_lobbies': active_lobbies,
        'created_at': game.created_at.isoformat()
    })

@app.route('/api/games/<slug>/favorite', methods=['POST'])
def toggle_favorite(slug):
    """Toggle favorite status for a game (requires authentication)."""
    uid = session.get('user_id')
    if not uid:
        return jsonify({'error': 'Not authenticated'}), 401

    game = Game.query.filter_by(slug=slug).first()
    if not game:
        return jsonify({'error': 'Game not found'}), 404

    # Check if already favorited
    favorite = FavoriteGame.query.filter_by(user_id=uid, game_id=game.id).first()

    if favorite:
        # Unfavorite
        db.session.delete(favorite)
        db.session.commit()
        return jsonify({'status': 'unfavorited', 'is_favorited': False})
    else:
        # Favorite
        new_favorite = FavoriteGame(user_id=uid, game_id=game.id)
        db.session.add(new_favorite)
        db.session.commit()
        return jsonify({'status': 'favorited', 'is_favorited': True})

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

@app.route('/api/yamls/<int:yaml_id>', methods=['PUT', 'DELETE'])
def manage_yaml(yaml_id):
    """Edit or delete a YAML file"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    yaml_file = YamlFile.query.filter_by(id=yaml_id, user_id=uid).first()
    if not yaml_file:
        return jsonify({"error": "YAML file not found"}), 404

    if request.method == 'DELETE':
        db.session.delete(yaml_file)
        db.session.commit()
        return jsonify({"status": "deleted"})

    # PUT - Update YAML
    data = request.json
    try:
        # Validate YAML content
        y_data = yaml.safe_load(data.get('content', yaml_file.content))
        game_name = next((k for k in y_data if k not in ['name', 'description', 'requires']), yaml_file.game)

        # Update fields
        if 'filename' in data:
            yaml_file.filename = data['filename']
        if 'content' in data:
            yaml_file.content = data['content']
            yaml_file.game = game_name

        db.session.commit()
        return jsonify({"status": "updated", "id": yaml_file.id})
    except Exception as e:
        return jsonify({"error": f"Invalid YAML: {str(e)}"}), 400

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

@app.route('/api/lobbies/<int:lobby_id>/chat', methods=['GET', 'POST'])
def lobby_chat(lobby_id):
    """Get or send chat messages in a lobby"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404

    # Check if user is in lobby
    player = LobbyPlayer.query.filter_by(lobby_id=lobby_id, user_id=uid).first()
    if not player:
        return jsonify({"error": "Not in this lobby"}), 403

    if request.method == 'GET':
        # Get chat messages
        limit = request.args.get('limit', 100, type=int)
        messages = ChatMessage.query.filter_by(lobby_id=lobby_id)\
            .filter(ChatMessage.deleted == False)\
            .order_by(ChatMessage.sent_at.desc())\
            .limit(limit)\
            .all()

        result = []
        for msg in reversed(messages):  # Reverse to get chronological order
            user_data = None
            if msg.user_id:
                user = User.query.get(msg.user_id)
                if user:
                    user_data = {
                        'username': user.username,
                        'avatar': user.avatar_url
                    }

            result.append({
                'id': msg.id,
                'user': user_data,
                'message': msg.message,
                'type': msg.message_type,
                'sent_at': msg.sent_at.isoformat(),
                'is_pinned': msg.is_pinned
            })

        return jsonify(result)

    # POST - Send message
    data = request.json
    message_text = data.get('message', '').strip()

    if not message_text or len(message_text) > 500:
        return jsonify({"error": "Invalid message length"}), 400

    new_message = ChatMessage(
        lobby_id=lobby_id,
        user_id=uid,
        message=message_text,
        message_type='user'
    )
    db.session.add(new_message)
    db.session.commit()

    return jsonify({"status": "sent", "id": new_message.id}), 201

@app.route('/api/lobbies/<int:lobby_id>/settings', methods=['GET', 'PUT'])
def lobby_settings(lobby_id):
    """Get or update lobby settings (host only for PUT)"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "Not authenticated"}), 401

    lobby = Lobby.query.get(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404

    settings = LobbySettings.query.filter_by(lobby_id=lobby_id).first()
    if not settings:
        return jsonify({"error": "Settings not found"}), 404

    if request.method == 'GET':
        return jsonify({
            'max_players': settings.max_players,
            'time_limit_hours': settings.time_limit_hours,
            'sync_rules': settings.sync_rules,
            'allow_multigame': settings.allow_multigame,
            'allow_broadcast': settings.allow_broadcast,
            'disallow_rom_games': settings.disallow_rom_games,
            'disallow_custom_worlds': settings.disallow_custom_worlds,
            'voice_chat_enabled': settings.voice_chat_enabled,
            'blacklisted_games': json.loads(settings.blacklisted_games) if settings.blacklisted_games else []
        })

    # PUT - Update settings (host only)
    if lobby.host_id != uid:
        return jsonify({"error": "Only host can update settings"}), 403

    # Can't update settings after generation started
    if lobby.status not in ['open', 'pending']:
        return jsonify({"error": "Cannot update settings after generation"}), 400

    data = request.json

    # Update allowed fields
    if 'max_players' in data:
        settings.max_players = max(2, min(data['max_players'], 100))
    if 'time_limit_hours' in data:
        settings.time_limit_hours = data['time_limit_hours']
    if 'sync_rules' in data:
        settings.sync_rules = data['sync_rules']
    if 'allow_multigame' in data:
        settings.allow_multigame = data['allow_multigame']
    if 'allow_broadcast' in data:
        settings.allow_broadcast = data['allow_broadcast']
    if 'disallow_rom_games' in data:
        settings.disallow_rom_games = data['disallow_rom_games']
    if 'disallow_custom_worlds' in data:
        settings.disallow_custom_worlds = data['disallow_custom_worlds']
    if 'voice_chat_enabled' in data:
        settings.voice_chat_enabled = data['voice_chat_enabled']
    if 'blacklisted_games' in data:
        settings.blacklisted_games = json.dumps(data['blacklisted_games'])

    db.session.commit()

    return jsonify({"status": "updated"})

@app.route('/api/users/<user_id>', methods=['GET'])
def get_user_profile(user_id):
    """Get public user profile"""
    user = User.query.get(user_id)
    if not user:
        return jsonify({"error": "User not found"}), 404

    # Get user stats
    total_syncs = LobbyPlayer.query.filter_by(user_id=user_id, status='finished').count()
    total_dnf = LobbyPlayer.query.filter_by(user_id=user_id, status='dnf').count()

    # Get server rating
    server_rating = ServerRating.query.filter_by(user_id=user_id).first()

    # Get user rating (average from other users)
    user_ratings = UserRating.query.filter_by(rated_user_id=user_id).all()
    avg_rating = 0
    if user_ratings:
        total = sum(r.punctuality + r.respect_others + r.respect_rules + r.fair_release for r in user_ratings)
        avg_rating = total / (len(user_ratings) * 4)  # Average out of 5

    return jsonify({
        'id': user.id,
        'username': user.username,
        'avatar': user.avatar_url,
        'bio': user.bio,
        'pronouns': user.pronouns,
        'created_at': user.created_at.isoformat(),
        'last_seen': user.last_seen.isoformat() if user.last_seen else None,
        'is_online': (datetime.utcnow() - user.last_seen).seconds < 300 if user.last_seen else False,
        'stats': {
            'total_syncs': total_syncs,
            'total_dnf': total_dnf,
            'completion_rate': round(total_syncs / (total_syncs + total_dnf) * 100, 1) if (total_syncs + total_dnf) > 0 else 0
        },
        'rating': {
            'user_rating': round(avg_rating, 2),
            'server_rating': server_rating.rating if server_rating else 5.0,
            'total_reviews': len(user_ratings)
        }
    })

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

# ============================================================================
# HTML TEMPLATE ROUTES
# ============================================================================

@app.route('/')
def index():
    """Landing page"""
    from models import Game, Lobby

    # Fetch top games by sync count
    games = Game.query.order_by(Game.sync_count.desc()).limit(12).all()

    # Fetch active lobbies (for now, just get all lobbies since we don't have state field yet)
    # TODO: Filter by state once Lobby model is updated with state field
    lobbies = Lobby.query.limit(10).all()

    return render_template('index.html', user=session.get('user'), games=games, lobbies=lobbies)

@app.route('/dashboard.html')
def dashboard():
    """User dashboard - requires authentication"""
    if 'user' not in session:
        return redirect('/api/auth/login')
    return render_template('dashboard.html', user=session.get('user'))

@app.route('/lobby.html')
def lobby_page():
    """Lobby detail page"""
    return render_template('lobby.html', user=session.get('user'))

@app.route('/game/<slug>')
def game_page(slug):
    """Individual game page"""
    game = Game.query.filter_by(slug=slug).first()
    if not game:
        return "Game not found", 404

    # Get user data if logged in
    user_data = None
    if 'user_id' in session:
        user = User.query.get(session['user_id'])
        if user:
            user_data = {'username': user.username, 'avatar': user.avatar_url}

    return render_template('game.html', user=user_data, game=game, slug=slug)

@app.route('/create_room.html')
def create_room_page():
    """Create lobby page - requires authentication"""
    if 'user' not in session:
        return redirect('/api/auth/login')
    return render_template('create_room.html', user=session.get('user'))

@app.route('/profile/<int:user_id>')
@app.route('/profile.html')
def profile_page(user_id=None):
    """User profile page"""
    # TODO: Fetch profile user from database
    placeholder_profile_user = {
        'id': user_id or 1,
        'username': 'SampleUser',
        'avatar': '/static/favicon.png',
        'bio': 'This profile will load from the database once APIs are implemented.',
        'pronouns': 'they/them',
        'created_at': '2026-01-01',
        'sync_count': 0,
        'server_rating': 5.0,
        'ratings': {
            'punctuality': 5.0,
            'respect_others': 5.0,
            'respect_rules': 5.0,
            'valid_release': 5.0
        }
    }
    return render_template('profile.html', user=session.get('user'), profile_user=placeholder_profile_user)

@app.route('/moderation.html')
def moderation_page():
    """Moderation dashboard - requires moderator role"""
    if 'user' not in session:
        return redirect('/api/auth/login')
    user = session.get('user')
    if user.get('role') not in ['moderator', 'admin']:
        return "Forbidden", 403
    return render_template('moderation.html', user=user)

@app.route('/admin.html')
def admin_page():
    """Admin dashboard - requires admin role"""
    if 'user' not in session:
        return redirect('/api/auth/login')
    user = session.get('user')
    if user.get('role') != 'admin':
        return "Forbidden", 403
    return render_template('admin.html', user=user)

@app.route('/help.html')
def help_page():
    """Help and documentation"""
    return render_template('help.html', user=session.get('user'))

@app.route('/faq.html')
def faq_page():
    """Frequently asked questions"""
    return render_template('faq.html', user=session.get('user'))

@app.route('/about.html')
def about_page():
    """About SekaiLink"""
    return render_template('about.html', user=session.get('user'))

@app.route('/rules.html')
def rules_page():
    """Rules and code of conduct"""
    return render_template('rules.html', user=session.get('user'))

@app.route('/docs.html')
def docs_page():
    """API documentation"""
    return render_template('docs.html', user=session.get('user'))

@app.route('/donate.html')
def donate_page():
    """Donation page"""
    return render_template('donate.html', user=session.get('user'))

@app.route('/credits.html')
def credits_page():
    """Credits and acknowledgments"""
    return render_template('credits.html', user=session.get('user'))

@app.route('/contact.html')
def contact_page():
    """Contact form"""
    return render_template('contact.html', user=session.get('user'))

@app.route('/favicon.ico')
def favicon():
    """Serve favicon from static folder"""
    return send_file(os.path.join(app.static_folder, 'favicon.png'), mimetype='image/png')

@app.route('/games')
def games_list():
    """Games list page"""
    # TODO: Fetch games from database
    return render_template('index.html', user=session.get('user'))

@app.route('/lobbies')
def lobbies_list():
    """Active lobbies list page"""
    # TODO: Fetch lobbies from database
    return render_template('index.html', user=session.get('user'))

@app.route('/settings')
def settings_page():
    """User settings page - requires authentication"""
    if 'user' not in session:
        return redirect('/api/auth/login')
    # TODO: Create settings.html template
    return render_template('dashboard.html', user=session.get('user'))

if __name__ == '__main__':
    # Use socketio.run() instead of app.run() for WebSocket support
    socketio.run(app, host='0.0.0.0', port=7000, debug=os.getenv('FLASK_DEBUG', 'False') == 'True')