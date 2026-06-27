#!/usr/bin/env python3
"""RetroArch UDP memory protocol shim for Sekaiemu.

Some Archipelago clients talk to RetroArch's network command interface with
commands such as READ_CORE_MEMORY and WRITE_CORE_MEMORY. Sekaiemu already
exposes a generic JSON memory socket, so this shim translates the small subset
of RetroArch commands those clients use into Sekaiemu memory requests.
"""

from __future__ import annotations

import argparse
import base64
import json
import socket
import sys
from typing import Any


def emit(event: str, **payload: Any) -> None:
    print(json.dumps({"event": event, **payload}, ensure_ascii=False), flush=True)


def parse_endpoint(endpoint: str) -> tuple[str, Any]:
    endpoint = endpoint.strip()
    if endpoint.startswith("tcp://"):
        rest = endpoint[len("tcp://") :]
        host, _, port_raw = rest.rpartition(":")
        return "tcp", (host or "127.0.0.1", int(port_raw))
    return "unix", endpoint


def memory_request(endpoint: str, requests: list[dict[str, Any]], timeout: float) -> list[dict[str, Any]]:
    mode, target = parse_endpoint(endpoint)
    if mode == "tcp":
        sock = socket.create_connection(target, timeout=timeout)
    else:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(target)
    try:
        payload = (json.dumps(requests, separators=(",", ":")) + "\n").encode("utf-8")
        sock.sendall(payload)
        data = bytearray()
        while b"\n" not in data:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data.extend(chunk)
        line = bytes(data).split(b"\n", 1)[0].decode("utf-8", errors="replace")
        parsed = json.loads(line) if line else []
        return parsed if isinstance(parsed, list) else [{"type": "ERROR", "err": "bad_memory_response"}]
    finally:
        sock.close()


def first_response(responses: list[dict[str, Any]], expected: str) -> dict[str, Any]:
    for response in responses:
        if response.get("type") == expected or response.get("type") == "ERROR":
            return response
    return {"type": "ERROR", "err": "missing_memory_response"}


def parse_int(value: str) -> int:
    return int(value, 16) if value.lower().startswith("0x") else int(value, 10)


def format_failure(command: str, address: str, reason: str = "read_failed") -> bytes:
    emit("retroarch_bridge_command_failed", command=command, address=address, reason=reason)
    return f"{command} {address} -1 {reason}\n".encode("ascii", errors="replace")


def is_interesting_gameboy_address(address: int, size: int) -> bool:
    interesting = {
        0x0134,  # LADX slot/auth name in ROM header
        0xC0FB,  # LADX safe gameplay guard
        0xDB95,  # LADX gameplay mode
        0xDAA3,  # LADX start item/check bit lives in the location flag block
        0xDDF6,  # LADX receive index high
        0xDDF7,  # LADX receive index low
        0xDDF8,  # LADX command block
    }
    end = address + max(1, size)
    return any(address <= marker < end for marker in interesting)


def maybe_emit_read(args: argparse.Namespace, address: int, size: int, data: bytes) -> None:
    args.read_count += 1
    preview = data[:16].hex()
    interesting = is_interesting_gameboy_address(address, size)
    key = f"{address:x}:{size}"
    changed = args.read_snapshots.get(key) != preview
    if interesting and changed:
        args.read_snapshots[key] = preview
    if (interesting and (changed or args.read_count % 500 == 0)) or args.read_count <= 12 or args.read_count % 1000 == 0:
        emit(
            "retroarch_bridge_read",
            count=args.read_count,
            address=hex(address),
            size=size,
            preview=preview,
        )


def emit_write(args: argparse.Namespace, address: int, values: bytes) -> None:
    args.write_count += 1
    emit(
        "retroarch_bridge_write",
        count=args.write_count,
        address=hex(address),
        size=len(values),
        value=values[:32].hex(),
    )


def handle_command(args: argparse.Namespace, command_line: str) -> bytes:
    command_line = command_line.strip()
    if not command_line:
        return b""
    parts = command_line.split()
    command = parts[0].upper()

    if command == "VERSION":
        return b"1.19.0\n"
    if command == "GET_STATUS":
        args.status_count += 1
        if args.status_count <= 6 or args.status_count % 60 == 0:
            emit("retroarch_bridge_status", count=args.status_count, core_type=args.core_type, rom_name=args.rom_name)
        return f"GET_STATUS PLAYING {args.core_type},{args.rom_name},{args.rom_crc}\n".encode("ascii", errors="replace")

    if command == "READ_CORE_MEMORY" and len(parts) >= 3:
        address_raw = parts[1]
        try:
            address = parse_int(address_raw)
            size = max(1, min(65536, parse_int(parts[2])))
        except ValueError:
            return format_failure(command, address_raw, "invalid_arguments")
        responses = memory_request(args.memory_socket, [{
            "type": "READ",
            "domain": "System Bus",
            "address": address,
            "size": size,
        }], args.timeout)
        response = first_response(responses, "READ_RESPONSE")
        if response.get("type") == "ERROR":
            return format_failure(command, address_raw, str(response.get("err") or "read_failed"))
        try:
            data = base64.b64decode(str(response.get("value") or ""), validate=True)
        except Exception:
            return format_failure(command, address_raw, "bad_read_payload")
        maybe_emit_read(args, address, size, data)
        return f"{command} {hex(address)} {data.hex()}\n".encode("ascii")

    if command == "WRITE_CORE_MEMORY" and len(parts) >= 3:
        address_raw = parts[1]
        try:
            address = parse_int(address_raw)
            values = bytes(parse_int(part) & 0xFF for part in parts[2:])
        except ValueError:
            return format_failure(command, address_raw, "invalid_arguments")
        responses = memory_request(args.memory_socket, [{
            "type": "WRITE",
            "domain": "System Bus",
            "address": address,
            "value": base64.b64encode(values).decode("ascii"),
        }], args.timeout)
        response = first_response(responses, "WRITE_RESPONSE")
        if response.get("type") == "ERROR":
            return format_failure(command, address_raw, str(response.get("err") or "write_failed"))
        emit_write(args, address, values)
        return f"{command} {hex(address)} {len(values)}\n".encode("ascii")

    emit("retroarch_bridge_unknown_command", command=command_line)
    return f"{command} -1 unsupported_command\n".encode("ascii", errors="replace")


def serve(args: argparse.Namespace) -> int:
    args.read_count = 0
    args.write_count = 0
    args.status_count = 0
    args.read_snapshots = {}
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.host, args.port))
    sock.settimeout(0.25)
    emit("retroarch_bridge_ready", host=args.host, port=args.port, memory_socket=args.memory_socket)
    while True:
        try:
            payload, addr = sock.recvfrom(8192)
        except socket.timeout:
            continue
        except KeyboardInterrupt:
            break
        command_line = payload.decode("ascii", errors="replace").strip()
        try:
            response = handle_command(args, command_line)
        except Exception as exc:
            emit("retroarch_bridge_error", command=command_line, error=str(exc))
            response = format_failure(command_line.split()[0] if command_line else "ERROR", "0x0", str(exc))
        if response:
            sock.sendto(response, addr)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--memory-socket", required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=55355)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--core-type", default="game_boy")
    parser.add_argument("--rom-name", default="Sekaiemu")
    parser.add_argument("--rom-crc", default="00000000")
    return serve(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
