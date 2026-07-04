#!/usr/bin/env python3
"""Smoke-test Sekaiemu + SNI bridge + Archipelago SNI clients.

This intentionally uses already generated test artifacts from the current
Client Core lobby flow.  It does not generate seeds and it does not patch ROMs.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import re
import signal
import socket
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import websockets


REPO_ROOT = Path(__file__).resolve().parents[2]
RUNTIME_ROOT = REPO_ROOT / "runtime"
USER_RUNTIME = Path.home() / ".config" / "sekailink-client" / "runtime"
DOWNLOADS_DIR = USER_RUNTIME / "downloads"
ROMS_DIR = USER_RUNTIME / "roms"
LOG_DIR = Path.home() / ".config" / "sekailink-client" / "logs" / "runtime-smoke"
PYTHON = USER_RUNTIME / "tools" / "python" / "venv" / "bin" / "python"
if not PYTHON.exists():
    PYTHON = Path(sys.executable)


PATCH_EXT_TO_MODULE = {
    ".aplttp": "alttp",
    ".apeb": "earthbound",
    ".apdkc": "donkey_kong_country",
    ".apdkc2": "donkey_kong_country_2",
    ".apdkc3": "donkey_kong_country_3",
    ".apffv": "final_fantasy_v",
    ".apkdl3": "kirbys_dream_land_3",
    ".apl2ac": "lufia_ii",
    ".apmmx3": "mega_man_x3",
    ".apsmw": "super_mario_world",
    ".apsm": "super_metroid",
    ".apsmz3": "smz3",
    ".apsom": "secret_of_mana",
}


@dataclass
class Artifact:
    module_id: str
    patch_path: Path
    rom_path: Path
    slot: str
    seed: str
    ap_game: str = ""
    game_key: str = ""
    world: str = ""
    status: str = "pending"
    notes: list[str] = field(default_factory=list)
    events: list[dict[str, Any]] = field(default_factory=list)


def load_manifest(module_id: str) -> dict[str, Any]:
    path = RUNTIME_ROOT / "modules" / module_id / "manifest.json"
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def slot_from_name(path: Path) -> tuple[str, str]:
    match = re.match(r"AP_([0-9]+)_P[0-9]+_(.+?)(?:-[0-9]+)?(?:\.[^.]+)$", path.name)
    if not match:
        return "", ""
    return match.group(1), match.group(2)


def discover_artifacts(seed_prefix: str) -> list[Artifact]:
    artifacts: list[Artifact] = []
    for rom_path in sorted(ROMS_DIR.glob(f"AP_{seed_prefix}*.sfc")):
        stem = rom_path.stem
        patch = None
        module_id = ""
        for candidate in sorted(DOWNLOADS_DIR.glob(stem + ".ap*")):
            ext = candidate.suffix.lower()
            if ext in PATCH_EXT_TO_MODULE:
                patch = candidate
                module_id = PATCH_EXT_TO_MODULE[ext]
                break
        if patch is None:
            # Client Core sometimes keeps patched ROMs in downloads too.
            for candidate in sorted(DOWNLOADS_DIR.glob(rom_path.stem + ".ap*")):
                ext = candidate.suffix.lower()
                if ext in PATCH_EXT_TO_MODULE:
                    patch = candidate
                    module_id = PATCH_EXT_TO_MODULE[ext]
                    break
        if patch is None:
            continue
        manifest = load_manifest(module_id)
        if (manifest.get("archipelago_client") or {}).get("wrapper") != "sni":
            continue
        seed, slot = slot_from_name(rom_path)
        sekaiemu = manifest.get("sekaiemu") if isinstance(manifest.get("sekaiemu"), dict) else {}
        client = manifest.get("archipelago_client") if isinstance(manifest.get("archipelago_client"), dict) else {}
        artifacts.append(Artifact(
            module_id=module_id,
            patch_path=patch,
            rom_path=rom_path,
            slot=slot,
            seed=seed,
            ap_game=str(sekaiemu.get("ap_game") or manifest.get("ap_game") or manifest.get("display_name") or ""),
            game_key=str(client.get("game_key") or manifest.get("game_id") or module_id),
            world=str(client.get("world") or manifest.get("ap_world") or ""),
        ))
    return artifacts


def latest_seed_prefix() -> str:
    candidates: list[tuple[float, str]] = []
    for path in list(ROMS_DIR.glob("AP_*.sfc")) + list(DOWNLOADS_DIR.glob("AP_*.sfc")):
        seed, _slot = slot_from_name(path)
        if seed:
            candidates.append((path.stat().st_mtime, seed))
    if not candidates:
        return ""
    candidates.sort()
    return candidates[-1][1]


async def wait_for_memory_socket(path: Path, timeout: float = 6.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.exists():
            return True
        await asyncio.sleep(0.05)
    return False


def memory_request(sock_path: Path, payload: list[dict[str, Any]], timeout: float = 2.0) -> list[dict[str, Any]]:
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(str(sock_path))
        sock.sendall((json.dumps(payload) + "\n").encode("utf-8"))
        chunks = []
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            if b"\n" in chunk:
                break
        raw = b"".join(chunks).split(b"\n", 1)[0].decode("utf-8", "replace")
        parsed = json.loads(raw)
        return parsed if isinstance(parsed, list) else []
    finally:
        sock.close()


async def sni_get(ws: websockets.WebSocketClientProtocol, addr: int, size: int) -> bytes:
    await ws.send(json.dumps({"Opcode": "GetAddress", "Space": "SNES", "Operands": [f"{addr:x}", f"{size:x}"]}))
    data = await asyncio.wait_for(ws.recv(), timeout=3.0)
    return bytes(data) if isinstance(data, (bytes, bytearray)) else b""


async def sni_put(ws: websockets.WebSocketClientProtocol, addr: int, payload: bytes) -> None:
    await ws.send(json.dumps({"Opcode": "PutAddress", "Space": "SNES", "Operands": [f"{addr:x}", f"{len(payload):x}"]}))
    await ws.send(bytes(payload))


async def sni_write_probe(port: int, log_file: Path) -> bool:
    # Smoke uses an isolated save dir; 0xE007FE maps near the end of SNES SRAM.
    addr = 0xE007FE
    async with websockets.connect(f"ws://127.0.0.1:{port}", ping_interval=None) as ws:
        await ws.send(json.dumps({"Opcode": "DeviceList", "Space": "SNES", "Operands": []}))
        await asyncio.wait_for(ws.recv(), timeout=3.0)
        await ws.send(json.dumps({"Opcode": "Attach", "Space": "SNES", "Operands": ["SekaiLink BizHawk"]}))
        before = await sni_get(ws, addr, 1)
        if len(before) != 1:
            return False
        test = bytes([before[0] ^ 0x5A])
        await sni_put(ws, addr, test)
        await asyncio.sleep(0.15)
        after = await sni_get(ws, addr, 1)
        await sni_put(ws, addr, before)
        ok = after == test
        with log_file.open("a", encoding="utf-8") as fh:
            print(json.dumps({
                "source": "sni_write_probe",
                "addr": hex(addr),
                "before": before.hex(),
                "test": test.hex(),
                "after": after.hex(),
                "ok": ok,
            }, ensure_ascii=False), file=fh, flush=True)
        return ok


async def terminate(proc: asyncio.subprocess.Process | None, name: str) -> None:
    if proc is None or proc.returncode is not None:
        return
    try:
        proc.terminate()
        await asyncio.wait_for(proc.wait(), timeout=2.0)
        return
    except Exception:
        pass
    if proc.returncode is None:
        proc.kill()
        await proc.wait()


async def stream_lines(proc: asyncio.subprocess.Process, label: str, artifact: Artifact, log_file: Path) -> None:
    assert proc.stdout is not None
    with log_file.open("a", encoding="utf-8") as fh:
        while True:
            line = await proc.stdout.readline()
            if not line:
                return
            text = line.decode("utf-8", "replace").rstrip()
            print(json.dumps({"source": label, "line": text}, ensure_ascii=False), file=fh, flush=True)
            if label == "client":
                try:
                    payload = json.loads(text)
                except Exception:
                    continue
                if isinstance(payload, dict):
                    artifact.events.append(payload)


async def stream_stderr(proc: asyncio.subprocess.Process, label: str, artifact: Artifact, log_file: Path) -> None:
    assert proc.stderr is not None
    with log_file.open("a", encoding="utf-8") as fh:
        while True:
            line = await proc.stderr.readline()
            if not line:
                return
            text = line.decode("utf-8", "replace").rstrip()
            print(json.dumps({"source": f"{label}:stderr", "line": text}, ensure_ascii=False), file=fh, flush=True)
            if "No ROM detected" in text:
                artifact.notes.append("client_no_rom_detected")
            if "Error connecting to SNI" in text:
                artifact.notes.append("client_sni_connect_error")


def event_name(event: dict[str, Any]) -> str:
    return str(event.get("event") or event.get("payload", {}).get("event") or "")


def classify(artifact: Artifact) -> str:
    names = [event_name(e) for e in artifact.events]
    if "connected" in names:
        return "connected"
    if "connection_refused" in names:
        return "connection_refused"
    if "server_auth_start" in names or "room_info" in names:
        return "room_seen_auth_pending"
    if "wrapper_patch_active" in names:
        return "client_started_no_auth"
    return "failed"


async def run_one(artifact: Artifact, server: str, index: int, args: argparse.Namespace) -> Artifact:
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    session = f"{artifact.seed}-{artifact.module_id}-{int(time.time())}"
    log_file = LOG_DIR / f"{session}.jsonl"
    sni_port = int(args.base_sni_port) + index
    memory_socket = Path(tempfile.gettempdir()) / f"sekailink-smoke-{artifact.module_id}-{os.getpid()}-{index}.sock"
    system_dir = USER_RUNTIME / "smoke" / artifact.module_id / "system"
    save_dir = USER_RUNTIME / "smoke" / artifact.module_id / "saves" / session
    system_dir.mkdir(parents=True, exist_ok=True)
    save_dir.mkdir(parents=True, exist_ok=True)

    core = RUNTIME_ROOT / "platforms" / "linux-x64" / "cores" / "bsnes_mercury_performance_libretro.so"
    emu = RUNTIME_ROOT / "platforms" / "linux-x64" / "bin" / "sekaiemu_libretro_spike"
    sklmi = RUNTIME_ROOT / "platforms" / "linux-x64" / "bin" / "sekailink_sklmi_runtime"
    sklmi_manifests = RUNTIME_ROOT / "sklmi" / "manifests"

    if memory_socket.exists():
        memory_socket.unlink()

    emu_argv = [
        str(emu),
        "--core", str(core),
        "--game", str(artifact.rom_path),
        "--system-dir", str(system_dir),
        "--save-dir", str(save_dir),
        "--log-dir", str(LOG_DIR),
        "--memory-socket", str(memory_socket),
        "--sklmi-runtime", str(sklmi),
        "--sklmi-manifest-dir", str(sklmi_manifests),
        "--ap-host", server.split(":", 1)[0],
        "--ap-port", server.split(":", 1)[1] if ":" in server else "",
        "--ap-game", artifact.ap_game,
        "--ap-slot-name", artifact.slot,
        "--player-alias", "thelovenityjade",
        "--ap-tags", "AP,SekaiLink,Smoke",
        "--quit-after-frame", str(args.frames),
    ]
    emu_argv = [part for part in emu_argv if part != ""]

    env = os.environ.copy()
    env["SEKAILINK_AP_ROOT"] = str(RUNTIME_ROOT / "ap")
    env["SEKAILINK_AP_WRAPPER"] = "1"
    env["PYTHONPATH"] = os.pathsep.join([str(RUNTIME_ROOT), str(RUNTIME_ROOT / "ap"), env.get("PYTHONPATH", "")])
    env["KIVY_NO_ARGS"] = "1"
    env["KIVY_NO_FILELOG"] = "1"
    env["SKIP_REQUIREMENTS_UPDATE"] = "1"

    emu_proc = bridge_proc = client_proc = None
    tasks: list[asyncio.Task[Any]] = []
    try:
        emu_proc = await asyncio.create_subprocess_exec(
            *emu_argv,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(REPO_ROOT),
            env=env,
        )
        tasks.append(asyncio.create_task(stream_lines(emu_proc, "sekaiemu", artifact, log_file)))
        tasks.append(asyncio.create_task(stream_stderr(emu_proc, "sekaiemu", artifact, log_file)))

        if not await wait_for_memory_socket(memory_socket):
            artifact.notes.append("memory_socket_missing")
            artifact.status = "failed"
            return artifact

        try:
            responses = memory_request(memory_socket, [
                {"type": "SYSTEM"},
                {"type": "DOMAINS"},
                {"type": "READ", "domain": "System Bus", "address": 0xFFC0, "size": 21},
            ])
            artifact.notes.append("memory_probe_ok")
            with log_file.open("a", encoding="utf-8") as fh:
                print(json.dumps({"source": "memory_probe", "responses": responses}, ensure_ascii=False), file=fh, flush=True)
        except Exception as exc:
            artifact.notes.append(f"memory_probe_failed:{exc}")
            artifact.status = "failed"
            return artifact

        bridge_proc = await asyncio.create_subprocess_exec(
            str(PYTHON), str(RUNTIME_ROOT / "sni_bridge.py"),
            "--host", "127.0.0.1",
            "--port", str(sni_port),
            "--memory-socket", str(memory_socket),
            "--log-level", "info",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(REPO_ROOT),
            env=env,
        )
        tasks.append(asyncio.create_task(stream_lines(bridge_proc, "sni_bridge", artifact, log_file)))
        tasks.append(asyncio.create_task(stream_stderr(bridge_proc, "sni_bridge", artifact, log_file)))
        await asyncio.sleep(0.5)
        if args.write_probe:
            try:
                if await sni_write_probe(sni_port, log_file):
                    artifact.notes.append("sni_write_probe_ok")
                else:
                    artifact.notes.append("sni_write_probe_failed")
            except Exception as exc:
                artifact.notes.append(f"sni_write_probe_error:{exc}")

        client_proc = await asyncio.create_subprocess_exec(
            str(PYTHON), str(RUNTIME_ROOT / "sniclient_wrapper.py"),
            "--kind", "sni",
            "--nogui",
            "--connect", server,
            "--slot", artifact.slot,
            "--sni-address", f"ws://127.0.0.1:{sni_port}",
            "--patch", str(artifact.patch_path),
            "--rom", str(artifact.rom_path),
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(REPO_ROOT),
            env=env,
        )
        tasks.append(asyncio.create_task(stream_lines(client_proc, "client", artifact, log_file)))
        tasks.append(asyncio.create_task(stream_stderr(client_proc, "client", artifact, log_file)))

        deadline = time.monotonic() + float(args.timeout)
        while time.monotonic() < deadline:
            artifact.status = classify(artifact)
            if artifact.status in ("connected", "connection_refused"):
                break
            await asyncio.sleep(0.25)
        artifact.status = classify(artifact)
        artifact.notes.append(f"log={log_file}")
        return artifact
    finally:
        await terminate(client_proc, "client")
        await terminate(bridge_proc, "sni_bridge")
        await terminate(emu_proc, "sekaiemu")
        for task in tasks:
            if not task.done():
                task.cancel()
        if memory_socket.exists():
            try:
                memory_socket.unlink()
            except Exception:
                pass


async def run(args: argparse.Namespace) -> int:
    seed = args.seed_prefix or latest_seed_prefix()
    if not seed:
        print("No generated AP SNES artifacts found.", file=sys.stderr)
        return 2
    artifacts = discover_artifacts(seed)
    if args.module:
        wanted = {m.strip() for m in args.module.split(",") if m.strip()}
        artifacts = [a for a in artifacts if a.module_id in wanted]
    if not artifacts:
        print(f"No SNES/SNI patched ROM artifacts found for seed {seed}.", file=sys.stderr)
        return 2

    print(f"SNES/SNI smoke using seed {seed} on {args.server}")
    for artifact in artifacts:
        print(f"- {artifact.module_id}: slot={artifact.slot} rom={artifact.rom_path.name}")

    results = []
    for index, artifact in enumerate(artifacts):
        print(f"\nTesting {artifact.module_id} ({artifact.slot})...")
        result = await run_one(artifact, args.server, index, args)
        results.append(result)
        print(f"  status={result.status} notes={', '.join(result.notes[-4:])}")

    print("\nSummary")
    ok = 0
    for result in results:
        if result.status == "connected":
            ok += 1
        print(f"{result.module_id:24} {result.status:24} {result.slot}")
    return 0 if ok == len(results) else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Smoke test current generated SNES/SNI runtime artifacts")
    parser.add_argument("--seed-prefix", default="", help="AP seed id/prefix, default: latest generated AP_*.sfc")
    parser.add_argument("--server", default="link.sekailink.com:38290", help="Room server host:port")
    parser.add_argument("--module", default="", help="Comma-separated module filter, e.g. donkey_kong_country,earthbound")
    parser.add_argument("--timeout", type=float, default=18.0)
    parser.add_argument("--frames", type=int, default=2400)
    parser.add_argument("--base-sni-port", type=int, default=23174)
    parser.add_argument("--write-probe", action="store_true", help="Write/read/restore a byte through SNI before AP client auth")
    return parser.parse_args()


if __name__ == "__main__":
    try:
        raise SystemExit(asyncio.run(run(parse_args())))
    except KeyboardInterrupt:
        raise SystemExit(130)
