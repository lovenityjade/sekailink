#!/usr/bin/env python3
"""Export Archipelago APWorld options into SekaiLink Nexus seed-config imports.

Nexus is the source of truth for SekaiLink seed configuration.  This tool reads
the installed Archipelago worlds only as an import source, then writes payloads
that can be POSTed to `/admin/seed-configs/games`.
"""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import importlib
import inspect
import io
import json
import re
import sys
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_AP_ROOT = REPO_ROOT / "runtime" / "ap"
DEFAULT_CLIENT_REGISTRY = REPO_ROOT / "runtime" / "game-registry" / "archipelago-clients.json"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "runtime" / "generated" / "nexus-seed-config-imports"

LIVE_APWORLD_OPTION_EXCLUDES: dict[str, set[str]] = {
    # The live LADX Beta APWorld currently rejects this old option at generation time.
    "ladx": {"stabilize_item_pool"},
}


def slugify(value: str) -> str:
    value = value.strip().lower()
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return value.strip("_") or "unknown"


def labelize(value: str) -> str:
    return value.replace("_", " ").strip().title()


def clean_doc(cls: type) -> str:
    return inspect.cleandoc(inspect.getdoc(cls) or "")


def json_default(value: Any) -> Any:
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    if isinstance(value, (set, frozenset, tuple, list)):
        return sorted(value) if not isinstance(value, list) else value
    if isinstance(value, dict):
        return value
    return str(value)


def json_sanitize(value: Any) -> Any:
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    if isinstance(value, dict):
        return {str(key): json_sanitize(item) for key, item in value.items()}
    if isinstance(value, (set, frozenset)):
        return [json_sanitize(item) for item in sorted(value, key=str)]
    if isinstance(value, (tuple, list)):
        return [json_sanitize(item) for item in value]
    return str(value)


def stable_hash(payload: dict[str, Any]) -> str:
    raw = json.dumps(json_sanitize(payload), sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def import_archipelago(ap_root: Path) -> dict[str, Any]:
    sys.path.insert(0, str(ap_root))
    captured_stdout = io.StringIO()
    captured_stderr = io.StringIO()
    with contextlib.redirect_stdout(captured_stdout), contextlib.redirect_stderr(captured_stderr):
        worlds_module = importlib.import_module("worlds")
        options_module = importlib.import_module("Options")
    return {
        "AutoWorldRegister": worlds_module.AutoWorldRegister,
        "get_option_groups": options_module.get_option_groups,
        "Visibility": options_module.Visibility,
        "Toggle": options_module.Toggle,
        "Choice": options_module.Choice,
        "TextChoice": options_module.TextChoice,
        "Range": options_module.Range,
        "NamedRange": options_module.NamedRange,
        "OptionDict": options_module.OptionDict,
        "OptionCounter": options_module.OptionCounter,
        "OptionList": options_module.OptionList,
        "OptionSet": options_module.OptionSet,
        "ItemSet": options_module.ItemSet,
        "LocationSet": getattr(options_module, "LocationSet", None),
        "FreeText": options_module.FreeText,
        "Removed": getattr(options_module, "Removed", None),
    }


def load_client_registry(path: Path) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    if not path.exists():
        return {}, {}
    data = json.loads(path.read_text(encoding="utf-8"))
    by_game: dict[str, dict[str, Any]] = {}
    by_world: dict[str, dict[str, Any]] = {}
    for entry in data.get("clients", []):
        game = str(entry.get("game") or "").strip()
        world = str(entry.get("world") or "").strip()
        if game:
            by_game[game] = entry
        if world:
            by_world[world] = entry
    return by_game, by_world


def world_module_key(world: type) -> str:
    module = getattr(world, "__module__", "")
    parts = module.split(".")
    if len(parts) >= 2 and parts[0] == "worlds":
        return parts[1]
    return slugify(module)


def option_type(option_cls: type, ap: dict[str, Any]) -> str:
    mro_names = {cls.__name__ for cls in getattr(option_cls, "__mro__", ())}
    if mro_names & {"PlandoItems", "PlandoTexts", "PlandoConnections"}:
        return "array"
    if issubclass(option_cls, ap["Toggle"]):
        return "boolean"
    if issubclass(option_cls, ap["Choice"]) and not issubclass(option_cls, ap["TextChoice"]):
        return "enum"
    if issubclass(option_cls, ap["Range"]):
        return "integer"
    if issubclass(option_cls, ap["OptionList"]) or issubclass(option_cls, ap["OptionSet"]):
        return "array"
    if issubclass(option_cls, ap["OptionDict"]):
        return "object"
    return "string"


def choice_value_name(option_cls: type, raw_value: Any) -> str:
    name_lookup = getattr(option_cls, "name_lookup", {})
    if isinstance(raw_value, int) and raw_value in name_lookup:
        return str(name_lookup[raw_value])
    return str(raw_value)


def default_value(option_cls: type, type_name: str, ap: dict[str, Any]) -> Any:
    raw = getattr(option_cls, "default", None)
    if type_name == "array":
        return list(json_sanitize(raw or []))
    if type_name == "object":
        return dict(json_sanitize(raw or {}))
    if issubclass(option_cls, ap["Toggle"]):
        return bool(raw)
    if issubclass(option_cls, ap["Choice"]):
        return choice_value_name(option_cls, raw)
    if type_name in {"integer", "number"}:
        value = int(raw or 0)
        if issubclass(option_cls, ap["Range"]):
            value = max(value, int(getattr(option_cls, "range_start", value)))
            value = min(value, int(getattr(option_cls, "range_end", value)))
        return value
    return "" if raw is None else str(raw)


def choices_for(option_cls: type, ap: dict[str, Any]) -> list[dict[str, str]]:
    if not issubclass(option_cls, ap["Choice"]):
        return []
    choices: list[dict[str, str]] = []
    for option_id, option_name in sorted(getattr(option_cls, "name_lookup", {}).items(), key=lambda item: (str(item[0]), item[1])):
        key = str(option_name)
        choices.append({
            "choice_key": key,
            "yaml_value": key,
            "label": labelize(key),
            "description": "",
        })
    return choices


def validation_rules(option_cls: type, group_key: str, group_label: str, ap: dict[str, Any]) -> dict[str, Any]:
    rules: dict[str, Any] = {
        "group_key": group_key,
        "group_label": group_label,
        "ap_type": option_cls.__name__,
        "ap_module": getattr(option_cls, "__module__", ""),
    }
    if issubclass(option_cls, ap["Range"]):
        rules["range_start"] = int(getattr(option_cls, "range_start", 0))
        rules["range_end"] = int(getattr(option_cls, "range_end", 0))
        rules["minimum"] = rules["range_start"]
        rules["maximum"] = rules["range_end"]
        special = getattr(option_cls, "special_range_names", None)
        if isinstance(special, dict) and special:
            rules["special_range_names"] = special
    for attr in ("valid_keys", "valid_keys_casefold", "verify_item_name", "verify_location_name",
                 "convert_name_groups", "supports_weighting"):
        if hasattr(option_cls, attr):
            value = getattr(option_cls, attr)
            if isinstance(value, (set, frozenset)):
                value = sorted(value, key=str)
            if isinstance(value, (str, int, float, bool, list, dict, tuple)) or value is None:
                rules[attr] = json_sanitize(value)
    if issubclass(option_cls, ap["TextChoice"]):
        rules["free_text"] = True
        rules["suggested_choices"] = [choice["choice_key"] for choice in choices_for(option_cls, ap)]
    return rules


def option_definition(option_name: str, option_cls: type, group_key: str, group_label: str, ap: dict[str, Any]) -> dict[str, Any]:
    type_name = option_type(option_cls, ap)
    definition: dict[str, Any] = {
        "option_key": option_name,
        "yaml_key": option_name,
        "label": getattr(option_cls, "display_name", labelize(option_name)),
        "description": clean_doc(option_cls),
        "type": type_name,
        "default_value": default_value(option_cls, type_name, ap),
        "required": True,
        "validation_rules": validation_rules(option_cls, group_key, group_label, ap),
    }
    choices = choices_for(option_cls, ap)
    if choices:
        definition["choices"] = choices
    return definition


def is_removed_option(option_cls: type, ap: dict[str, Any]) -> bool:
    removed_cls = ap.get("Removed")
    return bool(removed_cls and issubclass(option_cls, removed_cls))


def export_world(world_name: str, world: type, registry_by_game: dict[str, dict[str, Any]],
                 registry_by_world: dict[str, dict[str, Any]], ap: dict[str, Any]) -> dict[str, Any]:
    game_display = str(getattr(world, "game", world_name))
    module_key = world_module_key(world)
    registry = registry_by_game.get(game_display) or registry_by_game.get(world_name) or registry_by_world.get(module_key) or {}
    game_key = str(registry.get("game_key") or slugify(game_display))
    system_key = str(registry.get("platform") or "unknown").lower()

    groups_payload: list[dict[str, Any]] = []
    options_payload: list[dict[str, Any]] = []
    option_groups = ap["get_option_groups"](world, ap["Visibility"].template)
    for group_index, (group_label, group_options) in enumerate(option_groups.items()):
        if not group_options:
            continue
        group_key = slugify(group_label)
        groups_payload.append({
            "group_key": group_key,
            "label": group_label,
            "description": "",
            "sort_order": group_index,
        })
        for option_name, option_cls in group_options.items():
            if is_removed_option(option_cls, ap):
                continue
            if option_name in LIVE_APWORLD_OPTION_EXCLUDES.get(module_key, set()):
                continue
            options_payload.append(option_definition(option_name, option_cls, group_key, group_label, ap))

    payload: dict[str, Any] = {
        "game_key": game_key,
        "display_name": game_display,
        "system_key": system_key,
        "linkedworld_id": module_key,
        "schema_version": f"apworld-{module_key}-v1-sekailink-live1" if module_key in LIVE_APWORLD_OPTION_EXCLUDES else f"apworld-{module_key}-v1",
        "source_kind": "archipelago_apworld",
        "source_ref": f"runtime/ap/worlds/{module_key}",
        "groups": groups_payload,
        "options": options_payload,
    }
    payload["source_hash"] = stable_hash({k: v for k, v in payload.items() if k != "source_hash"})
    return payload


def write_payload(output_dir: Path, payload: dict[str, Any]) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / f"{payload['game_key']}.import.json"
    path.write_text(json.dumps(json_sanitize(payload), indent=2, ensure_ascii=False, sort_keys=False) + "\n", encoding="utf-8")
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description="Export APWorld option schemas into Nexus seed-config import JSON.")
    parser.add_argument("--ap-root", type=Path, default=DEFAULT_AP_ROOT)
    parser.add_argument("--client-registry", type=Path, default=DEFAULT_CLIENT_REGISTRY)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--game", action="append", default=[], help="Archipelago game display name to export. Can be repeated.")
    parser.add_argument("--installed-only", action="store_true", help="Only export games present in archipelago-clients.json.")
    args = parser.parse_args()

    ap = import_archipelago(args.ap_root)
    registry_by_game, registry_by_world = load_client_registry(args.client_registry)
    selected_games = set(args.game)
    exported: list[dict[str, Any]] = []

    for world_name, world in sorted(ap["AutoWorldRegister"].world_types.items(), key=lambda item: item[0]):
        if world_name == "Archipelago":
            continue
        if selected_games and world_name not in selected_games and getattr(world, "game", world_name) not in selected_games:
            continue
        if args.installed_only:
            game_display = str(getattr(world, "game", world_name))
            module_key = world_module_key(world)
            if game_display not in registry_by_game and world_name not in registry_by_game and module_key not in registry_by_world:
                continue
        payload = export_world(world_name, world, registry_by_game, registry_by_world, ap)
        write_payload(args.output_dir, payload)
        exported.append({
            "game_key": payload["game_key"],
            "display_name": payload["display_name"],
            "system_key": payload["system_key"],
            "linkedworld_id": payload["linkedworld_id"],
            "options": len(payload["options"]),
        })

    manifest = {
        "format": "sekailink-nexus-seed-config-import-manifest-v1",
        "source": str(args.ap_root),
        "count": len(exported),
        "games": exported,
    }
    (args.output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"exported {len(exported)} game schemas to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
