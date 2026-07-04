#!/usr/bin/env bash
set -euo pipefail

TARGET_JSON="${1:-/opt/sekailink/evolution/admin-agent/data/postfix_queue_state.json}"

mkdir -p "$(dirname "$TARGET_JSON")"

raw_output="$(postqueue -p 2>&1 || true)"
queue_count="$(printf '%s\n' "$raw_output" | grep -E '^[A-F0-9]+' | wc -l | tr -d ' ')"

python3 - "$TARGET_JSON" "$queue_count" <<'PY'
import json
import sys
from datetime import datetime, timezone

target = sys.argv[1]
queue_count = int(sys.argv[2])
raw = sys.stdin.read()

lines = [line for line in raw.splitlines() if line.strip()]
preview = lines[:25]

payload = {
    "ok": True,
    "service": "postfix_queue_snapshot",
    "queue_count": queue_count,
    "preview": preview,
    "updated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
}

with open(target, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2)
    handle.write("\n")
PY
