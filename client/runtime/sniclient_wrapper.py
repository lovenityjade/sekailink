#!/usr/bin/env python3
"""
Headless SNIClient wrapper for the SekaiLink Electron app (SNES worlds).

Contract:
- Receives JSON commands on stdin (one JSON object per line)
- Emits JSON events on stdout (one JSON object per line)

This mirrors the BizHawkClient wrapper event shapes so the UI can reuse the same listeners.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import sys
import threading
from typing import Any, Dict, Optional

os.environ.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")

from CommonClient import server_loop, logger as client_logger, websocket_is_open  # noqa: E402
from NetUtils import ClientStatus  # noqa: E402
from Utils import async_start  # noqa: E402

from SNIClient import SNIContext, SNESState, snes_connect  # noqa: E402
from SNIClient import game_watcher as sni_game_watcher  # noqa: E402


def _emit(event: Dict[str, Any]) -> None:
  sys.stdout.write(json.dumps(event, ensure_ascii=True) + "\n")
  sys.stdout.flush()


class _IPCLogHandler(logging.Handler):
  def emit(self, record: logging.LogRecord) -> None:
    # Keep noise down; forward client/server/snes logs.
    if record.name not in ("Client", "FileLog", "StreamLog", "SNES"):
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


class HeadlessSNIContext(SNIContext):
  def __init__(self, snes_address: str = "127.0.0.1:23074") -> None:
    super().__init__(snes_address=snes_address, server_address=None, password=None)
    self._input_futures: Dict[str, asyncio.Future] = {}

  async def _request_value(self, key: str, message: str) -> str:
    if key not in self._input_futures or self._input_futures[key].done():
      self._input_futures[key] = asyncio.get_event_loop().create_future()
    _emit({"event": "request", "type": key, "message": message})
    return await self._input_futures[key]

  def provide_value(self, key: str, value: str) -> None:
    fut = self._input_futures.get(key)
    if fut and not fut.done():
      fut.set_result(value)

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
    super().handle_connection_loss(msg)

  def on_package(self, cmd: str, args: dict):
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
    super().on_package(cmd, args)


def _stdin_reader(loop: asyncio.AbstractEventLoop, queue: "asyncio.Queue[dict]") -> None:
  # Important: reading from sys.stdin is blocking. Run in a dedicated thread so the
  # asyncio loop can continue to handle SNI + AP websocket traffic.
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


async def _status_poller(ctx: HeadlessSNIContext) -> None:
  last = {"server": None, "bizhawk": None, "handler": None}
  while not ctx.exit_event.is_set():
    try:
      server = "connected" if (ctx.server is not None and websocket_is_open(ctx.server.socket)) else "disconnected"
      if ctx.snes_state == SNESState.SNES_ATTACHED:
        bizhawk = "connected"
      elif ctx.snes_state in (SNESState.SNES_CONNECTED, SNESState.SNES_CONNECTING):
        bizhawk = "tentative"
      else:
        bizhawk = "not_connected"

      handler = getattr(ctx.client_handler, "game", None) if ctx.client_handler else ""
      snap = {"server": server, "bizhawk": bizhawk, "handler": handler or ""}
      if any(snap.get(k) != last.get(k) for k in snap.keys()):
        last.update(snap)
        _emit({"event": "emu_status", **snap})
    except Exception:
      pass
    await asyncio.sleep(0.5)


async def _command_loop(ctx: HeadlessSNIContext, queue: "asyncio.Queue[dict]") -> None:
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


async def _main_async(args: argparse.Namespace) -> int:
  logging.getLogger().setLevel(logging.INFO)
  try:
    client_logger.setLevel(logging.INFO)
  except Exception:
    pass

  ctx = HeadlessSNIContext(snes_address=args.snes)
  if args.slot:
    ctx.username = args.slot
    ctx.auth = args.slot

  handler = _IPCLogHandler()
  handler.setFormatter(logging.Formatter("%(message)s"))
  logging.getLogger().addHandler(handler)
  client_logger.addHandler(handler)
  logging.getLogger("SNES").addHandler(handler)

  queue: asyncio.Queue[dict] = asyncio.Queue()

  # Start stdin reader thread (non-blocking for event loop).
  loop = asyncio.get_event_loop()
  t = threading.Thread(target=_stdin_reader, args=(loop, queue), daemon=True, name="stdin_reader")
  t.start()

  # Always try to connect/attach to SNI immediately (0 user interaction).
  async_start(snes_connect(ctx, ctx.snes_address), name="snes_connect")

  watcher_task = asyncio.create_task(sni_game_watcher(ctx), name="SNIWatcher")
  poller_task = asyncio.create_task(_status_poller(ctx), name="StatusPoller")

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
  parser = argparse.ArgumentParser(description="SekaiLink SNIClient wrapper")
  parser.add_argument("--snes", default="127.0.0.1:23074", help="SNI websocket address host:port")
  parser.add_argument("--slot", default="", help="slot name (optional; can be sent via stdin)")
  args = parser.parse_args()
  return asyncio.run(_main_async(args))


if __name__ == "__main__":
  raise SystemExit(main())
