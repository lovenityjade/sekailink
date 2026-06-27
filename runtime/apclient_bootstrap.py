from __future__ import annotations

import importlib
import json
import logging
import os
import sys
import traceback
import types
import webbrowser
import zipfile
from typing import Any

from apclient_events import emit


def install_socket_compat_properties() -> None:
    """Expose legacy `.open` / `.closed` on newer websocket connection classes."""
    try:
        candidates: list[type[Any]] = []
        for dotted in (
            "websockets.asyncio.client.ClientConnection",
            "websockets.asyncio.server.ServerConnection",
            "websockets.asyncio.connection.Connection",
            "websockets.legacy.client.WebSocketClientProtocol",
            "websockets.legacy.server.WebSocketServerProtocol",
        ):
            try:
                module_name, class_name = dotted.rsplit(".", 1)
                current = getattr(importlib.import_module(module_name), class_name)
            except Exception:
                continue
            if isinstance(current, type):
                candidates.append(current)

        def is_closed(self: Any) -> bool:
            state = getattr(self, "state", None)
            return bool(state is not None and str(state).upper().endswith("CLOSED"))

        def is_open(self: Any) -> bool:
            return not is_closed(self)

        patched: list[str] = []
        for cls in candidates:
            if not hasattr(cls, "closed"):
                setattr(cls, "closed", property(is_closed))
                patched.append(f"{cls.__module__}.{cls.__name__}.closed")
            if not hasattr(cls, "open"):
                setattr(cls, "open", property(is_open))
                patched.append(f"{cls.__module__}.{cls.__name__}.open")
        if patched:
            emit("websocket_compat_patch_active", patched=patched)
    except Exception as exc:
        emit("websocket_compat_patch_failed", error=str(exc), traceback=traceback.format_exc(limit=6))


def prepare_ap_path() -> str:
    ap_root = os.environ.get("SEKAILINK_AP_ROOT", "")
    if not ap_root:
        ap_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ap")
    if ap_root and ap_root not in sys.path:
        sys.path.insert(0, ap_root)
    return ap_root


def install_scoped_worlds_package(ap_root: str) -> None:
    worlds_dir = os.path.join(ap_root, "worlds")
    if not os.path.isdir(worlds_dir):
        raise FileNotFoundError(f"ap_worlds_missing:{worlds_dir}")
    existing = sys.modules.get("worlds")
    if existing is not None and getattr(existing, "__sekailink_scoped__", False):
        return
    world_paths = [worlds_dir]
    for entry in os.scandir(worlds_dir):
        if entry.is_file() and entry.name.endswith(".apworld"):
            world_paths.append(entry.path)
    pkg = types.ModuleType("worlds")
    pkg.__path__ = world_paths  # type: ignore[attr-defined]
    pkg.__package__ = "worlds"
    pkg.__sekailink_scoped__ = True  # type: ignore[attr-defined]
    pkg.local_folder = worlds_dir  # type: ignore[attr-defined]
    pkg.user_folder = None  # type: ignore[attr-defined]
    pkg.failed_world_loads = []  # type: ignore[attr-defined]
    pkg.network_data_package = {"games": {}}  # type: ignore[attr-defined]
    pkg.network_data_package_single_game = {}  # type: ignore[attr-defined]
    sys.modules["worlds"] = pkg
    from worlds.AutoWorld import AutoWorldRegister
    pkg.AutoWorldRegister = AutoWorldRegister  # type: ignore[attr-defined]


def apply_scoped_world_manifest(ap_root: str, module_name: str) -> None:
    parts = module_name.split(".")
    if len(parts) < 2 or parts[0] != "worlds":
        return
    world_name = parts[1]
    manifest_path = os.path.join(ap_root, "worlds", world_name, "archipelago.json")
    manifest = {}
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception:
        apworld_path = os.path.join(ap_root, "worlds", f"{world_name}.apworld")
        try:
            with zipfile.ZipFile(apworld_path) as zf:
                with zf.open(f"{world_name}/archipelago.json") as f:
                    manifest = json.load(f)
        except Exception:
            return
    game = str(manifest.get("game") or "").strip()
    world_version = str(manifest.get("world_version") or "").strip()
    if not game or not world_version:
        return
    try:
        from Utils import tuplize_version
        from worlds.AutoWorld import AutoWorldRegister
        world_type = AutoWorldRegister.world_types.get(game)
        if world_type is not None:
            world_type.world_version = tuplize_version(world_version)
    except Exception:
        return


def refresh_scoped_world_data_package() -> None:
    pkg = sys.modules.get("worlds")
    if pkg is None or not getattr(pkg, "__sekailink_scoped__", False):
        return
    try:
        from worlds.AutoWorld import AutoWorldRegister
        games = {
            world_name: world.get_data_package_data()
            for world_name, world in AutoWorldRegister.world_types.items()
        }
        pkg.network_data_package["games"].clear()  # type: ignore[attr-defined]
        pkg.network_data_package["games"].update(games)  # type: ignore[attr-defined]
        pkg.network_data_package_single_game.clear()  # type: ignore[attr-defined]
        pkg.network_data_package_single_game.update({  # type: ignore[attr-defined]
            game_name: {"games": {game_name: pkg_data}}
            for game_name, pkg_data in games.items()
        })
    except Exception as exc:
        emit("scoped_datapackage_failed", error=str(exc), traceback=traceback.format_exc(limit=6))


def install_headless_ap_hooks(Utils: Any, *, rom_path: str = "", patch_path: str = "") -> None:
    """Disable upstream GUI escape hatches inside wrapped AP clients."""
    Utils.gui_enabled = False
    rom_path = os.path.abspath(rom_path) if rom_path else ""
    patch_path = os.path.abspath(patch_path) if patch_path else ""

    def messagebox(title: str, text: str, error: bool = False) -> None:
        emit("messagebox", title=str(title), text=str(text), error=bool(error))
        logger = logging.getLogger("SekaiLink")
        if error:
            logger.error("%s: %s", title, text)
        else:
            logger.info("%s: %s", title, text)

    def choose_headless_file(kind: str, title: str = "", suggest: str = "", filetypes: Any = None) -> str | None:
        title_l = str(title or "").lower()
        suggest_l = str(suggest or "").lower()
        filetypes_s = str(filetypes or "").lower()
        wants_patch = "patch" in title_l or "patch" in suggest_l or ".ap" in suggest_l or ".ap" in filetypes_s or "multiworld" in title_l
        wants_rom = "rom" in title_l or "game" in title_l or any(
            ext in filetypes_s for ext in (".sfc", ".smc", ".nes", ".gb", ".gbc", ".gba", ".z64", ".n64", ".iso", ".rvz")
        )
        selected = ""
        reason = "no_headless_file_available"
        if wants_patch and patch_path:
            selected = patch_path
            reason = "using_supplied_patch"
        elif wants_rom and rom_path:
            selected = rom_path
            reason = "using_supplied_rom"
        elif patch_path:
            selected = patch_path
            reason = "using_supplied_patch_fallback"
        elif rom_path:
            selected = rom_path
            reason = "using_supplied_rom_fallback"

        if selected:
            emit("headless_file", kind=kind, title=str(title or ""), suggest=str(suggest or ""), path=selected, detail=reason)
            return selected

        emit("headless_blocked", kind=kind, title=str(title or ""), suggest=str(suggest or ""), detail=reason)
        return None

    def blocked_open_file(filename: Any) -> None:
        emit("headless_blocked", kind="open_file", path=str(filename or ""), detail="upstream_open_file_blocked")

    def blocked_browser(url: str, *args: Any, **kwargs: Any) -> bool:
        emit("headless_blocked", kind="webbrowser", url=str(url or ""), detail="upstream_browser_open_blocked")
        return False

    Utils.messagebox = messagebox
    Utils.open_filename = lambda title, filetypes, suggest="": choose_headless_file("open_filename", title, suggest, filetypes)
    Utils.save_filename = lambda title, filetypes, suggest="": choose_headless_file("save_filename", title, suggest, filetypes)
    Utils.open_directory = lambda title, suggest="": choose_headless_file("open_directory", title, suggest)
    Utils.open_file = blocked_open_file
    webbrowser.open = blocked_browser
