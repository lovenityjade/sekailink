"""
YAML Creator - Dynamic form generation for Archipelago games

This module provides dynamic YAML creation forms based on game world definitions.
Uses Archipelago's Options system to generate forms automatically.
"""

import sys
import os

# Add Archipelago to Python path
# Use environment variable in Docker, fallback to relative path for local development
ARCHIPELAGO_PATH = os.getenv('ARCHIPELAGO_PATH', os.path.join(os.path.dirname(__file__), '..', 'archipelago_core'))
sys.path.insert(0, ARCHIPELAGO_PATH)

import yaml
import logging
from typing import Dict, Any, Type, List
from flask import request, jsonify

import Options
from worlds.AutoWorld import AutoWorldRegister

logger = logging.getLogger(__name__)


def get_game_options(game_slug: str, visibility_level=Options.Visibility.simple_ui) -> Dict[str, Any]:
    """
    Get options for a specific game with metadata for form generation

    Args:
        game_slug: The game identifier slug (e.g., "a-link-to-the-past")
        visibility_level: Which options to show (simple_ui or complex_ui)

    Returns:
        Dict with option groups and metadata
    """
    # Import here to avoid circular dependency
    from models import Game
    from sqlalchemy.orm import scoped_session, sessionmaker
    from sqlalchemy import create_engine
    import os

    # Get game name from database
    database_url = os.getenv('DATABASE_URL')
    engine = create_engine(database_url)
    Session = scoped_session(sessionmaker(bind=engine))
    session = Session()

    try:
        game = session.query(Game).filter_by(slug=game_slug).first()
        if not game:
            session.close()
            return {"error": f"Game with slug '{game_slug}' not found in database"}

        game_name = game.name
        session.close()

        # Now look up in AutoWorldRegister using the actual game name
        world = AutoWorldRegister.world_types.get(game_name)
        if not world:
            return {"error": f"Game '{game_name}' not found in Archipelago worlds"}
    except Exception as e:
        logger.error(f"Error looking up game: {e}")
        return {"error": f"Failed to lookup game: {str(e)}"}

    if world.hidden or world.web.options_page is False:
        return {"error": f"Game '{game_slug}' does not support YAML creation"}

    # Get option groups using Archipelago's function
    try:
        logger.info(f"Getting option groups for {game_name}, visibility_level={visibility_level}")
        option_groups = Options.get_option_groups(world, visibility_level=visibility_level)
        logger.info(f"Successfully got {len(option_groups)} option groups")
    except Exception as e:
        logger.error(f"Error in get_option_groups: {e}", exc_info=True)
        return {"error": f"Failed to get option groups: {str(e)}"}

    # Build form data
    form_data = {
        "game_name": game_slug,
        "game_display_name": world.__name__,
        "option_groups": {}
    }

    try:
        for group_name, group_options in option_groups.items():
            logger.info(f"Processing group: {group_name}")
            form_data["option_groups"][group_name] = {}

            for option_name, option_class in group_options.items():
                logger.debug(f"Building metadata for option: {option_name}")
                option_info = build_option_metadata(option_name, option_class)
                form_data["option_groups"][group_name][option_name] = option_info
    except Exception as e:
        logger.error(f"Error building form data: {e}", exc_info=True)
        return {"error": f"Failed to build form: {str(e)}"}

    # Add presets if available
    try:
        form_data["presets"] = {}
        logger.info(f"Loading presets from world.web.options_presets")
        for preset_name, preset_options in world.web.options_presets.items():
            form_data["presets"][preset_name] = preset_options
        logger.info(f"Successfully loaded {len(form_data['presets'])} presets")
    except Exception as e:
        logger.error(f"Error loading presets: {e}", exc_info=True)
        form_data["presets"] = {}  # Continue without presets

    # Convert any frozensets or sets to lists for JSON serialization
    import json
    try:
        # This will raise an error if there are non-serializable objects
        json.dumps(form_data)
    except TypeError as e:
        logger.warning(f"JSON serialization issue, attempting to fix: {e}")
        form_data = make_json_safe(form_data)

    return form_data


def make_json_safe(obj):
    """Recursively convert non-JSON-serializable objects"""
    if isinstance(obj, (set, frozenset)):
        return list(obj)
    elif isinstance(obj, dict):
        return {k: make_json_safe(v) for k, v in obj.items()}
    elif isinstance(obj, (list, tuple)):
        return [make_json_safe(item) for item in obj]
    else:
        return obj


def build_option_metadata(option_name: str, option_class: Type[Options.Option]) -> Dict[str, Any]:
    """
    Build metadata for a single option to render in form

    Args:
        option_name: Name of the option
        option_class: The Option class

    Returns:
        Dict with option metadata
    """
    metadata = {
        "name": option_name,
        "display_name": get_display_name(option_class, option_name),
        "description": option_class.__doc__ or "",
        "default": option_class.default,
        "type": get_option_type(option_class)
    }

    # Add type-specific metadata
    if issubclass(option_class, Options.Choice):
        # Dropdown options
        metadata["choices"] = {
            name: value for name, value in option_class.options.items()
            if name not in option_class.aliases  # Exclude aliases
        }
        # Get unique values, try to sort but handle mixed types
        unique_values = set(option_class.options.values())
        try:
            sorted_values = sorted(unique_values)
        except TypeError:
            # Mixed types (e.g., tuples and ints), can't sort
            sorted_values = list(unique_values)

        metadata["choice_names"] = [
            option_class.name_lookup[value]
            for value in sorted_values
        ]

    elif issubclass(option_class, Options.Range):
        # Numeric range
        metadata["min"] = option_class.range_start
        metadata["max"] = option_class.range_end

        # Check for NamedRange with special values
        if issubclass(option_class, Options.NamedRange) and hasattr(option_class, 'special_range_names'):
            metadata["named_values"] = option_class.special_range_names

    elif issubclass(option_class, Options.Toggle):
        # Boolean toggle
        metadata["options"] = ["No", "Yes"]

    elif issubclass(option_class, Options.OptionSet):
        # Set of valid keys
        if hasattr(option_class, 'valid_keys'):
            metadata["valid_keys"] = list(option_class.valid_keys)

    elif issubclass(option_class, Options.OptionList):
        # List of valid keys
        if hasattr(option_class, 'valid_keys'):
            metadata["valid_keys"] = list(option_class.valid_keys)

    elif issubclass(option_class, Options.OptionCounter):
        # Counter with valid keys
        if hasattr(option_class, 'valid_keys'):
            metadata["valid_keys"] = list(option_class.valid_keys)

    return metadata


def get_display_name(option_class: Type[Options.Option], fallback_name: str) -> str:
    """Get display name for an option"""
    if option_class.auto_display_name:
        return fallback_name.replace("_", " ").title()
    return fallback_name


def get_option_type(option_class: Type[Options.Option]) -> str:
    """
    Determine the form input type for an option

    Returns:
        One of: toggle, choice, text_choice, range, named_range, free_text,
                option_set, option_list, option_counter, location_set, item_set
    """
    if issubclass(option_class, Options.Toggle):
        return "toggle"
    elif issubclass(option_class, Options.TextChoice):
        return "text_choice"
    elif issubclass(option_class, Options.Choice):
        return "choice"
    elif issubclass(option_class, Options.NamedRange):
        return "named_range"
    elif issubclass(option_class, Options.Range):
        return "range"
    elif issubclass(option_class, Options.FreeText):
        return "free_text"
    elif issubclass(option_class, Options.OptionCounter):
        return "option_counter"
    elif issubclass(option_class, Options.OptionList):
        return "option_list"
    elif issubclass(option_class, Options.LocationSet):
        return "location_set"
    elif issubclass(option_class, Options.ItemSet):
        return "item_set"
    elif issubclass(option_class, Options.OptionSet):
        return "option_set"
    else:
        return "unknown"


def form_data_to_yaml(game_slug: str, form_data: Dict[str, Any]) -> str:
    """
    Convert form data to YAML string

    Args:
        game_slug: The game identifier
        form_data: Form data from POST request

    Returns:
        YAML string
    """
    player_name = form_data.get("name", "Player")
    description = form_data.get("description", f"Generated by SekaiLink for {game_slug}")

    # Extract game options (remove meta fields)
    game_options = {}
    for key, value in form_data.items():
        if key not in ["name", "description", "game", "intent-export", "intent-save", "game-options-preset"]:
            # Skip empty values
            if value == "" or value is None:
                continue

            # Handle special field suffixes from WebHostLib pattern
            if "||" in key:
                # OptionCounter pattern: option||item||qty
                key_parts = key.rsplit("||", 2)
                if key_parts[-1] == "qty":
                    base_key = key_parts[0]
                    item_name = key_parts[1]

                    if base_key not in game_options:
                        game_options[base_key] = {}

                    if value and int(value) > 0:
                        game_options[base_key][item_name] = int(value)
                    continue

            # Handle custom value fields
            if key.endswith("-custom"):
                base_key = key[:-7]
                if value:
                    game_options[base_key] = value
                continue

            # Handle range custom values
            if key.endswith("-range"):
                base_key = key[:-6]
                if game_options.get(base_key) == "custom" and value:
                    game_options[base_key] = int(value)
                continue

            # Handle random-* keys
            if key.startswith("random-"):
                real_key = key[7:]  # Remove "random-" prefix
                game_options[real_key] = "random"
                continue

            # Regular option
            game_options[key] = value

    # Build YAML structure
    yaml_data = {
        "name": player_name,
        "game": game_slug,
        "description": description,
        game_slug: game_options
    }

    return yaml.dump(yaml_data, sort_keys=False, allow_unicode=True)


def validate_and_save_yaml(user_id: int, game_slug: str, yaml_content: str, db_session) -> Dict[str, Any]:
    """
    Validate YAML and save to database

    Args:
        user_id: User ID
        game_slug: Game identifier
        yaml_content: YAML content
        db_session: SQLAlchemy session

    Returns:
        Dict with success status or error
    """
    from generation_bridge import validate_yaml_content
    from models import YamlFile

    # Validate YAML
    validation_result = validate_yaml_content(yaml_content, f"temp_{user_id}.yaml")

    if not validation_result.get("valid"):
        return {
            "success": False,
            "error": validation_result.get("error", "YAML validation failed")
        }

    # Extract player name from YAML
    yaml_data = yaml.safe_load(yaml_content)
    player_name = yaml_data.get("name", "Player")

    # Save to database
    yaml_file = YamlFile(
        user_id=user_id,
        filename=f"{player_name}.yaml",
        content=yaml_content,
        game=validation_result.get("game", game_slug)
    )

    db_session.add(yaml_file)
    db_session.commit()

    return {
        "success": True,
        "yaml_id": yaml_file.id,
        "game": yaml_file.game,
        "player_name": player_name
    }
