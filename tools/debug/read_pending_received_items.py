#!/usr/bin/env python3
"""Read pending received items from local Sekaiemu/SKLMI traces.

This is intentionally read-only. It mirrors the source Sekaiemu can use when
room.state is not present: SKLMI trace.jsonl room_item_pending records.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


DEFAULT_ROOT = Path.home() / ".config" / "sekailink-client" / "sekaiemu"


ROOM_ITEM_KEYS = (
    "delivery_id",
    "ap_item_id",
    "ap_location_id",
    "ap_player_id",
    "item_name",
    "event_key",
    "mapped_value",
)


@dataclass(frozen=True)
class PendingItem:
    trace: Path
    delivery_id: str
    item_name: str
    player_id: str
    item_id: str
    location_id: str
    tick_index: int | None


def latest_trace(root: Path) -> Path | None:
    traces = [p for p in root.rglob("trace.jsonl") if p.is_file()]
    if not traces:
        return None
    return max(traces, key=lambda p: p.stat().st_mtime)


def all_traces(root: Path) -> list[Path]:
    return sorted((p for p in root.rglob("trace.jsonl") if p.is_file()), key=lambda p: p.stat().st_mtime)


def field(detail: str, key: str) -> str:
    marker = f"{key}="
    start = detail.find(marker)
    if start < 0:
        return ""
    value_start = start + len(marker)
    value_end = len(detail)
    for next_key in ROOM_ITEM_KEYS:
        if next_key == key:
            continue
        match = re.search(rf"\s{re.escape(next_key)}=", detail[value_start:])
        if match:
            value_end = min(value_end, value_start + match.start())
    return detail[value_start:value_end].strip()


def read_pending(trace: Path) -> list[PendingItem]:
    items: list[PendingItem] = []
    seen: set[str] = set()
    with trace.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            if record.get("record_type") != "trace" or record.get("event") != "room_item_pending":
                continue
            detail = str(record.get("detail") or "")
            item_name = field(detail, "item_name")
            if not item_name or item_name == "None":
                continue
            delivery_id = field(detail, "delivery_id") or f"line:{len(items)}"
            if delivery_id in seen:
                continue
            seen.add(delivery_id)
            tick = record.get("tick_index")
            items.append(
                PendingItem(
                    trace=trace,
                    delivery_id=delivery_id,
                    item_name=item_name,
                    player_id=field(detail, "ap_player_id"),
                    item_id=field(detail, "ap_item_id"),
                    location_id=field(detail, "ap_location_id"),
                    tick_index=tick if isinstance(tick, int) else None,
                )
            )
    return items


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    traces = [args.trace] if args.trace else (all_traces(args.root) if args.all else [latest_trace(args.root)])
    traces = [trace for trace in traces if trace is not None]
    if not traces:
        print(f"No trace.jsonl found under {args.root}")
        return 1

    items: list[PendingItem] = []
    for trace in traces:
        items.extend(read_pending(trace))
    if args.json:
        print(
            json.dumps(
                [
                    {
                        "trace": str(item.trace),
                        "delivery_id": item.delivery_id,
                        "item_name": item.item_name,
                        "player_id": item.player_id,
                        "item_id": item.item_id,
                        "location_id": item.location_id,
                        "tick_index": item.tick_index,
                    }
                    for item in items
                ],
                indent=2,
            )
        )
        return 0

    if len(traces) == 1:
        print(f"Trace: {traces[0]}")
    else:
        print(f"Traces scanned: {len(traces)}")
    print(f"Pending received items: {len(items)}")
    for index, item in enumerate(items, start=1):
        sender = f"Player {item.player_id}" if item.player_id else "Sekailink"
        print(
            f"{index:02d}. {sender} -> {item.item_name} [{item.trace.parent.parent.parent.name}] "
            f"(delivery={item.delivery_id}, item={item.item_id or '-'}, "
            f"location={item.location_id or '-'}, tick={item.tick_index or '-'})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
