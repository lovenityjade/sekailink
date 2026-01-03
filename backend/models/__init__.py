"""
SekaiLink Database Models

This module contains all database models for the SekaiLink platform.
Adopts patterns from racetime.gg while maintaining Flask-SQLAlchemy.
"""

from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
import json

db = SQLAlchemy()

# ============================================================================
# EXISTING MODELS (Updated with new fields)
# ============================================================================

class User(db.Model):
    """User account model with Discord OAuth"""
    __tablename__ = 'users'

    # Core fields
    id = db.Column(db.String, primary_key=True)
    discord_id = db.Column(db.String, unique=True, nullable=False)
    username = db.Column(db.String, nullable=False)
    avatar_url = db.Column(db.String)

    # Profile fields
    email = db.Column(db.String)
    email_verified = db.Column(db.Boolean, default=False)  # NEW
    pronouns = db.Column(db.String(50))
    bio = db.Column(db.Text)
    language = db.Column(db.String(5), default='en')

    # Permissions
    role = db.Column(db.String(20), default='user')  # user/moderator/admin

    # Status fields (NEW)
    is_banned = db.Column(db.Boolean, default=False)
    is_suspended = db.Column(db.Boolean, default=False)
    last_seen = db.Column(db.DateTime, default=datetime.utcnow)

    # Badges
    badge_supporter = db.Column(db.Boolean, default=False)
    badge_host_master = db.Column(db.Boolean, default=False)
    badge_sync_gamer = db.Column(db.Boolean, default=False)

    # Timestamps
    created_at = db.Column(db.DateTime, default=datetime.utcnow)


class YamlFile(db.Model):
    """YAML configuration files for games"""
    __tablename__ = 'yaml_files'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    filename = db.Column(db.String(100))
    content = db.Column(db.Text)
    game = db.Column(db.String(50))
    updated_at = db.Column(db.DateTime, default=datetime.utcnow)


class RomFile(db.Model):
    """ROM file uploads (temporary storage)"""
    __tablename__ = 'rom_files'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    filename = db.Column(db.String(255))
    file_path = db.Column(db.String(500))  # Temporary storage path
    sha1 = db.Column(db.String(40))
    game_detected = db.Column(db.String(100))
    status = db.Column(db.String(20), default='unverified')
    uploaded_at = db.Column(db.DateTime, default=datetime.utcnow)  # For cleanup tracking


class Lobby(db.Model):
    """Multiworld lobby/session"""
    __tablename__ = 'lobbies'

    id = db.Column(db.Integer, primary_key=True)
    host_id = db.Column(db.String, db.ForeignKey('users.id'))
    name = db.Column(db.String(100))
    slug = db.Column(db.String(100), unique=True, nullable=True)  # NEW: readable URLs (word-word-1234)

    # State (racetime.gg pattern)
    status = db.Column(db.String(20), default='open')  # open/pending/generating/ready/active/finished/cancelled/failed
    visibility = db.Column(db.String(20), default='open')  # open/private/closed

    # Generation
    seed_url = db.Column(db.String(255))
    server_port = db.Column(db.Integer, nullable=True)

    # Timer fields (NEW)
    time_limit_hours = db.Column(db.Integer, nullable=True)
    restrict_time_limit = db.Column(db.Boolean, default=False)
    timer_started_at = db.Column(db.DateTime, nullable=True)

    # Timestamps
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    started_at = db.Column(db.DateTime, nullable=True)
    ended_at = db.Column(db.DateTime, nullable=True)


class LobbySettings(db.Model):
    """Lobby configuration settings"""
    __tablename__ = 'lobby_settings'

    id = db.Column(db.Integer, primary_key=True)
    lobby_id = db.Column(db.Integer, db.ForeignKey('lobbies.id'), unique=True)

    # Configuration
    max_players = db.Column(db.Integer, default=10)
    time_limit_hours = db.Column(db.Integer, nullable=True)
    sync_rules = db.Column(db.Text, default='')

    # Options
    allow_multigame = db.Column(db.Boolean, default=True)
    allow_broadcast = db.Column(db.Boolean, default=True)
    disallow_rom_games = db.Column(db.Boolean, default=False)  # NEW
    disallow_custom_worlds = db.Column(db.Boolean, default=False)  # NEW
    voice_chat_enabled = db.Column(db.Boolean, default=True)  # NEW
    blacklisted_games = db.Column(db.Text, default='[]')  # JSON array


class LobbyPlayer(db.Model):
    """Player participation in a lobby"""
    __tablename__ = 'lobby_players'

    id = db.Column(db.Integer, primary_key=True)
    lobby_id = db.Column(db.Integer, db.ForeignKey('lobbies.id'))
    user_id = db.Column(db.String, db.ForeignKey('users.id'))

    # Player game choice
    game = db.Column(db.String(100))
    yaml_file_id = db.Column(db.Integer, db.ForeignKey('yaml_files.id'))
    rom_file_id = db.Column(db.Integer, db.ForeignKey('rom_files.id'), nullable=True)

    # Status (racetime.gg pattern)
    is_ready = db.Column(db.Boolean, default=False)
    status = db.Column(db.String(20), default='waiting')  # waiting/ready/active/finished/dnf/forfeit
    patch_url = db.Column(db.String(500), nullable=True)

    # Timestamps
    joined_at = db.Column(db.DateTime, default=datetime.utcnow)
    finished_at = db.Column(db.DateTime, nullable=True)


class ChatMessage(db.Model):
    """Chat messages in lobbies"""
    __tablename__ = 'chat_messages'

    id = db.Column(db.Integer, primary_key=True)
    lobby_id = db.Column(db.Integer, db.ForeignKey('lobbies.id'))
    user_id = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)  # null for system messages

    message = db.Column(db.String(500))
    message_type = db.Column(db.String(20), default='user')  # user/system/bot

    # Moderation fields (NEW - racetime.gg pattern)
    is_pinned = db.Column(db.Boolean, default=False)
    pinned_by = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)
    deleted = db.Column(db.Boolean, default=False)
    deleted_by = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)
    deleted_at = db.Column(db.DateTime, nullable=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)


# ============================================================================
# NEW MODELS
# ============================================================================

class Game(db.Model):
    """Archipelago game catalog"""
    __tablename__ = 'games'

    id = db.Column(db.Integer, primary_key=True)
    slug = db.Column(db.String(100), unique=True, nullable=False)  # URL-friendly identifier
    name = db.Column(db.String(200), nullable=False)
    description = db.Column(db.Text)
    boxart_url = db.Column(db.String(500))

    # Game properties
    requires_rom = db.Column(db.Boolean, default=False)
    world_type = db.Column(db.String(20), default='official')  # official/custom
    sync_count = db.Column(db.Integer, default=0)  # For sorting by popularity
    is_active = db.Column(db.Boolean, default=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)


class FavoriteGame(db.Model):
    """User favorite games"""
    __tablename__ = 'favorite_games'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    game_slug = db.Column(db.String(100), db.ForeignKey('games.slug'))
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # Unique constraint
    __table_args__ = (db.UniqueConstraint('user_id', 'game_slug', name='_user_game_uc'),)


class Friend(db.Model):
    """Friend relationships and blacklist"""
    __tablename__ = 'friends'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    friend_id = db.Column(db.String, db.ForeignKey('users.id'))
    status = db.Column(db.String(20), default='pending')  # pending/accepted/blocked
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # Unique constraint
    __table_args__ = (db.UniqueConstraint('user_id', 'friend_id', name='_user_friend_uc'),)


class Ban(db.Model):
    """User bans with appeal system"""
    __tablename__ = 'bans'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    banned_by = db.Column(db.String, db.ForeignKey('users.id'))

    reason = db.Column(db.Text, nullable=False)
    duration_hours = db.Column(db.Integer, nullable=True)  # NULL = permanent

    # Appeal system
    appeal_text = db.Column(db.Text, nullable=True)
    appeal_status = db.Column(db.String(20), nullable=True)  # pending/approved/rejected
    appeal_reviewed_by = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)
    appeal_reviewed_at = db.Column(db.DateTime, nullable=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    expires_at = db.Column(db.DateTime, nullable=True)
    is_active = db.Column(db.Boolean, default=True)


class Warning(db.Model):
    """User warnings"""
    __tablename__ = 'warnings'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'))
    warned_by = db.Column(db.String, db.ForeignKey('users.id'))
    reason = db.Column(db.Text, nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)


class UserRating(db.Model):
    """User-to-user ratings"""
    __tablename__ = 'user_ratings'

    id = db.Column(db.Integer, primary_key=True)
    from_user_id = db.Column(db.String, db.ForeignKey('users.id'))
    to_user_id = db.Column(db.String, db.ForeignKey('users.id'))

    # Rating criteria (1-5 stars each)
    punctuality = db.Column(db.Integer)  # 1-5
    respect_others = db.Column(db.Integer)  # 1-5
    respect_rules = db.Column(db.Integer)  # 1-5
    valid_release = db.Column(db.Integer)  # 1-5
    overall_rating = db.Column(db.Float)  # Average of above

    # Optional review text
    review_text = db.Column(db.Text, nullable=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # Unique constraint
    __table_args__ = (db.UniqueConstraint('from_user_id', 'to_user_id', name='_user_rating_uc'),)


class UserReview(db.Model):
    """User reviews with moderation"""
    __tablename__ = 'user_reviews'

    id = db.Column(db.Integer, primary_key=True)
    from_user_id = db.Column(db.String, db.ForeignKey('users.id'))
    to_user_id = db.Column(db.String, db.ForeignKey('users.id'))

    review_text = db.Column(db.Text, nullable=False)

    # Community feedback
    likes = db.Column(db.Integer, default=0)
    dislikes = db.Column(db.Integer, default=0)
    reports = db.Column(db.Integer, default=0)

    # Moderation
    status = db.Column(db.String(20), default='pending')  # pending/approved/rejected
    moderated_by = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)
    moderated_at = db.Column(db.DateTime, nullable=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)


class ServerRating(db.Model):
    """Server-calculated user ratings based on behavior"""
    __tablename__ = 'server_ratings'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'), unique=True)

    # Behavior metrics
    kicks_count = db.Column(db.Integer, default=0)
    bans_count = db.Column(db.Integer, default=0)
    suspensions_count = db.Column(db.Integer, default=0)
    syncs_finished = db.Column(db.Integer, default=0)
    syncs_ragequit = db.Column(db.Integer, default=0)

    # Calculated rating (1-5 stars)
    rating = db.Column(db.Float, default=5.0)

    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class TwitchConnection(db.Model):
    """Twitch OAuth connections"""
    __tablename__ = 'twitch_connections'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.String, db.ForeignKey('users.id'), unique=True)

    twitch_id = db.Column(db.String, nullable=False)
    twitch_username = db.Column(db.String, nullable=False)

    # OAuth tokens
    access_token = db.Column(db.String(500), nullable=False)
    refresh_token = db.Column(db.String(500), nullable=False)
    token_expires_at = db.Column(db.DateTime, nullable=False)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class CustomWorld(db.Model):
    """Custom Archipelago worlds (.apworld files)"""
    __tablename__ = 'custom_worlds'

    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(200), nullable=False)
    slug = db.Column(db.String(100), unique=True, nullable=False)
    version = db.Column(db.String(50))
    file_path = db.Column(db.String(500), nullable=False)

    # Uploader
    uploader_id = db.Column(db.String, db.ForeignKey('users.id'))

    # Moderation
    status = db.Column(db.String(20), default='pending')  # pending/approved/rejected
    reviewed_by = db.Column(db.String, db.ForeignKey('users.id'), nullable=True)
    reviewed_at = db.Column(db.DateTime, nullable=True)
    rejection_reason = db.Column(db.Text, nullable=True)

    created_at = db.Column(db.DateTime, default=datetime.utcnow)


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def init_db(app):
    """Initialize database with app context"""
    db.init_app(app)
    with app.app_context():
        db.create_all()


def calculate_server_rating(user_id):
    """Calculate server rating for a user"""
    rating = ServerRating.query.filter_by(user_id=user_id).first()
    if not rating:
        rating = ServerRating(user_id=user_id)
        db.session.add(rating)

    # Formula from plan
    base_score = 5.0
    penalty = (rating.kicks_count * 0.1) + (rating.bans_count * 0.5) + \
              (rating.suspensions_count * 0.3) + (rating.syncs_ragequit * 0.2)
    bonus = rating.syncs_finished * 0.01

    rating.rating = max(1.0, min(5.0, base_score - penalty + bonus))
    rating.updated_at = datetime.utcnow()
    db.session.commit()

    return rating.rating


def generate_lobby_slug(name):
    """Generate a readable lobby slug (word-word-1234 pattern)"""
    import random
    import re

    # Adjectives and nouns for readable URLs (racetime.gg pattern)
    adjectives = ['awesome', 'epic', 'super', 'mega', 'wild', 'crazy', 'happy', 'lucky', 'brave', 'swift']
    nouns = ['racer', 'runner', 'gamer', 'hero', 'champion', 'player', 'warrior', 'master', 'legend', 'pro']

    # Generate slug
    adj = random.choice(adjectives)
    noun = random.choice(nouns)
    number = random.randint(1000, 9999)

    slug = f"{adj}-{noun}-{number}"

    # Ensure uniqueness
    while Lobby.query.filter_by(slug=slug).first():
        number = random.randint(1000, 9999)
        slug = f"{adj}-{noun}-{number}"

    return slug
