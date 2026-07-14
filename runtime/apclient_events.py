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


def lookup_player_name(ctx: Any, player: Any) -> str:
    try:
        slot = int(player)
    except Exception:
        return str(player or "")
    try:
        names = getattr(ctx, "player_names", None) or {}
        value = names.get(slot)
        if value:
            return str(value)
    except Exception:
        pass
    return str(slot)


def lookup_game_for_player(ctx: Any, player: Any) -> str:
    try:
        slot = int(player)
    except Exception:
        return ""
    try:
        slot_info = getattr(ctx, "slot_info", None) or {}
        info = slot_info.get(slot)
        if info is None:
            return ""
        value = getattr(info, "game", None)
        if value:
            return str(value)
        if isinstance(info, (list, tuple)) and len(info) > 1:
            return str(info[1])
    except Exception:
        pass
    return ""


def lookup_location_name(ctx: Any, location_id: Any, player: Any = None) -> str:
    try:
        location = int(location_id)
    except Exception:
        return str(location_id)
    names = getattr(ctx, "location_names", None)
    slot = player if player not in (None, "") else getattr(ctx, "slot", None)
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
    item_player = getattr(ctx, "slot", None) or player
    return {
        "item": item_id,
        "item_name": lookup_item_name(ctx, item_id, item_player),
        "item_player": item_player,
        "location": location_id,
        "location_name": lookup_location_name(ctx, location_id, player),
        "player": player,
        "source_player": player,
        "source_player_name": lookup_player_name(ctx, player),
        "source_game": lookup_game_for_player(ctx, player),
        "flags": flags,
        "raw": scrub_packet_for_trace(item),
    }


def _int_from_print_part(part: dict[str, Any], key: str = "text") -> int | None:
    try:
        return int(part.get(key))
    except Exception:
        return None


def summarize_print_json_item_send(ctx: Any, args: dict[str, Any]) -> dict[str, Any] | None:
    if not isinstance(args, dict) or str(args.get("type") or "") != "ItemSend":
        return None
    data = [part for part in list(args.get("data") or []) if isinstance(part, dict)]
    player_parts = [part for part in data if part.get("type") == "player_id"]
    item_part = next((part for part in data if part.get("type") == "item_id"), None)
    location_part = next((part for part in data if part.get("type") == "location_id"), None)
    if not player_parts or item_part is None:
        return None
    sender_slot = _int_from_print_part(player_parts[0])
    recipient_slot = args.get("receiving")
    try:
        recipient_slot = int(recipient_slot)
    except Exception:
        recipient_slot = _int_from_print_part(player_parts[-1])
    item_id = _int_from_print_part(item_part)
    if sender_slot is None or recipient_slot is None or item_id is None:
        return None
    item_player = _int_from_print_part(item_part, "player") or recipient_slot
    location_id = _int_from_print_part(location_part) if location_part is not None else None
    location_player = (_int_from_print_part(location_part, "player") if location_part is not None else None) or sender_slot
    flags = item_part.get("flags", 0)
    return {
        "sender_slot": sender_slot,
        "sender_name": lookup_player_name(ctx, sender_slot),
        "sender_game": lookup_game_for_player(ctx, sender_slot),
        "recipient_slot": recipient_slot,
        "recipient_name": lookup_player_name(ctx, recipient_slot),
        "recipient_game": lookup_game_for_player(ctx, recipient_slot),
        "item_id": item_id,
        "item_player": item_player,
        "item_name": lookup_item_name(ctx, item_id, item_player),
        "location_id": location_id,
        "location_player": location_player,
        "location_name": lookup_location_name(ctx, location_id, location_player) if location_id is not None else "",
        "flags": flags,
        "raw": scrub_packet_for_trace(args),
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
