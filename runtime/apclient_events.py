from __future__ import annotations

import asyncio
import json
import logging
import re
import threading
import traceback
import zipfile
from typing import Any


_emit_lock = threading.Lock()


def emit(event: str, **payload: Any) -> None:
    data = {"event": event, **payload}
    with _emit_lock:
        print(json.dumps(data, ensure_ascii=True, default=str), flush=True)


class JsonLogHandler(logging.Handler):
    def emit(self, record: logging.LogRecord) -> None:
        try:
            emit(
                "log",
                level=str(record.levelname or "INFO").lower(),
                logger=record.name,
                message=self.format(record),
            )
        except Exception:
            pass


def install_json_logging() -> None:
    handler = JsonLogHandler()
    handler.setFormatter(logging.Formatter("%(message)s"))
    root = logging.getLogger()
    root.setLevel(logging.INFO)
    root.addHandler(handler)


def monitor_task(task: asyncio.Task[Any], name: str) -> asyncio.Task[Any]:
    def done(completed: asyncio.Task[Any]) -> None:
        if completed.cancelled():
            emit("task_cancelled", task=name)
            return
        try:
            exc = completed.exception()
        except asyncio.CancelledError:
            emit("task_cancelled", task=name)
            return
        if exc is not None:
            emit("task_failed", task=name, error=str(exc), traceback="".join(traceback.format_exception(exc)))
        else:
            emit("task_done", task=name)

    task.add_done_callback(done)
    return task


def scrub_packet_for_trace(packet: Any) -> Any:
    if isinstance(packet, dict):
        out: dict[str, Any] = {}
        for key, value in packet.items():
            lower = str(key).lower()
            if lower in {"password", "passwd", "token"}:
                out[key] = "[REDACTED]"
            else:
                out[key] = scrub_packet_for_trace(value)
        return out
    if isinstance(packet, (list, tuple, set)):
        return [scrub_packet_for_trace(value) for value in packet]
    return packet


def read_archipelago_patch_metadata(path: str) -> dict[str, Any]:
    if not path:
        return {}
    try:
        with zipfile.ZipFile(path) as archive:
            for name in ("archipelago.json", "patch.json"):
                if name not in archive.namelist():
                    continue
                try:
                    data = json.loads(archive.read(name).decode("utf-8", "replace"))
                except Exception:
                    continue
                if isinstance(data, dict):
                    return {
                        key: data.get(key)
                        for key in ("player", "player_name", "seed_name", "game", "version", "compatible_version")
                        if key in data
                    }
    except Exception:
        return {}
    return {}


def lookup_location_name(ctx: Any, location_id: Any) -> str:
    try:
        location = int(location_id)
    except Exception:
        return str(location_id)
    names = getattr(ctx, "location_names", None)
    slot = getattr(ctx, "slot", None)
    for call in (
        lambda: names.lookup_in_slot(location, slot),
        lambda: names.lookup_in_game(location, getattr(ctx, "game", "")),
        lambda: names[location],
    ):
        try:
            value = call()
            if value:
                return str(value)
        except Exception:
            continue
    return str(location)


def lookup_item_name(ctx: Any, item_id: Any, player: Any = None) -> str:
    try:
        item = int(item_id)
    except Exception:
        return str(item_id)
    names = getattr(ctx, "item_names", None)
    slot = player if player not in (None, "") else getattr(ctx, "slot", None)
    for call in (
        lambda: names.lookup_in_slot(item, slot),
        lambda: names.lookup_in_game(item, getattr(ctx, "game", "")),
        lambda: names[item],
    ):
        try:
            value = call()
            if value:
                return str(value)
        except Exception:
            continue
    return str(item)


def network_item_field(item: Any, field: str, index: int) -> Any:
    if isinstance(item, dict):
        return item.get(field)
    if isinstance(item, (list, tuple)) and len(item) > index:
        return item[index]
    return getattr(item, field, None)


def summarize_network_item(ctx: Any, item: Any) -> dict[str, Any]:
    item_id = network_item_field(item, "item", 0)
    location_id = network_item_field(item, "location", 1)
    player = network_item_field(item, "player", 2)
    flags = network_item_field(item, "flags", 3)
    return {
        "item": item_id,
        "item_name": lookup_item_name(ctx, item_id, player),
        "location": location_id,
        "location_name": lookup_location_name(ctx, location_id),
        "player": player,
        "flags": flags,
        "raw": scrub_packet_for_trace(item),
    }


def normalize_game_name_for_match(value: Any) -> str:
    return re.sub(r"[^a-z0-9]+", "", str(value or "").lower())


def summarize_server_send(ctx: Any, messages: Any) -> list[dict[str, Any]]:
    summaries: list[dict[str, Any]] = []
    for message in list(messages or []):
        if not isinstance(message, dict):
            continue
        cmd = str(message.get("cmd") or "")
        summary: dict[str, Any] = {"cmd": cmd, "raw": scrub_packet_for_trace(message)}
        if cmd == "LocationChecks":
            locations = list(message.get("locations") or [])
            summary["locations"] = locations
            summary["location_names"] = [lookup_location_name(ctx, location) for location in locations[:20]]
            summary["count"] = len(locations)
        elif cmd == "StatusUpdate":
            summary["status"] = message.get("status")
        elif cmd == "Connect":
            summary["name"] = message.get("name")
            summary["game"] = message.get("game")
            summary["tags"] = sorted(str(tag) for tag in (message.get("tags") or []))
        elif cmd in {"Sync", "Get", "SetNotify", "LocationScouts"}:
            summary["keys"] = list(message.get("keys") or [])
            summary["count"] = len(message.get("locations") or message.get("keys") or [])
        summaries.append(summary)
    return summaries
