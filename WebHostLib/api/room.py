from typing import Any, Dict
from uuid import UUID
import uuid

from flask import abort, url_for

from WebHostLib import to_url, to_python
import worlds.Files
from . import api_endpoints, get_players
from ..models import Room, Slot


def _resolve_room_id(value: str) -> UUID | None:
    # Accept both short-url UUID (suuid) and canonical UUID for compatibility.
    if not value:
        return None
    try:
        return to_python(value)
    except Exception:
        pass
    try:
        return uuid.UUID(value)
    except Exception:
        return None


@api_endpoints.route('/room_status/<string:room_id>')
def room_info(room_id: str) -> Dict[str, Any]:
    resolved_id = _resolve_room_id(room_id)
    if resolved_id is None:
        return abort(404)
    room = Room.get(id=resolved_id)
    if room is None:
        return abort(404)

    def supports_apdeltapatch(game: str) -> bool:
        return game in worlds.Files.AutoPatchRegister.patch_types

    downloads = []
    for slot in sorted(room.seed.slots):
        if slot.data and not supports_apdeltapatch(slot.game):
            slot_download = {
                "slot": slot.player_id,
                "download": url_for("download_slot_file", room_id=room.id, player_id=slot.player_id)
            }
            downloads.append(slot_download)
        elif slot.data:
            slot_download = {
                "slot": slot.player_id,
                "download": url_for("download_patch", patch_id=slot.id, room_id=room.id)
            }
            downloads.append(slot_download)

    return {
        "tracker": to_url(room.tracker),
        "players": [
            {"slot": slot.player_id, "name": slot.player_name, "game": slot.game}
            for slot in room.seed.slots.order_by(Slot.player_id)
        ],
        "last_port": room.last_port,
        "last_activity": room.last_activity,
        "timeout": room.timeout,
        "downloads": downloads,
    }
