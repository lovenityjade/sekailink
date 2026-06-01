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
import types
import warnings
import zipfile
from typing import Any, Dict

os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
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
    pkg = types.ModuleType("worlds")
    pkg.__path__ = [worlds_dir]  # type: ignore[attr-defined]
    pkg.__package__ = "worlds"
    pkg.__sekailink_scoped__ = True  # type: ignore[attr-defined]
    sys.modules["worlds"] = pkg


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
    return module_name


def _apply_rom_settings(config: Dict[str, Any]) -> None:
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
            container[key] = value
            return
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

    for game_id, rom_path in roms.items():
        if not game_id or not rom_path or game_id in special:
            continue
        group = _get_option_group(f"{game_id}_options")
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
    args = parser.parse_args()

    try:
        world_module = _import_patch_world(args.ap_root, args.patch, args.world_module)
        import Patch

        config = _load_config(args.config)
        _apply_rom_settings(config)

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
        _emit({"ok": False, "error": str(exc)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
