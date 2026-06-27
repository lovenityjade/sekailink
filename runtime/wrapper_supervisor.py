#!/usr/bin/env python3
"""SekaiLink Archipelago wrapper supervisor.

The supervisor is the process boundary between the future bootchain/SKLMI
orchestrator and upstream Archipelago client wrappers.  It resolves a game from
the local registry, starts the matching wrapper with the bundled Python runtime,
normalizes wrapper stdout/stderr into JSONL events, and accepts small JSONL
commands on stdin.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import signal
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


WRAPPER_SCRIPTS = {
    "text": "commonclient_wrapper.py",
    "bizhawk": "bizhawkclient_wrapper.py",
    "sni": "sniclient_wrapper.py",
    "oot": "ootclient_wrapper.py",
    "dolphin": "dolphinclient_wrapper.py",
    "module": "moduleclient_wrapper.py",
    "fixture": "tests/fixtures/supervisor_fixture_wrapper.py",
}

GAME_KEY_ALIASES = {
    "alttp": "a_link_to_the_past",
    "oot": "ocarina_of_time",
    "dk64": "donkey_kong_64",
    "ttyd": "thousand_year_door",
    "tww": "the_wind_waker",
    "tp": "twilight_princess",
    "sms": "super_mario_sunshine",
    "mkdd": "mario_kart_double_dash",
}


@dataclass
class RuntimePaths:
    repo_root: Path
    runtime_root: Path
    ap_root: Path
    registry_path: Path


@dataclass
class WrapperPlan:
    game_key: str
    game: str
    platform: str
    world: str
    wrapper: str
    script: Path
    module: str = ""
    client_file: str = ""
    status: str = "manual"

    def to_json(self) -> dict[str, Any]:
        return {
            "game_key": self.game_key,
            "game": self.game,
            "platform": self.platform,
            "world": self.world,
            "wrapper": self.wrapper,
            "script": str(self.script),
            "module": self.module,
            "client_file": self.client_file,
            "status": self.status,
        }


_log_file: Any = None


def emit(event: str, **payload: Any) -> None:
    line = json.dumps({"event": event, **payload}, ensure_ascii=True, default=str)
    print(line, flush=True)
    if _log_file is not None:
        print(line, file=_log_file, flush=True)


def default_paths() -> RuntimePaths:
    runtime_root = Path(__file__).resolve().parent
    return RuntimePaths(
        repo_root=runtime_root.parent,
        runtime_root=runtime_root,
        ap_root=runtime_root / "ap",
        registry_path=runtime_root / "game-registry" / "archipelago-clients.json",
    )


def load_registry(paths: RuntimePaths) -> dict[str, Any]:
    with paths.registry_path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def normalize(value: str) -> str:
    return "".join(ch for ch in value.lower() if ch.isalnum())


def find_client(registry: dict[str, Any], *, game_key: str = "", game: str = "") -> dict[str, Any] | None:
    clients = registry.get("clients") or []
    if game_key:
        canonical = GAME_KEY_ALIASES.get(game_key.lower(), game_key)
        needle = normalize(canonical)
        for client in clients:
            if normalize(str(client.get("game_key") or "")) == needle:
                return client
    if game:
        needle = normalize(game)
        for client in clients:
            if normalize(str(client.get("game") or "")) == needle:
                return client
    return None


def resolve_python(runtime_root: Path, explicit: str = "") -> Path:
    if explicit:
        return Path(explicit)
    candidates = []
    if os.name == "nt":
        candidates.extend([
            runtime_root / "tools" / "python" / "portable-win" / "tools" / "python.exe",
            runtime_root / "tools" / "python" / "portable-win" / "python.exe",
        ])
    candidates.append(Path(sys.executable))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return Path(sys.executable)


def build_plan(paths: RuntimePaths, args: argparse.Namespace) -> WrapperPlan:
    registry = load_registry(paths)
    client = find_client(registry, game_key=args.game_key, game=args.game)
    if client is None and not args.wrapper:
        raise RuntimeError("wrapper_client_not_found")

    wrapper = args.wrapper or str(client.get("wrapper") or "")
    if wrapper not in WRAPPER_SCRIPTS:
        raise RuntimeError(f"unsupported_wrapper:{wrapper}")
    script = paths.runtime_root / WRAPPER_SCRIPTS[wrapper]
    if not script.exists():
        raise RuntimeError(f"wrapper_script_missing:{script}")

    def text(value: Any) -> str:
        return "" if value is None else str(value)

    return WrapperPlan(
        game_key=args.game_key or text(client.get("game_key") if client else "manual"),
        game=args.game or text(client.get("game") if client else args.game_key or "Manual Wrapper"),
        platform=text(client.get("platform") if client else args.platform or ""),
        world=text(client.get("world") if client else args.world or ""),
        wrapper=wrapper,
        script=script,
        module=args.module or text(client.get("module") if client else ""),
        client_file=text(client.get("client_file") if client else ""),
        status=text(client.get("status") if client else "manual"),
    )


def build_env(paths: RuntimePaths) -> dict[str, str]:
    env = os.environ.copy()
    env["SEKAILINK_AP_ROOT"] = str(paths.ap_root)
    env["SEKAILINK_AP_WRAPPER"] = "1"
    env["SKIP_REQUIREMENTS_UPDATE"] = "1"
    env["KIVY_NO_ARGS"] = "1"
    env["KIVY_NO_FILELOG"] = "1"
    python_path = [str(paths.runtime_root), str(paths.ap_root)]
    if env.get("PYTHONPATH"):
        python_path.append(env["PYTHONPATH"])
    env["PYTHONPATH"] = os.pathsep.join(python_path)
    return env


def build_wrapper_argv(python: Path, plan: WrapperPlan, args: argparse.Namespace) -> list[str]:
    argv = [str(python), str(plan.script), "--nogui"]
    if args.connect:
        argv.extend(["--connect", args.connect])
    if args.password:
        argv.extend(["--password", args.password])
    if args.slot:
        argv.extend(["--slot", args.slot])
    if plan.wrapper in ("module", "dolphin"):
        if not plan.module:
            raise RuntimeError(f"wrapper_module_required:{plan.wrapper}:{plan.game_key}")
        argv.extend(["--module", plan.module])
    if plan.wrapper == "sni":
        if args.sni_address:
            argv.extend(["--sni-address", args.sni_address])
        if args.sni_device:
            argv.extend(["--sni-device", args.sni_device])
    return argv


async def pipe_stdout(proc: asyncio.subprocess.Process, plan: WrapperPlan) -> None:
    assert proc.stdout is not None
    while True:
        line = await proc.stdout.readline()
        if not line:
            return
        text = line.decode("utf-8", errors="replace").strip()
        if not text:
            continue
        try:
            data = json.loads(text)
        except json.JSONDecodeError:
            emit("wrapper_stdout", text=text, wrapper=plan.wrapper, game_key=plan.game_key)
            continue
        if isinstance(data, dict):
            emit("wrapper_event", wrapper=plan.wrapper, game_key=plan.game_key, payload=data)
        else:
            emit("wrapper_stdout_json", wrapper=plan.wrapper, game_key=plan.game_key, payload=data)


async def pipe_stderr(proc: asyncio.subprocess.Process, plan: WrapperPlan) -> None:
    assert proc.stderr is not None
    while True:
        line = await proc.stderr.readline()
        if not line:
            return
        text = line.decode("utf-8", errors="replace").rstrip("\n")
        if text:
            emit("wrapper_stderr", wrapper=plan.wrapper, game_key=plan.game_key, text=text)


async def command_loop(proc: asyncio.subprocess.Process) -> None:
    loop = asyncio.get_running_loop()
    queue: asyncio.Queue[str | None] = asyncio.Queue()

    def reader() -> None:
        try:
            for line in sys.stdin:
                loop.call_soon_threadsafe(queue.put_nowait, line)
        finally:
            loop.call_soon_threadsafe(queue.put_nowait, None)

    threading.Thread(target=reader, name="SekaiLinkWrapperSupervisorStdin", daemon=True).start()

    while proc.returncode is None:
        line = await queue.get()
        if not line:
            return
        text = line.strip()
        if not text:
            continue
        try:
            command = json.loads(text)
        except json.JSONDecodeError:
            command = {"cmd": "command", "text": text}
        cmd = str(command.get("cmd") or "").lower()
        if cmd in ("stop", "shutdown"):
            await send_to_wrapper(proc, {"cmd": "shutdown"})
            return
        if cmd == "status":
            emit("status", running=proc.returncode is None, pid=proc.pid)
            continue
        await send_to_wrapper(proc, command)


async def send_to_wrapper(proc: asyncio.subprocess.Process, payload: dict[str, Any]) -> None:
    if proc.stdin is None or proc.stdin.is_closing():
        emit("command_error", error="wrapper_stdin_closed", command=payload)
        return
    proc.stdin.write((json.dumps(payload, ensure_ascii=True) + "\n").encode("utf-8"))
    await proc.stdin.drain()
    emit("command_forwarded", command=payload.get("cmd") or "unknown")


async def terminate(proc: asyncio.subprocess.Process, timeout: float = 4.0) -> None:
    if proc.returncode is not None:
        return
    try:
        await send_to_wrapper(proc, {"cmd": "shutdown"})
        await asyncio.wait_for(proc.wait(), timeout=timeout)
        return
    except Exception:
        pass
    if proc.returncode is None:
        proc.terminate()
    try:
        await asyncio.wait_for(proc.wait(), timeout=timeout)
    except asyncio.TimeoutError:
        if proc.returncode is None:
            proc.kill()
            await proc.wait()


async def run_launch(args: argparse.Namespace) -> int:
    global _log_file
    paths = default_paths()
    plan = build_plan(paths, args)
    python = resolve_python(paths.runtime_root, args.python)
    argv = build_wrapper_argv(python, plan, args)

    if args.dry_run:
        emit("launch_plan", python=str(python), argv=argv, plan=plan.to_json())
        return 0

    log_dir = Path(args.log_dir) if args.log_dir else paths.runtime_root / "logs" / "wrapper-supervisor"
    log_dir.mkdir(parents=True, exist_ok=True)
    session_id = args.session_id or f"{plan.game_key}-{int(time.time())}"
    log_path = log_dir / f"{session_id}.jsonl"
    _log_file = log_path.open("a", encoding="utf-8")

    try:
        emit("launching", session_id=session_id, python=str(python), argv=argv, plan=plan.to_json(), log_path=str(log_path))
        proc = await asyncio.create_subprocess_exec(
            *argv,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(paths.repo_root),
            env=build_env(paths),
        )
        emit("started", session_id=session_id, pid=proc.pid, wrapper=plan.wrapper, game_key=plan.game_key)

        stop_event = asyncio.Event()

        def _stop_signal() -> None:
            stop_event.set()

        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                loop.add_signal_handler(sig, _stop_signal)
            except NotImplementedError:
                pass

        stdout_task = asyncio.create_task(pipe_stdout(proc, plan))
        stderr_task = asyncio.create_task(pipe_stderr(proc, plan))
        command_task = asyncio.create_task(command_loop(proc))
        wait_task = asyncio.create_task(proc.wait())
        stop_task = asyncio.create_task(stop_event.wait())
        max_runtime_task = None
        if args.max_runtime > 0:
            max_runtime_task = asyncio.create_task(asyncio.sleep(args.max_runtime))

        waiters = {command_task, wait_task, stop_task}
        if max_runtime_task is not None:
            waiters.add(max_runtime_task)
        done, _pending = await asyncio.wait(waiters, return_when=asyncio.FIRST_COMPLETED)
        if max_runtime_task is not None and max_runtime_task in done:
            emit("max_runtime_reached", session_id=session_id, seconds=args.max_runtime)
        if stop_task in done or command_task in done or (max_runtime_task is not None and max_runtime_task in done):
            await terminate(proc)
        returncode = await proc.wait()
        for task in (stdout_task, stderr_task, command_task, wait_task, stop_task, max_runtime_task):
            if task is None:
                continue
            if not task.done():
                task.cancel()
        emit("exited", session_id=session_id, pid=proc.pid, returncode=returncode)
        return int(returncode or 0)
    finally:
        if _log_file is not None:
            _log_file.close()
            _log_file = None


def list_clients(args: argparse.Namespace) -> int:
    registry = load_registry(default_paths())
    clients = registry.get("clients") or []
    if args.status:
        clients = [client for client in clients if str(client.get("status") or "") == args.status]
    if args.wrapper:
        clients = [client for client in clients if str(client.get("wrapper") or "") == args.wrapper]
    print(json.dumps({"ok": True, "clients": clients}, ensure_ascii=True, indent=2))
    return 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SekaiLink Archipelago wrapper supervisor")
    sub = parser.add_subparsers(dest="command", required=True)

    list_parser = sub.add_parser("list", help="List registered wrapper clients")
    list_parser.add_argument("--status", default="")
    list_parser.add_argument("--wrapper", default="")

    plan_parser = sub.add_parser("plan", help="Resolve a wrapper launch plan")
    launch_parser = sub.add_parser("launch", help="Launch and supervise one wrapper")
    for child in (plan_parser, launch_parser):
        child.add_argument("--game-key", default="")
        child.add_argument("--game", default="")
        child.add_argument("--platform", default="")
        child.add_argument("--world", default="")
        child.add_argument("--wrapper", choices=sorted(WRAPPER_SCRIPTS), default="")
        child.add_argument("--module", default="")
        child.add_argument("--python", default="")
        child.add_argument("--connect", default="")
        child.add_argument("--password", default="")
        child.add_argument("--slot", default="")
        child.add_argument("--sni-address", default="")
        child.add_argument("--sni-device", default="")
        child.add_argument("--session-id", default="")
        child.add_argument("--log-dir", default="")
    launch_parser.add_argument("--dry-run", action="store_true")
    launch_parser.add_argument("--max-runtime", type=float, default=0.0, help="Stop wrapper after N seconds; 0 disables.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.command == "list":
        return list_clients(args)
    if args.command == "plan":
        paths = default_paths()
        plan = build_plan(paths, args)
        python = resolve_python(paths.runtime_root, args.python)
        print(json.dumps({"ok": True, "python": str(python), "plan": plan.to_json(), "argv": build_wrapper_argv(python, plan, args)}, indent=2))
        return 0
    if args.command == "launch":
        return asyncio.run(run_launch(args))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
