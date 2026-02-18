#!/usr/bin/env python3
"""
Headless patcher wrapper for Electron.
- Reads rom paths from config.json
- Applies AP patch using Patch.create_rom_file
- Emits JSON to stdout
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from typing import Dict, Any

# Packaged builds ship AP python sources in a separate folder and pass its path via env.
_ap_root = os.environ.get("SEKAILINK_AP_ROOT", "").strip()
if _ap_root and os.path.isdir(_ap_root) and _ap_root not in sys.path:
    sys.path.insert(0, _ap_root)

import ModuleUpdate
ModuleUpdate.update()

import Patch
from settings import get_settings


def _emit(payload: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=True) + "\n")
    sys.stdout.flush()


def _load_config(path: str) -> Dict[str, Any]:
    if not path or not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _apply_rom_settings(config: Dict[str, Any]) -> None:
    roms = (config or {}).get("roms", {})
    settings = get_settings()

    # Pokemon Red/Blue (pokemon_rb_options)
    if "pokemon_red" in roms:
        settings.pokemon_rb_options.red_rom_file = roms["pokemon_red"]
    if "pokemon_blue" in roms:
        settings.pokemon_rb_options.blue_rom_file = roms["pokemon_blue"]

    # Pokemon Crystal
    if "pokemon_crystal" in roms:
        settings.pokemon_crystal_settings.rom_file = roms["pokemon_crystal"]

    # Pokemon FireRed/LeafGreen
    if "pokemon_firered" in roms:
        settings.pokemon_frlg_settings.firered_rom_file = roms["pokemon_firered"]
    if "pokemon_leafgreen" in roms:
        settings.pokemon_frlg_settings.leafgreen_rom_file = roms["pokemon_leafgreen"]

    # Pokemon Emerald
    if "pokemon_emerald" in roms:
        settings.pokemon_emerald_settings.rom_file = roms["pokemon_emerald"]

    # A Link to the Past
    if "alttp" in roms:
        settings.lttp_options.rom_file = roms["alttp"]

    # Ocarina of Time
    if "oot" in roms:
        settings.oot_options.rom_file = roms["oot"]

    # Generic support for most AP worlds that use the settings system:
    # if config.roms contains a key matching the world folder name (ex: "mzm"),
    # the settings group key is usually "<world>_options" (ex: "mzm_options") and
    # the group usually exposes "rom_file".
    for game_id, rom_path in (roms or {}).items():
        if not game_id or not rom_path:
            continue
        if game_id in {
            # handled above / special cases
            "pokemon_red", "pokemon_blue",
            "pokemon_crystal",
            "pokemon_firered", "pokemon_leafgreen",
            "pokemon_emerald",
            "alttp", "oot",
        }:
            continue

        settings_key = f"{game_id}_options"
        try:
            if settings_key not in settings:
                continue
            group = settings[settings_key]
        except Exception:
            continue

        try:
            if "rom_file" in group:
                group.rom_file = rom_path
        except Exception:
            # Keep patching best-effort; Patch.create_rom_file will surface the real error if needed.
            continue


def main() -> int:
    parser = argparse.ArgumentParser(description="SekaiLink patcher wrapper")
    parser.add_argument("--patch", required=True, help="Path to .ap* patch file")
    parser.add_argument("--config", default="", help="Path to config.json")
    parser.add_argument("--out-dir", default="", help="Optional output directory")
    args = parser.parse_args()

    try:
        # Ensure the relevant world is imported so its patch container registers with AutoPatchRegister.
        ext = os.path.splitext(args.patch)[1].lower()
        world_by_ext = {
            ".aplttp": "worlds.alttp",
            ".apdkc": "worlds.dkc",
            ".apdkc2": "worlds.dkc2",
            ".apdkc3": "worlds.dkc3",
            ".apeb": "worlds.earthbound",
            ".apff4fe": "worlds.ff4fe",
            ".apffta": "worlds.ffta",
            ".apkdl3": "worlds.kdl3",
            ".apl2ac": "worlds.lufia2ac",
            ".apsml2": "worlds.marioland2",
            ".apmetfus": "worlds.metroidfusion",
            ".apmm2": "worlds.mm2",
            ".apmm3": "worlds.mm3",
            ".apmzm": "worlds.mzm",
            ".apz5": "worlds.oot",
            ".apred": "worlds.pokemon_rb",
            ".apblue": "worlds.pokemon_rb",
            ".apcrystal": "worlds.pokemon_crystal",
            ".apemerald": "worlds.pokemon_emerald",
            ".apfirered": "worlds.pokemon_frlg",
            ".apleafgreen": "worlds.pokemon_frlg",
            ".apsm": "worlds.sm",
            ".apsmw": "worlds.smw",
            ".apsmz3": "worlds.smz3",
            ".aptloz": "worlds.tloz",
            ".apoos": "worlds.tloz_oos",
            ".aptmc": "worlds.tmc",
            ".apwl": "worlds.wl",
            ".apwl4": "worlds.wl4",
            ".apz2": "worlds.zelda2",
            ".apyi": "worlds.yoshisisland",
        }
        mod = world_by_ext.get(ext)
        if mod:
            import importlib
            importlib.import_module(mod)

        config = _load_config(args.config)
        _apply_rom_settings(config)

        meta, output_path = Patch.create_rom_file(args.patch)
        final_path = output_path

        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            final_path = os.path.join(args.out_dir, os.path.basename(output_path))
            shutil.copy2(output_path, final_path)

        _emit({
            "ok": True,
            "meta": meta,
            "output": final_path,
        })
        return 0
    except Exception as exc:
        _emit({"ok": False, "error": str(exc)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
