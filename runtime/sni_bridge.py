#!/usr/bin/env python3
"""
SekaiLink SNI bridge for SNES worlds running via Sekaiemu.

Why:
- Many SNES Archipelago worlds use SNIClient (usb2snes/SNI websocket protocol) instead of BizHawkClient.
- SekaiLink runs Sekaiemu and wants 0 user interaction.
- This bridge exposes a minimal SNI websocket server on localhost:23074 and forwards memory
  reads/writes to Sekaiemu's JSON memory socket.

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
import os
import socket
import time
from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple, Union

import websockets

_LOG = logging.getLogger("sekailink.sni_bridge")

# FXPak Pro mapping used by Archipelago's SNI worlds.
ROM_START = 0x000000
SRAM_START = 0xE00000
WRAM_START = 0xF50000
WRAM_SIZE = 0x20000
MEMORY_IO_TIMEOUT_SECONDS = 1.0
SNI_READ_TIMEOUT_SECONDS = 2.0
SNI_WRITE_TIMEOUT_SECONDS = 2.0


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


class MemoryConnector:
  def __init__(self, memory_socket: str, host: str, port_start: int, port_end: int) -> None:
    self._memory_socket = str(memory_socket or "").strip()
    self._host = host
    self._port_start = int(port_start)
    self._port_end = int(port_end)
    self._reader: Optional[Union[asyncio.StreamReader, asyncio.StreamReader]] = None
    self._writer: Optional[Union[asyncio.StreamWriter, asyncio.StreamWriter]] = None
    self._lock = asyncio.Lock()
    self._domains: Dict[str, str] = {}
    self._domain_sizes: Dict[str, int] = {}
    self._keepalive_task: Optional[asyncio.Task] = None
    self._domains_resolved_at = 0.0
    self._sram_ready = False
    self._system_bus_domain: str = ""
    self._virtual_sram: Dict[int, int] = {}
    self._ap_rom_name_cache: Optional[bytes] = None
    self._connected_once = False

  async def _invalidate_connection(self) -> None:
    writer = self._writer
    self._reader = None
    self._writer = None
    self._domains = {}
    self._domain_sizes = {}
    self._domains_resolved_at = 0.0
    self._sram_ready = False
    self._system_bus_domain = ""
    self._virtual_sram = {}
    self._ap_rom_name_cache = None
    if writer is not None:
      try:
        writer.close()
        await writer.wait_closed()
      except Exception:
        pass

  async def reset(self) -> None:
    await self._invalidate_connection()

  async def _connect_once(self) -> bool:
    if self._memory_socket:
      try:
        if self._memory_socket.startswith("tcp://"):
          rest = self._memory_socket[len("tcp://"):]
          host, _, port_raw = rest.rpartition(":")
          reader, writer = await asyncio.open_connection(host or "127.0.0.1", int(port_raw))
        else:
          reader, writer = await asyncio.open_unix_connection(self._memory_socket)
        self._reader = reader
        self._writer = writer
        try:
          system_id = await self._system_id()
        except Exception:
          system_id = ""
        if str(system_id or "").upper() == "NULL":
          try:
            writer.close()
            await writer.wait_closed()
          except Exception:
            pass
          self._reader = None
          self._writer = None
          return False
        _LOG.info("Connected to Sekaiemu memory socket %s (system=%s)", self._memory_socket, system_id or "?")
        self._connected_once = True
        self._domains = {}
        self._domain_sizes = {}
        if self._keepalive_task is None or self._keepalive_task.done():
          self._keepalive_task = asyncio.create_task(self._keepalive_loop(), name="MemoryKeepalive")
        return True
      except Exception as exc:
        _LOG.debug("Sekaiemu memory socket connect failed: %s", exc)
        return False

    # Back-compat fallback for older BizHawk/Lua connector builds.
    for port in range(self._port_start, self._port_end + 1):
      try:
        reader, writer = await asyncio.open_connection(self._host, port)
        self._reader = reader
        self._writer = writer
        # Reject stale/invalid connector sockets (e.g. no ROM/core loaded).
        # Keeping those leads to zero-byte SNI reads and reconnect loops.
        try:
          system_id = await self._system_id()
        except Exception:
          system_id = ""
        try:
          wram_size = await self._memory_size("WRAM")
        except Exception:
          wram_size = 0
        try:
          rom_size = await self._memory_size("CARTROM")
        except Exception:
          rom_size = 0
        if str(system_id or "").upper() == "NULL" or (int(wram_size) <= 0 and int(rom_size) <= 0):
          try:
            writer.close()
            await writer.wait_closed()
          except Exception:
            pass
          self._reader = None
          self._writer = None
          continue
        _LOG.info("Connected to legacy BizHawk Lua connector at %s:%d (system=%s wram=%d rom=%d)",
                  self._host, port, system_id or "?", int(wram_size), int(rom_size))
        self._connected_once = True
        self._domains = {}
        self._domain_sizes = {}
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
    await self._invalidate_connection()
    for _ in range(20):  # ~1s total
      if await self._connect_once():
        return
      await asyncio.sleep(0.05)
    raise RuntimeError("memory_connector_unreachable")

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
            await self._request([{"type": "MEMORY_SIZE", "domain": "System Bus"}])
          except Exception:
            # Connection may have dropped; next normal request will reconnect.
            pass
      except asyncio.CancelledError:
        return
      except Exception:
        # Never let keepalive crash the bridge.
        pass

  async def _request(self, payload: Any) -> Any:
    # Retry once after resetting socket state when BizHawk/Lua reconnects mid-session.
    for attempt in range(2):
      try:
        await self.ensure_connected()
        assert self._reader is not None
        assert self._writer is not None
        line = json.dumps(payload, ensure_ascii=True) + "\n"
        self._writer.write(line.encode("utf-8", "replace"))
        await asyncio.wait_for(self._writer.drain(), timeout=MEMORY_IO_TIMEOUT_SECONDS)
        # The connector replies with one JSON line. Ignore occasional non-JSON noise if any.
        for _ in range(10):
          raw = await asyncio.wait_for(self._reader.readline(), timeout=MEMORY_IO_TIMEOUT_SECONDS)
          if not raw:
            raise RuntimeError("memory_connector_closed")
          txt = raw.decode("utf-8", "replace").strip()
          if not txt:
            continue
          try:
            return json.loads(txt)
          except Exception:
            continue
        raise RuntimeError("memory_connector_invalid_reply")
      except Exception:
        await self._invalidate_connection()
        if attempt == 0:
          continue
        raise

  async def _system_id(self) -> str:
    resp = await self._request([{"type": "SYSTEM"}])
    if not isinstance(resp, list) or not resp:
      return ""
    one = resp[0] if isinstance(resp[0], dict) else {}
    if str(one.get("type", "")).upper() != "SYSTEM_RESPONSE":
      return ""
    return str(one.get("value", "") or "")

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

  async def _resolve_domains(self, force: bool = False) -> None:
    now = time.monotonic()
    if (not force) and self._domains:
      # Re-probe periodically while SRAM isn't ready (common after pause/resume).
      if self._sram_ready:
        return
      if (now - self._domains_resolved_at) < 1.0:
        return
    if not force and self._domains and self._sram_ready:
      return
    resolved: Dict[str, str] = {}
    sizes: Dict[str, int] = {}

    # Resolve the optional SNES System Bus, but do not treat it as the primary
    # FXPak address space. Some cores expose a CPU bus here; AP SNI clients use
    # FXPak-style linear ROM/SRAM/WRAM addresses.
    bus_candidates = ["System Bus", "SystemBus", "BUS", "Cpu Bus"]
    for name in bus_candidates:
      try:
        size = await self._memory_size(name)
      except Exception:
        size = -1
      if int(size) > 0:
        self._system_bus_domain = name
        sizes[name] = int(size)
        _LOG.info("Resolved System Bus domain: %s (size=%d)", name, int(size))
        break

    if not self._system_bus_domain:
      self._system_bus_domain = ""

    # Probe common SNES memory domains from both Sekaiemu/libretro and BizHawk.
    candidates = {
      "rom": ["ROM", "Cart ROM", "CARTROM", "CartROM"],
      "sram": ["SRAM", "Save RAM", "Battery RAM", "CARTSRAM", "CartSRAM", "CARTRAM", "CartRAM", "SaveRAM"],
      "wram": ["WRAM", "Work RAM", "WorkRAM", "RAM", "System RAM"],
    }
    for key, names in candidates.items():
      best_name: Optional[str] = None
      best_size = 0
      fallback_name: Optional[str] = None
      for name in names:
        try:
          size = await self._memory_size(name)
        except Exception:
          size = -1
        if size >= 0 and fallback_name is None:
          fallback_name = name
        if size > best_size:
          best_size = int(size)
          best_name = name
      if best_name and best_size > 0:
        resolved[key] = best_name
        sizes[best_name] = best_size
      elif fallback_name:
        resolved[key] = fallback_name
        sizes[fallback_name] = int(best_size if best_size > 0 else 0)
    # WRAM size might be 0 until a core is fully initialized; keep best-effort defaults.
    resolved.setdefault("rom", "CARTROM")
    resolved.setdefault("sram", "CARTRAM")
    resolved.setdefault("wram", "WRAM")
    self._domains = resolved
    self._domain_sizes = sizes
    self._domains_resolved_at = now
    sram_domain = self._domains.get("sram", "")
    self._sram_ready = bool(int(self._domain_sizes.get(sram_domain, 0) or 0) > 0)
    _LOG.info("Resolved domains: %s (sizes=%s bus=%s)", resolved, {k: sizes.get(v, 0) for k, v in resolved.items()}, self._system_bus_domain or "-")

  async def _read_domain_raw(self, domain: str, offset: int, size: int) -> bytes:
    if size <= 0:
      return b""
    resp = await self._request([{"type": "READ", "address": int(offset), "size": int(size), "domain": str(domain)}])
    if not isinstance(resp, list) or not resp:
      return b""
    one = resp[0] if isinstance(resp[0], dict) else {}
    if str(one.get("type", "")).upper() != "READ_RESPONSE":
      return b""
    b64 = str(one.get("value", "") or "")
    try:
      data = base64.b64decode(b64.encode("ascii", "ignore"), validate=False)
    except Exception:
      return b""
    if len(data) < size:
      data = data + bytes(size - len(data))
    elif len(data) > size:
      data = data[:size]
    return data

  async def _get_ap_rom_name_from_rom(self) -> Optional[bytes]:
    if self._ap_rom_name_cache is not None:
      return self._ap_rom_name_cache
    rom_domain = self._domains.get("rom", "CARTROM")
    for header in (0x7FC0, 0xFFC0):
      try:
        data = await self._read_domain_raw(rom_domain, header, 0x15)
      except Exception:
        data = b""
      if len(data) == 0x15 and data[:2] == b"AP" and any(b != 0 for b in data):
        self._ap_rom_name_cache = data
        return data
    return None

  async def _read_virtual_sram(self, addr: int, size: int) -> Optional[bytes]:
    # Compatibility path for cores exposing 0-byte SRAM domains.
    # ALttP/FF4 ROM authentication reads at 0xE02000.
    if size <= 0:
      return b""
    if addr < SRAM_START or addr >= (SRAM_START + 0x40000):
      return None
    out = bytearray(int(size))
    rom_name = await self._get_ap_rom_name_from_rom()
    for i in range(int(size)):
      cur = int(addr) + i
      off = cur - SRAM_START
      if off in self._virtual_sram:
        out[i] = int(self._virtual_sram[off]) & 0xFF
        continue
      # ROM name window expected by AP SNES clients.
      if 0x2000 <= off < 0x2015 and rom_name is not None:
        out[i] = rom_name[off - 0x2000]
        continue
      # DeathLink/config byte defaults to 0 when SRAM isn't available.
      if off == 0x2015:
        out[i] = 0
        continue
      out[i] = 0
    return bytes(out)

  def _write_virtual_sram(self, addr: int, data: bytes) -> bool:
    if not data:
      return True
    if addr < SRAM_START or addr >= (SRAM_START + 0x40000):
      return False
    for i, b in enumerate(data):
      off = (int(addr) + int(i)) - SRAM_START
      self._virtual_sram[int(off)] = int(b) & 0xFF
    return True

  def _translate(self, addr: int) -> _Addr:
    # Match FXPak mapping: WRAM at 0xF50000, SRAM at 0xE00000, ROM at 0x000000.
    if WRAM_START <= addr < WRAM_START + WRAM_SIZE:
      domain = self._domains.get("wram", "WRAM")
      if int(self._domain_sizes.get(domain, 0) or 0) > 0:
        return _Addr(domain=domain, offset=addr - WRAM_START)
    if addr >= SRAM_START:
      domain = self._domains.get("sram", "CARTRAM")
      if int(self._domain_sizes.get(domain, 0) or 0) > 0:
        return _Addr(domain=domain, offset=addr - SRAM_START)
    if addr < SRAM_START:
      domain = self._domains.get("rom", "CARTROM")
      if int(self._domain_sizes.get(domain, 0) or 0) > 0:
        return _Addr(domain=domain, offset=addr - ROM_START)
    if self._system_bus_domain:
      return _Addr(domain=self._system_bus_domain, offset=int(addr))
    if WRAM_START <= addr < WRAM_START + WRAM_SIZE:
      return _Addr(domain=self._domains.get("wram", "WRAM"), offset=addr - WRAM_START)
    if addr >= SRAM_START:
      return _Addr(domain=self._domains.get("sram", "CARTRAM"), offset=addr - SRAM_START)
    return _Addr(domain=self._domains.get("rom", "CARTROM"), offset=addr - ROM_START)

  async def read(self, addr: int, size: int) -> bytes:
    async with self._lock:
      await self._resolve_domains(force=False)
      a = self._translate(int(addr))
      req_size = int(size)
      domain_size = int(self._domain_sizes.get(a.domain, 0) or 0)
      if int(addr) >= SRAM_START and domain_size <= 0:
        virtual = await self._read_virtual_sram(int(addr), req_size)
        if virtual is not None:
          return virtual
      if domain_size > 0 and a.offset >= domain_size:
        # Keep SNI socket stable when a probing handler reads outside this domain.
        # Returning zeroes matches how AP handlers treat non-matching game memory.
        return bytes(req_size)
      read_size = req_size
      if domain_size > 0 and (a.offset + read_size) > domain_size:
        read_size = max(0, domain_size - a.offset)
      if read_size <= 0:
        return bytes(req_size)
      resp = await self._request([{"type": "READ", "address": int(a.offset), "size": int(read_size), "domain": a.domain}])
      if not isinstance(resp, list) or not resp:
        return bytes(req_size)
      one = resp[0] if isinstance(resp[0], dict) else {}
      if str(one.get("type", "")).upper() != "READ_RESPONSE":
        return bytes(req_size)
      b64 = str(one.get("value", "") or "")
      data = base64.b64decode(b64.encode("ascii", "ignore"), validate=False)
      if len(data) < req_size:
        data = data + bytes(req_size - len(data))
      elif len(data) > req_size:
        data = data[:req_size]
      return data

  async def write(self, addr: int, data: bytes) -> None:
    async with self._lock:
      if int(addr) >= SRAM_START:
        a = self._translate(int(addr))
        domain_size = int(self._domain_sizes.get(a.domain, 0) or 0)
        if domain_size <= 0 and self._write_virtual_sram(int(addr), data):
          return
      b64 = base64.b64encode(data).decode("ascii")
      for attempt in range(2):
        await self._resolve_domains(force=attempt > 0)
        a = self._translate(int(addr))
        resp = await self._request([{"type": "WRITE", "address": int(a.offset), "value": b64, "domain": a.domain}])
        if isinstance(resp, list) and resp:
          one = resp[0] if isinstance(resp[0], dict) else {}
          if str(one.get("type", "")).upper() == "WRITE_RESPONSE":
            _LOG.info("PutAddress wrote %d byte(s) fxpak=%s domain=%s offset=%s",
                      len(data), hex(int(addr)), a.domain, hex(int(a.offset)))
            return
        if attempt == 0:
          # Domain map can be stale just after focus/resume. Re-probe once.
          self._domains = {}
          self._domain_sizes = {}
          self._domains_resolved_at = 0.0
          self._sram_ready = False
          continue
        raise RuntimeError("memory_write_bad_response")


async def _handle_client(ws: websockets.WebSocketServerProtocol, lua: MemoryConnector) -> None:
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
      _LOG.info("PutAddress request fxpak=%s size=%d", hex(int(addr)), len(payload))
      off = 0
      while off < len(payload):
        chunk = payload[off: off + 0x8000]
        try:
          await asyncio.wait_for(lua.write(addr + off, chunk), timeout=SNI_WRITE_TIMEOUT_SECONDS)
        except asyncio.TimeoutError:
          _LOG.warning("PutAddress timed out at %s size=%d", hex(int(addr + off)), len(chunk))
          try:
            await lua.reset()
          except Exception:
            pass
          break
        except Exception as exc:
          _LOG.warning("PutAddress failed at %s size=%d (%s)", hex(int(addr + off)), len(chunk), exc)
          try:
            await lua.reset()
          except Exception:
            pass
          break
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
      try:
        data = await asyncio.wait_for(lua.read(addr, size), timeout=SNI_READ_TIMEOUT_SECONDS)
      except asyncio.TimeoutError:
        _LOG.warning("GetAddress timed out at %s size=%d", hex(int(addr)), int(size))
        try:
          await lua.reset()
        except Exception:
          pass
        data = bytes(int(size))
      except Exception as exc:
        _LOG.warning("GetAddress failed at %s size=%d (%s)", hex(int(addr)), int(size), exc)
        try:
          await lua.reset()
        except Exception:
          pass
        data = bytes(int(size))
      if len(data) != int(size):
        data = (bytes(data) + bytes(max(0, int(size) - len(data))))[: int(size)]
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
  parser.add_argument("--memory-socket", default="")
  parser.add_argument("--lua-host", default="127.0.0.1")
  parser.add_argument("--lua-port-start", type=int, default=43055)
  parser.add_argument("--lua-port-end", type=int, default=43060)
  parser.add_argument("--log-level", default="info")
  args = parser.parse_args(argv)

  logging.basicConfig(level=getattr(logging, str(args.log_level).upper(), logging.INFO))
  lua = MemoryConnector(args.memory_socket, args.lua_host, args.lua_port_start, args.lua_port_end)
  stop_event = asyncio.Event()

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

  async def memory_socket_lifetime_monitor() -> None:
    if not args.memory_socket or str(args.memory_socket).startswith("tcp://"):
      return
    missing_since = 0.0
    while not stop_event.is_set():
      await asyncio.sleep(0.5)
      if not lua._connected_once:
        continue
      if os.path.exists(args.memory_socket):
        missing_since = 0.0
        continue
      if missing_since <= 0:
        missing_since = time.monotonic()
        continue
      if (time.monotonic() - missing_since) >= 2.0:
        _LOG.info("Sekaiemu memory socket disappeared; stopping SNI bridge (%s)", args.memory_socket)
        stop_event.set()
        return

  monitor_task = asyncio.create_task(memory_socket_lifetime_monitor(), name="MemorySocketLifetime")
  try:
    await stop_event.wait()
  finally:
    monitor_task.cancel()
    for server in servers:
      server.close()
    for server in servers:
      await server.wait_closed()
  return 0


def main() -> int:
  return asyncio.run(main_async())


if __name__ == "__main__":
  raise SystemExit(main())
