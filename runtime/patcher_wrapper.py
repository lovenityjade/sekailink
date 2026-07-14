#!/usr/bin/env python3
"""
SekaiLink AP patch adapter.

This is a thin private-runtime bridge from Core to the bundled AP/MWGG patcher:
it receives a .ap* artifact plus SekaiLink's local ROM config, imports only the
world module required for that artifact, calls Patch.create_rom_file, and emits
one JSON result line. Users must not need to install Python themselves.
"""
from __future__ import annotations

import argparse
import importlib
import json
import os
import shutil
import sys
import traceback
import types
import warnings
import webbrowser
import zipfile
from typing import Any, Dict

os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
os.environ.setdefault("KIVY_NO_ARGS", "1")
os.environ.setdefault("KIVY_NO_FILELOG", "1")
os.environ.setdefault("SEKAILINK_AP_PATCHER", "1")
sys.dont_write_bytecode = True
warnings.filterwarnings("ignore", message=r"_speedups not available.*", category=UserWarning)


DEFAULT_WORLD_BY_EXT = {
    ".aplttp": "worlds.alttp",
    ".apdkc": "worlds.dkc",
    ".apdkc2": "worlds.dkc2",
    ".apdkc3": "worlds.dkc3",
    ".apeb": "worlds.earthbound",
    ".apff4fe": "worlds.ff4fe",
    ".apffta": "worlds.ffta",
    ".apkdl3": "worlds.kdl3",
    ".apl2ac": "worlds.lufia2ac",
    ".apladxb": "worlds.ladx_beta",
    ".apladx": "worlds.ladx_beta",
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


def _emit(payload: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=True) + "\n")
    sys.stdout.flush()


def _load_config(path: str) -> Dict[str, Any]:
    if not path or not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data if isinstance(data, dict) else {}


def _read_patch_manifest(patch_path: str) -> Dict[str, Any]:
    try:
        with zipfile.ZipFile(patch_path) as zf:
            with zf.open("archipelago.json") as f:
                data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def _add_ap_root(ap_root: str) -> None:
    if not ap_root or not os.path.isdir(ap_root):
        raise FileNotFoundError(f"ap_root_missing:{ap_root}")
    if ap_root not in sys.path:
        sys.path.insert(0, ap_root)


def _install_headless_hooks(rom_path: str = "", patch_path: str = "") -> None:
    """Keep upstream AP patch handlers from opening file/browser dialogs."""
    rom_path = os.path.abspath(rom_path) if rom_path else ""
    patch_path = os.path.abspath(patch_path) if patch_path else ""
    try:
        import Utils
    except Exception:
        return

    try:
        Utils.gui_enabled = False
    except Exception:
        pass

    def choose_file(title: str = "", filetypes: Any = None, suggest: str = "") -> str | None:
        title_l = str(title or "").lower()
        suggest_l = str(suggest or "").lower()
        filetypes_s = str(filetypes or "").lower()
        wants_patch = "patch" in title_l or ".ap" in suggest_l or ".ap" in filetypes_s
        wants_rom = (
            "rom" in title_l
            or "game" in title_l
            or any(ext in filetypes_s for ext in (".sfc", ".smc", ".nes", ".gb", ".gbc", ".gba", ".z64", ".n64", ".iso", ".rvz"))
        )
        if wants_patch and patch_path:
            _emit({"event": "headless_file", "kind": "open_filename", "path": patch_path, "detail": "using_supplied_patch"})
            return patch_path
        if wants_rom and rom_path:
            _emit({"event": "headless_file", "kind": "open_filename", "path": rom_path, "detail": "using_supplied_rom"})
            return rom_path
        if rom_path:
            _emit({"event": "headless_file", "kind": "open_filename", "path": rom_path, "detail": "using_supplied_rom_fallback"})
            return rom_path
        if patch_path:
            _emit({"event": "headless_file", "kind": "open_filename", "path": patch_path, "detail": "using_supplied_patch_fallback"})
            return patch_path
        _emit({"event": "headless_blocked", "kind": "open_filename", "title": str(title or ""), "detail": "no_headless_file_available"})
        return None

    def messagebox(title: str, text: str, error: bool = False) -> None:
        _emit({"event": "messagebox", "title": str(title), "text": str(text), "error": bool(error)})

    def blocked_browser(url: str, *args: Any, **kwargs: Any) -> bool:
        _emit({"event": "headless_blocked", "kind": "webbrowser", "url": str(url or "")})
        return False

    Utils.open_filename = lambda title, filetypes, suggest="": choose_file(title, filetypes, suggest)
    Utils.save_filename = lambda title, filetypes, suggest="": choose_file(title, filetypes, suggest)
    Utils.open_directory = lambda title, suggest="": None
    Utils.messagebox = messagebox
    webbrowser.open = blocked_browser


def _install_scoped_worlds_package(ap_root: str) -> None:
    worlds_dir = os.path.join(ap_root, "worlds")
    if not os.path.isdir(worlds_dir):
        raise FileNotFoundError(f"ap_worlds_missing:{worlds_dir}")

    existing = sys.modules.get("worlds")
    if existing is not None and getattr(existing, "__sekailink_scoped__", False):
        return

    # Avoid executing worlds/__init__.py because upstream imports every world and
    # can pull optional dependencies or network-updater paths. For patching, one
    # target world module is enough to register its AutoPatch handler.
    world_paths = [worlds_dir]
    for entry in os.scandir(worlds_dir):
        if entry.is_file() and entry.name.endswith(".apworld"):
            world_paths.append(entry.path)
    pkg = types.ModuleType("worlds")
    pkg.__path__ = world_paths  # type: ignore[attr-defined]
    pkg.__package__ = "worlds"
    pkg.__sekailink_scoped__ = True  # type: ignore[attr-defined]
    sys.modules["worlds"] = pkg


def _read_world_manifest(ap_root: str, module_name: str) -> Dict[str, Any]:
    parts = module_name.split(".")
    if len(parts) < 2 or parts[0] != "worlds":
        return {}
    world_name = parts[1]
    manifest_path = os.path.join(ap_root, "worlds", *parts[1:], "archipelago.json")
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception:
        apworld_path = os.path.join(ap_root, "worlds", f"{world_name}.apworld")
        try:
            with zipfile.ZipFile(apworld_path) as zf:
                with zf.open(f"{world_name}/archipelago.json") as f:
                    data = json.load(f)
            return data if isinstance(data, dict) else {}
        except Exception:
            return {}


def _import_patch_world(ap_root: str, patch_path: str, world_module: str = "") -> str:
    _add_ap_root(ap_root)
    _install_scoped_worlds_package(ap_root)

    ext = os.path.splitext(patch_path)[1].lower()
    module_name = (world_module or DEFAULT_WORLD_BY_EXT.get(ext, "")).strip()
    if not module_name:
        manifest = _read_patch_manifest(patch_path)
        game = str(manifest.get("game") or "").strip()
        raise RuntimeError(f"missing_world_module:{ext}:{game}")

    importlib.import_module("worlds.Files")
    importlib.import_module(module_name)
    manifest = _read_world_manifest(ap_root, module_name)
    game = str(manifest.get("game") or "").strip()
    world_version = str(manifest.get("world_version") or "").strip()
    if game and world_version:
        try:
            from Utils import tuplize_version
            from worlds.AutoWorld import AutoWorldRegister
            world_type = AutoWorldRegister.world_types.get(game)
            if world_type is not None:
                world_type.world_version = tuplize_version(world_version)
        except Exception:
            pass
    return module_name


def _direct_rom_override(patch_path: str, rom_path: str) -> tuple[str, str] | None:
    if not rom_path:
        return None
    ext = os.path.splitext(patch_path or "")[1].lower()
    by_patch_extension = {
        ".apfirered": ("pokemon_frlg_settings", "firered_rom_file"),
        ".apleafgreen": ("pokemon_frlg_settings", "leafgreen_rom_file"),
    }
    target = by_patch_extension.get(ext)
    if target is None:
        return None
    return target[0], target[1]


def _apply_rom_settings(config: Dict[str, Any], patch_path: str = "", rom_path: str = "") -> None:
    roms = (config or {}).get("roms", {})
    if not isinstance(roms, dict):
        return

    import settings as settings_module
    from settings import get_settings

    settings = get_settings()

    def _ensure_world_group(group_name: str, factory: Any) -> Any:
        try:
            group = getattr(settings, group_name)
            if group is not None:
                return group
        except Exception:
            pass
        group = factory()
        setattr(settings, group_name, group)
        return group

    def _set_option(container: Any, key: str, value: Any) -> None:
        if container is None:
            return
        if isinstance(container, dict):
            current = container.get(key)
            if isinstance(value, str) and current is not None and hasattr(current, "read") and hasattr(current, "resolve"):
                try:
                    container[key] = type(current)(value)
                    return
                except Exception:
                    pass
            container[key] = value
            return
        try:
            current = getattr(container, key)
            if isinstance(value, str) and current is not None and hasattr(current, "read") and hasattr(current, "resolve"):
                setattr(container, key, type(current)(value))
                return
        except Exception:
            pass
        try:
            setattr(container, key, value)
        except Exception:
            try:
                container[key] = value  # type: ignore[index]
            except Exception:
                pass

    def _get_option_group(group_name: str) -> Any:
        try:
            group = getattr(settings, group_name)
            if group is not None:
                return group
        except Exception:
            pass
        try:
            settings_module._update_cache()
            group = getattr(settings, group_name)
            if group is not None:
                return group
        except Exception:
            pass
        try:
            return settings[group_name]  # type: ignore[index]
        except Exception:
            return None

    if "alttp" in roms:
        try:
            from worlds.alttp import ALTTPSettings
            group = _ensure_world_group("lttp_options", ALTTPSettings)
        except Exception:
            group = _get_option_group("lttp_options")
        _set_option(group, "rom_file", roms["alttp"])

    if "earthbound" in roms:
        try:
            from worlds.earthbound import EBSettings
            group = _ensure_world_group("earthbound_options", EBSettings)
        except Exception:
            group = _get_option_group("earthbound_options")
        _set_option(group, "rom_file", roms["earthbound"])

    if "oot" in roms:
        _set_option(_get_option_group("oot_options"), "rom_file", roms["oot"])

    if "smw" in roms:
        try:
            from worlds.smw import SMWSettings
            group = _ensure_world_group("smw_options", SMWSettings)
        except Exception:
            group = _get_option_group("smw_options")
        _set_option(group, "rom_file", roms["smw"])

    special = {
        "alttp",
        "earthbound",
        "oot",
        "smw",
        "pokemon_red",
        "pokemon_blue",
        "pokemon_crystal",
        "pokemon_firered",
        "pokemon_leafgreen",
        "pokemon_emerald",
    }

    if "pokemon_red" in roms:
        _set_option(_get_option_group("pokemon_rb_options"), "red_rom_file", roms["pokemon_red"])
    if "pokemon_blue" in roms:
        _set_option(_get_option_group("pokemon_rb_options"), "blue_rom_file", roms["pokemon_blue"])
    if "pokemon_crystal" in roms:
        _set_option(_get_option_group("pokemon_crystal_settings"), "rom_file", roms["pokemon_crystal"])
    if "pokemon_firered" in roms:
        _set_option(_get_option_group("pokemon_frlg_settings"), "firered_rom_file", roms["pokemon_firered"])
    if "pokemon_leafgreen" in roms:
        _set_option(_get_option_group("pokemon_frlg_settings"), "leafgreen_rom_file", roms["pokemon_leafgreen"])
    if "pokemon_emerald" in roms:
        _set_option(_get_option_group("pokemon_emerald_settings"), "rom_file", roms["pokemon_emerald"])
    if "pokemon_red_blue" in roms:
        _set_option(_get_option_group("pokemon_rb_options"), "red_rom_file", roms["pokemon_red_blue"])
        _set_option(_get_option_group("pokemon_rb_options"), "blue_rom_file", roms["pokemon_red_blue"])

    # The launch resolver has already selected and validated the base ROM for
    # this patch. Prefer that exact file over a stale persisted AP setting.
    direct_override = _direct_rom_override(patch_path, rom_path)
    if direct_override is not None:
        group_name, option_name = direct_override
        _set_option(_get_option_group(group_name), option_name, os.path.abspath(rom_path))

    rom_group_aliases = {
        "donkey_kong_country": "dkc",
        "donkey_kong_country_2": "dkc2",
        "donkey_kong_country_3": "dkc3",
        "final_fantasy_tactics_advance": "ffta",
        "final_fantasy_v": "ffvcd",
        "kirbys_dream_land_3": "kdl3",
        "ladx": "ladx_beta",
        "link_s_awakening_dx": "ladx_beta",
        "links_awakening_dx": "ladx_beta",
        "links_awakening_dx_beta": "ladx_beta",
        "lufia_ii": "lufia2ac",
        "mario_luigi_superstar_saga": "mlss",
        "mega_man_2": "mm2",
        "mega_man_3": "mm3",
        "mega_man_x3": "mmx3",
        "metroid_fusion": "metroidfusion",
        "metroid_zero_mission": "mzm",
        "ocarina_of_time": "oot",
        "oracle_of_ages": "tloz_ooa",
        "oracle_of_seasons": "tloz_oos",
        "super_mario_land_2": "marioland2",
        "super_mario_sunshine": "sms",
        "super_mario_world": "smw",
        "super_metroid": "sm",
        "the_legend_of_zelda": "tloz",
        "the_minish_cap": "tmc",
        "thousand_year_door": "ttyd",
        "wario_land": "wl",
        "wario_land_4": "wl4",
        "zelda_ii": "zelda2",
    }

    for game_id, rom_path in roms.items():
        if not game_id or not rom_path or game_id in special:
            continue
        group_key = rom_group_aliases.get(game_id, game_id)
        group = _get_option_group(f"{group_key}_options")
        if group is None:
            group = _get_option_group(f"{group_key}_settings")
        if group is None:
            continue
        if isinstance(group, dict) and "rom_file" in group:
            _set_option(group, "rom_file", rom_path)
        elif hasattr(group, "rom_file"):
            _set_option(group, "rom_file", rom_path)


def main() -> int:
    parser = argparse.ArgumentParser(description="SekaiLink AP patch adapter")
    parser.add_argument("--patch", required=True, help="Path to .ap* patch file")
    parser.add_argument("--config", default="", help="Path to SekaiLink config.json")
    parser.add_argument("--out-dir", default="", help="Optional output directory")
    parser.add_argument("--ap-root", default=os.environ.get("SEKAILINK_AP_ROOT", ""), help="Bundled AP/MWGG root")
    parser.add_argument("--world-module", default="", help="AP world module to import, for example worlds.earthbound")
    parser.add_argument("--rom", default="", help="Base ROM path selected by SekaiLink")
    args = parser.parse_args()

    try:
        world_module = _import_patch_world(args.ap_root, args.patch, args.world_module)
        _install_headless_hooks(rom_path=args.rom, patch_path=args.patch)
        import Patch

        config = _load_config(args.config)
        _apply_rom_settings(config, patch_path=args.patch, rom_path=args.rom)

        meta, output_path = Patch.create_rom_file(args.patch)
        final_path = output_path

        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            final_path = os.path.join(args.out_dir, os.path.basename(output_path))
            if os.path.abspath(output_path) != os.path.abspath(final_path):
                shutil.copy2(output_path, final_path)

        _emit({
            "ok": True,
            "meta": meta,
            "output": final_path,
            "world_module": world_module,
        })
        return 0
    except Exception as exc:
        _emit({"ok": False, "error": str(exc), "traceback": traceback.format_exc(limit=20)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
