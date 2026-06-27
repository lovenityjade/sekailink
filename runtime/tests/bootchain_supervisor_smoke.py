#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def bootchain() -> Path:
    return repo_root() / "runtime" / "bootchain_supervisor.py"


def run_cmd(args: list[str], timeout: float = 15.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(bootchain()), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=str(repo_root()),
        timeout=timeout,
        check=False,
    )


def assert_ok(proc: subprocess.CompletedProcess[str]) -> None:
    if proc.returncode != 0:
        raise AssertionError(f"rc={proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")


def parse_lines(text: str) -> list[dict]:
    return [json.loads(line) for line in text.splitlines() if line.strip()]


def main() -> int:
    generic = run_cmd(["list-generic"])
    assert_ok(generic)
    generic_payload = json.loads(generic.stdout)
    if len(generic_payload["clients"]) < 20:
        raise AssertionError("generic client pool unexpectedly small")
    wrappers = {client["wrapper"] for client in generic_payload["clients"]}
    if not {"sni", "bizhawk"}.issubset(wrappers):
        raise AssertionError(f"missing generic wrappers: {wrappers}")

    plan = run_cmd(["plan", "--random-generic", "--random-seed", "sekailink-smoke", "--server", "127.0.0.1:38281", "--slot", "Smoke"])
    assert_ok(plan)
    plan_payload = json.loads(plan.stdout)["plan"]
    if plan_payload["wrapper"] not in {"sni", "bizhawk"}:
        raise AssertionError(f"not generic: {plan_payload['wrapper']}")
    if not plan_payload["wrapper_command"]:
        raise AssertionError("wrapper command missing")

    launched = run_cmd([
        "launch",
        "--game-key", "alttp",
        "--server", "127.0.0.1:38281",
        "--slot", "Smoke",
        "--session-id", "bootchain-smoke",
        "--wrapper-override", "fixture",
        "--max-runtime", "1.0",
    ])
    assert_ok(launched)
    events = parse_lines(launched.stdout)
    names = [event["event"] for event in events]
    for required in ["bootchain_plan", "child_started", "child_line", "bootchain_exited"]:
        if required not in names:
            raise AssertionError(f"missing {required}: {names}")
    print("bootchain_supervisor_smoke_ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
