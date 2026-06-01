#!/usr/bin/env python3
"""
Smoke test for the SekaiLink native BizHawk bridge PoC.

This validates protocol compatibility against the existing Python `_bizhawk`
helpers, not real BizHawk integration.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def ensure_repo_syspath() -> str:
    root = str(repo_root())
    if root not in sys.path:
        sys.path.insert(0, root)
    return root


def load_bizhawk_module():
    module_path = repo_root() / "worlds" / "_bizhawk" / "__init__.py"
    spec = importlib.util.spec_from_file_location("sekailink_bizhawk_module", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable_to_load_bizhawk_module:{module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


async def wait_for_line(stream: asyncio.StreamReader, needle: str, timeout: float) -> str:
    while True:
        line = await asyncio.wait_for(stream.readline(), timeout=timeout)
        if not line:
            raise RuntimeError("bridge_exited_before_ready")
        text = line.decode("utf-8", errors="replace").strip()
        if needle in text:
            return text


async def run_smoke(args: argparse.Namespace) -> int:
    root = ensure_repo_syspath()
    bizhawk = load_bizhawk_module()

    env = os.environ.copy()
    env["PYTHONPATH"] = root + (os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")

    bridge_path = repo_root() / "client" / "runtime" / "bizhawk_bridge.py"
    proc = await asyncio.create_subprocess_exec(
        args.python,
        str(bridge_path),
        "--host",
        args.host,
        "--port",
        str(args.port),
        "--system",
        args.system,
        "--rom-hash",
        args.rom_hash,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=root,
        env=env,
    )

    stderr_task = asyncio.create_task(proc.stderr.read())
    ctx = bizhawk.BizHawkContext()
    ctx._port = args.port
    results: dict[str, object] = {"ok": False}

    try:
        ready_line = await wait_for_line(proc.stdout, "listening on", timeout=args.timeout)

        connected = await bizhawk.connect(ctx)
        if not connected:
            raise RuntimeError("connect_failed")

        version = await bizhawk.get_script_version(ctx)
        await bizhawk.ping(ctx)
        system = await bizhawk.get_system(ctx)
        rom_hash = await bizhawk.get_hash(ctx)
        rom_size = await bizhawk.get_memory_size(ctx, "ROM")
        initial = (await bizhawk.read(ctx, [(0, 8, "ROM"), (0x100, 4, "System Bus")]))
        guard_ok = await bizhawk.guarded_read(ctx, [(0, 4, "System Bus")], [(0x100, initial[1], "System Bus")])
        guard_fail = await bizhawk.guarded_read(ctx, [(0, 4, "System Bus")], [(0x100, b"\x00\x00\x00\x00", "System Bus")])
        write_ok = await bizhawk.guarded_write(
            ctx,
            [(0x104, [0xAA, 0xBB], "System Bus")],
            [(0x100, initial[1], "System Bus")],
        )
        after_write = (await bizhawk.read(ctx, [(0x104, 2, "System Bus")]))[0]

        await bizhawk.lock(ctx)
        await bizhawk.unlock(ctx)
        await bizhawk.display_message(ctx, "hello from smoke")
        await bizhawk.set_message_interval(ctx, 0.25)

        results = {
            "ok": True,
            "ready": ready_line,
            "version": version,
            "system": system,
            "rom_hash": rom_hash,
            "rom_size": rom_size,
            "initial_rom_magic": initial[0].decode("ascii", errors="replace"),
            "initial_bus": list(initial[1]),
            "guard_ok": [list(item) for item in guard_ok] if guard_ok is not None else None,
            "guard_fail": guard_fail,
            "write_ok": write_ok,
            "after_write": list(after_write),
        }
    except Exception as exc:
        results = {"ok": False, "error": str(exc)}
    finally:
        with contextlib.suppress(Exception):
            bizhawk.disconnect(ctx)
        if proc.returncode is None:
            proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(proc.wait(), timeout=3)
        stderr = (await stderr_task).decode("utf-8", errors="replace").strip()
        if stderr:
            results["bridge_stderr"] = stderr

    print(json.dumps(results, ensure_ascii=True))
    return 0 if results.get("ok") else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Smoke test for SekaiLink BizHawk bridge")
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=43055)
    parser.add_argument("--system", default="GBA")
    parser.add_argument("--rom-hash", dest="rom_hash", default="SEKAILINKMOCKHASH")
    parser.add_argument("--timeout", type=float, default=5.0)
    return parser.parse_args()


def main() -> int:
    return asyncio.run(run_smoke(parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
