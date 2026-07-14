import copy
import hashlib
import json
import logging
from pathlib import Path


MAX_COMPENSATION = 2048
SOFT_COMPENSATION_LIMIT = 512


def compensator_name(seed_value, existing_names):
    names = (
        "Helpful Stranger", "Spare Check Dept", "Cosmic Courier", "Lucky Assistant",
        "Quiet Benefactor", "Check Keeper", "Kindly Wanderer", "Backup Hero",
    )
    offset = int(hashlib.sha256(str(seed_value).encode("utf-8")).hexdigest()[:8], 16)
    existing = {str(name).casefold() for name in existing_names}
    for index in range(len(names)):
        candidate = names[(offset + index) % len(names)]
        if candidate.casefold() not in existing:
            return candidate
    return f"Compensator{offset % 1000:03d}"[:16]


def add_compensator(args, failure, seed_value, roll_settings):
    code = str(getattr(failure, "code", "") or "")
    raw_deficits = getattr(failure, "deficits", {}) or {}
    deficits = {str(int(player)): int(count) for player, count in raw_deficits.items() if int(count) > 0}
    total = sum(deficits.values())
    if code not in {"item_location_deficit", "filler_item_deficit"} or total <= 0:
        raise failure
    if total > MAX_COMPENSATION:
        raise RuntimeError(f"compensator_limit_exceeded:{total}") from failure
    if total > SOFT_COMPENSATION_LIMIT:
        logging.warning(
            "SekaiLink Compensator is above the soft limit: amount=%s soft_limit=%s hard_limit=%s",
            total, SOFT_COMPENSATION_LIMIT, MAX_COMPENSATION,
        )

    recovered = copy.deepcopy(args)
    original_players = int(recovered.multi)
    player = original_players + 1
    name = compensator_name(seed_value, recovered.name.values())
    mode = "locations" if code == "item_location_deficit" else "fillers"
    weights = {
        "name": name,
        "game": "SekaiLink Compensator",
        "SekaiLink Compensator": {
            "compensation_mode": mode,
            "compensation_deficits": json.dumps(deficits, sort_keys=True, separators=(",", ":")),
            "compensation_location_count": total if mode == "locations" else 0,
            "original_player_count": original_players,
        },
    }
    settings_object = roll_settings(weights, recovered.plando)
    for key, value in vars(settings_object).items():
        if value is None:
            continue
        target = getattr(recovered, key, None)
        if not isinstance(target, dict):
            target = {}
            setattr(recovered, key, target)
        target[player] = value
    recovered.multi = player
    recovered.name[player] = name
    if hasattr(recovered, "player_options"):
        recovered.player_options[player] = settings_object
    return recovered, {
        "schema_version": "sekailink-compensator-v1",
        "active": True,
        "slot": player,
        "slot_name": name,
        "display_name": name,
        "reason": code,
        "mode": mode,
        "amount": total,
        "deficits": deficits,
        "attempts": 2,
        "original_player_count": original_players,
        "release_state": "scheduled",
    }


def run_generation_with_compensation(erargs, seed, ermain, roll_settings):
    recovery_source = copy.deepcopy(erargs)
    try:
        return ermain(erargs, seed), None
    except Exception as failure:
        if not getattr(failure, "code", ""):
            raise
        recovery_args, compensation = add_compensator(recovery_source, failure, seed, roll_settings)
        logging.warning(
            "SekaiLink Compensator invoked: name=%s reason=%s amount=%s",
            compensation["slot_name"], compensation["reason"], compensation["amount"],
        )
        multiworld = ermain(recovery_args, seed)
        output_dir = Path(str(recovery_args.outputpath))
        output_dir.mkdir(parents=True, exist_ok=True)
        (output_dir / "sekailink_compensator.json").write_text(
            json.dumps(compensation, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        return multiworld, compensation
