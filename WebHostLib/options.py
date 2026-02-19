import collections.abc
import datetime
import json
import os
import re
import uuid
import urllib.parse
import urllib.request
from textwrap import dedent
from typing import Any, Dict, Union
from docutils.core import publish_parts

import yaml
from flask import redirect, render_template, request, Response, abort, session, url_for, make_response, jsonify
from pony.orm import db_session, select
from urllib.parse import quote

import Options
from Utils import local_path, get_file_safe_name
from worlds.AutoWorld import AutoWorldRegister
from . import app, cache
from .generate import get_meta
from .misc import get_world_theme, _get_user_yaml_dir, _get_session_user
from .models import User, UserYaml

_PLAYER_OPTIONS_MT_CACHE = {}
_PLAYER_OPTIONS_MT_CACHE_PATH = local_path("WebHostLib", "generated", "player_options_mt_cache.json")


def _load_player_options_mt_cache() -> dict:
    global _PLAYER_OPTIONS_MT_CACHE
    if _PLAYER_OPTIONS_MT_CACHE:
        return _PLAYER_OPTIONS_MT_CACHE
    try:
        with open(_PLAYER_OPTIONS_MT_CACHE_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            if isinstance(data, dict):
                _PLAYER_OPTIONS_MT_CACHE = data
    except Exception:
        _PLAYER_OPTIONS_MT_CACHE = {}
    return _PLAYER_OPTIONS_MT_CACHE


def _save_player_options_mt_cache() -> None:
    try:
        os.makedirs(os.path.dirname(_PLAYER_OPTIONS_MT_CACHE_PATH), exist_ok=True)
        with open(_PLAYER_OPTIONS_MT_CACHE_PATH, "w", encoding="utf-8") as f:
            json.dump(_PLAYER_OPTIONS_MT_CACHE, f, ensure_ascii=False)
    except Exception:
        pass


def _translate_player_options_text(text: str, locale: str):
    if not text or locale == 'en':
        return None
    source = str(text).strip()
    if not source:
        return None
    key = f"{locale}::{source}"
    cache = _load_player_options_mt_cache()
    return cache.get(key)


def create() -> None:
    target_folder = local_path("WebHostLib", "static", "generated")
    yaml_folder = os.path.join(target_folder, "configs")

    Options.generate_yaml_templates(yaml_folder)


def render_options_page(template: str, world_name: str, is_complex: bool = False) -> Union[Response, str]:
    world = AutoWorldRegister.world_types[world_name]
    if world.hidden or world.web.options_page is False or world_name in app.config["HIDDEN_WEBWORLDS"]:
        return redirect("games")
    visibility_flag = Options.Visibility.complex_ui if is_complex else Options.Visibility.simple_ui

    start_collapsed = {"Game Options": False}
    for group in world.web.option_groups:
        start_collapsed[group.name] = group.start_collapsed

    return render_template(
        template,
        world_name=world_name,
        world=world,
        option_groups=Options.get_option_groups(world, visibility_level=visibility_flag),
        start_collapsed=start_collapsed,
        issubclass=issubclass,
        Options=Options,
        theme=get_world_theme(world_name),
    )


def render_dashboard_options_page(world_name: str, form_action: str, yaml_prefill: str | None = None,
                                  yaml_id: str | None = None, yaml_title: str | None = None,
                                  error_message: str | None = None) -> Union[Response, str]:
    world = AutoWorldRegister.world_types[world_name]
    if world.hidden or world.web.options_page is False or world_name in app.config["HIDDEN_WEBWORLDS"]:
        return redirect("games")
    visibility_flag = Options.Visibility.simple_ui

    start_collapsed = {"Game Options": False}
    for group in world.web.option_groups:
        start_collapsed[group.name] = group.start_collapsed

    return render_template(
        "playerOptions/playerOptions.html",
        world_name=world_name,
        world=world,
        option_groups=Options.get_option_groups(world, visibility_level=visibility_flag),
        start_collapsed=start_collapsed,
        issubclass=issubclass,
        Options=Options,
        theme=get_world_theme(world_name),
        dashboard_mode=True,
        form_action=form_action,
        yaml_prefill=yaml_prefill,
        yaml_id=yaml_id,
        yaml_title=yaml_title,
        error_message=error_message,
    )


def generate_game(options: Dict[str, Union[dict, str]]) -> Union[Response, str]:
    from .generate import start_generation
    return start_generation(options, get_meta({}))


def send_yaml(player_name: str, formatted_options: dict) -> Response:
    response = Response(yaml.dump(formatted_options, sort_keys=False))
    response.headers["Content-Type"] = "text/yaml"
    response.headers["Content-Disposition"] = f"attachment; filename={player_name}.yaml"
    return response


def _parse_player_options_form():
    options = {}
    intent_generate = False

    for key, val in request.form.items(multi=True):
        if val == "_ensure-empty-list":
            options[key] = []
        elif options.get(key):
            if not isinstance(options[key], list):
                options[key] = [options[key]]
            options[key].append(val)
        else:
            options[key] = val

    for key, val in options.copy().items():
        key_parts = key.rsplit("||", 2)
        if key_parts[-1] == "qty":
            if key_parts[0] not in options:
                options[key_parts[0]] = {}
            if val and val != "0":
                options[key_parts[0]][key_parts[1]] = int(val)
            del options[key]

        elif key_parts[-1].endswith("-custom"):
            if val:
                options[key_parts[-1][:-7]] = val
            del options[key]

        elif key_parts[-1].endswith("-range"):
            if options[key_parts[-1][:-6]] == "custom":
                options[key_parts[-1][:-6]] = val
            del options[key]

    for key, val in options.copy().items():
        if key.startswith("random-"):
            options[key.removeprefix("random-")] = "random"
            del options[key]

    if not options.get("name"):
        return None, None, None, None, None, None

    preset_name = 'default'
    if "intent-generate" in options:
        intent_generate = True
        del options["intent-generate"]
    if "intent-export" in options:
        del options["intent-export"]
    if "intent-save" in options:
        del options["intent-save"]
    if "game-options-preset" in options:
        preset_name = options["game-options-preset"]
        del options["game-options-preset"]

    yaml_title = options.pop("yaml-name", None)
    yaml_id = options.pop("yaml-id", None)

    player_name = options["name"]
    del options["name"]
    return options, player_name, preset_name, intent_generate, yaml_title, yaml_id


def _get_current_user_yaml_dir() -> str | None:
    user = _get_session_user()
    if not user:
        return None
    return _get_user_yaml_dir(user)


def _get_current_user() -> User | None:
    return _get_session_user()


def _get_custom_flag(user: User, yaml_id: str) -> bool:
    try:
        with db_session:
            entry = UserYaml.get(user=user, yaml_id=yaml_id)
            return bool(entry.custom) if entry else False
    except Exception:
        return False


def _upsert_yaml_custom(user: User, yaml_id: str, custom: bool) -> None:
    try:
        entry = UserYaml.get(user=user, yaml_id=yaml_id)
        if entry:
            entry.custom = custom
            entry.updated_at = datetime.datetime.utcnow()
            return
        UserYaml(user=user, yaml_id=yaml_id, custom=custom)
    except Exception:
        return


def _list_user_yamls_with_custom(user: User) -> list[dict[str, str]]:
    yaml_entries: list[dict[str, str]] = []
    user_dir = _get_user_yaml_dir(user)
    try:
        with db_session:
            custom_map = {entry.yaml_id: entry.custom for entry in select(e for e in UserYaml if e.user == user)}
    except Exception:
        custom_map = {}
    for filename in os.listdir(user_dir):
        if not filename.endswith(".yaml"):
            continue
        yaml_id = filename[:-5]
        path = os.path.join(user_dir, filename)
        title = filename
        game = ""
        player_name = ""
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = yaml.safe_load(f) or {}
            player_name = content.get("name", "")
            game = content.get("game", "")
            title = (content.get("sekailink") or {}).get("title") or title
        except Exception:
            pass
        yaml_entries.append({
            "id": yaml_id,
            "title": title,
            "game": game,
            "player_name": player_name,
            "custom": bool(custom_map.get(yaml_id)),
        })
    return sorted(yaml_entries, key=lambda entry: entry["title"].lower())


@app.template_filter("dedent")
def filter_dedent(text: str) -> str:
    return dedent(text).strip("\n ")

@app.template_filter('encodeURIComponent')
def encodeURIComponent_filter(s):
    return quote(str(s), safe='~()*!.\'')  

@app.template_filter("rst_to_html")
def filter_rst_to_html(text: str) -> str:
    """Converts reStructuredText (such as a Python docstring) to HTML."""
    if text.startswith(" ") or text.startswith("\t"):
        text = dedent(text)
    elif "\n" in text:
        lines = text.splitlines()
        text = lines[0] + "\n" + dedent("\n".join(lines[1:]))

    return publish_parts(text, writer_name='html', settings=None, settings_overrides={
        'raw_enable': False,
        'file_insertion_enabled': False,
        'output_encoding': 'unicode'
    })['body']


@app.template_test("ordered")
def test_ordered(obj):
    return isinstance(obj, collections.abc.Sequence)


@app.route("/games/<string:game>/option-presets", methods=["GET"])
@cache.cached()
def option_presets(game: str) -> Response:
    world = AutoWorldRegister.world_types[game]
    json_data = json.dumps(_build_option_presets(world), cls=_SetEncoder)
    response = Response(json_data)
    response.headers["Content-Type"] = "application/json"
    return response


class _SetEncoder(json.JSONEncoder):
    def default(self, obj):
        from collections.abc import Set
        if isinstance(obj, Set):
            return list(obj)
        return json.JSONEncoder.default(self, obj)


def _build_option_presets(world: Any) -> dict:
    presets = {}
    for preset_name, preset in world.web.options_presets.items():
        presets[preset_name] = {}
        for preset_option_name, preset_option in preset.items():
            if preset_option == "random":
                presets[preset_name][preset_option_name] = preset_option
                continue

            option = world.options_dataclass.type_hints[preset_option_name].from_any(preset_option)
            if isinstance(option, Options.NamedRange) and isinstance(preset_option, str):
                assert preset_option in option.special_range_names, \
                    f"Invalid preset value '{preset_option}' for '{preset_option_name}' in '{preset_name}'. " \
                    f"Expected {option.special_range_names.keys()} or {option.range_start}-{option.range_end}."

                presets[preset_name][preset_option_name] = option.value
            elif isinstance(option, (Options.Range, Options.OptionSet, Options.OptionList, Options.OptionCounter)):
                presets[preset_name][preset_option_name] = option.value
            elif isinstance(preset_option, str):
                # Ensure the option value is valid for Choice and Toggle options
                assert option.name_lookup[option.value] == preset_option, \
                    f"Invalid option value '{preset_option}' for '{preset_option_name}' in preset '{preset_name}'. " \
                    f"Values must not be resolved to a different option via option.from_text (or an alias)."
                # Use the name of the option
                presets[preset_name][preset_option_name] = option.current_key
            else:
                # Use the name of the option
                presets[preset_name][preset_option_name] = option.current_key
    return presets


def _option_rich_text(option, world) -> bool:
    if option.rich_text_doc is None:
        return bool(world.web.rich_text_options_doc)
    return bool(option.rich_text_doc)




def _sorted_name_lookup_keys(option) -> list:
    # Some worlds use mixed key types (e.g. int + tuple) in name_lookup.
    # Python 3 won"t sort mixed types by default; order isn"t semantically important here.
    return sorted(option.name_lookup.keys(), key=lambda k: (0, k) if isinstance(k, int) else (1, str(k)))

def _serialize_option(option_name: str, option, world) -> dict:
    data = {
        "name": option_name,
        "display_name": getattr(option, "display_name", None) or option_name,
        "doc": (option.__doc__ or "").strip(),
        "rich_text": _option_rich_text(option, world),
        "default": option.default,
    }

    if issubclass(option, Options.Toggle):
        data["type"] = "toggle"
        data["allow_random"] = True
        data["default_is_random"] = option.default == "random"
        data["default_value"] = (
            option.name_lookup.get(option.default)
            if isinstance(option.default, int)
            else option.name_lookup.get(0)
        )
        data["choices"] = [
            {"value": option.name_lookup[key], "label": option.get_option_name(key)}
            for key in _sorted_name_lookup_keys(option)
            if option.name_lookup[key] != "random"
        ]
        return data

    if issubclass(option, Options.TextChoice):
        data["type"] = "text_choice"
        data["allow_random"] = True
        data["default_is_random"] = option.default == "random"
        data["default_value"] = (
            option.name_lookup.get(option.default)
            if isinstance(option.default, int)
            else option.default
        )
        data["choices"] = [
            {"value": option.name_lookup[key], "label": option.get_option_name(key)}
            for key in _sorted_name_lookup_keys(option)
            if option.name_lookup[key] != "random"
        ]
        return data

    if issubclass(option, Options.Choice):
        data["type"] = "choice"
        data["allow_random"] = True
        data["default_is_random"] = option.default == "random"
        data["default_value"] = (
            option.name_lookup.get(option.default)
            if isinstance(option.default, int)
            else option.default
        )
        data["choices"] = [
            {"value": option.name_lookup[key], "label": option.get_option_name(key)}
            for key in _sorted_name_lookup_keys(option)
            if option.name_lookup[key] != "random"
        ]
        return data

    if issubclass(option, Options.NamedRange):
        data["type"] = "named_range"
        data["allow_random"] = True
        data["range_start"] = option.range_start
        data["range_end"] = option.range_end
        data["special_range_names"] = dict(option.special_range_names)
        data["default_is_random"] = option.default == "random"
        data["default_value"] = option.range_start if option.default == "random" else option.default
        return data

    if issubclass(option, Options.Range):
        data["type"] = "range"
        data["allow_random"] = True
        data["range_start"] = option.range_start
        data["range_end"] = option.range_end
        data["default_is_random"] = option.default == "random"
        data["default_value"] = option.range_start if option.default == "random" else option.default
        return data

    if issubclass(option, Options.FreeText):
        data["type"] = "free_text"
        return data

    if issubclass(option, Options.OptionCounter):
        data["type"] = "option_counter"
        data["valid_keys"] = list(option.valid_keys or [])
        data["verify_item_name"] = bool(getattr(option, "verify_item_name", False))
        data["verify_location_name"] = bool(getattr(option, "verify_location_name", False))
        return data

    if issubclass(option, Options.LocationSet):
        data["type"] = "location_set"
        data["valid_keys"] = list(option.valid_keys or [])
        data["verify_location_name"] = bool(getattr(option, "verify_location_name", False))
        return data

    if issubclass(option, Options.ItemSet):
        data["type"] = "item_set"
        data["valid_keys"] = list(option.valid_keys or [])
        data["verify_item_name"] = bool(getattr(option, "verify_item_name", False))
        return data

    if issubclass(option, Options.OptionSet):
        data["type"] = "option_set"
        data["valid_keys"] = list(option.valid_keys or [])
        return data

    if issubclass(option, Options.OptionList):
        if option.valid_keys:
            data["type"] = "option_list"
            data["valid_keys"] = list(option.valid_keys or [])
        else:
            data["type"] = "manual"
        return data

    if issubclass(option, Options.OptionDict):
        data["type"] = "manual"
        return data

    data["type"] = "manual"
    return data



def _normalize_locale(raw_locale: str) -> str:
    value = str(raw_locale or '').strip().lower()
    if value.startswith('fr'):
        return 'fr'
    if value.startswith('es'):
        return 'es'
    if value.startswith('ja'):
        return 'ja'
    if value.startswith('zh-cn') or value.startswith('zh-hans'):
        return 'zh-CN'
    if value.startswith('zh-tw') or value.startswith('zh-hant'):
        return 'zh-TW'
    return 'en'


def _request_locale() -> str:
    query_locale = request.args.get('lang', '')
    header_locale = request.headers.get('X-SekaiLink-Locale') or request.headers.get('Accept-Language') or ''
    return _normalize_locale(query_locale or header_locale)


PLAYER_OPTIONS_I18N = {
    'A Link to the Past': {
        'fr': {
            'groups': {
                'Game Options': 'Options de jeu',
                'Item & Location Options': 'Objets et emplacements',
                'Dungeon & Boss Options': 'Donjons et boss',
                'Enemy & Difficulty Options': 'Ennemis et difficulte',
                'Generation Options': 'Options de generation',
                'Aesthetics & Accessibility': 'Accessibilite et visuel',
            },
            'options': {
                'Accessibility': 'Accessibilite',
                'Goal': 'Objectif',
                'Mode': 'Mode',
                'Glitches Required': 'Glitches requis',
                'Dark Room Logic': 'Logique des salles sombres',
                'Open Pyramid Hole': 'Pyramide ouverte',
                'Crystals for GT': 'Cristaux pour GT',
                'Crystals for Ganon': 'Cristaux pour Ganon',
                'Triforce Pieces Mode': 'Mode pieces de Triforce',
                'Triforce Pieces Percentage': 'Pourcentage de pieces de Triforce',
                'Triforce Pieces Required': 'Pieces requises',
                'Triforce Pieces Available': 'Pieces disponibles',
                'Triforce Pieces Extra': 'Pieces bonus',
                'Entrance Shuffle': 'Melange des entrees',
                'Entrance Shuffle Seed': 'Graine du melange des entrees',
                'Boss Shuffle': 'Melange des boss',
                'Enemy Shuffle': 'Melange des ennemis',
                'Enemy Damage': 'Degats ennemis',
                'Enemy Health': 'Vie ennemie',
                'Menu Speed': 'Vitesse des menus',
                'Play music': 'Musique',
                'Reduce Screen Flashes': 'Reduire les flashs ecran',
                'Display Method for Triforce Hunt': 'Affichage chasse Triforce',
                'Sprite Pool': 'Pool de sprites',
                'Random Sprite on Hit': 'Sprite aleatoire a l impact',
                'Random Sprite on Enter': 'Sprite aleatoire en entrant',
                'Random Sprite on Exit': 'Sprite aleatoire en sortant',
                'Random Sprite on Slash': 'Sprite aleatoire a l epee',
                'Random Sprite on Item': 'Sprite aleatoire sur objet',
                'Random Sprite on Bonk': 'Sprite aleatoire au choc',
                'Random Sprite on Everything': 'Sprite aleatoire partout',
            },
            'docs': {
                'Goal': 'Definit la condition de victoire de la partie.',
                'Mode': 'Choisit la logique d acces du monde.',
                'Glitches Required': 'Niveau de techniques attendu pour la logique.',
                'Crystals for Ganon': 'Nombre de cristaux necessaires pour affronter Ganon.',
                'Crystals for GT': 'Nombre de cristaux necessaires pour ouvrir la Tour de Ganon.'
            },
            'choices': {
                'glitches_required': {
                    'no_glitches': 'Aucun glitch',
                    'minor_glitches': 'Glitches mineurs',
                    'overworld_glitches': 'Glitches overworld',
                    'hybrid_major_glitches': 'Glitches majeurs hybrides',
                    'no_logic': 'Sans logique',
                    'none': 'Aucun glitch',
                    'noglitches': 'Aucun glitch',
                    'nologic': 'Sans logique'
                }
            }
        }
    }
}


GENERIC_PLAYER_OPTIONS_I18N = {
    'fr': {
        'groups': {
            'Game Options': 'Options de jeu',
            'Item & Location Options': 'Objets et emplacements',
            'Dungeon & Boss Options': 'Donjons et boss',
            'Enemy & Difficulty Options': 'Ennemis et difficulte',
            'Generation Options': 'Options de generation',
            'Aesthetics & Accessibility': 'Accessibilite et visuel',
        },
        'option_names': {
            'accessibility': 'Accessibilite',
            'goal': 'Objectif',
            'mode': 'Mode',
            'logic': 'Logique',
            'glitches_required': 'Glitches requis',
            'difficulty': 'Difficulte',
            'hints': 'Indices',
            'hint_cost': 'Cout des indices',
            'progression_balancing': 'Equilibrage de progression',
            'item_pool': 'Pool d objets',
            'entrance_shuffle': 'Melange des entrees',
            'entrance_shuffle_seed': 'Graine du melange des entrees',
            'boss_shuffle': 'Melange des boss',
            'enemy_shuffle': 'Melange des ennemis',
            'enemy_damage': 'Degats ennemis',
            'enemy_health': 'Vie ennemie',
            'music': 'Musique',
            'menu_speed': 'Vitesse des menus',
            'death_link': 'Lien de mort',
            'dark_room_logic': 'Logique des salles sombres',
            'open_pyramid': 'Pyramide ouverte',
            'crystals_needed_for_gt': 'Cristaux pour GT',
            'crystals_needed_for_ganon': 'Cristaux pour Ganon',
            'triforce_pieces_mode': 'Mode pieces de Triforce',
            'triforce_pieces_required': 'Pieces de Triforce requises',
            'triforce_pieces_available': 'Pieces de Triforce disponibles',
            'triforce_pieces_extra': 'Pieces de Triforce bonus',
            'reduceflashing': 'Reduire les flashs ecran',
            'display_triforce_mode': 'Affichage chasse Triforce',
        },
        'choices': {
            'true': 'Oui',
            'false': 'Non',
            'yes': 'Oui',
            'no': 'Non',
            'random': 'Aleatoire',
            'default': 'Defaut',
            'normal': 'Normal',
            'hard': 'Difficile',
            'easy': 'Facile',
            'standard': 'Standard',
            'open': 'Ouvert',
        },
    },
    'es': {
        'groups': {
            'Game Options': 'Opciones de juego',
            'Item & Location Options': 'Objetos y ubicaciones',
            'Dungeon & Boss Options': 'Mazmorras y jefes',
            'Enemy & Difficulty Options': 'Enemigos y dificultad',
            'Generation Options': 'Opciones de generacion',
            'Aesthetics & Accessibility': 'Accesibilidad y estetica',
        },
        'option_names': {
            'accessibility': 'Accesibilidad',
            'goal': 'Objetivo',
            'mode': 'Modo',
            'logic': 'Logica',
            'glitches_required': 'Glitches requeridos',
            'difficulty': 'Dificultad',
            'hints': 'Pistas',
            'hint_cost': 'Costo de pistas',
            'progression_balancing': 'Balance de progresion',
            'item_pool': 'Pool de objetos',
            'entrance_shuffle': 'Mezcla de entradas',
            'entrance_shuffle_seed': 'Semilla de mezcla de entradas',
            'boss_shuffle': 'Mezcla de jefes',
            'enemy_shuffle': 'Mezcla de enemigos',
            'enemy_damage': 'Danio enemigo',
            'enemy_health': 'Salud enemiga',
            'music': 'Musica',
            'menu_speed': 'Velocidad del menu',
            'death_link': 'Enlace de muerte',
            'dark_room_logic': 'Logica de cuarto oscuro',
            'open_pyramid': 'Piramide abierta',
            'crystals_needed_for_gt': 'Cristales para GT',
            'crystals_needed_for_ganon': 'Cristales para Ganon',
            'triforce_pieces_mode': 'Modo piezas de Trifuerza',
            'triforce_pieces_required': 'Piezas de Trifuerza requeridas',
            'triforce_pieces_available': 'Piezas de Trifuerza disponibles',
            'triforce_pieces_extra': 'Piezas de Trifuerza extra',
            'reduceflashing': 'Reducir destellos de pantalla',
            'display_triforce_mode': 'Metodo de visualizacion de Trifuerza',
        },
        'choices': {
            'true': 'Si',
            'false': 'No',
            'yes': 'Si',
            'no': 'No',
            'random': 'Aleatorio',
            'default': 'Predeterminado',
            'normal': 'Normal',
            'hard': 'Dificil',
            'easy': 'Facil',
            'standard': 'Estandar',
            'open': 'Abierto',
        },
    },
    'ja': {
        'groups': {
            'Game Options': 'ゲームオプション',
            'Item & Location Options': 'アイテムとロケーション',
            'Dungeon & Boss Options': 'ダンジョンとボス',
            'Enemy & Difficulty Options': '敵と難易度',
            'Generation Options': '生成オプション',
            'Aesthetics & Accessibility': '見た目とアクセシビリティ',
        },
        'option_names': {
            'accessibility': 'アクセシビリティ',
            'goal': '目標',
            'mode': 'モード',
            'logic': 'ロジック',
            'glitches_required': '必要グリッチ',
            'difficulty': '難易度',
            'hints': 'ヒント',
            'hint_cost': 'ヒントコスト',
            'progression_balancing': '進行バランス',
            'item_pool': 'アイテムプール',
            'entrance_shuffle': '入口シャッフル',
            'entrance_shuffle_seed': '入口シャッフルシード',
            'boss_shuffle': 'ボスシャッフル',
            'enemy_shuffle': '敵シャッフル',
            'enemy_damage': '敵ダメージ',
            'enemy_health': '敵体力',
            'music': '音楽',
            'menu_speed': 'メニュー速度',
            'death_link': 'デスリンク',
            'dark_room_logic': '暗闇部屋ロジック',
            'open_pyramid': 'ピラミッド開放',
            'crystals_needed_for_gt': 'ガノンタワー必要クリスタル',
            'crystals_needed_for_ganon': 'ガノン必要クリスタル',
            'triforce_pieces_mode': 'トライフォース欠片モード',
            'triforce_pieces_required': '必要トライフォース欠片',
            'triforce_pieces_available': '配置トライフォース欠片',
            'triforce_pieces_extra': '追加トライフォース欠片',
            'reduceflashing': '画面フラッシュ軽減',
            'display_triforce_mode': 'トライフォース表示方法',
        },
        'choices': {
            'true': 'はい',
            'false': 'いいえ',
            'yes': 'はい',
            'no': 'いいえ',
            'random': 'ランダム',
            'default': 'デフォルト',
            'normal': '通常',
            'hard': 'ハード',
            'easy': 'イージー',
            'standard': '標準',
            'open': 'オープン',
        },
    },
    'zh-CN': {
        'groups': {
            'Game Options': '游戏选项',
            'Item & Location Options': '物品与地点',
            'Dungeon & Boss Options': '地牢与Boss',
            'Enemy & Difficulty Options': '敌人与难度',
            'Generation Options': '生成选项',
            'Aesthetics & Accessibility': '外观与辅助功能',
        },
        'option_names': {
            'accessibility': '辅助功能',
            'goal': '目标',
            'mode': '模式',
            'logic': '逻辑',
            'glitches_required': '所需漏洞技巧',
            'difficulty': '难度',
            'hints': '提示',
            'hint_cost': '提示消耗',
            'progression_balancing': '进度平衡',
            'item_pool': '物品池',
            'entrance_shuffle': '入口洗牌',
            'entrance_shuffle_seed': '入口洗牌种子',
            'boss_shuffle': 'Boss洗牌',
            'enemy_shuffle': '敌人洗牌',
            'enemy_damage': '敌人伤害',
            'enemy_health': '敌人生命',
            'music': '音乐',
            'menu_speed': '菜单速度',
            'death_link': '死亡链接',
            'dark_room_logic': '黑屋逻辑',
            'open_pyramid': '金字塔开放',
            'crystals_needed_for_gt': 'GT所需水晶',
            'crystals_needed_for_ganon': '甘侬所需水晶',
            'triforce_pieces_mode': '三角力量碎片模式',
            'triforce_pieces_required': '所需三角力量碎片',
            'triforce_pieces_available': '可用三角力量碎片',
            'triforce_pieces_extra': '额外三角力量碎片',
            'reduceflashing': '减少屏幕闪烁',
            'display_triforce_mode': '三角力量狩猎显示方式',
        },
        'choices': {
            'true': '是',
            'false': '否',
            'yes': '是',
            'no': '否',
            'random': '随机',
            'default': '默认',
            'normal': '普通',
            'hard': '困难',
            'easy': '简单',
            'standard': '标准',
            'open': '开放',
        },
    },
    'zh-TW': {
        'groups': {
            'Game Options': '遊戲選項',
            'Item & Location Options': '物品與地點',
            'Dungeon & Boss Options': '地城與Boss',
            'Enemy & Difficulty Options': '敵人與難度',
            'Generation Options': '生成選項',
            'Aesthetics & Accessibility': '外觀與輔助功能',
        },
        'option_names': {
            'accessibility': '輔助功能',
            'goal': '目標',
            'mode': '模式',
            'logic': '邏輯',
            'glitches_required': '所需漏洞技巧',
            'difficulty': '難度',
            'hints': '提示',
            'hint_cost': '提示成本',
            'progression_balancing': '進度平衡',
            'item_pool': '物品池',
            'entrance_shuffle': '入口洗牌',
            'entrance_shuffle_seed': '入口洗牌種子',
            'boss_shuffle': 'Boss洗牌',
            'enemy_shuffle': '敵人洗牌',
            'enemy_damage': '敵人傷害',
            'enemy_health': '敵人生命',
            'music': '音樂',
            'menu_speed': '選單速度',
            'death_link': '死亡連結',
            'dark_room_logic': '黑屋邏輯',
            'open_pyramid': '金字塔開放',
            'crystals_needed_for_gt': 'GT所需水晶',
            'crystals_needed_for_ganon': '加農所需水晶',
            'triforce_pieces_mode': '三角神力碎片模式',
            'triforce_pieces_required': '所需三角神力碎片',
            'triforce_pieces_available': '可用三角神力碎片',
            'triforce_pieces_extra': '額外三角神力碎片',
            'reduceflashing': '減少畫面閃爍',
            'display_triforce_mode': '三角神力狩獵顯示方式',
        },
        'choices': {
            'true': '是',
            'false': '否',
            'yes': '是',
            'no': '否',
            'random': '隨機',
            'default': '預設',
            'normal': '普通',
            'hard': '困難',
            'easy': '簡單',
            'standard': '標準',
            'open': '開放',
        },
    },
}


GENERIC_OPTION_TOKEN_TRANSLATIONS = {
    'fr': {
        'accessibility': 'accessibilite', 'allow': 'autoriser', 'balancing': 'equilibrage', 'boss': 'boss',
        'cost': 'cout', 'crystal': 'cristal', 'crystals': 'cristaux', 'death': 'mort', 'difficulty': 'difficulte',
        'display': 'affichage', 'enemy': 'ennemi', 'enter': 'entrer', 'entrance': 'entree', 'extra': 'bonus',
        'for': 'pour', 'game': 'jeu', 'ganon': 'ganon', 'glitches': 'glitches', 'goal': 'objectif', 'gt': 'gt',
        'health': 'vie', 'hint': 'indice', 'hints': 'indices', 'item': 'objet', 'items': 'objets', 'link': 'lien',
        'location': 'emplacement', 'logic': 'logique', 'menu': 'menu', 'mode': 'mode', 'music': 'musique',
        'needed': 'necessaires', 'open': 'ouvert', 'options': 'options', 'pieces': 'pieces', 'pool': 'pool',
        'progression': 'progression', 'pyramid': 'pyramide', 'random': 'aleatoire', 'reduce': 'reduire',
        'required': 'requis', 'room': 'salle', 'screen': 'ecran', 'seed': 'graine', 'shuffle': 'melange',
        'speed': 'vitesse', 'sprite': 'sprite', 'triforce': 'triforce'
    },
    'es': {
        'accessibility': 'accesibilidad', 'allow': 'permitir', 'balancing': 'balance', 'boss': 'jefe',
        'cost': 'costo', 'crystal': 'cristal', 'crystals': 'cristales', 'death': 'muerte', 'difficulty': 'dificultad',
        'display': 'visualizacion', 'enemy': 'enemigo', 'enter': 'entrar', 'entrance': 'entrada', 'extra': 'extra',
        'for': 'para', 'game': 'juego', 'ganon': 'ganon', 'glitches': 'glitches', 'goal': 'objetivo', 'gt': 'gt',
        'health': 'salud', 'hint': 'pista', 'hints': 'pistas', 'item': 'objeto', 'items': 'objetos', 'link': 'enlace',
        'location': 'ubicacion', 'logic': 'logica', 'menu': 'menu', 'mode': 'modo', 'music': 'musica',
        'needed': 'necesarios', 'open': 'abierto', 'options': 'opciones', 'pieces': 'piezas', 'pool': 'pool',
        'progression': 'progresion', 'pyramid': 'piramide', 'random': 'aleatorio', 'reduce': 'reducir',
        'required': 'requeridos', 'room': 'sala', 'screen': 'pantalla', 'seed': 'semilla', 'shuffle': 'mezcla',
        'speed': 'velocidad', 'sprite': 'sprite', 'triforce': 'trifuerza'
    },
    'ja': {
        'accessibility': 'アクセシビリティ', 'allow': '許可', 'balancing': 'バランス', 'boss': 'ボス',
        'cost': 'コスト', 'crystal': 'クリスタル', 'crystals': 'クリスタル', 'death': '死亡', 'difficulty': '難易度',
        'display': '表示', 'enemy': '敵', 'enter': '入る', 'entrance': '入口', 'extra': '追加', 'for': '用',
        'game': 'ゲーム', 'ganon': 'ガノン', 'glitches': 'グリッチ', 'goal': '目標', 'gt': 'GT', 'health': '体力',
        'hint': 'ヒント', 'hints': 'ヒント', 'item': 'アイテム', 'items': 'アイテム', 'link': 'リンク',
        'location': 'ロケーション', 'logic': 'ロジック', 'menu': 'メニュー', 'mode': 'モード', 'music': '音楽',
        'needed': '必要', 'open': 'オープン', 'options': 'オプション', 'pieces': '欠片', 'pool': 'プール',
        'progression': '進行', 'pyramid': 'ピラミッド', 'random': 'ランダム', 'reduce': '軽減', 'required': '必須',
        'room': '部屋', 'screen': '画面', 'seed': 'シード', 'shuffle': 'シャッフル', 'speed': '速度',
        'sprite': 'スプライト', 'triforce': 'トライフォース'
    },
    'zh-CN': {
        'accessibility': '辅助功能', 'allow': '允许', 'balancing': '平衡', 'boss': 'Boss', 'cost': '消耗',
        'crystal': '水晶', 'crystals': '水晶', 'death': '死亡', 'difficulty': '难度', 'display': '显示',
        'enemy': '敌人', 'enter': '进入', 'entrance': '入口', 'extra': '额外', 'for': '用于', 'game': '游戏',
        'ganon': '甘侬', 'glitches': '漏洞技巧', 'goal': '目标', 'gt': 'GT', 'health': '生命', 'hint': '提示',
        'hints': '提示', 'item': '物品', 'items': '物品', 'link': '链接', 'location': '地点', 'logic': '逻辑',
        'menu': '菜单', 'mode': '模式', 'music': '音乐', 'needed': '所需', 'open': '开放', 'options': '选项',
        'pieces': '碎片', 'pool': '池', 'progression': '进度', 'pyramid': '金字塔', 'random': '随机',
        'reduce': '减少', 'required': '需要', 'room': '房间', 'screen': '屏幕', 'seed': '种子', 'shuffle': '洗牌',
        'speed': '速度', 'sprite': '精灵', 'triforce': '三角力量'
    },
    'zh-TW': {
        'accessibility': '輔助功能', 'allow': '允許', 'balancing': '平衡', 'boss': 'Boss', 'cost': '成本',
        'crystal': '水晶', 'crystals': '水晶', 'death': '死亡', 'difficulty': '難度', 'display': '顯示',
        'enemy': '敵人', 'enter': '進入', 'entrance': '入口', 'extra': '額外', 'for': '用於', 'game': '遊戲',
        'ganon': '加農', 'glitches': '漏洞技巧', 'goal': '目標', 'gt': 'GT', 'health': '生命', 'hint': '提示',
        'hints': '提示', 'item': '物品', 'items': '物品', 'link': '連結', 'location': '地點', 'logic': '邏輯',
        'menu': '選單', 'mode': '模式', 'music': '音樂', 'needed': '所需', 'open': '開放', 'options': '選項',
        'pieces': '碎片', 'pool': '池', 'progression': '進度', 'pyramid': '金字塔', 'random': '隨機',
        'reduce': '減少', 'required': '需要', 'room': '房間', 'screen': '畫面', 'seed': '種子', 'shuffle': '洗牌',
        'speed': '速度', 'sprite': '精靈', 'triforce': '三角神力'
    },
}


def _localized_text_fallback(value: str, locale: str):
    if not value or locale == 'en':
        return None
    token_map = GENERIC_OPTION_TOKEN_TRANSLATIONS.get(locale)
    if not token_map:
        return None
    chunks = re.split(r'([^A-Za-z0-9_]+)', str(value))
    changed = False
    out = []
    for chunk in chunks:
        if not chunk:
            continue
        if not re.match(r'^[A-Za-z0-9_]+$', chunk):
            out.append(chunk)
            continue
        parts = [p for p in chunk.lower().split('_') if p]
        if not parts:
            out.append(chunk)
            continue
        mapped = [token_map.get(part, part) for part in parts]
        if any(a != b for a, b in zip(parts, mapped)):
            changed = True
        if locale.startswith('ja') or locale.startswith('zh'):
            out.append(''.join(mapped))
        else:
            out.append(' '.join(mapped))
    if not changed:
        return None
    return ''.join(out)


def _localized_option_name_fallback(option_name: str, locale: str):
    if not option_name or locale == 'en':
        return None
    token_map = GENERIC_OPTION_TOKEN_TRANSLATIONS.get(locale)
    if not token_map:
        return None
    parts = [p for p in str(option_name).lower().replace(' ', '_').split('_') if p]
    if not parts:
        return None
    translated = [token_map.get(part, part) for part in parts]
    if all(a == b for a, b in zip(parts, translated)):
        return None
    if locale.startswith('ja') or locale.startswith('zh'):
        return ''.join(translated)
    return ' '.join(translated)


def _localize_player_options_payload(payload: dict, locale: str) -> dict:
    if locale == 'en':
        return payload
    world_locale = PLAYER_OPTIONS_I18N.get(payload.get('game'), {}).get(locale) or {}
    generic_locale = GENERIC_PLAYER_OPTIONS_I18N.get(locale, {})

    groups_map = {**generic_locale.get('groups', {}), **world_locale.get('groups', {})}
    options_map = world_locale.get('options', {})
    option_name_map = generic_locale.get('option_names', {})
    docs_map = world_locale.get('docs', {})
    choices_map = world_locale.get('choices', {})
    generic_choices = generic_locale.get('choices', {})

    for group in payload.get('groups', []):
        group_name = group.get('name', '')
        if group_name in groups_map:
            group['name'] = groups_map[group_name]
        for option in group.get('options', []):
            display_name = option.get('display_name', '')
            option_name = option.get('name', '')
            if display_name in options_map:
                option['display_name'] = options_map[display_name]
            elif option_name in option_name_map:
                option['display_name'] = option_name_map[option_name]
            else:
                fallback_name = _localized_option_name_fallback(option_name, locale)
                if fallback_name:
                    option['display_name'] = fallback_name
                else:
                    mt_name = _translate_player_options_text(display_name or option_name, locale)
                    if mt_name:
                        option['display_name'] = mt_name
            if display_name in docs_map:
                option['doc'] = docs_map[display_name]
            elif option.get('doc'):
                source_doc = option.get('doc')
                mt_doc = _translate_player_options_text(source_doc, locale)
                if mt_doc:
                    option['doc'] = mt_doc
                else:
                    fallback_doc = _localized_text_fallback(source_doc, locale)
                    if fallback_doc:
                        option['doc'] = fallback_doc
            choice_map = choices_map.get(option_name, {})
            for choice in option.get('choices', []) or []:
                value = str(choice.get('value', '')).lower()
                if value in choice_map:
                    choice['label'] = choice_map[value]
                elif value in generic_choices:
                    choice['label'] = generic_choices[value]
                else:
                    fallback_label = _localized_option_name_fallback(str(choice.get('value', '')), locale)
                    if fallback_label:
                        choice['label'] = fallback_label
                    else:
                        mt_choice = _translate_player_options_text(choice.get('label') or choice.get('value'), locale)
                        if mt_choice:
                            choice['label'] = mt_choice
    return payload

def _resolve_world(game: str):
    if game in AutoWorldRegister.world_types:
        return AutoWorldRegister.world_types[game]
    lowered = game.lower().replace("_", " ").strip()
    for key, world in AutoWorldRegister.world_types.items():
        if key.lower() == game.lower():
            return world
        display = getattr(world, "game", "")
        if display and display.lower() == lowered:
            return world
        if display and get_file_safe_name(display).lower() == game.lower():
            return world
    return None


@app.route("/api/player-options/<string:game>", methods=["GET"])
def api_player_options(game: str):
    locale = _request_locale()
    world = _resolve_world(game)
    if not world:
        return abort(404)
    if world.hidden or world.web.options_page is False or game in app.config["HIDDEN_WEBWORLDS"]:
        return abort(404)

    visibility_flag = Options.Visibility.simple_ui
    option_groups = Options.get_option_groups(world, visibility_level=visibility_flag)
    group_defs = {g.name: g for g in world.web.option_groups}
    groups_payload = []
    for group_name, options_map in option_groups.items():
        group_payload = {
            "name": group_name,
            "start_collapsed": bool(group_defs.get(group_name).start_collapsed) if group_name in group_defs else False,
            "options": [
                _serialize_option(option_name, option, world)
                for option_name, option in options_map.items()
            ],
        }
        groups_payload.append(group_payload)

    payload = {
        "game": world.game,
        "game_id": world.__name__ if hasattr(world, "__name__") else game,
        "display_name": world.game,
        "rich_text_options_doc": bool(world.web.rich_text_options_doc),
        "item_names": list(getattr(world, "item_names", [])),
        "location_names": list(getattr(world, "location_names", [])),
        "item_name_groups": {k: list(v) for k, v in getattr(world, "item_name_groups", {}).items()},
        "location_name_groups": {k: list(v) for k, v in getattr(world, "location_name_groups", {}).items()},
        "groups": groups_payload,
        "presets": _build_option_presets(world),
    }
    payload = _localize_player_options_payload(payload, locale)
    json_data = json.dumps(payload, cls=_SetEncoder)
    response = Response(json_data)
    response.headers["Content-Type"] = "application/json"
    return response


@app.route("/dashboard/yaml/<string:game>/edit/option-presets", methods=["GET"])
@cache.cached()
def dashboard_option_presets(game: str) -> Response:
    return option_presets(game)


@app.route("/weighted-options")
def weighted_options_old():
    return redirect("games", 301)


@app.route("/games/<string:game>/weighted-options")
@cache.cached()
def weighted_options(game: str):
    try:
        return render_options_page("weightedOptions/weightedOptions.html", game, is_complex=True)
    except KeyError:
        return abort(404)


@app.route("/games/<string:game>/generate-weighted-yaml", methods=["POST"])
def generate_weighted_yaml(game: str):
    if request.method == "POST":
        intent_generate = False
        options = {}

        for key, val in request.form.items():
            if val == "_ensure-empty-list":
                options[key] = {}
            elif "||" not in key:
                if len(str(val)) == 0:
                    continue

                options[key] = val
            else:
                if int(val) == 0:
                    continue

                [option, setting] = key.split("||")
                options.setdefault(option, {})[setting] = int(val)

        # Error checking
        if "name" not in options:
            return "Player name is required."

        # Remove POST data irrelevant to YAML
        if "intent-generate" in options:
            intent_generate = True
            del options["intent-generate"]
        if "intent-export" in options:
            del options["intent-export"]

        # Properly format YAML output
        player_name = options["name"]
        del options["name"]

        formatted_options = {
            "name": player_name,
            "game": game,
            "description": f"Generated by SekaiLink for {game}",
            game: options,
        }

        if intent_generate:
            return generate_game({player_name: formatted_options})

        else:
            return send_yaml(player_name, formatted_options)


# Player options pages
@app.route("/games/<string:game>/player-options")
@cache.cached()
def player_options(game: str):
    try:
        return render_options_page("playerOptions/playerOptions.html", game, is_complex=False)
    except KeyError:
        return abort(404)


# YAML generator for player-options
@app.route("/games/<string:game>/generate-yaml", methods=["POST"])
def generate_yaml(game: str):
    if request.method == "POST":
        options, player_name, preset_name, intent_generate, _, _ = _parse_player_options_form()
        if not options or not player_name:
            return "Player name is required."

        description = f"Generated by SekaiLink for {game}"
        if preset_name != 'default' and preset_name != 'custom':
            description += f" using {preset_name} preset"

        formatted_options = {
            "name": player_name,
            "game": game,
            "description": description,
            game: options,
        }

        if intent_generate:
            return generate_game({player_name: formatted_options})

        else:
            return send_yaml(player_name, formatted_options)


@app.route("/dashboard/yaml/new")
def dashboard_yaml_new():
    from .misc import get_visible_worlds
    if not _get_current_user():
        return redirect("/api/auth/login")
    worlds = get_visible_worlds()
    user = _get_current_user()
    yamls = _list_user_yamls_with_custom(user) if user else []
    response = make_response(render_template("gameManager.html", worlds=worlds, yamls=yamls))
    response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0"
    response.headers["Pragma"] = "no-cache"
    response.headers["Expires"] = "0"
    return response


@app.route("/dashboard/yaml/<string:game>/create")
def dashboard_yaml_create(game: str):
    if not _get_current_user():
        return redirect("/api/auth/login")
    form_action = url_for("dashboard_yaml_save", game=game)
    return render_dashboard_options_page(game, form_action=form_action)


@app.route("/dashboard/yaml/import", methods=["POST"])
def dashboard_yaml_import():
    if not _get_current_user():
        return abort(401)
    upload = request.files.get("yaml-file")
    if not upload:
        return redirect("/account")
    try:
        content = upload.read().decode("utf-8", errors="ignore")
        parsed = yaml.safe_load(content) or {}
    except Exception:
        return "Invalid YAML file."
    game = parsed.get("game")
    if not game:
        return "Invalid YAML file: missing game."
    yaml_title = (parsed.get("sekailink") or {}).get("title")
    if not yaml_title:
        player_name = parsed.get("name", "Player")
        yaml_title = f"{player_name} - {game}"
    form_action = url_for("dashboard_yaml_save", game=game)
    return render_dashboard_options_page(game, form_action=form_action, yaml_prefill=content, yaml_title=yaml_title)






@app.route("/dashboard/yaml/import/<string:import_id>")
def dashboard_yaml_import_token(import_id: str):
    user = _get_current_user()
    if not user:
        return redirect("/api/auth/login")
    cached = cache.get(f"yaml_import:{import_id}")
    if not cached:
        return "Import token expired.", 410
    if cached.get("user_id") != user.discord_id:
        return abort(403)
    content = cached.get("content") or ""
    game = cached.get("game")
    yaml_title = cached.get("yaml_title")
    if not game:
        return "Invalid YAML import.", 400
    form_action = url_for("dashboard_yaml_save", game=game)
    return render_dashboard_options_page(game, form_action=form_action, yaml_prefill=content, yaml_title=yaml_title)


@app.route("/dashboard/yaml/<string:game>/edit/<string:yaml_id>")
def dashboard_yaml_edit(game: str, yaml_id: str):
    if not _get_current_user():
        return redirect("/api/auth/login")
    user_dir = _get_current_user_yaml_dir()
    if not user_dir:
        return abort(403)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if not os.path.exists(yaml_path):
        return abort(404)
    with open(yaml_path, "r", encoding="utf-8") as f:
        yaml_content = f.read()
    yaml_title = None
    try:
        parsed = yaml.safe_load(yaml_content)
        yaml_title = (parsed.get("sekailink") or {}).get("title")
    except Exception:
        pass
    form_action = url_for("dashboard_yaml_save", game=game)
    return render_dashboard_options_page(game, form_action=form_action, yaml_prefill=yaml_content,
                                         yaml_id=yaml_id, yaml_title=yaml_title)


@app.route("/dashboard/yaml/<string:game>/save", methods=["POST"])
def dashboard_yaml_save(game: str):
    if not _get_current_user():
        return abort(401)
    user_dir = _get_current_user_yaml_dir()
    if not user_dir:
        return abort(403)
    options, player_name, preset_name, _, yaml_title, yaml_id = _parse_player_options_form()
    if not options or not player_name or not yaml_title:
        form_action = url_for("dashboard_yaml_save", game=game)
        return render_dashboard_options_page(
            game,
            form_action=form_action,
            yaml_prefill=None,
            yaml_id=yaml_id,
            yaml_title=yaml_title,
            error_message="Please fill in Player Name and YAML Title before saving.",
        )

    description = f"SekaiLink dashboard YAML for {game}"
    if preset_name != 'default' and preset_name != 'custom':
        description += f" using {preset_name} preset"

    formatted_options = {
        "name": player_name,
        "game": game,
        "description": description,
        "sekailink": {
            "title": yaml_title or f"{player_name} - {game}",
        },
        game: options,
    }

    if not yaml_id:
        yaml_id = str(uuid.uuid4())
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    with open(yaml_path, "w", encoding="utf-8") as f:
        yaml.dump(formatted_options, f, sort_keys=False, allow_unicode=True)
    user = _get_current_user()
    if user:
        with db_session:
            _upsert_yaml_custom(user, yaml_id, False)
    return redirect("/dashboard/yaml/new")


@app.route("/dashboard/yaml/<string:yaml_id>/delete", methods=["POST"])
def dashboard_yaml_delete(yaml_id: str):
    if not _get_current_user():
        return abort(401)
    user_dir = _get_current_user_yaml_dir()
    if not user_dir:
        return abort(403)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if os.path.exists(yaml_path):
        os.remove(yaml_path)
    user = _get_current_user()
    if user:
        with db_session:
            entry = UserYaml.get(user=user, yaml_id=yaml_id)
            if entry:
                entry.delete()
    return redirect("/dashboard/yaml/new")


@app.route("/dashboard/yaml/<string:yaml_id>/download")
def dashboard_yaml_download(yaml_id: str):
    if not _get_current_user():
        return abort(401)
    user_dir = _get_current_user_yaml_dir()
    if not user_dir:
        return abort(403)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if not os.path.exists(yaml_path):
        return abort(404)
    with open(yaml_path, "r", encoding="utf-8") as f:
        content = f.read()
    response = Response(content)
    response.headers["Content-Type"] = "text/yaml"
    response.headers["Content-Disposition"] = f"attachment; filename={yaml_id}.yaml"
    return response


@app.route("/api/yamls/<string:yaml_id>/duplicate", methods=["POST"])
def api_yaml_duplicate(yaml_id: str):
    user = _get_current_user()
    if not user:
        return abort(401)
    user_dir = _get_user_yaml_dir(user)
    src_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if not os.path.exists(src_path):
        return abort(404)
    with open(src_path, "r", encoding="utf-8") as f:
        content = f.read()
    try:
        parsed = yaml.safe_load(content) or {}
    except Exception:
        return {"error": "Invalid YAML format."}, 400
    title = (parsed.get("sekailink") or {}).get("title") or f"{parsed.get('name')} - {parsed.get('game')}"
    if " (Copy)" not in title:
        title = f"{title} (Copy)"
    parsed.setdefault("sekailink", {})
    parsed["sekailink"]["title"] = title
    new_id = str(uuid.uuid4())
    new_path = os.path.join(user_dir, f"{new_id}.yaml")
    new_content = yaml.dump(parsed, sort_keys=False, allow_unicode=True).strip("\n") + "\n"
    with open(new_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    with db_session:
        _upsert_yaml_custom(user, new_id, _get_custom_flag(user, yaml_id))
    return {
        "status": "ok",
        "yaml": {
            "id": new_id,
            "title": title,
            "game": parsed.get("game", ""),
            "player_name": parsed.get("name", ""),
            "custom": _get_custom_flag(user, yaml_id),
        },
    }


@app.route("/api/yamls", methods=["GET"])
def api_yamls():
    user = _get_current_user()
    if not user:
        return abort(401)
    return {"yamls": _list_user_yamls_with_custom(user)}


@app.route("/api/yamls/new", methods=["POST"])
def api_yaml_new():
    user = _get_current_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    content = (payload.get("content") or "").strip("\n")
    if not content:
        return {"error": "YAML content is empty."}, 400
    try:
        parsed = yaml.safe_load(content)
    except Exception:
        return {"error": "Invalid YAML format."}, 400
    if not isinstance(parsed, dict):
        return {"error": "YAML root must be a mapping."}, 400
    if not parsed.get("game") or not parsed.get("name"):
        return {"error": "YAML must include name and game fields."}, 400
    user_dir = _get_user_yaml_dir(user)
    new_id = str(uuid.uuid4())
    new_path = os.path.join(user_dir, f"{new_id}.yaml")
    title = (parsed.get("sekailink") or {}).get("title") or f"{parsed.get('name')} - {parsed.get('game')}"
    if " (Custom)" not in title:
        title = f"{title} (Custom)"
    parsed.setdefault("sekailink", {})
    parsed["sekailink"]["title"] = title
    new_content = yaml.dump(parsed, sort_keys=False, allow_unicode=True).strip("\n") + "\n"
    with open(new_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    with db_session:
        _upsert_yaml_custom(user, new_id, True)
    return {
        "status": "ok",
        "yaml": {
            "id": new_id,
            "title": title,
            "game": parsed.get("game", ""),
            "player_name": parsed.get("name", ""),
            "custom": True,
        },
    }






@app.route("/api/yamls/import", methods=["POST"])
def api_yaml_import():
    user = _get_current_user()
    if not user:
        return abort(401)
    payload = request.get_json(silent=True) or {}
    content = (payload.get("content") or "").strip("\n")
    if not content:
        return {"error": "YAML content is empty."}, 400
    try:
        parsed = yaml.safe_load(content)
    except Exception:
        return {"error": "Invalid YAML format."}, 400
    if not isinstance(parsed, dict):
        return {"error": "YAML root must be a mapping."}, 400
    game = parsed.get("game")
    if not game:
        return {"error": "YAML must include game field."}, 400
    yaml_title = (parsed.get("sekailink") or {}).get("title")
    if not yaml_title:
        player_name = parsed.get("name", "Player")
        yaml_title = f"{player_name} - {game}"
    import_id = uuid.uuid4().hex
    cache.set(f"yaml_import:{import_id}", {
        "user_id": user.discord_id,
        "content": content + "\n",
        "game": game,
        "yaml_title": yaml_title,
    }, timeout=600)
    return {"status": "ok", "import_id": import_id, "dashboard_url": url_for("dashboard_yaml_import_token", import_id=import_id)}


@app.route("/api/yamls/import/<string:import_id>", methods=["GET"])
def api_yaml_import_get(import_id: str):
    user = _get_current_user()
    if not user:
        return abort(401)
    cached = cache.get(f"yaml_import:{import_id}")
    if not cached:
        return {"error": "Import token expired."}, 410
    if cached.get("user_id") != user.discord_id:
        return abort(403)
    return {
        "status": "ok",
        "import_id": import_id,
        "content": cached.get("content") or "",
        "game": cached.get("game") or "",
        "yaml_title": cached.get("yaml_title") or "",
    }


@app.route("/api/player-options/<string:game>/dashboard-save", methods=["POST"])
def api_player_options_dashboard_save(game: str):
    user = _get_current_user()
    if not user:
        return abort(401)
    world = _resolve_world(game)
    if not world:
        return abort(404)
    if world.hidden or world.web.options_page is False or world.game in app.config["HIDDEN_WEBWORLDS"]:
        return abort(404)
    payload = request.get_json(silent=True) or {}
    player_name = (payload.get("player_name") or "").strip()
    yaml_title = (payload.get("yaml_title") or "").strip()
    yaml_id = (payload.get("yaml_id") or "").strip() or None
    preset_name = (payload.get("preset_name") or "default").strip()
    options_payload = payload.get("options") or {}
    if not player_name:
        return {"error": "Player name is required."}, 400
    if not yaml_title:
        return {"error": "YAML title is required."}, 400
    if not isinstance(options_payload, dict):
        return {"error": "Options payload is invalid."}, 400

    game_name = world.game
    description = f"SekaiLink dashboard YAML for {game_name}"
    if preset_name and preset_name not in {"default", "custom"}:
        description += f" using {preset_name} preset"

    formatted_options = {
        "name": player_name,
        "game": game_name,
        "description": description,
        "sekailink": {
            "title": yaml_title,
        },
        game_name: options_payload,
    }

    user_dir = _get_user_yaml_dir(user)
    if not yaml_id:
        yaml_id = str(uuid.uuid4())
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    with open(yaml_path, "w", encoding="utf-8") as f:
        yaml.dump(formatted_options, f, sort_keys=False, allow_unicode=True)
    with db_session:
        _upsert_yaml_custom(user, yaml_id, False)
    return {
        "status": "ok",
        "yaml": {
            "id": yaml_id,
            "title": yaml_title,
            "game": game,
            "player_name": player_name,
            "custom": False,
        },
    }


@app.route("/api/yamls/<string:yaml_id>/raw", methods=["GET", "POST"])
def api_yaml_raw(yaml_id: str):
    user = _get_current_user()
    if not user:
        return abort(401)
    user_dir = _get_user_yaml_dir(user)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if not os.path.exists(yaml_path):
        return abort(404)
    if request.method == "GET":
        with open(yaml_path, "r", encoding="utf-8") as f:
            content = f.read()
        return {"yaml": content, "custom": _get_custom_flag(user, yaml_id)}
    payload = request.get_json(silent=True) or {}
    content = (payload.get("content") or "").strip("\n")
    save_as_new = bool(payload.get("save_as_new"))
    if not content:
        return {"error": "YAML content is empty."}, 400
    try:
        parsed = yaml.safe_load(content)
    except Exception:
        return {"error": "Invalid YAML format."}, 400
    if not isinstance(parsed, dict):
        return {"error": "YAML root must be a mapping."}, 400
    if not parsed.get("game") or not parsed.get("name"):
        return {"error": "YAML must include name and game fields."}, 400
    if save_as_new:
        new_id = str(uuid.uuid4())
        new_path = os.path.join(user_dir, f"{new_id}.yaml")
        title = (parsed.get("sekailink") or {}).get("title") or f"{parsed.get('name')} - {parsed.get('game')}"
        if " (Custom)" not in title:
            title = f"{title} (Custom)"
        parsed.setdefault("sekailink", {})
        parsed["sekailink"]["title"] = title
        new_content = yaml.dump(parsed, sort_keys=False, allow_unicode=True).strip("\n") + "\n"
        with open(new_path, "w", encoding="utf-8") as f:
            f.write(new_content)
        with db_session:
            _upsert_yaml_custom(user, new_id, True)
        return {
            "status": "ok",
            "yaml": {
                "id": new_id,
                "title": title,
                "game": parsed.get("game", ""),
                "player_name": parsed.get("name", ""),
                "custom": True,
            },
        }
    with open(yaml_path, "w", encoding="utf-8") as f:
        f.write(content + "\n")
    with db_session:
        _upsert_yaml_custom(user, yaml_id, True)
    return {"status": "ok"}


@app.route("/api/yamls/<string:yaml_id>/delete", methods=["POST", "DELETE"])
def api_yaml_delete(yaml_id: str):
    user = _get_current_user()
    if not user:
        return abort(401)
    user_dir = _get_user_yaml_dir(user)
    yaml_path = os.path.join(user_dir, f"{yaml_id}.yaml")
    if os.path.exists(yaml_path):
        os.remove(yaml_path)
    with db_session:
        entry = UserYaml.get(user=user, yaml_id=yaml_id)
        if entry:
            entry.delete()
    return {"status": "ok"}
