#!/usr/bin/env python3
"""
Game Catalog Population Script

Scans the Archipelago core worlds directory and populates the Game table
with metadata for all supported games.

Usage:
    python populate_games.py
"""

import os
import sys
import re
from pathlib import Path

# Add backend to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from main import app, db
from models import Game

# Known ROM requirements (from Archipelago documentation)
ROM_REQUIRED_GAMES = {
    'A Link to the Past', 'Super Metroid', 'Super Mario World', 'Donkey Kong Country 3',
    'Final Fantasy', 'Kirby\'s Dream Land 3', 'Pokemon Red and Blue', 'Pokemon Emerald',
    'Super Mario 64', 'Ocarina of Time', 'Majora\'s Mask', 'StarCraft', 'Warcraft 3',
    'Sonic Adventure 2 Battle', 'Super Mario Bros', 'Mega Man Battle Network 3',
    'The Legend of Zelda', 'Metroid', 'Hollow Knight', 'Pokémon FireRed', 'Pokémon Ruby and Sapphire',
    'Timespinner', 'ChecksFinder', 'Dark Souls III', 'Secret of Evermore', 'Lufia II Ancient Cave',
    'Overcooked! 2'
}


def slug_from_name(name):
    """Convert game name to URL-friendly slug"""
    # Remove special characters, convert to lowercase, replace spaces with hyphens
    slug = re.sub(r'[^\w\s-]', '', name.lower())
    slug = re.sub(r'[-\s]+', '-', slug).strip('-')
    return slug


def scan_archipelago_worlds():
    """Scan Archipelago core directory for worlds"""
    # Use environment variable or default to Docker path
    base_path = os.getenv('ARCHIPELAGO_PATH', '/opt/Archipelago')
    archipelago_path = Path(base_path) / 'worlds'

    if not archipelago_path.exists():
        print(f"❌ Archipelago path not found: {archipelago_path}")
        return []

    games = []

    # Scan world directories
    for world_dir in sorted(archipelago_path.iterdir()):
        if not world_dir.is_dir():
            continue

        # Skip special directories
        if world_dir.name.startswith('_') or world_dir.name.startswith('.'):
            continue

        # Check for __init__.py to confirm it's a valid world
        init_file = world_dir / '__init__.py'
        if not init_file.exists():
            continue

        # Extract game name (try to find it in the world's code)
        game_name = extract_game_name(world_dir)
        if not game_name:
            game_name = world_dir.name.replace('_', ' ').title()

        # Determine if ROM required
        requires_rom = game_name in ROM_REQUIRED_GAMES

        # Create game entry
        game = {
            'name': game_name,
            'slug': slug_from_name(game_name),
            'description': f"Archipelago randomizer support for {game_name}",
            'requires_rom': requires_rom,
            'world_type': 'official',
            'is_active': True,
        }

        games.append(game)
        print(f"✓ Found: {game_name} {'(ROM required)' if requires_rom else ''}")

    return games


def extract_game_name(world_dir):
    """Try to extract the official game name from the world's __init__.py"""
    init_file = world_dir / '__init__.py'

    try:
        with open(init_file, 'r', encoding='utf-8') as f:
            content = f.read()

            # Look for game = "..." pattern
            match = re.search(r'game\s*[=:]\s*["\']([^"\']+)["\']', content, re.IGNORECASE)
            if match:
                return match.group(1)
    except Exception as e:
        print(f"  Warning: Could not read {init_file}: {e}")

    return None


def populate_database(games):
    """Populate the Game table with discovered games"""
    with app.app_context():
        # Clear existing games (optional - comment out to preserve custom entries)
        # Game.query.delete()

        added = 0
        updated = 0

        for game_data in games:
            # Check if game already exists
            existing = Game.query.filter_by(slug=game_data['slug']).first()

            if existing:
                # Update existing game
                existing.name = game_data['name']
                existing.description = game_data['description']
                existing.requires_rom = game_data['requires_rom']
                existing.world_type = game_data['world_type']
                existing.is_active = game_data['is_active']
                updated += 1
            else:
                # Add new game
                game = Game(**game_data)
                db.session.add(game)
                added += 1

        db.session.commit()

        print(f"\n✅ Database updated:")
        print(f"   - Added: {added} games")
        print(f"   - Updated: {updated} games")
        print(f"   - Total: {Game.query.count()} games in catalog")


def main():
    """Main execution"""
    print("=" * 60)
    print("SekaiLink - Game Catalog Population")
    print("=" * 60)
    print()

    # Scan for games
    print("Scanning Archipelago worlds...")
    games = scan_archipelago_worlds()

    if not games:
        print("\n❌ No games found! Check Archipelago installation.")
        return 1

    print(f"\nFound {len(games)} games")

    # Populate database
    print("\nPopulating database...")
    try:
        populate_database(games)
        print("\n✅ Game catalog population complete!")
        return 0
    except Exception as e:
        print(f"\n❌ Error populating database: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
