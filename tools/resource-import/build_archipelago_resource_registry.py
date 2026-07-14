#!/usr/bin/env python3
from __future__ import annotations

import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


REPO = Path(__file__).resolve().parents[2]
SOURCE_DIR = REPO / "runtime/downloaded-resources/archipelago-sheet"
OUT_PATH = REPO / "runtime/game-registry/archipelago-resource-index.json"
DOC_PATH = REPO / "docs/runtime/archipelago-resource-index.md"


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def normalize_status(status: str) -> str:
    lower = str(status or "").strip().lower()
    if lower == "core-verified":
        return "core-verified"
    if lower == "stable":
        return "stable"
    if lower == "unstable":
        return "unstable"
    return lower or "unknown"


def unique_key(entry: dict[str, Any]) -> tuple[str, str, str]:
    return (
        str(entry.get("game") or "").casefold(),
        str(entry.get("kind") or "").casefold(),
        str(entry.get("url") or "").casefold(),
    )


def main() -> int:
    inventory = load_json(SOURCE_DIR / "inventory.json")
    report = load_json(SOURCE_DIR / "download-report.json")

    source_links: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in inventory:
        game = str(row.get("game") or "").strip()
        if not game:
            continue
        source_links[game].append({
            "status": normalize_status(row.get("status", "")),
            "sheet": row.get("sheet", ""),
            "row": row.get("row", 0),
            "label": row.get("link_label", ""),
            "url": row.get("url", ""),
            "notes": row.get("notes", ""),
        })

    resources: list[dict[str, Any]] = []
    seen: set[tuple[str, str, str]] = set()
    for item in report:
        kind = str(item.get("kind") or "")
        url = str(item.get("url") or "")
        if kind in {"none", "unknown"} or not url:
            continue
        entry = {
            "game": item.get("game", ""),
            "status": normalize_status(item.get("status", "")),
            "kind": kind,
            "file_name": item.get("file_name", ""),
            "url": url,
            "source_url": item.get("source_url", ""),
            "source_label": item.get("source_label", ""),
            "download_policy": "on_demand",
            "enabled_by_default": False,
        }
        key = unique_key(entry)
        if key not in seen:
            seen.add(key)
            resources.append(entry)

    for game, links in source_links.items():
        if any(link["status"] == "core-verified" for link in links):
            resources.append({
                "game": game,
                "status": "core-verified",
                "kind": "core-world",
                "file_name": "",
                "url": "",
                "source_url": next((link["url"] for link in links if link["status"] == "core-verified" and "archipelago.gg/games" in link["url"]), ""),
                "source_label": "Archipelago core world",
                "download_policy": "bundled_with_archipelago",
                "enabled_by_default": False,
            })

    resources.sort(key=lambda r: (str(r["game"]).casefold(), str(r["status"]), str(r["kind"]), str(r["file_name"]).casefold()))
    by_kind = defaultdict(int)
    by_status = defaultdict(int)
    for res in resources:
        by_kind[res["kind"]] += 1
        by_status[res["status"]] += 1

    out = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source": {
            "xlsx": str(Path(os.environ.get("SEKAILINK_ARCHIPELAGO_SHEET", "Archipelago Games Sheet.xlsx"))),
            "inventory": str(SOURCE_DIR / "inventory.json"),
            "download_report": str(SOURCE_DIR / "download-report.json"),
        },
        "policy": {
            "included_statuses": ["stable", "unstable", "core-verified"],
            "excluded": ["18+ / Unrated entries", "Discord-only links", "setup-only links without downloadable artifact"],
            "downloads": "URLs are stored for on-demand server/client update jobs; artifacts are not vendored by this index.",
        },
        "counts": {
            "resources": len(resources),
            "by_kind": dict(sorted(by_kind.items())),
            "by_status": dict(sorted(by_status.items())),
        },
        "resources": resources,
        "source_links_by_game": dict(sorted(source_links.items())),
    }

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = [
        "# Archipelago Resource Index",
        "",
        "Generated from the configured Archipelago games sheet.",
        "",
        "This file is a URL registry, not a vendored dependency folder. Servers and clients can use it to fetch APWorlds and tracker packs on demand without re-scraping the XLSX.",
        "",
        "## Counts",
        "",
        f"- Resources: {len(resources)}",
    ]
    for kind, count in sorted(by_kind.items()):
        lines.append(f"- `{kind}`: {count}")
    lines.extend(["", "## Policy", ""])
    for item in out["policy"]["excluded"]:
        lines.append(f"- Excluded: {item}")
    lines.extend(["", "## Notable Outputs", "", f"- JSON registry: `{OUT_PATH}`", f"- Raw inventory: `{SOURCE_DIR / 'inventory.json'}`", f"- Raw scrape report: `{SOURCE_DIR / 'download-report.json'}`", ""])
    DOC_PATH.parent.mkdir(parents=True, exist_ok=True)
    DOC_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(json.dumps(out["counts"], indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
