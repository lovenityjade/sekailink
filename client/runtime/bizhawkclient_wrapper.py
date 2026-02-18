#!/usr/bin/env python3
"""
Headless BizHawkClient wrapper for the SekaiLink Electron app.

Why:
- CommonClient alone is not enough for BizHawk/Lua-based games (it does not talk to the emulator).
- The upstream BizHawk client context already implements emulator connection + per-game handlers.

Contract:
- Receives JSON commands on stdin (one JSON object per line)
- Emits JSON events on stdout (one JSON object per line)

Notes:
- We intentionally avoid ModuleUpdate.update() here. Dependency management is handled at packaging time.
- We import `worlds` to register BizHawk handlers via import side-effects (AutoBizHawkClientRegister).
"""

from __future__ import annotations

import argparse
import asyncio
import copy
import json
import logging
import os
import sys
from typing import Any, Dict, Optional

# Prevent any optional runtime pip/update flows from triggering inside the packaged client.
os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")

# Importing worlds registers BizHawk handlers via AutoBizHawkClientRegister.
# This can be expensive, but it's the most reliable MVP path without maintaining a mapping table.
try:
    import worlds  # noqa: F401
except Exception:
    # World import failures are non-fatal; the wrapper will still run and report "no handler" when appropriate.
    pass

from CommonClient import server_loop, logger as client_logger, websocket_is_open  # noqa: E402
from NetUtils import ClientStatus  # noqa: E402
from Utils import async_start  # noqa: E402
from worlds._bizhawk import ConnectionStatus  # noqa: E402
from worlds._bizhawk import display_message  # noqa: E402
from worlds._bizhawk import disconnect as bizhawk_disconnect  # noqa: E402
from worlds._bizhawk.context import (  # noqa: E402
    AuthStatus,
    BizHawkClientContext,
    TextCategory,
    _game_watcher,
    logger,
)

_SLOT_DATA_FALLBACKS: Dict[str, Any] = {
    # Common compatibility keys for BizHawk worlds when slot_data is partial/missing.
    # Keep defaults conservative (feature-off / empty) to avoid crashes.
    "remote_items": 0,
    "goal": 0,
    "completion_goal": 0,
    "final_mission": 0,
    "law_cards": 0,
    "job_unlock_req": 0,
    "magical_seeds": 0,
    "iron_maiden_behavior": 0,
    "provide_hints": 0,
    "provide_shop_hints": 0,
    "famesanity": 0,
    "shopsanity": 0,
    "vending_machines": 0,
    "prizesanity": 0,
    "decouple_entrances_warps": 0,
    "skip_tutorials": 0,
    "energy_link": 0,
    "Badges": 0,
    "Cannons": 0,
    "ProgressiveKeys": 0,
    "sr3.5": 0,
    "sr6.25": 0,
    "johto_only": 0,
    "death_link": 0,
    "death_link_gameover": 0,
    "legendary_hunt_catch": 0,
    "legendary_hunt_count": 0,
    "dexsanity": 0,
    "options": {},
    "grass_location_mapping": {},
    "trap_locations": {},
    "trap_weights": {},
    "entrances": [],
    "allowed_legendary_hunt_encounters": [],
    "dexcountsanity_counts": [],
    "version": "",
}


def _emit(event: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(event, ensure_ascii=True) + "\n")
    sys.stdout.flush()


class _IPCLogHandler(logging.Handler):
    def emit(self, record: logging.LogRecord) -> None:
        # Keep the noise down; forward the loggers players care about.
        # NOTE:
        # - "FileLog" and "StreamLog" carry the same PrintJSON payload in different formats.
        #   Keep only FileLog to avoid duplicated lines in the UI/debug snapshot.
        if record.name not in ("Client", "FileLog", "BizHawkClient"):
            return
        msg = self.format(record)
        _emit(
            {
                "event": "log",
                "level": record.levelname.lower(),
                "logger": record.name,
                "message": msg,
            }
        )


class HeadlessBizHawkContext(BizHawkClientContext):
    def __init__(self, server_address: Optional[str] = None, password: Optional[str] = None) -> None:
        super().__init__(server_address, password)
        self._input_futures: Dict[str, asyncio.Future] = {}
        self.text_passthrough_categories.update(
            (
                TextCategory.INCOMING,
                TextCategory.OUTGOING,
                TextCategory.CHAT,
            )
        )

    def _categorize_text(self, args: dict) -> TextCategory:
        # Treat AP "ServerChat" as chatroom text instead of generic server logs.
        if args.get("type") == "ServerChat":
            return TextCategory.CHAT
        return super()._categorize_text(args)

    async def _request_value(self, key: str, message: str) -> str:
        if key not in self._input_futures or self._input_futures[key].done():
            self._input_futures[key] = asyncio.get_event_loop().create_future()
        _emit({"event": "request", "type": key, "message": message})
        return await self._input_futures[key]

    def provide_value(self, key: str, value: str) -> None:
        fut = self._input_futures.get(key)
        if fut and not fut.done():
            fut.set_result(value)

    async def server_auth(self, password_requested: bool = False):
        # Copy of upstream logic, except that we request required values via IPC rather than stdin GUI/CLI.
        self.password_requested = password_requested

        if self.bizhawk_ctx.connection_status != ConnectionStatus.CONNECTED:
            logger.info("Awaiting connection to BizHawk before authenticating")
            return

        if self.client_handler is None:
            return

        if self.auth is None:
            self.auth_status = AuthStatus.NEED_INFO
            await self.client_handler.set_auth(self)
            if self.auth is None:
                await self.get_username()

        if password_requested and not self.password:
            self.auth_status = AuthStatus.NEED_INFO
            self.password = await self._request_value("password", "Password required")

        await self.send_connect()
        self.auth_status = AuthStatus.PENDING

    async def get_username(self):
        if not self.auth:
            self.auth = self.username
            if not self.auth:
                self.auth = await self._request_value("slot", "Slot name required")

    async def connection_closed(self):
        await super().connection_closed()
        _emit({"event": "status", "server": "disconnected"})

    def handle_connection_loss(self, msg: str) -> None:
        _emit({"event": "error", "message": msg})
        if self.bizhawk_ctx.connection_status == ConnectionStatus.CONNECTED:
            async_start(display_message(self.bizhawk_ctx, f"[error] {msg}"))
        super().handle_connection_loss(msg)

    def on_print_json(self, args: dict):
        # Forward PrintJSON to the UI for rendering (hints, item sends, etc.)
        # Also include a parsed plain-text fallback so the UI doesn't need the datapackage.
        try:
            data = args.get("data", []) or []
            text = self.rawjsontotextparser(copy.deepcopy(data)) if data else ""
        except Exception:
            data = args.get("data", []) or []
            text = ""

        _emit(
            {
                "event": "print_json",
                "type": args.get("type", ""),
                "text": text,
                "data": data,
            }
        )
        super().on_print_json(args)

    def on_package(self, cmd: str, args: dict):
        # Preserve upstream behavior, but mirror key state transitions to the UI.
        if cmd == "Connected":
            _emit({"event": "status", "server": "connected"})
        elif cmd == "RoomInfo":
            _emit(
                {
                    "event": "room_info",
                    "seed_name": args.get("seed_name"),
                    "password": bool(args.get("password")),
                    "players": args.get("players", []),
                    "hint_cost": args.get("hint_cost"),
                    "location_check_points": args.get("location_check_points"),
                }
            )
        elif cmd == "RoomUpdate":
            if "hint_points" in args:
                _emit({"event": "status", "hint_points": args.get("hint_points")})
            if "permissions" in args:
                _emit({"event": "status", "permissions": args.get("permissions")})
        super().on_package(cmd, args)


async def _command_loop(ctx: HeadlessBizHawkContext, queue: "asyncio.Queue[dict]") -> None:
    while not ctx.exit_event.is_set():
        cmd = await queue.get()
        if not cmd:
            continue

        name = cmd.get("cmd")
        if name == "connect":
            address = cmd.get("address")
            slot = cmd.get("slot")
            password = cmd.get("password")

            if address:
                ctx.server_address = address
            if slot:
                ctx.username = slot
                ctx.auth = slot
            if password is not None:
                ctx.password = password

            if ctx.server_task is None:
                ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")

        elif name == "disconnect":
            async_start(ctx.disconnect(), name="disconnect")

        elif name == "ready":
            value = cmd.get("value")
            if value is None:
                ctx.ready = not ctx.ready
            else:
                ctx.ready = bool(value)
            status = ClientStatus.CLIENT_READY if ctx.ready else ClientStatus.CLIENT_CONNECTED
            async_start(ctx.send_msgs([{"cmd": "StatusUpdate", "status": status}]), name="send StatusUpdate")
            _emit({"event": "status", "ready": ctx.ready})

        elif name == "say":
            text = cmd.get("text", "")
            if text:
                async_start(ctx.send_msgs([{"cmd": "Say", "text": text}]), name="send Say")

        elif name == "toggle_text":
            # Convenience for the BizHawk-specific /toggle_text command.
            category = str(cmd.get("category") or "").strip().lower()
            toggle = str(cmd.get("toggle") or "").strip().lower()
            if not category:
                _emit({"event": "error", "message": "Missing category for toggle_text"})
                continue

            value: bool | None
            if not toggle:
                value = None
            elif toggle in ("on", "true"):
                value = True
            elif toggle in ("off", "false"):
                value = False
            else:
                _emit({"event": "error", "message": f"Invalid toggle: {toggle}"})
                continue

            valid = {c.value for c in TextCategory}
            if category not in valid:
                _emit({"event": "error", "message": f"Invalid category: {category}"})
                continue

            if category == TextCategory.ALL:
                if value is None:
                    _emit({"event": "error", "message": 'Must specify "on" or "off" for category "all"'})
                    continue
                if value:
                    ctx.text_passthrough_categories.update(
                        (
                            TextCategory.OTHER,
                            TextCategory.INCOMING,
                            TextCategory.OUTGOING,
                            TextCategory.HINT,
                            TextCategory.CHAT,
                            TextCategory.SERVER,
                        )
                    )
                else:
                    ctx.text_passthrough_categories.clear()
            else:
                if value is None:
                    value = category not in ctx.text_passthrough_categories
                if value:
                    ctx.text_passthrough_categories.add(category)
                else:
                    ctx.text_passthrough_categories.discard(category)

            _emit({"event": "status", "text_passthrough": sorted(ctx.text_passthrough_categories)})

        elif name == "set_password":
            ctx.password = cmd.get("value", "")
            ctx.provide_value("password", ctx.password)

        elif name == "set_slot":
            ctx.username = cmd.get("value", "")
            ctx.auth = ctx.username
            ctx.provide_value("slot", ctx.username)

        elif name == "shutdown":
            ctx.exit_event.set()
            await ctx.shutdown()
            return


def _stdin_reader(loop: asyncio.AbstractEventLoop, queue: "asyncio.Queue[dict]") -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            _emit({"event": "error", "message": "Invalid JSON command"})
            continue
        asyncio.run_coroutine_threadsafe(queue.put(data), loop)


async def _status_poller(ctx: HeadlessBizHawkContext) -> None:
    """Emit periodic status snapshots for the UI (best-effort)."""
    last = {
        "server": None,
        "bizhawk": None,
        "handler": None,
        "auth": None,
    }
    while not ctx.exit_event.is_set():
        try:
            server = "connected" if (ctx.server is not None and websocket_is_open(ctx.server.socket)) else "disconnected"
            bizhawk = str(int(ctx.bizhawk_ctx.connection_status))  # stable, but not user-friendly
            handler = getattr(ctx.client_handler, "game", None) if ctx.client_handler else None
            auth = int(ctx.auth_status) if hasattr(ctx, "auth_status") else None

            # Convert bizhawk enum to friendly string.
            if ctx.bizhawk_ctx.connection_status == ConnectionStatus.NOT_CONNECTED:
                bizhawk_label = "not_connected"
            elif ctx.bizhawk_ctx.connection_status == ConnectionStatus.TENTATIVE:
                bizhawk_label = "tentative"
            else:
                bizhawk_label = "connected"

            snapshot = {
                "server": server,
                "bizhawk": bizhawk_label,
                "handler": handler or "",
                "auth_status": auth,
            }
            if any(snapshot.get(k) != last.get(k) for k in ("server", "bizhawk", "handler", "auth_status")):
                last.update(snapshot)
                _emit({"event": "emu_status", **snapshot})
        except Exception:
            # never crash the client for a status poll
            pass
        await asyncio.sleep(0.5)


def _recover_missing_slot_data_key(ctx: HeadlessBizHawkContext, exc: KeyError) -> bool:
    if not isinstance(ctx.slot_data, dict):
        return False
    key = exc.args[0] if exc.args else None
    if not isinstance(key, str):
        return False
    if key in ctx.slot_data:
        return False
    if key not in _SLOT_DATA_FALLBACKS:
        return False
    default = copy.deepcopy(_SLOT_DATA_FALLBACKS[key])
    ctx.slot_data[key] = default
    game = getattr(ctx.client_handler, "game", "") if ctx.client_handler else ""
    logger.warning("Missing slot_data key '%s' for %s; defaulting to %r for compatibility.", key, game or "unknown", default)
    return True


async def _resilient_game_watcher(ctx: HeadlessBizHawkContext) -> None:
    """Run upstream game watcher with crash-recovery instead of silent task death."""
    while not ctx.exit_event.is_set():
        try:
            await _game_watcher(ctx)
            return
        except asyncio.CancelledError:
            raise
        except KeyError as exc:
            if _recover_missing_slot_data_key(ctx, exc):
                await asyncio.sleep(0.1)
                continue
            logger.exception("Game watcher crashed; restarting.")
            _emit({"event": "error", "message": "Game watcher crashed; restarting."})
            if ctx.bizhawk_ctx.connection_status == ConnectionStatus.CONNECTED:
                async_start(display_message(ctx.bizhawk_ctx, "[error] Game watcher crashed; restarting."))
            try:
                bizhawk_disconnect(ctx.bizhawk_ctx)
            except Exception:
                pass
            # Force handler re-discovery on next iteration.
            ctx.client_handler = None
            ctx.rom_hash = None
            await asyncio.sleep(1.0)
        except Exception:
            logger.exception("Game watcher crashed; restarting.")
            _emit({"event": "error", "message": "Game watcher crashed; restarting."})
            if ctx.bizhawk_ctx.connection_status == ConnectionStatus.CONNECTED:
                async_start(display_message(ctx.bizhawk_ctx, "[error] Game watcher crashed; restarting."))
            try:
                bizhawk_disconnect(ctx.bizhawk_ctx)
            except Exception:
                pass
            # Force handler re-discovery on next iteration.
            ctx.client_handler = None
            ctx.rom_hash = None
            await asyncio.sleep(1.0)


async def _main_async(args: argparse.Namespace) -> int:
    # Keep behavior close to upstream clients: default to INFO so status messages are visible.
    logging.getLogger().setLevel(logging.INFO)
    try:
        client_logger.setLevel(logging.INFO)
    except Exception:
        pass
    try:
        logger.setLevel(logging.INFO)
    except Exception:
        pass

    ctx = HeadlessBizHawkContext(server_address=None, password=None)
    if args.slot:
        ctx.username = args.slot
        ctx.auth = args.slot

    # Logging -> IPC
    handler = _IPCLogHandler()
    handler.setFormatter(logging.Formatter("%(message)s"))
    # Attach once at root; child loggers propagate by default.
    # Adding the same handler to child loggers duplicates every log line.
    logging.getLogger().addHandler(handler)

    queue: asyncio.Queue[dict] = asyncio.Queue()

    # Start stdin reader thread
    loop = asyncio.get_event_loop()
    loop.run_in_executor(None, _stdin_reader, loop, queue)

    # Game watcher loop: connects to BizHawk + selects game handler.
    watcher_task = asyncio.create_task(_resilient_game_watcher(ctx), name="GameWatcher")
    poller_task = asyncio.create_task(_status_poller(ctx), name="StatusPoller")

    # Optional immediate connect. (Password is intentionally not accepted via argv.)
    if args.address:
        ctx.server_address = args.address
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")

    try:
        await _command_loop(ctx, queue)
    finally:
        watcher_task.cancel()
        poller_task.cancel()
        try:
            await watcher_task
        except Exception:
            pass
        try:
            await poller_task
        except Exception:
            pass
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="SekaiLink BizHawkClient wrapper")
    parser.add_argument("--address", default="", help="server address host:port (optional; can be sent via stdin)")
    parser.add_argument("--slot", default="", help="slot name (optional; can be sent via stdin)")
    args = parser.parse_args()
    return asyncio.run(_main_async(args))


if __name__ == "__main__":
    raise SystemExit(main())
