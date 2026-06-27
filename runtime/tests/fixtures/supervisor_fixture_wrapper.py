#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import time


def emit(event: str, **payload):
    print(json.dumps({"event": event, **payload}), flush=True)


def main() -> int:
    emit("ready", kind="fixture")
    for line in sys.stdin:
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            payload = {"cmd": "command", "text": line.strip()}
        cmd = str(payload.get("cmd") or "").lower()
        if cmd in {"shutdown", "stop"}:
            emit("bye")
            return 0
        emit("echo", payload=payload)
    while True:
        time.sleep(0.1)


if __name__ == "__main__":
    raise SystemExit(main())
