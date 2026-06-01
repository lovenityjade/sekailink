#!/usr/bin/env python3
from __future__ import annotations

import argparse
import asyncio
import importlib.util
import os
import tempfile
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def load_bizhawk_module():
    module_path = repo_root() / "worlds" / "_bizhawk" / "__init__.py"
    spec = importlib.util.spec_from_file_location("sekailink_bizhawk_module", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable_to_load_bizhawk_module:{module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


async def wait_for_port(host: str, port: int, timeout: float) -> None:
    deadline = asyncio.get_event_loop().time() + timeout
    while True:
        try:
            reader, writer = await asyncio.open_connection(host, port)
            writer.close()
            await writer.wait_closed()
            return
        except OSError:
            if asyncio.get_event_loop().time() >= deadline:
                raise RuntimeError("runtime_listen_timeout")
            await asyncio.sleep(0.1)


async def run_smoke(args: argparse.Namespace) -> int:
    bizhawk = load_bizhawk_module()
    root = repo_root()
    env = os.environ.copy()
    existing = env.get("LD_LIBRARY_PATH", "")
    env["LD_LIBRARY_PATH"] = args.library_dir + (os.pathsep + existing if existing else "")

    proc = await asyncio.create_subprocess_exec(
        args.binary,
        "--rom",
        args.rom,
        "--save",
        args.save,
        "--port",
        str(args.port),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=root,
        env=env,
    )

    ctx = bizhawk.BizHawkContext()
    ctx._port = args.port

    try:
        wait_task = asyncio.create_task(wait_for_port("127.0.0.1", args.port, args.timeout))
        done, _ = await asyncio.wait({wait_task, asyncio.create_task(proc.wait())}, return_when=asyncio.FIRST_COMPLETED)
        if wait_task not in done:
            stdout = (await proc.stdout.read()).decode("utf-8", errors="replace").strip()
            stderr = (await proc.stderr.read()).decode("utf-8", errors="replace").strip()
            raise RuntimeError(f"runtime_exited_early stdout={stdout!r} stderr={stderr!r}")
        await wait_task
        if not await bizhawk.connect(ctx):
            raise RuntimeError("connect_failed")

        version = await bizhawk.get_script_version(ctx)
        await bizhawk.ping(ctx)
        system = await bizhawk.get_system(ctx)
        rom_hash = await bizhawk.get_hash(ctx)
        rom_size = await bizhawk.get_memory_size(ctx, "ROM")
        initial = await bizhawk.read(ctx, [(0, 4, "ROM"), (0x02000000, 4, "System Bus")])
        guard_ok = await bizhawk.guarded_read(ctx, [(0, 4, "ROM")], [(0, initial[0], "ROM")])
        write_ok = await bizhawk.guarded_write(
            ctx,
            [(0x02000000, [1, 2, 3, 4], "System Bus")],
            [(0x02000000, initial[1], "System Bus")],
        )
        after_write = await bizhawk.read(ctx, [(0x02000000, 4, "System Bus")])
        await bizhawk.display_message(ctx, "smoke")
        await bizhawk.set_message_interval(ctx, 0.25)

        print(
            {
                "ok": True,
                "version": version,
                "system": system,
                "rom_hash": rom_hash,
                "rom_size": rom_size,
                "rom0": list(initial[0]),
                "ewram0": list(initial[1]),
                "guard_ok": [list(item) for item in guard_ok] if guard_ok else None,
                "write_ok": write_ok,
                "ewram0_after": list(after_write[0]),
            }
        )
        return 0
    finally:
        bizhawk.disconnect(ctx)
        if proc.returncode is None:
            proc.terminate()
            try:
                await asyncio.wait_for(proc.wait(), timeout=3)
            except asyncio.TimeoutError:
                proc.kill()
                await proc.wait()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Smoke test for SekaiLink GBA runtime")
    parser.add_argument("--binary", default="/tmp/sekailink-runtime-build/sekailink_runtime_gba")
    parser.add_argument("--library-dir", default="/tmp/sekailink-runtime-build/mgba")
    parser.add_argument("--rom", required=True)
    parser.add_argument("--save", default=str(Path(tempfile.gettempdir()) / "sekailink-runtime-gba-smoke.sav"))
    parser.add_argument("--port", type=int, default=43055)
    parser.add_argument("--timeout", type=float, default=5.0)
    return parser.parse_args()


def main() -> int:
    return asyncio.run(run_smoke(parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
