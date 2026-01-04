#!/usr/bin/env python3
"""
Simple text-based parser for Archipelago game options.
Reads Options.py files and extracts option definitions without importing them.
"""

import os
import re
import ast
import json
from typing import Dict, Any, List


def parse_options_file(file_path: str) -> Dict[str, Any]:
    """Parse an Options.py file and extract option definitions"""

    with open(file_path, 'r') as f:
        content = f.read()

    try:
        tree = ast.parse(content)
    except SyntaxError:
        return {'error': 'Failed to parse Python file'}

    options = {}
    option_classes = {}

    # First pass: collect all class definitions
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef):
            class_info = parse_class_definition(node, content)
            if class_info:
                option_classes[node.name] = class_info

    # Second pass: find the main Options dataclass
    for node in ast.walk(tree):
        if isinstance(node, ast.ClassDef) and node.name.endswith('Options'):
            # Check if it's a dataclass
            is_dataclass = any(
                isinstance(dec, ast.Name) and dec.id == 'dataclass'
                or isinstance(dec, ast.Call) and isinstance(dec.func, ast.Name) and dec.func.id == 'dataclass'
                for dec in node.decorator_list
            )

            if is_dataclass:
                # Extract field annotations
                for item in node.body:
                    if isinstance(item, ast.AnnAssign) and isinstance(item.target, ast.Name):
                        field_name = item.target.id
                        if isinstance(item.annotation, ast.Name):
                            class_name = item.annotation.id
                            if class_name in option_classes:
                                options[field_name] = option_classes[class_name]

    return options


def parse_class_definition(node: ast.ClassDef, source: str) -> Dict[str, Any]:
    """Extract information from a class definition"""

    # Get base class
    base_class = None
    if node.bases:
        if isinstance(node.bases[0], ast.Name):
            base_class = node.bases[0].id

    # Skip if not an option class
    if base_class not in ['Range', 'Choice', 'Toggle', 'DefaultOnToggle', 'DeathLink', 'PerGameCommonOptions', 'FreeText', 'NamedRange', 'TextChoice']:
        return None

    # Extract docstring
    docstring = ast.get_docstring(node) or ''

    # Extract class attributes
    attributes = {}
    for item in node.body:
        if isinstance(item, ast.Assign):
            for target in item.targets:
                if isinstance(target, ast.Name):
                    attr_name = target.id  # Fixed: use target.id not target.name
                    # Try to evaluate the value
                    try:
                        if isinstance(item.value, ast.Constant):
                            attributes[attr_name] = item.value.value
                        elif isinstance(item.value, ast.Num):  # Python 3.7 compatibility
                            attributes[attr_name] = item.value.n
                        elif isinstance(item.value, ast.Str):  # Python 3.7 compatibility
                            attributes[attr_name] = item.value.s
                    except:
                        pass

    option_data = {
        'type': base_class,
        'display_name': attributes.get('display_name', node.name),
        'description': docstring.strip(),
    }

    # Add type-specific attributes
    if base_class == 'Range' or base_class == 'NamedRange':
        option_data['range_start'] = attributes.get('range_start', 0)
        option_data['range_end'] = attributes.get('range_end', 100)
        option_data['default'] = attributes.get('default', attributes.get('range_start', 0))

    elif base_class == 'Choice' or base_class == 'TextChoice':
        # Extract option_xxx attributes
        choices = {}
        for attr_name, attr_value in attributes.items():
            if attr_name.startswith('option_'):
                choice_name = attr_name.replace('option_', '')
                choices[choice_name] = attr_value

        option_data['choices'] = choices
        option_data['default'] = attributes.get('default', None)

    elif base_class in ['Toggle', 'DefaultOnToggle', 'DeathLink']:
        option_data['default'] = attributes.get('default', 0)

    return option_data


def get_game_options(game_slug: str) -> Dict[str, Any]:
    """Get options for a specific game by slug"""

    # Paths to check - Docker vs local dev
    if os.path.exists('/opt/Archipelago/worlds'):
        base_path = '/opt/Archipelago/worlds'
    else:
        base_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'archipelago_core', 'worlds'))

    game_path = os.path.join(base_path, game_slug)

    if not os.path.exists(game_path):
        return {'error': f'Game {game_slug} not found'}

    # Try Options.py first
    options_file = os.path.join(game_path, 'Options.py')
    if not os.path.exists(options_file):
        # Try __init__.py
        options_file = os.path.join(game_path, '__init__.py')
        if not os.path.exists(options_file):
            return {'error': f'No Options.py or __init__.py found for {game_slug}'}

    return parse_options_file(options_file)


def get_all_games() -> List[Dict[str, str]]:
    """Get list of all available games with their info"""
    # Paths to check - Docker vs local dev
    if os.path.exists('/opt/Archipelago/worlds'):
        base_path = '/opt/Archipelago/worlds'
    else:
        base_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'archipelago_core', 'worlds'))

    games = []

    for item in os.listdir(base_path):
        item_path = os.path.join(base_path, item)
        # Skip files and special directories
        if not os.path.isdir(item_path):
            continue
        if item.startswith('_') or item.startswith('.'):
            continue
        if item == '__pycache__':
            continue

        # Check if it has an __init__.py
        if os.path.exists(os.path.join(item_path, '__init__.py')):
            games.append({
                'slug': item,
                'name': item.replace('_', ' ').title()
            })

    return sorted(games, key=lambda x: x['name'])


if __name__ == '__main__':
    # Test with Adventure
    print("Testing with Adventure...")
    options = get_game_options('adventure')
    print(json.dumps(options, indent=2))

    print("\n\nAll games:")
    games = get_all_games()
    print(f"Found {len(games)} games")
    for game in games[:10]:
        print(f"  - {game['name']} ({game['slug']})")
