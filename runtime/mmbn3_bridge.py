#!/usr/bin/env python3
"""Sekaiemu bridge for Archipelago's MMBN3Client.

MMBN3 does not use the generic BizHawk client handler. Its upstream client
expects connector_mmbn3.lua to listen on localhost:28922 and exchange JSON
payloads. This bridge emulates the read side of that Lua connector against the
Sekaiemu runtime memory socket so the upstream client can authenticate and send
location state.
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import sys
from typing import Any
from urllib.parse import urlparse


SCRIPT_VERSION = 4
PLAYER_NAME_ADDR = 0x7FFFC0
PLAYER_NAME_SIZE = 63
LOCATION_ADDR = 0x02000000
LOCATION_SIZE = 0x434
CANARY_BYTE = 0x020001A9
TITLE_STATE_ADDR = 0x020097F8
ALPHA_DEFEATED_ADDR = 0x02000433


def emit(event: str, **payload: Any) -> None:
    print(json.dumps({"event": event, **payload}, ensure_ascii=True), flush=True)


async def memory_request(endpoint: str, requests: list[dict[str, Any]], timeout: float = 1.8) -> list[dict[str, Any]]:
    parsed = urlparse(endpoint)
    if parsed.scheme != "tcp" or parsed.hostname not in {"127.0.0.1", "localhost"} or not parsed.port:
        raise RuntimeError(f"unsupported_memory_endpoint:{endpoint}")
    reader, writer = await asyncio.wait_for(asyncio.open_connection(parsed.hostname, parsed.port), timeout=timeout)
    try:
        writer.write((json.dumps(requests, separators=(",", ":")) + "\n").encode("utf-8"))
        await asyncio.wait_for(writer.drain(), timeout=timeout)
        line = await asyncio.wait_for(reader.readline(), timeout=timeout)
        if not line:
            raise RuntimeError("memory_socket_closed")
        decoded = json.loads(line.decode("utf-8"))
        if not isinstance(decoded, list):
            raise RuntimeError("memory_bad_response")
        return decoded
    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:
            pass


def decode_read_response(response: dict[str, Any]) -> bytes:
    if response.get("type") == "ERROR":
        raise RuntimeError(str(response.get("value") or "memory_read_error"))
    value = response.get("value")
    if not isinstance(value, str):
        raise RuntimeError("memory_read_missing_value")
    return base64.b64decode(value)


async def read_memory(endpoint: str, domain: str, address: int, size: int) -> bytes:
    responses = await memory_request(endpoint, [{"type": "READ", "domain": domain, "address": address, "size": size}])
    for response in responses:
        if response.get("type") in {"READ_RESPONSE", "ERROR"}:
            return decode_read_response(response)
    raise RuntimeError("memory_read_no_response")


async def read_first(endpoint: str, requests: list[tuple[str, int, int]]) -> bytes:
    last_error: Exception | None = None
    for domain, address, size in requests:
        try:
            return await read_memory(endpoint, domain, address, size)
        except Exception as exc:
            last_error = exc
    raise RuntimeError(str(last_error or "memory_read_failed"))


async def build_connector_payload(endpoint: str, fallback_player_name: str) -> dict[str, Any]:
    player_bytes = await read_first(endpoint, [
        ("ROM", PLAYER_NAME_ADDR, PLAYER_NAME_SIZE),
        ("rom", PLAYER_NAME_ADDR, PLAYER_NAME_SIZE),
        ("System Bus", PLAYER_NAME_ADDR, PLAYER_NAME_SIZE),
    ])
    if not any(player_bytes):
        player_bytes = fallback_player_name.encode("utf-8")[:PLAYER_NAME_SIZE]
    locations = await read_first(endpoint, [
        ("System Bus", LOCATION_ADDR, LOCATION_SIZE),
        ("system_bus", LOCATION_ADDR, LOCATION_SIZE),
        ("EWRAM", LOCATION_ADDR, LOCATION_SIZE),
    ])
    canary = locations[CANARY_BYTE - LOCATION_ADDR] if 0 <= CANARY_BYTE - LOCATION_ADDR < len(locations) else 0
    title_state = locations[TITLE_STATE_ADDR - LOCATION_ADDR] if 0 <= TITLE_STATE_ADDR - LOCATION_ADDR < len(locations) else 0
    alpha = locations[ALPHA_DEFEATED_ADDR - LOCATION_ADDR] if 0 <= ALPHA_DEFEATED_ADDR - LOCATION_ADDR < len(locations) else 0
    on_title = (title_state & 0x04) == 0
    complete = False if canary == 0xFF or on_title else bool(alpha & 0x01)
    return {
        "playerName": list(player_bytes.rstrip(b"\x00")),
        "scriptVersion": SCRIPT_VERSION,
        "locations": {format(LOCATION_ADDR + index, "x"): value for index, value in enumerate(locations)},
        "gameComplete": complete,
    }


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter, endpoint: str, fallback_player_name: str) -> None:
    peer = writer.get_extra_info("peername")
    emit("mmbn3_bridge_client_connected", peer=str(peer or ""))
    try:
        while True:
            line = await reader.readline()
            if not line:
                break
            try:
                incoming = json.loads(line.decode("utf-8"))
                item_count = len(incoming.get("items") or []) if isinstance(incoming, dict) else 0
                payload = await build_connector_payload(endpoint, fallback_player_name)
                writer.write((json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8"))
                await writer.drain()
                emit("mmbn3_bridge_tick", item_count=item_count, location_bytes=len(payload["locations"]))
            except Exception as exc:
                emit("mmbn3_bridge_error", error=str(exc))
                await asyncio.sleep(0.25)
    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:
            pass
        emit("mmbn3_bridge_client_disconnected")


async def main_async() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--memory-socket", required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=28922)
    parser.add_argument("--player-name", default="")
    args = parser.parse_args()
    server = await asyncio.start_server(
        lambda r, w: handle_client(r, w, args.memory_socket, args.player_name),
        args.host,
        args.port,
    )
    emit("mmbn3_bridge_ready", host=args.host, port=args.port, memory_socket=args.memory_socket)
    async with server:
        await server.serve_forever()
    return 0


def main() -> int:
    try:
        return asyncio.run(main_async())
    except KeyboardInterrupt:
        return 0
    except Exception as exc:
        emit("mmbn3_bridge_fatal", error=str(exc))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
