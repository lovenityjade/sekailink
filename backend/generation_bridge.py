"""
Generation Bridge - WebHostLib Integration

This module bridges SekaiLink's lobby system with Archipelago's proven generation logic.
Instead of custom generation code, we use WebHostLib's battle-tested implementation.
"""

import sys
import os

# Add Archipelago to Python path
ARCHIPELAGO_PATH = os.path.join(os.path.dirname(__file__), '..', 'archipelago_core')
sys.path.insert(0, ARCHIPELAGO_PATH)

import tempfile
import random
import logging
from collections import Counter
from typing import Dict, Any

# Archipelago imports
from BaseClasses import get_seed, seeddigits
from Generate import mystery_argparse, PlandoOptions, handle_name
from Main import main as archipelago_main
from WebHostLib.check import get_yaml_data, roll_options

logger = logging.getLogger(__name__)


def generate_multiworld(lobby_id: int, db_session) -> Dict[str, Any]:
    """
    Generate multiworld using Archipelago's WebHostLib logic

    Args:
        lobby_id: Database ID of the lobby
        db_session: SQLAlchemy session

    Returns:
        Dict with generation results or error
    """
    from models import Lobby, LobbyPlayer, YamlFile, User

    logger.info(f"🎮 Starting WebHostLib generation for lobby {lobby_id}")

    # Get lobby and players
    lobby = db_session.query(Lobby).get(lobby_id)
    if not lobby:
        return {"error": "Lobby not found"}

    players = db_session.query(LobbyPlayer).filter_by(lobby_id=lobby_id).all()

    if not players:
        return {"error": "No players in lobby"}

    logger.info(f"📋 Found {len(players)} players")

    # Collect YAML contents
    yaml_contents = {}
    player_map = {}  # Map filenames to player info

    for player in players:
        yaml_file = db_session.query(YamlFile).get(player.yaml_file_id)
        if not yaml_file:
            user = db_session.query(User).get(player.user_id)
            return {"error": f"YAML not found for player {user.username}"}

        user = db_session.query(User).get(player.user_id)
        filename = f"{user.username}.yaml"

        yaml_contents[filename] = yaml_file.content
        player_map[filename] = {
            "user_id": user.id,
            "username": user.username,
            "game": yaml_file.game
        }

        logger.info(f"👤 Player: {user.username} - Game: {yaml_file.game}")

    # Validate YAMLs using WebHostLib
    logger.info("🔍 Validating YAMLs...")

    try:
        # Create temporary files for validation
        temp_files = []
        for filename, content in yaml_contents.items():
            temp_file = tempfile.NamedTemporaryFile(
                mode='w',
                suffix='.yaml',
                delete=False,
                encoding='utf-8'
            )
            temp_file.write(content)
            temp_file.close()
            temp_files.append(temp_file.name)

        # Read files back as file objects for get_yaml_data
        file_handles = []
        for temp_path in temp_files:
            file_handles.append(open(temp_path, 'r', encoding='utf-8'))

        # Validate using WebHostLib
        options = get_yaml_data(file_handles)

        # Close file handles
        for fh in file_handles:
            fh.close()

        # Clean up temp files
        for temp_path in temp_files:
            os.unlink(temp_path)

        if isinstance(options, str):
            # Validation error
            logger.error(f"❌ YAML validation failed: {options}")
            return {"error": f"YAML validation failed: {options}"}

    except Exception as e:
        logger.error(f"❌ YAML validation exception: {str(e)}")
        return {"error": f"YAML validation error: {str(e)}"}

    # Roll options using WebHostLib
    logger.info("🎲 Rolling options...")

    try:
        results, gen_options = roll_options(options, set())

        # Check for errors in results
        errors = [msg for msg in results.values() if isinstance(msg, str)]
        if errors:
            logger.error(f"❌ Option rolling failed: {errors[0]}")
            return {"error": f"Option error: {errors[0]}"}

    except Exception as e:
        logger.error(f"❌ Option rolling exception: {str(e)}")
        return {"error": f"Option rolling error: {str(e)}"}

    # Create temporary directory for generation
    temp_dir = tempfile.TemporaryDirectory()
    logger.info(f"📁 Temp directory: {temp_dir.name}")

    # Set up Archipelago generation arguments
    playercount = len(gen_options)
    seed = get_seed()

    # Generate seed name
    random.seed(seed)
    seedname = "W" + f"{random.randint(0, pow(10, seeddigits) - 1)}".zfill(seeddigits)

    logger.info(f"🌱 Seed: {seed} ({seedname})")

    # Create args namespace
    args = mystery_argparse([])
    args.multi = playercount
    args.seed = seed
    args.name = {x: "" for x in range(1, playercount + 1)}
    args.spoiler = 0  # No spoiler for now
    args.race = False  # Not a race
    args.outputname = seedname
    args.outputpath = temp_dir.name
    args.teams = 1
    args.plando_options = PlandoOptions.from_set({"bosses", "items", "connections", "texts"})
    args.skip_prog_balancing = False
    args.skip_output = False
    args.sprite = dict.fromkeys(range(1, args.multi + 1), None)
    args.sprite_pool = dict.fromkeys(range(1, args.multi + 1), None)

    # Apply player options
    name_counter = Counter()
    for player_num, (playerfile, settings) in enumerate(gen_options.items(), 1):
        for k, v in settings.items():
            if v is not None:
                if hasattr(args, k):
                    getattr(args, k)[player_num] = v
                else:
                    setattr(args, k, {player_num: v})

        # Set player name
        if not args.name[player_num]:
            args.name[player_num] = os.path.splitext(os.path.split(playerfile)[-1])[0]

        args.name[player_num] = handle_name(args.name[player_num], player_num, name_counter)

    # Verify unique names
    if len(set(args.name.values())) != len(args.name):
        logger.error(f"❌ Duplicate player names: {Counter(args.name.values())}")
        return {"error": f"Player names must be unique. Duplicates: {Counter(args.name.values())}"}

    # Generate server options
    server_options = {
        "hint_cost": 10,
        "release_mode": "auto",  # auto/goal/manual
        "remaining_mode": "enabled",  # enabled/disabled
        "collect_mode": "enabled",  # enabled/disabled
        "countdown_mode": "disabled",  # Time until release
        "item_cheat": True,  # Allow item spawning
        "server_password": None,
    }

    # Run Archipelago generation
    logger.info("⚙️ Running Archipelago generation...")

    try:
        archipelago_main(args, seed, baked_server_options=server_options)
        logger.info("✅ Generation completed successfully!")

    except Exception as e:
        logger.error(f"❌ Generation failed: {str(e)}")
        temp_dir.cleanup()
        return {"error": f"Generation failed: {str(e)}"}

    # Find generated files
    logger.info("📦 Collecting generated files...")

    generated_files = []
    multidata_file = None

    for root, dirs, files in os.walk(temp_dir.name):
        for filename in files:
            filepath = os.path.join(root, filename)

            if filename.endswith('.zip'):
                # Main multiworld data
                multidata_file = filepath
                logger.info(f"  📦 Multidata: {filename}")
            elif filename.endswith(('.apbp', '.aplttp', '.apsoe', '.apsmz3', '.appatch', '.apz5')):
                # Player patch files
                generated_files.append(filepath)
                logger.info(f"  🎮 Patch: {filename}")

    if not multidata_file:
        logger.error("❌ Multidata ZIP not found!")
        temp_dir.cleanup()
        return {"error": "Generation did not produce multidata file"}

    logger.info(f"✅ Generated {len(generated_files)} patch files")

    return {
        "success": True,
        "seed": seedname,
        "multidata_file": multidata_file,
        "patch_files": generated_files,
        "temp_dir": temp_dir,  # Keep alive until files are processed
        "player_count": playercount
    }


def validate_yaml_content(content: str, filename: str) -> Dict[str, Any]:
    """
    Validate YAML content using WebHostLib validation

    Args:
        content: YAML file content
        filename: Original filename

    Returns:
        Dict with validation results
    """
    try:
        # Create temp file
        temp_file = tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.yaml',
            delete=False,
            encoding='utf-8'
        )
        temp_file.write(content)
        temp_file.close()

        # Validate using WebHostLib
        with open(temp_file.name, 'r', encoding='utf-8') as fh:
            options = get_yaml_data([fh])

        # Clean up
        os.unlink(temp_file.name)

        if isinstance(options, str):
            # Error message returned
            return {"valid": False, "error": options}

        # Roll options to extract game info
        results, gen_options = roll_options(options, set())

        errors = [msg for msg in results.values() if isinstance(msg, str)]
        if errors:
            return {"valid": False, "error": errors[0]}

        # Extract game name from first player
        for playerfile, opts in gen_options.items():
            game_name = opts.game
            player_name = getattr(opts, 'name', 'Player')

            return {
                "valid": True,
                "game": game_name,
                "player_name": player_name
            }

        return {"valid": False, "error": "No game found in YAML"}

    except Exception as e:
        logger.error(f"YAML validation error: {str(e)}")
        return {"valid": False, "error": str(e)}
