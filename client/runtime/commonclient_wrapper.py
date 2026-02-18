#!/usr/bin/env python3
"""
Headless CommonClient wrapper for Electron.
- Receives JSON commands on stdin
- Emits JSON events on stdout

Goal: no user-typed commands; all actions are clickable in the UI.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import logging
import sys
from typing import Any, Dict, Optional

from CommonClient import CommonContext, server_loop, logger as client_logger
from NetUtils import ClientStatus
from Utils import async_start


def _emit(event: Dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(event, ensure_ascii=True) + "\n")
    sys.stdout.flush()


class _IPCLogHandler(logging.Handler):
    def emit(self, record: logging.LogRecord) -> None:
        if record.name not in ("Client", "StreamLog", "FileLog"):
            return
        msg = self.format(record)
        _emit({
            "event": "log",
            "level": record.levelname.lower(),
            "logger": record.name,
            "message": msg,
        })


class HeadlessContext(CommonContext):
    def __init__(self, server_address: Optional[str] = None, password: Optional[str] = None) -> None:
        super().__init__(server_address=server_address, password=password)
        self._input_futures: Dict[str, asyncio.Future] = {}
        self._connected = False

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
        if password_requested and not self.password:
            self.password = await self._request_value("password", "Password required")
        return await super().server_auth(password_requested)

    async def get_username(self):
        if not self.auth:
            self.auth = self.username
            if not self.auth:
                self.auth = await self._request_value("slot", "Slot name required")

    async def connection_closed(self):
        await super().connection_closed()
        if self._connected:
            self._connected = False
            _emit({"event": "status", "server": "disconnected"})

    def handle_connection_loss(self, msg: str) -> None:
        _emit({"event": "error", "message": msg})
        super().handle_connection_loss(msg)

    def on_print_json(self, args: dict):
        # Forward JSON output for UI rendering if needed.
        _emit({"event": "print_json", "data": args.get("data", [])})
        super().on_print_json(args)

    def on_package(self, cmd: str, args: dict):
        if cmd == "Connected":
            self._connected = True
            _emit({"event": "status", "server": "connected"})
        elif cmd == "RoomInfo":
            _emit({
                "event": "room_info",
                "seed_name": args.get("seed_name"),
                "password": bool(args.get("password")),
                "players": args.get("players", []),
            })


async def _command_loop(ctx: HeadlessContext, queue: "asyncio.Queue[dict]") -> None:
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
                ctx.server_task = asyncio.create_task(server_loop(ctx, ctx.server_address), name="server loop")
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


async def _main_async(args: argparse.Namespace) -> int:
    ctx = HeadlessContext(server_address=args.address, password=args.password)
    if args.slot:
        ctx.username = args.slot
        ctx.auth = args.slot

    # Logging -> IPC
    handler = _IPCLogHandler()
    handler.setFormatter(logging.Formatter("%(message)s"))
    logging.getLogger().addHandler(handler)
    client_logger.addHandler(handler)

    queue: asyncio.Queue[dict] = asyncio.Queue()

    # Kick initial connect if provided
    if args.address:
        ctx.server_task = asyncio.create_task(server_loop(ctx, args.address), name="server loop")

    # Start stdin reader thread
    loop = asyncio.get_event_loop()
    loop.run_in_executor(None, _stdin_reader, loop, queue)

    await _command_loop(ctx, queue)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="SekaiLink CommonClient wrapper")
    parser.add_argument("--address", default="", help="server address host:port")
    parser.add_argument("--slot", default="", help="slot name")
    parser.add_argument("--password", default="", help="room password")
    args = parser.parse_args()
    return asyncio.run(_main_async(args))


if __name__ == "__main__":
    raise SystemExit(main())
