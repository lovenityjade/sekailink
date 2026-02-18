#!/usr/bin/env python3
"""
SekaiLink SNI bridge for SNES worlds running via BizHawk.

Why:
- Many SNES Archipelago worlds use SNIClient (usb2snes/SNI websocket protocol) instead of BizHawkClient.
- SekaiLink runs BizHawk (with our Lua TCP connector) and wants 0 user interaction.
- This bridge exposes a minimal SNI websocket server on localhost:23074 and forwards memory
  reads/writes to BizHawk's Lua connector.

Protocol notes (matching Archipelago SNIClient.py):
- Text frames: JSON with {"Opcode": "...", "Space": "SNES", "Operands": [..]}
- GetAddress replies are binary frames (raw bytes).
- PutAddress is a JSON request immediately followed by a binary frame payload.
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import logging
from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple

import websockets

_LOG = logging.getLogger("sekailink.sni_bridge")

# FXPak Pro mapping used by Archipelago's SNI worlds.
ROM_START = 0x000000
SRAM_START = 0xE00000
WRAM_START = 0xF50000
WRAM_SIZE = 0x20000


def _parse_hex(value: str) -> int:
  s = str(value or "").strip().lower()
  if s.startswith("0x"):
    s = s[2:]
  if not s:
    return 0
  return int(s, 16)


@dataclass
class _Addr:
  domain: str
  offset: int


class LuaConnector:
  def __init__(self, host: str, port_start: int, port_end: int) -> None:
    self._host = host
    self._port_start = int(port_start)
    self._port_end = int(port_end)
    self._reader: Optional[asyncio.StreamReader] = None
    self._writer: Optional[asyncio.StreamWriter] = None
    self._lock = asyncio.Lock()
    self._domains: Dict[str, str] = {}
    self._keepalive_task: Optional[asyncio.Task] = None

  async def _connect_once(self) -> bool:
    for port in range(self._port_start, self._port_end + 1):
      try:
        reader, writer = await asyncio.open_connection(self._host, port)
        self._reader = reader
        self._writer = writer
        _LOG.info("Connected to BizHawk Lua connector at %s:%d", self._host, port)
        self._domains = {}
        if self._keepalive_task is None or self._keepalive_task.done():
          # Keep the Lua connector script from timing out and marking itself disconnected.
          # The overlay uses a ~5s idle timeout unless it receives any message.
          self._keepalive_task = asyncio.create_task(self._keepalive_loop(), name="LuaKeepalive")
        return True
      except Exception:
        continue
    return False

  async def ensure_connected(self) -> None:
    if self._writer is not None and not self._writer.is_closing():
      return
    self._reader = None
    self._writer = None
    for _ in range(80):  # ~8s total
      if await self._connect_once():
        return
      await asyncio.sleep(0.1)
    raise RuntimeError("lua_connector_unreachable")

  async def _keepalive_loop(self) -> None:
    # Send a benign request periodically so the Lua script sees traffic and doesn't time out.
    while True:
      try:
        await asyncio.sleep(1.5)
        if self._writer is None or self._writer.is_closing():
          continue
        async with self._lock:
          # Use MEMORY_SIZE on a common domain; response content isn't important.
          try:
            await self._request([{"type": "MEMORY_SIZE", "domain": "WRAM"}])
          except Exception:
            # Connection may have dropped; next normal request will reconnect.
            pass
      except asyncio.CancelledError:
        return
      except Exception:
        # Never let keepalive crash the bridge.
        pass

  async def _request(self, payload: Any) -> Any:
    await self.ensure_connected()
    assert self._reader is not None
    assert self._writer is not None
    line = json.dumps(payload, ensure_ascii=True) + "\n"
    self._writer.write(line.encode("utf-8", "replace"))
    await self._writer.drain()
    # The connector replies with one JSON line. Ignore occasional non-JSON noise if any.
    for _ in range(10):
      raw = await self._reader.readline()
      if not raw:
        raise RuntimeError("lua_connector_closed")
      txt = raw.decode("utf-8", "replace").strip()
      if not txt:
        continue
      try:
        return json.loads(txt)
      except Exception:
        continue
    raise RuntimeError("lua_connector_invalid_reply")

  async def _memory_size(self, domain: str) -> int:
    resp = await self._request([{"type": "MEMORY_SIZE", "domain": domain}])
    if not isinstance(resp, list) or not resp:
      return 0
    one = resp[0] if isinstance(resp[0], dict) else {}
    if str(one.get("type", "")).upper() != "MEMORY_SIZE_RESPONSE":
      return 0
    try:
      return int(one.get("value", 0) or 0)
    except Exception:
      return 0

  async def _resolve_domains(self) -> None:
    if self._domains:
      return
    # Probe common BizHawk SNES memory domains.
    candidates = {
      "rom": ["CARTROM", "CartROM", "ROM"],
      "sram": ["CARTRAM", "CartRAM", "SRAM"],
      "wram": ["WRAM", "Work RAM", "WorkRAM"],
    }
    resolved: Dict[str, str] = {}
    for key, names in candidates.items():
      for name in names:
        try:
          size = await self._memory_size(name)
        except Exception:
          size = 0
        if size > 0:
          resolved[key] = name
          break
    # WRAM size might be 0 until a core is fully initialized; keep best-effort defaults.
    resolved.setdefault("rom", "CARTROM")
    resolved.setdefault("sram", "CARTRAM")
    resolved.setdefault("wram", "WRAM")
    self._domains = resolved
    _LOG.info("Resolved domains: %s", resolved)

  def _translate(self, addr: int) -> _Addr:
    # Match FXPak mapping: WRAM at 0xF50000, SRAM at 0xE00000, ROM at 0x000000.
    if WRAM_START <= addr < WRAM_START + WRAM_SIZE:
      return _Addr(domain=self._domains.get("wram", "WRAM"), offset=addr - WRAM_START)
    if addr >= SRAM_START:
      return _Addr(domain=self._domains.get("sram", "CARTRAM"), offset=addr - SRAM_START)
    return _Addr(domain=self._domains.get("rom", "CARTROM"), offset=addr - ROM_START)

  async def read(self, addr: int, size: int) -> bytes:
    async with self._lock:
      await self._resolve_domains()
      a = self._translate(int(addr))
      resp = await self._request([{"type": "READ", "address": int(a.offset), "size": int(size), "domain": a.domain}])
      if not isinstance(resp, list) or not resp:
        raise RuntimeError("lua_read_failed")
      one = resp[0] if isinstance(resp[0], dict) else {}
      if str(one.get("type", "")).upper() != "READ_RESPONSE":
        raise RuntimeError("lua_read_bad_response")
      b64 = str(one.get("value", "") or "")
      return base64.b64decode(b64.encode("ascii", "ignore"), validate=False)

  async def write(self, addr: int, data: bytes) -> None:
    async with self._lock:
      await self._resolve_domains()
      a = self._translate(int(addr))
      b64 = base64.b64encode(data).decode("ascii")
      resp = await self._request([{"type": "WRITE", "address": int(a.offset), "value": b64, "domain": a.domain}])
      if not isinstance(resp, list) or not resp:
        raise RuntimeError("lua_write_failed")
      one = resp[0] if isinstance(resp[0], dict) else {}
      if str(one.get("type", "")).upper() != "WRITE_RESPONSE":
        raise RuntimeError("lua_write_bad_response")


async def _handle_client(ws: websockets.WebSocketServerProtocol, lua: LuaConnector) -> None:
  pending_put: Optional[Tuple[int, int]] = None  # (addr, size)
  is_poptracker = False  # usb2snes client (expects Attach response)
  async for message in ws:
    if isinstance(message, (bytes, bytearray)):
      if not pending_put:
        continue
      addr, expected = pending_put
      pending_put = None
      payload = bytes(message)
      if expected > 0 and len(payload) != expected:
        payload = payload[:expected]
      # Chunk writes a bit to avoid huge lua messages.
      off = 0
      while off < len(payload):
        chunk = payload[off: off + 0x8000]
        await lua.write(addr + off, chunk)
        off += len(chunk)
      continue

    # Text frame.
    try:
      req = json.loads(str(message))
    except Exception:
      continue

    opcode = str(req.get("Opcode", "") or "")
    operands = req.get("Operands") or []
    if not isinstance(operands, list):
      operands = []

    if opcode == "Name":
      # PopTracker's usb2snes client sends this right after connect.
      is_poptracker = True
      continue

    if opcode == "DeviceList":
      await ws.send(json.dumps({"Results": ["SekaiLink BizHawk"]}, ensure_ascii=True))
      continue
    if opcode == "AppVersion":
      await ws.send(json.dumps({"Results": ["SekaiLinkSNI/1.0"]}, ensure_ascii=True))
      continue
    if opcode == "Attach":
      # PopTracker (usb2snes) expects a JSON reply to Attach with a non-empty Results
      # array, otherwise it will stay in "connected but no device" state.
      #
      # Archipelago's SNIClient does NOT read an Attach reply and will route any extra
      # text frames into its binary recv queue, breaking reads. Only reply for usb2snes.
      if is_poptracker:
        await ws.send(json.dumps({"Results": ["1.0.0", "NWAccess"]}, ensure_ascii=True))
      continue
    if opcode == "Info":
      # PopTracker sends Info right after Attach but does not strictly require it.
      # Keep it lean to avoid unexpected extra frames later in the session.
      if is_poptracker:
        await ws.send(json.dumps({"Results": ["SekaiLinkSNI/1.0"]}, ensure_ascii=True))
      continue
    if opcode == "GetAddress":
      addr = _parse_hex(operands[0] if len(operands) > 0 else "0")
      size = _parse_hex(operands[1] if len(operands) > 1 else "0")
      data = await lua.read(addr, size)
      await ws.send(data)
      continue
    if opcode == "PutAddress":
      addr = _parse_hex(operands[0] if len(operands) > 0 else "0")
      size = _parse_hex(operands[1] if len(operands) > 1 else "0")
      pending_put = (addr, size)
      continue

    # Unknown opcode: ignore (same reason as Attach; unsolicited text frames are dangerous).
    continue


async def main_async(argv: Optional[list[str]] = None) -> int:
  parser = argparse.ArgumentParser(description="SekaiLink SNI bridge")
  parser.add_argument("--host", default="127.0.0.1")
  parser.add_argument("--port", type=int, default=23074)
  parser.add_argument("--lua-host", default="127.0.0.1")
  parser.add_argument("--lua-port-start", type=int, default=43055)
  parser.add_argument("--lua-port-end", type=int, default=43060)
  parser.add_argument("--log-level", default="info")
  args = parser.parse_args(argv)

  logging.basicConfig(level=getattr(logging, str(args.log_level).upper(), logging.INFO))
  lua = LuaConnector(args.lua_host, args.lua_port_start, args.lua_port_end)

  async def handler(ws: websockets.WebSocketServerProtocol) -> None:
    try:
      await _handle_client(ws, lua)
    except Exception as exc:
      _LOG.warning("client handler error: %s", exc)

  # Bind both IPv4 and IPv6 loopback by default. Some clients (and Python itself) may
  # resolve `localhost` to ::1 first, while we often probe 127.0.0.1.
  bind_hosts = [str(args.host or "127.0.0.1")]
  if bind_hosts[0] == "127.0.0.1":
    bind_hosts.append("::1")
  elif bind_hosts[0] == "::1":
    bind_hosts.append("127.0.0.1")

  servers = []
  for host in bind_hosts:
    try:
      servers.append(await websockets.serve(handler, host, args.port, ping_interval=None))
    except OSError as exc:
      _LOG.warning("Failed to bind SNI bridge on %s:%d (%s)", host, int(args.port), exc)
      continue

  if not servers:
    raise RuntimeError("sni_bridge_bind_failed")

  _LOG.info("SNI bridge listening on %s", ", ".join([f"ws://{h}:{int(args.port)}" for h in bind_hosts]))
  try:
    await asyncio.Future()  # run forever
  finally:
    server.close()
    await server.wait_closed()
  return 0


def main() -> int:
  return asyncio.run(main_async())


if __name__ == "__main__":
  raise SystemExit(main())
