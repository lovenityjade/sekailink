# ALTTP Private Room Link Flow

This document describes the precise handoff path for wiring an ALTTP tracker
bundle through the generic native tracker host to a real private Link Room
session.

## Goal

Launch one ALTTP patch run where:

- `Sekaiemu` materializes the `.aplttp` patch locally
- the ALTTP `SKLMI` companion owns the room-facing bridge
- `room.state` becomes the authoritative metadata handoff into the tracker
- the native tracker panel shows room, slot, seed, and selected `slot_data`

Architectural frame:

- `Sekaiemu` remains the generic libretro host and generic tracker renderer
- ALTTP-specific tracker semantics live in the selected bundle, LinkedWorld
  metadata, and `SKLMI` output
- this document is an ALTTP integration example, not a separate host design

## Runtime launch

```bash
./build/sekaiemu_libretro_spike \
  --profile ./profiles/alttp-starter.profile \
  --core /usr/lib64/libretro/bsnes_mercury_performance_libretro.so \
  --patch /path/to/AP_Seed.aplttp \
  --base-rom "/path/to/Zelda no Densetsu - Kamigami no Triforce (Japan).sfc" \
  --tracker-bundle ./tracker-bundles/alttp-native \
  --system-dir ./system \
  --save-dir ./saves \
  --log-dir ./logs
```

Relevant local artifacts:

- patched ROM:
  - `save_dir/patched/*.sfc`
- room metadata handoff:
  - `save_dir/sklmi/<bridge_id>/room.state`
- room/gameplay events:
  - `save_dir/sklmi/<bridge_id>/trace.jsonl`
- tracker persisted state:
  - `save_dir/tracker/alttp-native/state.json`

## Metadata handoff contract

`Sekaiemu` watches `room.state` and extracts `meta|...` lines into the tracker
authoritative snapshot used by the generic tracker runtime.

If `room.state` is still sparse and only carries a sentinel such as
`meta|connected|1`, `Sekaiemu` now also uses the existing `trace.jsonl` stream
as a fallback metadata source:

- `room_client_ready` contributes room/session identity
- `room_metadata_ready` contributes `world_id`, `seed_id`, `seed_hash`, and a
  `slot_data_present` hint
- `slot_connected` contributes `linkedworld_id`, `driver_instance_id`, and
  `core_profile`

Recommended metadata for a private Link Room run:

- `meta|world_id|<world-instance-id>`
- `meta|linkedworld_id|alttp`
- `meta|room_id|<private-room-id>`
- `meta|slot_id|<slot-id>`
- `meta|slot_name|<display-slot-name>`
- `meta|player_alias|<player-facing-alias>`
- `meta|seed_id|<seed-id>`
- `meta|seed_hash|<seed-hash>`
- `meta|tracker_pack|alttp-default`
- `meta|tracker_variant|side-by-side`
- `meta|slot_data|{"mode":"open","goal":"ganon",...}`

## What the tracker currently displays

Session-facing fields:

- `slot_id`
- `room_id`
- `seed_id`
- `current_zone_id`

Slot-facing identity fields:

- `slot_name`
- `player_alias`
- `world_instance_id`

Settings-facing fields:

- `slot_data.mode`
- `slot_data.goal`
- `slot_data.entrance_shuffle`
- `slot_data.item_pool`
- `slot_data.item_functionality`
- `slot_data.dark_room_logic`
- `slot_data.dungeon_counters`
- `slot_data.open_pyramid`
- `slot_data.difficulty`
- `slot_data.weapons`
- `slot_data.name`

## Link Room integration note

This repo does not need direct Link Room networking for tracker rendering.
The preferred handoff is a maintained `room.state` file produced by the active
`SKLMI` companion, with `trace.jsonl` acting as a bounded fallback for live
metadata recovery.

When the handoff is healthy:

- the active ALTTP bundle remains side-by-side through the generic host
- the session card shows room and player identity instead of only numeric slot data
- the settings card reflects live `slot_data`
- the runtime log prints a concise metadata update line when `room.state`
  changes materially
