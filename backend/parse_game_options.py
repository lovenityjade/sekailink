#!/usr/bin/env python3
"""
Parse Archipelago game options from world modules.
Extracts option definitions and converts them to JSON format for form generation.
"""

import os
import sys
import importlib
import inspect
from dataclasses import fields, is_dataclass
from typing import Dict, Any, List

# Add Archipelago to Python path - use relative path from backend
archipelago_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'archipelago_core'))
sys.path.insert(0, archipelago_path)

def parse_option_class(option_class) -> Dict[str, Any]:
    """Parse an option class and extract its properties"""
    option_data = {
        'type': option_class.__bases__[0].__name__ if option_class.__bases__ else 'Unknown',
        'display_name': getattr(option_class, 'display_name', option_class.__name__),
        'description': (option_class.__doc__ or '').strip(),
    }

    # Handle Range options
    if hasattr(option_class, 'range_start'):
        option_data['range_start'] = option_class.range_start
        option_data['range_end'] = option_class.range_end
        option_data['default'] = getattr(option_class, 'default', option_class.range_start)

    # Handle Choice options
    elif hasattr(option_class, 'options') or hasattr(option_class, 'option_names'):
        # Get all option_xxx attributes
        choices = {}
        for attr_name in dir(option_class):
            if attr_name.startswith('option_') and not attr_name.startswith('option_'):
                continue
            if attr_name.startswith('option_'):
                choice_name = attr_name.replace('option_', '')
                choice_value = getattr(option_class, attr_name)
                choices[choice_name] = choice_value

        option_data['choices'] = choices
        option_data['default'] = getattr(option_class, 'default', None)

        # Get display names if available
        if hasattr(option_class, 'option_names'):
            option_data['choice_names'] = option_class.option_names

    # Handle Toggle options (boolean)
    elif 'Toggle' in option_data['type']:
        option_data['default'] = getattr(option_class, 'default', 0)

    # Handle special range option
    if hasattr(option_class, 'special_range_names'):
        option_data['special_range_names'] = option_class.special_range_names

    return option_data

def get_game_options(game_name: str) -> Dict[str, Any]:
    """
    Get all options for a specific game.
    Returns a dictionary of option_name -> option_data
    """
    try:
        # Import the world module
        world_module = importlib.import_module(f'worlds.{game_name}')

        # Try to import Options module
        try:
            options_module = importlib.import_module(f'worlds.{game_name}.Options')
        except (ImportError, ModuleNotFoundError):
            # Try to get options from __init__
            options_module = world_module

        # Find the options dataclass (usually ends with "Options")
        options_class = None
        for name, obj in inspect.getmembers(options_module):
            if is_dataclass(obj) and name.endswith('Options'):
                options_class = obj
                break

        if not options_class:
            return {'error': f'No options dataclass found for {game_name}'}

        # Parse each field in the dataclass
        game_options = {}
        for field in fields(options_class):
            option_name = field.name
            option_type = field.type

            # Get the actual option class
            option_class = None
            for name, obj in inspect.getmembers(options_module):
                if inspect.isclass(obj) and name == option_type.__name__:
                    option_class = obj
                    break

            if option_class:
                game_options[option_name] = parse_option_class(option_class)

        return game_options

    except Exception as e:
        return {'error': str(e)}

def get_all_games() -> List[str]:
    """Get list of all available Archipelago games"""
    worlds_path = os.path.join(archipelago_path, 'worlds')
    games = []

    for item in os.listdir(worlds_path):
        item_path = os.path.join(worlds_path, item)
        # Skip files and special directories
        if not os.path.isdir(item_path):
            continue
        if item.startswith('_') or item.startswith('.'):
            continue
        if item == '__pycache__':
            continue

        # Check if it has an __init__.py (valid Python module)
        if os.path.exists(os.path.join(item_path, '__init__.py')):
            games.append(item)

    return sorted(games)

if __name__ == '__main__':
    # Test with Adventure
    print("Testing with Adventure...")
    options = get_game_options('adventure')

    import json
    print(json.dumps(options, indent=2))
