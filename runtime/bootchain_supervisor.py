#!/usr/bin/env python3
"""Generic SekaiLink runtime bootchain supervisor.

This prepares the end-to-end launch chain around the wrapper supervisor without
hardcoding ALTTP.  It is intentionally JSONL-oriented so Electron, SKLMI, or a
manual test shell can drive it the same way.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import random
import signal
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


GENERIC_WRAPPERS = {"sni", "bizhawk"}
CORE_BY_PLATFORM = {
    "NES": ["nestopia_libretro", "fceumm_libretro"],
    "GB": ["gambatte_libretro"],
    "GBC": ["gambatte_libretro"],
    "GBA": ["mgba_libretro"],
    "SNES": ["bsnes_mercury_performance_libretro", "snes9x_libretro"],
    "N64": ["mupen64plus_next_libretro"],
}


@dataclass
class Paths:
    runtime_root: Path
    repo_root: Path
    registry: Path


@dataclass
class BootchainPlan:
    session_id: str
    game_key: str
    game: str
    platform: str
    wrapper: str
    world: str
    server: str = ""
    slot: str = ""
    wrapper_command: list[str] = field(default_factory=list)
    sekaiemu_command: list[str] = field(default_factory=list)
    tracker_command: list[str] = field(default_factory=list)
    log_dir: str = ""
    notes: list[str] = field(default_factory=list)
    readiness: dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> dict[str, Any]:
        return {
            "session_id": self.session_id,
            "game_key": self.game_key,
            "game": self.game,
            "platform": self.platform,
            "wrapper": self.wrapper,
            "world": self.world,
            "server": self.server,
            "slot": self.slot,
            "wrapper_command": self.wrapper_command,
            "sekaiemu_command": self.sekaiemu_command,
            "tracker_command": self.tracker_command,
            "log_dir": self.log_dir,
            "notes": self.notes,
            "readiness": self.readiness,
        }


def emit(event: str, **payload: Any) -> None:
    print(json.dumps({"event": event, **payload}, ensure_ascii=True, default=str), flush=True)


def paths() -> Paths:
    runtime_root = Path(__file__).resolve().parent
    return Paths(
        runtime_root=runtime_root,
        repo_root=runtime_root.parent,
        registry=runtime_root / "game-registry" / "archipelago-clients.json",
    )


def normalize(value: str) -> str:
    return "".join(ch for ch in str(value).lower() if ch.isalnum())


def load_registry() -> dict[str, Any]:
    with paths().registry.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def clients() -> list[dict[str, Any]]:
    return list(load_registry().get("clients") or [])


def aliases() -> dict[str, str]:
    return {
        "alttp": "a_link_to_the_past",
        "oot": "ocarina_of_time",
        "dk64": "donkey_kong_64",
        "ttyd": "thousand_year_door",
        "tww": "the_wind_waker",
        "tp": "twilight_princess",
        "sms": "super_mario_sunshine",
    }


def find_client(game_key: str = "", game: str = "") -> dict[str, Any] | None:
    all_clients = clients()
    if game_key:
        canonical = aliases().get(game_key.lower(), game_key)
        needle = normalize(canonical)
        for client in all_clients:
            if normalize(client.get("game_key", "")) == needle:
                return client
    if game:
        needle = normalize(game)
        for client in all_clients:
            if normalize(client.get("game", "")) == needle:
                return client
    return None


def generic_clients() -> list[dict[str, Any]]:
    return [
        client for client in clients()
        if client.get("status") == "enabled" and client.get("wrapper") in GENERIC_WRAPPERS
    ]


def pick_random_generic(seed: str = "") -> dict[str, Any]:
    pool = generic_clients()
    if not pool:
        raise RuntimeError("no_generic_clients")
    rng = random.Random(seed or None)
    return rng.choice(pool)


def find_existing(candidates: list[Path]) -> Path | None:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def resolve_python(runtime_root: Path) -> Path:
    if os.name == "nt":
        win = runtime_root / "tools" / "python" / "portable-win" / "tools" / "python.exe"
        if win.exists():
            return win
    return Path(sys.executable)


def resolve_core(runtime_root: Path, platform: str, explicit: str = "") -> Path | None:
    if explicit:
        candidate = Path(explicit)
        return candidate if candidate.exists() else None
    names = CORE_BY_PLATFORM.get(platform, [])
    suffixes = [".dll"] if os.name == "nt" else [".so", ".dylib"]
    roots = [
        runtime_root / "cores",
        runtime_root / "platforms" / "win32-x64" / "cores",
    ]
    for root in roots:
        for name in names:
            for suffix in suffixes:
                found = root / f"{name}{suffix}"
                if found.exists():
                    return found
    return None


def resolve_sekaiemu(runtime_root: Path, explicit: str = "") -> Path | None:
    if explicit:
        candidate = Path(explicit)
        return candidate if candidate.exists() else None
    names = ["sekaiemu_libretro_spike.exe"] if os.name == "nt" else ["sekaiemu_libretro_spike"]
    candidates = []
    for name in names:
        candidates.extend([
            runtime_root / "bin" / name,
            runtime_root / "platforms" / "win32-x64" / "bin" / name,
        ])
    return find_existing(candidates)


def resolve_poptracker(runtime_root: Path, explicit: str = "") -> Path | None:
    if explicit:
        candidate = Path(explicit)
        return candidate if candidate.exists() else None
    names = ["sekailink-poptracker.exe", "sekailink-poptracker", "poptracker.exe", "poptracker"]
    return find_existing([runtime_root / "poptracker" / name for name in names])


def build_plan(args: argparse.Namespace) -> BootchainPlan:
    p = paths()
    client = pick_random_generic(args.random_seed) if args.random_generic else find_client(args.game_key, args.game)
    if client is None:
        raise RuntimeError("bootchain_client_not_found")
    if args.generic_only and client.get("wrapper") not in GENERIC_WRAPPERS:
        raise RuntimeError(f"bootchain_client_not_generic:{client.get('wrapper')}")

    game_key = args.game_key or str(client.get("game_key") or "game")
    session_id = args.session_id or f"{normalize(game_key) or 'game'}-{int(time.time())}"
    log_dir = Path(args.log_dir) if args.log_dir else p.runtime_root / "logs" / "bootchain" / session_id
    wrapper_log_dir = log_dir / "wrapper"
    wrapper_log_dir.mkdir(parents=True, exist_ok=True)

    python = resolve_python(p.runtime_root)
    wrapper_command = [
        str(python),
        str(p.runtime_root / "wrapper_supervisor.py"),
        "launch",
        "--game-key", game_key,
        "--session-id", session_id,
        "--log-dir", str(wrapper_log_dir),
    ]
    if args.wrapper_override:
        wrapper_command.extend(["--wrapper", args.wrapper_override])
    if args.server:
        wrapper_command.extend(["--connect", args.server])
    if args.slot:
        wrapper_command.extend(["--slot", args.slot])
    if args.password:
        wrapper_command.extend(["--password", args.password])
    if args.max_runtime > 0:
        wrapper_command.extend(["--max-runtime", str(args.max_runtime)])

    platform = str(client.get("platform") or "")
    sekaiemu = resolve_sekaiemu(p.runtime_root, args.sekaiemu)
    core = resolve_core(p.runtime_root, platform, args.core)
    sekaiemu_command: list[str] = []
    notes: list[str] = []
    readiness = {
        "wrapper_supervisor": (p.runtime_root / "wrapper_supervisor.py").exists(),
        "python": python.exists(),
        "generic_wrapper": client.get("wrapper") in GENERIC_WRAPPERS,
        "sekaiemu": bool(sekaiemu),
        "core": bool(core),
        "rom": bool(args.rom and Path(args.rom).exists()),
        "server": bool(args.server),
        "slot": bool(args.slot),
    }
    if not readiness["rom"]:
        notes.append("rom_missing_or_not_provided; wrapper can launch but emulator E2E needs --rom")
    if not readiness["core"]:
        notes.append(f"core_missing_for_platform:{platform}")
    if not readiness["sekaiemu"]:
        notes.append("sekaiemu_missing")
    if sekaiemu and core and args.rom:
        memory_endpoint = args.memory_endpoint or (
            "tcp://127.0.0.1:43080" if os.name == "nt" else str(Path("/tmp") / f"sekailink-{session_id}.sock")
        )
        sekaiemu_command = [
            str(sekaiemu),
            "--core", str(core),
            "--game", str(Path(args.rom)),
            "--system-dir", str(log_dir / "system"),
            "--save-dir", str(log_dir / "saves"),
            "--log-dir", str(log_dir / "sekaiemu"),
            "--memory-socket", memory_endpoint,
        ]

    tracker = resolve_poptracker(p.runtime_root, args.poptracker)
    tracker_command: list[str] = []
    tracker_pack = str(args.tracker_pack or "")
    if tracker and tracker_pack:
        tracker_command = [str(tracker), "--pack", tracker_pack]
        if args.server:
            tracker_command.extend(["--ap-host", args.server])
        if args.slot:
            tracker_command.extend(["--ap-slot", args.slot])
        if args.password:
            tracker_command.extend(["--ap-pass", args.password])
    elif not tracker:
        notes.append("poptracker_missing_or_not_found")
    elif not tracker_pack:
        notes.append("tracker_pack_not_provided")

    return BootchainPlan(
        session_id=session_id,
        game_key=game_key,
        game=str(client.get("game") or ""),
        platform=platform,
        wrapper=str(client.get("wrapper") or ""),
        world=str(client.get("world") or ""),
        server=args.server,
        slot=args.slot,
        wrapper_command=wrapper_command,
        sekaiemu_command=sekaiemu_command,
        tracker_command=tracker_command,
        log_dir=str(log_dir),
        notes=notes,
        readiness=readiness,
    )


async def attach_child(name: str, proc: asyncio.subprocess.Process) -> None:
    async def pipe(stream: asyncio.StreamReader | None, level: str) -> None:
        if stream is None:
            return
        while True:
            line = await stream.readline()
            if not line:
                return
            text = line.decode("utf-8", errors="replace").rstrip("\n")
            if text:
                try:
                    payload = json.loads(text)
                except json.JSONDecodeError:
                    payload = {"text": text}
                emit("child_line", process=name, level=level, payload=payload)
    await asyncio.gather(pipe(proc.stdout, "stdout"), pipe(proc.stderr, "stderr"))


async def terminate(proc: asyncio.subprocess.Process) -> None:
    if proc.returncode is not None:
        return
    try:
        proc.terminate()
        await asyncio.wait_for(proc.wait(), timeout=4)
    except Exception:
        if proc.returncode is None:
            proc.kill()
            await proc.wait()


async def launch(args: argparse.Namespace) -> int:
    plan = build_plan(args)
    emit("bootchain_plan", plan=plan.to_json())
    if args.dry_run:
        return 0

    children: list[tuple[str, asyncio.subprocess.Process]] = []
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:
            pass

    async def start_child(name: str, command: list[str]) -> None:
        if not command:
            return
        emit("child_starting", process=name, argv=command)
        proc = await asyncio.create_subprocess_exec(
            *command,
            stdin=asyncio.subprocess.PIPE if name == "wrapper" else asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(paths().repo_root),
        )
        children.append((name, proc))
        emit("child_started", process=name, pid=proc.pid)
        asyncio.create_task(attach_child(name, proc))

    if args.launch_sekaiemu:
        await start_child("sekaiemu", plan.sekaiemu_command)
    if args.launch_tracker:
        await start_child("tracker", plan.tracker_command)
    await start_child("wrapper", plan.wrapper_command)

    if not children:
        emit("bootchain_error", error="nothing_to_launch")
        return 2

    wrapper = next((proc for name, proc in children if name == "wrapper"), None)
    if wrapper and args.server and args.slot and wrapper.stdin:
        wrapper.stdin.write((json.dumps({"cmd": "connect", "address": args.server, "slot": args.slot, "password": args.password or ""}) + "\n").encode("utf-8"))
        await wrapper.stdin.drain()

    wait_tasks = [asyncio.create_task(proc.wait()) for _name, proc in children]
    stop_task = asyncio.create_task(stop.wait())
    timeout_task = asyncio.create_task(asyncio.sleep(args.max_runtime)) if args.max_runtime > 0 else None
    tasks = set(wait_tasks + [stop_task])
    if timeout_task:
        tasks.add(timeout_task)
    done, _pending = await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
    if timeout_task and timeout_task in done:
        emit("bootchain_timeout", seconds=args.max_runtime)
    for _name, proc in children:
        await terminate(proc)
    for task in tasks:
        if not task.done():
            task.cancel()
    emit("bootchain_exited", children=[{"process": name, "pid": proc.pid, "returncode": proc.returncode} for name, proc in children])
    return 0


def list_generic() -> int:
    print(json.dumps({"ok": True, "clients": generic_clients()}, indent=2))
    return 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SekaiLink generic runtime bootchain supervisor")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list-generic", help="List enabled generic wrapper candidates")
    plan_parser = sub.add_parser("plan", help="Build an E2E bootchain plan")
    launch_parser = sub.add_parser("launch", help="Launch bootchain components")
    for child in (plan_parser, launch_parser):
        child.add_argument("--game-key", default="")
        child.add_argument("--game", default="")
        child.add_argument("--random-generic", action="store_true")
        child.add_argument("--random-seed", default="")
        child.add_argument("--generic-only", action="store_true", default=True)
        child.add_argument("--server", default="")
        child.add_argument("--slot", default="")
        child.add_argument("--password", default="")
        child.add_argument("--rom", default="")
        child.add_argument("--core", default="")
        child.add_argument("--sekaiemu", default="")
        child.add_argument("--memory-endpoint", default="")
        child.add_argument("--poptracker", default="")
        child.add_argument("--tracker-pack", default="")
        child.add_argument("--wrapper-override", default="", help=argparse.SUPPRESS)
        child.add_argument("--session-id", default="")
        child.add_argument("--log-dir", default="")
        child.add_argument("--max-runtime", type=float, default=0.0)
    launch_parser.add_argument("--dry-run", action="store_true")
    launch_parser.add_argument("--launch-sekaiemu", action="store_true")
    launch_parser.add_argument("--launch-tracker", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.command == "list-generic":
        return list_generic()
    if args.command == "plan":
        print(json.dumps({"ok": True, "plan": build_plan(args).to_json()}, indent=2))
        return 0
    if args.command == "launch":
        return asyncio.run(launch(args))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
