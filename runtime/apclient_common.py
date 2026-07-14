#!/usr/bin/env python3
"""SekaiLink JSON shim for Archipelago client runtimes.

This file intentionally wraps upstream Archipelago clients instead of replacing
their game logic.  SekaiLink talks to this process with one JSON object per line
on stdin and receives normalized JSON events on stdout.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import inspect
import importlib
import sys
import threading
import traceback
import types
from typing import Any, Callable

from apclient_bootstrap import (
    apply_scoped_world_manifest,
    install_headless_ap_hooks,
    install_scoped_worlds_package,
    install_socket_compat_properties,
    prepare_ap_path,
    refresh_scoped_world_data_package,
)
from apclient_events import (
    emit,
    install_json_logging,
    monitor_task,
    normalize_game_name_for_match,
    read_archipelago_patch_metadata,
    scrub_packet_for_trace,
    summarize_print_json_item_send,
    summarize_network_item,
    summarize_server_send,
)


def _is_universal_tracker_argv(argv: list[str]) -> bool:
    try:
        module_index = argv.index("--module")
    except ValueError:
        return False
    module = argv[module_index + 1] if module_index + 1 < len(argv) else ""
    return module == "worlds.tracker.TrackerClient"


if not _is_universal_tracker_argv(sys.argv) and "--nogui" not in sys.argv:
    sys.argv.append("--nogui")
os.environ.setdefault("KIVY_NO_ARGS", "1")
os.environ.setdefault("KIVY_NO_FILELOG", "1")
os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")
os.environ.setdefault("SEKAILINK_AP_WRAPPER", "1")


def stdin_queue(loop: asyncio.AbstractEventLoop) -> asyncio.Queue[dict[str, Any]]:
    queue: asyncio.Queue[dict[str, Any]] = asyncio.Queue()

    def reader() -> None:
        try:
            for line in sys.stdin:
                text = line.strip()
                if not text:
                    continue
                try:
                    payload = json.loads(text)
                    if not isinstance(payload, dict):
                        payload = {"cmd": "invalid", "raw": text}
                except json.JSONDecodeError:
                    payload = {"cmd": "command", "text": text}
                loop.call_soon_threadsafe(queue.put_nowait, payload)
        finally:
            loop.call_soon_threadsafe(queue.put_nowait, {"cmd": "shutdown"})

    threading.Thread(target=reader, name="SekaiLinkJsonStdin", daemon=True).start()
    return queue


def patch_context_events(ctx: Any) -> None:
    original_on_print = getattr(ctx, "on_print", None)
    original_on_print_json = getattr(ctx, "on_print_json", None)
    original_on_package = getattr(ctx, "on_package", None)
    original_connection_closed = getattr(ctx, "connection_closed", None)
    original_server_auth = getattr(ctx, "server_auth", None)
    original_send_msgs = getattr(ctx, "send_msgs", None)
    original_check_locations = getattr(ctx, "check_locations", None)
    original_event_invalid_slot = getattr(ctx, "event_invalid_slot", None)
    original_event_invalid_game = getattr(ctx, "event_invalid_game", None)
    sekailink_room_games: set[str] = set()
    patch_metadata = read_archipelago_patch_metadata(str(getattr(ctx, "patch_file", "") or ""))

    def emit_connection_refused(reason: str, errors: list[str]) -> None:
        emit(
            "connection_refused",
            reason=reason,
            errors=errors,
            auth=getattr(ctx, "auth", None),
            username=getattr(ctx, "username", None),
            game=getattr(ctx, "game", None),
            slot=getattr(ctx, "slot", None),
            team=getattr(ctx, "team", None),
        )
        exit_event = getattr(ctx, "exit_event", None)
        if exit_event is not None:
            try:
                exit_event.set()
            except Exception:
                pass

    def on_print(args: dict[str, Any]) -> None:
        if callable(original_on_print):
            original_on_print(args)
        emit("print", text=str(args.get("text", "")), raw=args)

    def on_print_json(args: dict[str, Any]) -> None:
        if callable(original_on_print_json):
            original_on_print_json(args)
        emit("print_json", data=args.get("data", []), raw=args)
        item_send = summarize_print_json_item_send(ctx, args)
        if item_send is not None:
            emit("item_send", **item_send)

    def on_package(cmd: str, args: dict[str, Any]) -> None:
        nonlocal sekailink_room_games
        if callable(original_on_package):
            original_on_package(cmd, args)
        compact = {"cmd": cmd}
        for key in ("seed_name", "slot", "team", "players", "missing_locations", "checked_locations"):
            if key in args:
                compact[key] = args[key]
        emit("package", **compact)
        if cmd == "RoomInfo":
            sekailink_room_games = {str(game) for game in (args.get("games") or [])}
            room_seed = args.get("seed_name")
            emit(
                "room_info",
                seed_name=room_seed,
                games=args.get("games", []),
                patch_metadata=patch_metadata,
            )
            patch_seed = patch_metadata.get("seed_name")
            if patch_seed and room_seed and str(patch_seed) != str(room_seed):
                emit(
                    "room_patch_mismatch",
                    room_seed=room_seed,
                    patch_seed=patch_seed,
                    patch_player_name=patch_metadata.get("player_name"),
                    patch_game=patch_metadata.get("game"),
                )
        elif cmd == "Connected":
            emit(
                "connected",
                slot=getattr(ctx, "slot", None),
                team=getattr(ctx, "team", None),
                game=getattr(ctx, "game", None),
            )
            slot_data = args.get("slot_data")
            if isinstance(slot_data, dict):
                emit(
                    "connected_slot_data",
                    game=getattr(ctx, "game", None),
                    slot=getattr(ctx, "slot", None),
                    slot_data=scrub_packet_for_trace(slot_data),
                )
        elif cmd == "ReceivedItems":
            items = list(args.get("items") or [])
            summaries = [summarize_network_item(ctx, item) for item in items[:40]]
            emit(
                "received_items",
                index=args.get("index"),
                count=len(items),
                items=summaries,
            )
        elif cmd == "PrintJSON":
            emit("chat_json", data=args.get("data", []), raw=scrub_packet_for_trace(args))
            item_send = summarize_print_json_item_send(ctx, args)
            if item_send is not None:
                emit("item_send", **item_send)

    async def connection_closed() -> None:
        if callable(original_connection_closed):
            await original_connection_closed()
        emit("disconnected")

    async def server_auth(password_requested: bool = False) -> Any:
        emit(
            "server_auth_start",
            password_requested=bool(password_requested),
            auth=getattr(ctx, "auth", None),
            username=getattr(ctx, "username", None),
            auth_status=str(getattr(ctx, "auth_status", "")),
        )
        if callable(original_server_auth):
            result = await original_server_auth(password_requested)
            emit(
                "server_auth_done",
                auth=getattr(ctx, "auth", None),
                username=getattr(ctx, "username", None),
                auth_status=str(getattr(ctx, "auth_status", "")),
            )
            return result
        emit("server_auth_missing")
        return None

    async def send_msgs(self: Any, messages: Any) -> Any:
        normalized_room_games = {
            normalize_game_name_for_match(game): game
            for game in sekailink_room_games
        }
        for message in list(messages or []):
            if not isinstance(message, dict) or message.get("cmd") != "Connect":
                continue
            requested_game = str(message.get("game") or "")
            matched_game = normalized_room_games.get(normalize_game_name_for_match(requested_game))
            if matched_game and matched_game != requested_game:
                emit(
                    "connect_game_alias_applied",
                    requested=requested_game,
                    room_game=matched_game,
                )
                message["game"] = matched_game
        summaries = summarize_server_send(ctx, messages)
        if summaries:
            emit("server_send", messages=summaries)
        if callable(original_send_msgs):
            return await original_send_msgs(messages)
        raise RuntimeError("send_msgs_unavailable")

    async def check_locations(self: Any, locations: Any) -> Any:
        requested = sorted(int(location) for location in (locations or []))
        missing = set(getattr(self, "missing_locations", set()) or set())
        would_send = sorted(location for location in requested if location in missing)
        emit(
            "check_locations_call",
            requested=requested,
            would_send=would_send,
            requested_count=len(requested),
            would_send_count=len(would_send),
            game=getattr(self, "game", None),
            slot=getattr(self, "slot", None),
        )
        if callable(original_check_locations):
            return await original_check_locations(locations)
        raise RuntimeError("check_locations_unavailable")

    def event_invalid_slot() -> Any:
        emit_connection_refused("invalid_slot", ["InvalidSlot"])
        if callable(original_event_invalid_slot):
            return original_event_invalid_slot()
        raise RuntimeError("Invalid Slot")

    def event_invalid_game() -> Any:
        emit_connection_refused("invalid_game", ["InvalidGame"])
        if callable(original_event_invalid_game):
            return original_event_invalid_game()
        raise RuntimeError("Invalid Game")

    ctx.on_print = on_print
    ctx.on_print_json = on_print_json
    ctx.on_package = on_package
    ctx.connection_closed = connection_closed
    if callable(original_server_auth):
        ctx.server_auth = server_auth
    if callable(original_send_msgs):
        ctx.send_msgs = types.MethodType(send_msgs, ctx)
    if callable(original_check_locations):
        ctx.check_locations = types.MethodType(check_locations, ctx)
    if callable(original_event_invalid_slot):
        ctx.event_invalid_slot = event_invalid_slot
    if callable(original_event_invalid_game):
        ctx.event_invalid_game = event_invalid_game
    emit(
        "wrapper_patch_active",
        context=type(ctx).__name__,
        send_msgs_hooked=callable(original_send_msgs),
        check_locations_hooked=callable(original_check_locations),
        refusal_hooks=bool(callable(original_event_invalid_slot) or callable(original_event_invalid_game)),
        patch_metadata=patch_metadata,
    )


async def bizhawk_auth_watchdog(ctx: Any, auth_status_enum: Any, connection_status_enum: Any) -> None:
    in_flight = False
    while not getattr(ctx, "exit_event").is_set():
        await asyncio.sleep(1.0)
        if in_flight:
            continue
        try:
            server = getattr(ctx, "server", None)
            socket = getattr(server, "socket", None)
            if server is None or socket is None:
                continue
            from CommonClient import sekailink_socket_closed

            if sekailink_socket_closed(socket):
                continue
            if getattr(ctx, "client_handler", None) is None:
                continue
            bizhawk_ctx = getattr(ctx, "bizhawk_ctx", None)
            if getattr(bizhawk_ctx, "connection_status", None) != connection_status_enum.CONNECTED:
                continue
            if getattr(ctx, "auth_status", None) != auth_status_enum.NOT_AUTHENTICATED:
                continue
            in_flight = True
            emit(
                "auth_watchdog_start",
                handler=getattr(getattr(ctx, "client_handler", None), "game", None),
                auth=getattr(ctx, "auth", None),
                username=getattr(ctx, "username", None),
            )
            await ctx.server_auth(getattr(ctx, "password_requested", False))
            emit(
                "auth_watchdog_done",
                auth=getattr(ctx, "auth", None),
                username=getattr(ctx, "username", None),
                auth_status=str(getattr(ctx, "auth_status", "")),
            )
        except Exception as exc:
            emit("auth_watchdog_error", error=str(exc), traceback=traceback.format_exc(limit=8))
        finally:
            in_flight = False


async def bizhawk_handler_trace_watchdog(ctx: Any) -> None:
    """Wrap the selected BizHawk APWorld watcher once it is discovered.

    This is intentionally wrapper-side diagnostics: it does not change the
    APWorld logic, but it makes silent watcher exits visible in SekaiLink logs.
    """
    wrapped_handler_id: int | None = None
    while not getattr(ctx, "exit_event").is_set():
        await asyncio.sleep(0.25)
        handler = getattr(ctx, "client_handler", None)
        if handler is None:
            continue
        if id(handler) == wrapped_handler_id or getattr(handler, "_sekailink_watcher_traced", False):
            continue
        original = getattr(handler, "game_watcher", None)
        if not callable(original):
            continue
        wrapped_handler_id = id(handler)
        setattr(handler, "_sekailink_watcher_traced", True)
        last_emit = 0.0

        async def traced_game_watcher(inner_ctx: Any, _original: Any = original, _handler: Any = handler) -> Any:
            nonlocal last_emit
            try:
                import time

                now = time.monotonic()
                if now - last_emit >= 5.0:
                    last_emit = now
                    emit(
                        "bizhawk_watcher_tick",
                        handler=getattr(_handler, "game", type(_handler).__name__),
                        game=getattr(inner_ctx, "game", None),
                        slot=getattr(inner_ctx, "slot", None),
                        received_count=len(getattr(inner_ctx, "items_received", []) or []),
                        local_checked_count=len(getattr(inner_ctx, "locations_checked", []) or []),
                        handler_local_checked_count=len(getattr(_handler, "local_checked_locations", []) or []),
                        server_missing_count=len(getattr(inner_ctx, "missing_locations", []) or []),
                        server_checked_count=len(getattr(inner_ctx, "checked_locations", []) or []),
                    )
                return await _original(inner_ctx)
            except Exception as exc:
                emit(
                    "bizhawk_watcher_error",
                    handler=getattr(_handler, "game", type(_handler).__name__),
                    error=str(exc),
                    traceback=traceback.format_exc(limit=12),
                )
                raise

        setattr(handler, "game_watcher", traced_game_watcher)
        emit("bizhawk_watcher_trace_installed", handler=getattr(handler, "game", type(handler).__name__))


async def command_loop(ctx: Any, queue: asyncio.Queue[dict[str, Any]], command_processor_factory: Callable[[Any], Any]) -> None:
    processor = command_processor_factory(ctx)
    while not getattr(ctx, "exit_event").is_set():
        payload = await queue.get()
        cmd = str(payload.get("cmd", "")).strip().lower()
        try:
            if cmd == "shutdown":
                getattr(ctx, "exit_event").set()
                return
            if cmd == "connect":
                address = str(payload.get("address") or payload.get("server") or "").strip()
                slot = str(payload.get("slot") or payload.get("name") or "").strip()
                password = payload.get("password")
                if slot:
                    ctx.username = slot
                    if getattr(ctx, "auth", None) in (None, ""):
                        ctx.auth = slot
                if password is not None:
                    ctx.password = str(password)
                await ctx.connect(address or None)
                emit("command_ok", cmd="connect", address=address)
                continue
            if cmd == "disconnect":
                await ctx.disconnect()
                emit("command_ok", cmd="disconnect")
                continue
            if cmd == "say":
                text = str(payload.get("text") or "")
                if text:
                    processor.default(text)
                continue
            if cmd == "command":
                text = str(payload.get("text") or payload.get("command") or "")
                if text:
                    processor(text)
                continue
            emit("command_error", cmd=cmd or "missing", error="unknown_command")
        except Exception as exc:
            logging.getLogger("SekaiLink").exception("command failed")
            emit("command_error", cmd=cmd, error=str(exc))


async def run_text_client(args: argparse.Namespace) -> None:
    from CommonClient import ClientCommandProcessor, CommonContext, server_loop

    install_socket_compat_properties()

    class SekaiLinkTextContext(CommonContext):
        tags = CommonContext.tags | {"TextOnly", "SekaiLink"}
        game = ""
        items_handling = 0b111
        want_slot_data = False

        async def server_auth(self, password_requested: bool = False):
            if password_requested and not self.password:
                await super().server_auth(password_requested)
            await self.get_username()
            await self.send_connect(game="")

        def on_package(self, cmd: str, packet: dict[str, Any]):
            if cmd == "Connected":
                self.game = self.slot_info[self.slot].game

        async def disconnect(self, allow_autoreconnect: bool = False):
            self.game = ""
            await super().disconnect(allow_autoreconnect)

    ctx = SekaiLinkTextContext(args.connect, args.password)
    ctx.auth = args.slot or None
    patch_context_events(ctx)
    queue = stdin_queue(asyncio.get_running_loop())
    ctx.server_task = monitor_task(asyncio.create_task(server_loop(ctx), name="ServerLoop"), "ServerLoop")
    command_task = monitor_task(asyncio.create_task(command_loop(ctx, queue, ClientCommandProcessor), name="SekaiLinkCommandLoop"), "SekaiLinkCommandLoop")
    emit("ready", kind="text")
    await ctx.exit_event.wait()
    command_task.cancel()
    await ctx.shutdown()


async def run_bizhawk_client(args: argparse.Namespace) -> None:
    ap_root = prepare_ap_path()
    if args.client_module and args.client_module.startswith("worlds."):
        install_scoped_worlds_package(ap_root)
        importlib.import_module(args.client_module)
        apply_scoped_world_manifest(ap_root, args.client_module)
        refresh_scoped_world_data_package()
        emit("bizhawk_client_module_loaded", module=args.client_module)
    else:
        import worlds  # noqa: F401 - import all bundled/custom worlds to register BizHawk handlers
    from worlds._bizhawk.context import AuthStatus, BizHawkClientCommandProcessor, BizHawkClientContext, ConnectionStatus, _game_watcher
    from CommonClient import server_loop

    install_socket_compat_properties()

    ctx = BizHawkClientContext(args.connect, args.password)
    ctx.username = args.slot or None
    ctx.auth = args.slot or None
    if args.rom:
        ctx.rom_file = os.path.abspath(args.rom)
    if args.patch:
        ctx.patch_file = os.path.abspath(args.patch)
    patch_context_events(ctx)
    queue = stdin_queue(asyncio.get_running_loop())
    ctx.server_task = monitor_task(asyncio.create_task(server_loop(ctx), name="ServerLoop"), "ServerLoop")
    watcher_task = monitor_task(asyncio.create_task(_game_watcher(ctx), name="BizHawkWatcher"), "BizHawkWatcher")
    auth_watchdog_task = monitor_task(asyncio.create_task(
        bizhawk_auth_watchdog(ctx, AuthStatus, ConnectionStatus),
        name="SekaiLinkBizHawkAuthWatchdog",
    ), "SekaiLinkBizHawkAuthWatchdog")
    handler_trace_task = monitor_task(asyncio.create_task(
        bizhawk_handler_trace_watchdog(ctx),
        name="SekaiLinkBizHawkHandlerTrace",
    ), "SekaiLinkBizHawkHandlerTrace")
    command_task = monitor_task(asyncio.create_task(command_loop(ctx, queue, BizHawkClientCommandProcessor), name="SekaiLinkCommandLoop"), "SekaiLinkCommandLoop")
    emit("ready", kind="bizhawk")
    await ctx.exit_event.wait()
    command_task.cancel()
    watcher_task.cancel()
    auth_watchdog_task.cancel()
    handler_trace_task.cancel()
    await ctx.shutdown()


async def run_sni_client(args: argparse.Namespace) -> None:
    ap_root = prepare_ap_path()
    if args.client_module and args.client_module.startswith("worlds."):
        install_scoped_worlds_package(ap_root)
        importlib.import_module(args.client_module)
        apply_scoped_world_manifest(ap_root, args.client_module)
        refresh_scoped_world_data_package()
        emit("sni_client_module_loaded", module=args.client_module)
    else:
        import worlds  # noqa: F401 - fallback: import bundled worlds when no explicit client is provided
    from CommonClient import server_loop
    from SNIClient import SNIClientCommandProcessor, SNIContext, game_watcher

    install_socket_compat_properties()

    ctx = SNIContext(args.sni_address, args.connect, args.password)
    ctx.username = args.slot or None
    if args.rom:
        ctx.rom_file = os.path.abspath(args.rom)
    if args.patch:
        ctx.patch_file = os.path.abspath(args.patch)
    patch_context_events(ctx)
    queue = stdin_queue(asyncio.get_running_loop())
    ctx.server_task = monitor_task(asyncio.create_task(server_loop(ctx), name="ServerLoop"), "ServerLoop")
    watcher_task = monitor_task(asyncio.create_task(game_watcher(ctx), name="SNIWatcher"), "SNIWatcher")
    command_task = monitor_task(asyncio.create_task(command_loop(ctx, queue, SNIClientCommandProcessor), name="SekaiLinkCommandLoop"), "SekaiLinkCommandLoop")
    SNIClientCommandProcessor(ctx).connect_to_snes(args.sni_device)
    emit("ready", kind="sni", sni_address=args.sni_address)
    await ctx.exit_event.wait()
    SNIClientCommandProcessor(ctx)._cmd_snes_close()
    if ctx.snes_connect_task:
        ctx.snes_connect_task.cancel()
    if ctx.snes_autoreconnect_task:
        ctx.snes_autoreconnect_task.cancel()
    command_task.cancel()
    watcher_task.cancel()
    await ctx.shutdown()


async def run_oot_client(args: argparse.Namespace) -> None:
    from CommonClient import server_loop
    from worlds.oot.Client import OoTCommandProcessor, OoTContext, n64_sync_task

    install_socket_compat_properties()

    ctx = OoTContext(args.connect, args.password)
    ctx.username = args.slot or None
    if args.rom:
        ctx.rom_file = os.path.abspath(args.rom)
    if args.patch:
        ctx.patch_file = os.path.abspath(args.patch)
    patch_context_events(ctx)
    queue = stdin_queue(asyncio.get_running_loop())
    ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")
    sync_task = asyncio.create_task(n64_sync_task(ctx), name="N64Sync")
    command_task = asyncio.create_task(command_loop(ctx, queue, OoTCommandProcessor), name="SekaiLinkCommandLoop")
    emit("ready", kind="oot")
    await ctx.exit_event.wait()
    command_task.cancel()
    sync_task.cancel()
    await ctx.shutdown()


async def run_module_main_client(args: argparse.Namespace) -> None:
    """Run a game-specific AP client module that owns its own transport.

    This is primarily for Dolphin-style clients.  They are still upstream AP
    clients, but unlike BizHawk/SNI they usually ship as one module with its own
    CommonContext subclass and process hook.
    """
    install_socket_compat_properties()

    if not args.module:
        raise RuntimeError("module_required")
    ap_root = prepare_ap_path()
    if args.module.startswith("worlds."):
        install_scoped_worlds_package(ap_root)
    module = importlib.import_module(args.module)
    if args.module.startswith("worlds."):
        apply_scoped_world_manifest(ap_root, args.module)
        refresh_scoped_world_data_package()
    entry = getattr(module, "launch", None) or getattr(module, "main", None) or getattr(module, "run", None)
    if not callable(entry):
        raise RuntimeError(f"module_has_no_entrypoint:{args.module}")
    uses_universal_tracker = args.module == "worlds.tracker.TrackerClient"
    launch_args: list[str] = [] if uses_universal_tracker else ["--nogui"]
    if args.connect:
        launch_args.extend(["--connect", args.connect])
    if args.password:
        launch_args.extend(["--password", args.password])
    if args.slot:
        launch_args.extend(["--name", args.slot])
    if args.patch:
        launch_args.append(args.patch)
    emit("ready", kind=args.kind, module=args.module)
    # These upstream clients own their loop. SekaiLink keeps them headless and
    # supervises stdout/stderr at process level.
    signature = inspect.signature(entry)
    parameters = list(signature.parameters.values())
    has_varargs = any(param.kind == inspect.Parameter.VAR_POSITIONAL for param in parameters)
    positional = [
        param for param in parameters
        if param.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD)
        and param.default is inspect.Parameter.empty
    ]
    if has_varargs:
        await asyncio.to_thread(entry, *launch_args)
        return
    if len(positional) == 0:
        old_argv = sys.argv[:]
        def _call_with_argv() -> None:
            try:
                sys.argv = [old_argv[0], *launch_args]
                entry()
            finally:
                sys.argv = old_argv
        await asyncio.to_thread(_call_with_argv)
        return
    if len(positional) == 2 and [param.name for param in positional] == ["connect", "password"]:
        await asyncio.to_thread(entry, args.connect, args.password)
        return
    raise RuntimeError(f"unsupported_module_entrypoint:{args.module}:{signature}")


async def async_main(argv: list[str] | None = None) -> int:
    prepare_ap_path()
    install_json_logging()
    import Utils
    import colorama

    parser = argparse.ArgumentParser(description="SekaiLink Archipelago client JSON wrapper")
    parser.add_argument("--kind", choices=["text", "bizhawk", "sni", "oot", "dolphin", "module"], required=True)
    parser.add_argument("--module", default="")
    parser.add_argument("--client-module", default="")
    parser.add_argument("--connect", default=None)
    parser.add_argument("--password", default=None)
    parser.add_argument("--slot", default=None)
    parser.add_argument("--sni-address", default="ws://127.0.0.1:23074")
    parser.add_argument("--sni-device", default="")
    parser.add_argument("--patch", default="")
    parser.add_argument("--rom", default="")
    parser.add_argument("--nogui", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    if not (args.kind == "module" and args.module == "worlds.tracker.TrackerClient"):
        install_headless_ap_hooks(Utils, rom_path=args.rom, patch_path=args.patch)

    Utils.init_logging(f"SekaiLink-{args.kind}-Client", exception_logger="Client")
    install_json_logging()
    colorama.just_fix_windows_console()
    try:
        if args.kind == "text":
            await run_text_client(args)
        elif args.kind == "bizhawk":
            await run_bizhawk_client(args)
        elif args.kind == "sni":
            await run_sni_client(args)
        elif args.kind == "oot":
            await run_oot_client(args)
        elif args.kind in ("dolphin", "module"):
            await run_module_main_client(args)
        return 0
    except asyncio.CancelledError:
        return 0
    except Exception as exc:
        logging.getLogger("SekaiLink").exception("wrapper crashed")
        emit("fatal", error=str(exc), traceback=traceback.format_exc(limit=40))
        return 1
    finally:
        colorama.deinit()


def main(kind: str | None = None) -> None:
    argv = list(sys.argv[1:])
    if kind and "--kind" not in argv:
        argv = ["--kind", kind, *argv]
    if not argv:
        argv = None
    raise SystemExit(asyncio.run(async_main(argv)))


if __name__ == "__main__":
    main()
