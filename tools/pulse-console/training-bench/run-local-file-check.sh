#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

sha256sum -c SHA256SUMS >/tmp/pulse-bench-sha256-check.log
echo "sha256 ok: /tmp/pulse-bench-sha256-check.log"

python3 - <<'PY'
import json
from pathlib import Path

root = Path.cwd()
for rel in [
    "pulse/rag/datasets/pulse-train-v0.jsonl",
    "pulse/rag/datasets/pulse-eval-v0.jsonl",
    "pulse/rag/indexes/apworld-options.jsonl",
]:
    path = root / rel
    rows = 0
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if line.strip():
                json.loads(line)
                rows += 1
    print(f"{rel}: {rows} valid jsonl rows")
PY
