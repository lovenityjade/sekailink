#!/usr/bin/env python3
"""
Database initialization script for SekaiLink
Drops all existing tables and recreates them with the new schema
"""

import os
import sys
from flask import Flask

# Import all models to ensure they're registered
from models import (
    db, User, YamlFile, RomFile, Lobby, LobbySettings, LobbyPlayer, ChatMessage,
    Game, FavoriteGame, Friend, Ban, Warning, UserRating, UserReview,
    ServerRating, TwitchConnection, CustomWorld
)

def init_database():
    """Initialize database with fresh schema"""

    # Create minimal Flask app
    frontend_dir = os.path.join(os.path.dirname(__file__), '..', 'frontend')
    app = Flask(__name__,
                template_folder=os.path.join(frontend_dir, 'src'),
                static_folder=os.path.join(frontend_dir, 'static'))

    app.config['SQLALCHEMY_DATABASE_URI'] = os.getenv('DATABASE_URL')
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

    # Initialize database
    db.init_app(app)

    with app.app_context():
        print("🗑️  Dropping all existing tables...")
        db.drop_all()

        print("🏗️  Creating new tables with updated schema...")
        db.create_all()

        print("\n✅ Database initialized successfully!")
        print("\nTables created:")
        print("  - users (with email_verified, is_banned, is_suspended)")
        print("  - yaml_files")
        print("  - rom_files")
        print("  - lobbies (with slug, time_limit_hours, timer_started_at)")
        print("  - lobby_settings (with new options)")
        print("  - lobby_players")
        print("  - chat_messages (with is_pinned, deleted)")
        print("  - games")
        print("  - favorite_games")
        print("  - friends")
        print("  - bans")
        print("  - warnings")
        print("  - user_ratings")
        print("  - user_reviews")
        print("  - server_ratings")
        print("  - twitch_connections")
        print("  - custom_worlds")

if __name__ == '__main__':
    print("="*70)
    print("  SekaiLink Database Initialization")
    print("="*70)
    print("\n⚠️  WARNING: This will DROP ALL existing tables and data!")
    print("This is safe for development, but DO NOT run in production.\n")

    response = input("Continue? (yes/no): ").strip().lower()

    if response == 'yes':
        init_database()
    else:
        print("\n❌ Aborted. Database not modified.")
        sys.exit(0)
