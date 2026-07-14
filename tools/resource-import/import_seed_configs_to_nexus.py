#!/usr/bin/env python3
"""Import generated seed-config schemas into the Nexus seed-config API."""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parents[2] if len(SCRIPT_PATH.parents) > 2 else Path.cwd()
DEFAULT_INPUT_DIR = REPO_ROOT / "runtime" / "generated" / "nexus-seed-config-imports"


def post_json(base_url: str, token: str, payload: dict) -> dict:
    url = base_url.rstrip("/") + "/admin/seed-configs/games"
    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description="POST generated seed-config import JSON files into Nexus.")
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT_DIR)
    parser.add_argument("--base-url", required=True, help="Seed-config API base URL, for example http://127.0.0.1:19106")
    parser.add_argument(
        "--admin-token",
        default=os.environ.get("NEXUS_SEED_CONFIG_ADMIN_TOKEN", ""),
        help="Admin token. Prefer NEXUS_SEED_CONFIG_ADMIN_TOKEN so the secret is not exposed in process arguments.",
    )
    parser.add_argument("--game-key", action="append", default=[], help="Only import selected game_key values. Can be repeated.")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.admin_token:
        parser.error("--admin-token or NEXUS_SEED_CONFIG_ADMIN_TOKEN is required")

    selected = set(args.game_key)
    files = sorted(args.input_dir.glob("*.import.json"))
    if selected:
        files = [path for path in files if path.stem.removesuffix(".import") in selected]
    if not files:
        print("no import files matched", file=sys.stderr)
        return 2

    failures = 0
    for path in files:
        payload = json.loads(path.read_text(encoding="utf-8"))
        label = f"{payload.get('game_key')} ({payload.get('display_name')})"
        if args.dry_run:
            print(f"dry-run import {label}: {len(payload.get('options', []))} options")
            continue
        try:
            response = post_json(args.base_url, args.admin_token, payload)
        except urllib.error.HTTPError as exc:
            failures += 1
            detail = exc.read().decode("utf-8", errors="replace")
            print(f"failed {label}: HTTP {exc.code}: {detail}", file=sys.stderr)
            continue
        except Exception as exc:
            failures += 1
            print(f"failed {label}: {exc}", file=sys.stderr)
            continue
        if response.get("ok"):
            print(f"imported {label}")
        else:
            failures += 1
            print(f"failed {label}: {response}", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
