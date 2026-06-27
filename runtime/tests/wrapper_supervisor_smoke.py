#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def supervisor() -> Path:
    return repo_root() / "runtime" / "wrapper_supervisor.py"


def run_cmd(args: list[str], input_text: str | None = None, timeout: float = 12.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(supervisor()), *args],
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(repo_root()),
        timeout=timeout,
        check=False,
    )


def parse_json_lines(text: str) -> list[dict]:
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        out.append(json.loads(line))
    return out


def assert_ok(proc: subprocess.CompletedProcess[str]) -> None:
    if proc.returncode != 0:
        raise AssertionError(f"command failed rc={proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")


def main() -> int:
    listing = run_cmd(["list", "--status", "enabled"])
    assert_ok(listing)
    enabled = json.loads(listing.stdout)["clients"]
    if len(enabled) < 40:
        raise AssertionError(f"too few enabled clients: {len(enabled)}")

    for game_key, expected_wrapper in [
        ("a_link_to_the_past", "sni"),
        ("pokemon_emerald", "bizhawk"),
        ("oot", "oot"),
        ("ttyd", "dolphin"),
        ("dk64", "module"),
    ]:
        planned = run_cmd(["plan", "--game-key", game_key])
        assert_ok(planned)
        payload = json.loads(planned.stdout)
        if payload["plan"]["wrapper"] != expected_wrapper:
            raise AssertionError(f"{game_key} wrapper mismatch: {payload['plan']['wrapper']} != {expected_wrapper}")
        if expected_wrapper in {"dolphin", "module"} and "--module" not in payload["argv"]:
            raise AssertionError(f"{game_key} missing module arg")

    dry = run_cmd(["launch", "--wrapper", "text", "--game-key", "manual_text", "--dry-run"])
    assert_ok(dry)
    dry_events = parse_json_lines(dry.stdout)
    if dry_events[0]["event"] != "launch_plan":
        raise AssertionError("dry-run did not emit launch_plan")

    launched = run_cmd(
        ["launch", "--wrapper", "fixture", "--game-key", "fixture", "--session-id", "supervisor-smoke"],
        input_text='{"cmd":"status"}\n{"cmd":"shutdown"}\n',
        timeout=12,
    )
    assert_ok(launched)
    events = parse_json_lines(launched.stdout)
    names = [event["event"] for event in events]
    if "started" not in names or "exited" not in names or "wrapper_event" not in names:
        raise AssertionError(f"missing lifecycle events: {names}")
    print("wrapper_supervisor_smoke_ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
