#!/usr/bin/env python3
"""
Headless smoke test for SekaiLink AP client wrappers.

MVP goal:
- Run a local mock Archipelago websocket server.
- Launch a wrapper (commonclient or bizhawkclient) as a subprocess.
- Drive one connect flow through stdin JSON commands.
- Assert we can process RoomInfo -> GetDataPackage without the old websocket ".open" crash.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class SmokeState:
    room_info_seen: asyncio.Event = field(default_factory=asyncio.Event)
    get_data_package_seen: asyncio.Event = field(default_factory=asyncio.Event)
    wrapper_events: list[dict[str, Any]] = field(default_factory=list)
    wrapper_stderr: list[str] = field(default_factory=list)
    client_cmds_seen: list[str] = field(default_factory=list)
    failures: list[str] = field(default_factory=list)


def repo_root() -> Path:
    # client/runtime/tests/headless_ap_smoke.py -> repo root is parents[3]
    return Path(__file__).resolve().parents[3]


def ensure_repo_syspath() -> str:
    root = str(repo_root())
    if root not in sys.path:
        sys.path.insert(0, root)
    return root


def wrapper_script_path(which: str) -> Path:
    base = repo_root() / "client" / "runtime"
    if which == "bizhawk":
        return base / "bizhawkclient_wrapper.py"
    return base / "commonclient_wrapper.py"


def make_room_info() -> dict[str, Any]:
    return {
        "cmd": "RoomInfo",
        "seed_name": "sekailink-headless-smoke",
        "version": [0, 7, 212],
        "generator_version": [0, 7, 212],
        "tags": ["AP", "WebHost"],
        "password": False,
        "permissions": {
            "release": 0,
            "collect": 2,
            "remaining": 1,
        },
        "hint_cost": 5,
        "location_check_points": 1,
        "games": ["Pokemon FireRed and LeafGreen", "Archipelago"],
        "datapackage_checksums": {
            "Pokemon FireRed and LeafGreen": "mock-frlg-checksum",
            "Archipelago": "mock-ap-checksum",
        },
    }


def make_datapackage(games: list[str]) -> dict[str, Any]:
    payload_games: dict[str, Any] = {}
    for game in games:
        payload_games[game] = {
            "checksum": f"smoke-{game}",
            "item_name_to_id": {},
            "location_name_to_id": {},
        }
    return {"cmd": "DataPackage", "data": {"games": payload_games}}


async def mock_ap_handler(connection: Any, state: SmokeState) -> None:
    await connection.send(json.dumps([make_room_info()]))
    async for raw in connection:
        try:
            packet = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if not isinstance(packet, list):
            continue
        for msg in packet:
            if not isinstance(msg, dict):
                continue
            cmd = str(msg.get("cmd") or "")
            if cmd:
                state.client_cmds_seen.append(cmd)
            if cmd == "GetDataPackage":
                games = msg.get("games") or []
                if isinstance(games, list) and games:
                    await connection.send(json.dumps([make_datapackage([str(g) for g in games])]))
                state.get_data_package_seen.set()


async def _read_wrapper_stdout(stream: asyncio.StreamReader, state: SmokeState) -> None:
    while True:
        line = await stream.readline()
        if not line:
            return
        text = line.decode("utf-8", errors="replace").strip()
        if not text:
            continue
        try:
            event = json.loads(text)
        except json.JSONDecodeError:
            continue
        if isinstance(event, dict):
            state.wrapper_events.append(event)
            if event.get("event") == "room_info":
                state.room_info_seen.set()
            message = str(event.get("message") or "")
            if "AttributeError" in message and "object has no attribute 'open'" in message:
                state.failures.append("wrapper_event_attributeerror_open")


async def _read_wrapper_stderr(stream: asyncio.StreamReader, state: SmokeState) -> None:
    while True:
        line = await stream.readline()
        if not line:
            return
        text = line.decode("utf-8", errors="replace").rstrip("\n")
        if text:
            state.wrapper_stderr.append(text)
            if "AttributeError" in text and "object has no attribute 'open'" in text:
                state.failures.append("stderr_attributeerror_open")


async def _send_wrapper_cmd(proc: asyncio.subprocess.Process, payload: dict[str, Any]) -> None:
    if proc.stdin is None:
        return
    proc.stdin.write((json.dumps(payload, ensure_ascii=True) + "\n").encode("utf-8"))
    await proc.stdin.drain()


async def run_smoke(args: argparse.Namespace) -> int:
    import websockets

    state = SmokeState()
    host = args.host

    async def _handler(connection: Any) -> None:
        await mock_ap_handler(connection, state)

    server = await websockets.serve(_handler, host, args.port)
    sockets = server.sockets or []
    if not sockets:
        print("ERROR: mock server failed to bind sockets", file=sys.stderr)
        return 2
    port = int(sockets[0].getsockname()[1])

    wrapper_path = wrapper_script_path(args.wrapper)
    if not wrapper_path.exists():
        print(f"ERROR: wrapper missing: {wrapper_path}", file=sys.stderr)
        server.close()
        await server.wait_closed()
        return 2

    env = os.environ.copy()
    root = ensure_repo_syspath()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    env.setdefault("SKIP_REQUIREMENTS_UPDATE", "1")

    proc = await asyncio.create_subprocess_exec(
        args.python,
        str(wrapper_path),
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env,
        cwd=root,
    )

    stdout_task = asyncio.create_task(_read_wrapper_stdout(proc.stdout, state))
    stderr_task = asyncio.create_task(_read_wrapper_stderr(proc.stderr, state))

    forced_terminate = False
    try:
        await _send_wrapper_cmd(
            proc,
            {
                "cmd": "connect",
                "address": f"{host}:{port}",
                "slot": "HeadlessSmokeSlot",
            },
        )

        await asyncio.wait_for(state.room_info_seen.wait(), timeout=args.timeout)
        await asyncio.wait_for(state.get_data_package_seen.wait(), timeout=args.timeout)

        await _send_wrapper_cmd(proc, {"cmd": "shutdown"})
        with contextlib.suppress(asyncio.TimeoutError):
            await asyncio.wait_for(proc.wait(), timeout=5)
    except asyncio.TimeoutError:
        state.failures.append("timeout")
    finally:
        if proc.returncode is None:
            forced_terminate = True
            proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(proc.wait(), timeout=3)
        server.close()
        await server.wait_closed()
        stdout_task.cancel()
        stderr_task.cancel()
        with contextlib.suppress(Exception):
            await stdout_task
        with contextlib.suppress(Exception):
            await stderr_task

    # Some wrappers can take longer to teardown because they keep background tasks
    # alive after the handshake. For smoke coverage we accept a forced terminate once
    # the target handshake checks already passed.
    if proc.returncode not in (0, None) and not (
        forced_terminate and state.room_info_seen.is_set() and state.get_data_package_seen.is_set()
    ):
        state.failures.append(f"wrapper_exit_{proc.returncode}")

    if not state.room_info_seen.is_set():
        state.failures.append("no_room_info")
    if not state.get_data_package_seen.is_set():
        state.failures.append("no_get_data_package")

    summary = {
        "ok": not state.failures,
        "wrapper": args.wrapper,
        "python": args.python,
        "mock_server": f"{host}:{port}",
        "client_cmds_seen": state.client_cmds_seen,
        "failures": state.failures,
        "events_count": len(state.wrapper_events),
        "stderr_lines": len(state.wrapper_stderr),
    }
    print(json.dumps(summary, ensure_ascii=True))

    if state.failures:
        # Print the last few stderr lines to make debugging immediate.
        tail = state.wrapper_stderr[-15:]
        for line in tail:
            print(f"[wrapper stderr] {line}", file=sys.stderr)
        return 1
    return 0


async def run_compat_smoke(args: argparse.Namespace) -> int:
    root = repo_root()
    common_client = root / "CommonClient.py"
    failures: list[str] = []
    if not common_client.exists():
        failures.append("missing_commonclient_py")
        source = ""
    else:
        source = common_client.read_text(encoding="utf-8")

    # Static guard check for websocket compatibility path that fixed:
    # AttributeError: 'ClientConnection' object has no attribute 'open'
    if "async def send_msgs" not in source:
        failures.append("missing_send_msgs")
    if "getattr(socket, \"closed\", False)" not in source:
        failures.append("missing_closed_guard")
    if "hasattr(socket, \"open\")" not in source:
        failures.append("missing_open_guard")
    if "await socket.send(encode(msgs))" not in source:
        failures.append("missing_send_call")

    summary = {
        "ok": not failures,
        "mode": "compat",
        "file": str(common_client),
        "failures": failures,
        "checked": [
            "send_msgs_exists",
            "closed_guard_present",
            "open_guard_present",
            "send_call_present",
        ],
    }
    print(json.dumps(summary, ensure_ascii=True))
    return 0 if not failures else 1


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SekaiLink headless AP smoke test")
    parser.add_argument("--mode", choices=("auto", "integration", "compat"), default="auto")
    parser.add_argument("--wrapper", choices=("common", "bizhawk"), default="common")
    parser.add_argument("--python", default=sys.executable, help="Python interpreter for wrapper subprocess")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0, help="0 = random free port")
    parser.add_argument("--timeout", type=float, default=15.0)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.mode == "compat":
        return asyncio.run(run_compat_smoke(args))
    if args.mode == "integration":
        return asyncio.run(run_smoke(args))
    try:
        return asyncio.run(run_smoke(args))
    except OSError as exc:
        if "could not bind on any address" not in str(exc):
            raise
        print(
            json.dumps(
                {
                    "ok": False,
                    "mode": "integration",
                    "fallback": "compat",
                    "reason": str(exc),
                },
                ensure_ascii=True,
            ),
            file=sys.stderr,
        )
        return asyncio.run(run_compat_smoke(args))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
