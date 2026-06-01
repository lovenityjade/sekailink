# SKLMI Room Metadata Flow

Status: implemented
Date: 2026-05-05

## Goal

Document the current metadata path from room authority inputs into the local
`SKLMI` runtime cache that downstream tracker/settings code can inspect.

`SKLMI` transports and persists this metadata generically. It does not own the
game-specific meaning of fields such as `seed_hash`, `goal`, or `slot_data`.

## What `SKLMI` Can Already Read

Offline mode:

- `OfflineRoomClient` reads `room.state` lines using the existing
  `kind|key|value` contract.
- `pending|...`, `consumed|...`, and `checked|...` continue to work unchanged.
- `meta|...` lines are now preserved instead of being dropped on the next save.

Game-server mode:

- `GameServerRoomClient` already knows:
  - `room_session_name`
  - `room_slot_id`
  - `driver_instance_id`
  - `linkedworld_id`
  - `core_profile`
- those values come from runtime CLI options plus the loaded bridge manifest.

## Runtime Propagation Path

1. `sekailink_sklmi_runtime` loads the bridge manifest and creates a `RoomClient`.
2. `OfflineRoomClient` loads file-backed `meta|...` lines from `--room-state`.
3. `GameServerRoomClient` synthesizes a metadata snapshot from the live runtime
   session context.
4. `RoomSynchronizedRuntimeSession` merges:
   - room-client metadata
   - runtime-owned metadata (`driver_instance_id`, `linkedworld_id`, `core_profile`)
   - existing local `room-sync.state` metadata
5. After each successful tick, `RoomSynchronizedRuntimeSession` persists the
   merged snapshot into `<runtime-state>/<bridge>.room-sync.state`.

## Persisted `room-sync.state` Contract

The local room-sync cache now contains:

- `meta|driver_instance_id|...`
- `meta|linkedworld_id|...`
- `meta|core_profile|...`
- `meta|room_mode|offline|sekailink_game_server`
- in game-server mode:
  - `meta|room_session_name|...`
  - `meta|room_slot_id|...`
  - `meta|room_ticket_issued|0|1`
- in offline mode:
  - any seeded `meta|...` entries already present in `room.state`
  - examples: `world_id`, `seed_id`, `seed_hash`, `slot_data`
- existing sync markers:
  - `reported|<check-key>|<mapped-value>`
  - `applied|<delivery-or-item-id>|<mapped-value>`

## Practical Outcome

This gives tracker/settings consumers one stable runtime-owned file that contains:

- room sync progress
- runtime identity
- room/session identity
- offline seed metadata copied forward from the original room-state seed

No network contract was changed, and the existing room-state line format remains
the same.

Interpretation boundary:

- `SKLMI` owns the persistence and merge path
- the room layer owns issuance of room/session metadata
- the `LinkedWorld` and downstream tracker logic own the meaning of game-shaped
  metadata keys
