# Room -> SKLMI / tracker flow

Date: 2026-05-05

## Scope

This note documents the current integration flow inside `sekailink-link-room`.

Local assumption for this repo:
- `SKLMI` is the downstream consumer of room projections and audit-friendly room records.
- tracker-facing metadata is carried by room state fields such as `tracker_pack`, `tracker_variant`, `tracker_connected`, `seed_id`, `seed_hash`, and `slot_data`.
- there is no direct tracker transport implemented in this repo today; the repo prepares structured state for downstream consumers.

For a first live ALTTP test, read that literally:

- this repo can feed SKLMI-ready state
- it does not itself expose a native SKLMI transport contract yet

## Runtime flow

1. A room mutation enters through `handle_protocol_json(...)` in `src/room_server_protocol.cpp`.
2. The command is routed to `handle_room_server_command_with_audit(...)` in `src/room_registry.cpp`.
3. The room session mutates in `src/room_session.cpp`.
4. Audit persistence writes:
   - snapshots for `create_room` and `snapshot_room`
   - the last room event for mutation commands
   - the last client report for `ingest_client_report`
5. Projection persistence writes a `room_record` for the latest room snapshot and only the new event/report delta produced by the command.

## Seed / settings path

Seed and settings live in two layers:
- room identity and tracker-facing seed metadata:
  - `seed_name`
  - `seed_id`
  - `seed_hash`
  - `tracker_pack`
  - `tracker_variant`
- slot-specific runtime settings:
  - `slot_data`

Current projection behavior:
- `project_room_record(...)` publishes top-level seed and runtime fields for fast filtering.
- `slot_data_keys` and `slot_data_entry_count` give a compact summary for triage without opening the full snapshot payload.
- the full room snapshot still remains under `payload`.

## First live consumer options

For the first external test, downstream consumers should use one of:

- HTTP admin inspection on the private loopback surface
- the shared MySQL projection output at the configured `projection_root`

Recommended live test choice:

- `projection_backend=mysql`
- projection target shared with Nexus room-query

Why:

- it matches the active live topology on `link`
- it keeps room information flowing through the same source path that Nexus
  already reads
- it still avoids inventing a tracker transport that this repo does not
  implement yet

## Local proof status

The native loopback ALTTP path has now been exercised locally with:

- `create_room`
- `set_slot_data`
- `enqueue_received_item`
- `record_check`

Observed over the private HTTP inspection surface:

- `GET /rooms` exposes the room id
- `GET /rooms/<room_id>/summary` reflects:
  - `items_received`
  - `checks_recorded`
- `GET /rooms/<room_id>/snapshot` carries:
  - `seed_id`
  - `seed_hash`
  - `tracker_pack`
  - `tracker_variant`
  - `slot_data`
  - `received_items`
  - `checked_locations`
- `GET /rooms/<room_id>/events` shows:
  - `item_received`
  - `location_checked`

That means `sekailink-link-room` is already producing a usable ALTTP room state
for downstream `SKLMI` / tracker consumers on loopback, while the live host can
continue to export the same room state through the official:

- `Link Room -> MySQL projection -> Nexus room-query`

## Item journal path

Inbound item journal:
- `enqueue_received_item(...)` appends the item to `received_items`
- it also emits an `item_received` event

Current `item_received` event payload now includes:
- `item_index`
- `item_id`
- `item_name`
- `location_id`
- `sender_slot`
- `sender_alias`
- `flags`
- `received_item_count`

This makes the event stream usable as a compact journal without re-reading the full snapshot.

Current metadata visibility also includes:

- `slot_data_updated` room events with:
  - `slot_data_shape`
  - `slot_data_entry_count`
  - `slot_data_keys`
- `runtime_heartbeat` room events with:
  - `runtime_kind`
  - `runtime_session_name`
  - `driver_instance_id`
  - `linkedworld_id`
  - `core_profile`
- compact room sync responses through:
  - TCP command `sync_room`
  - HTTP route `GET /rooms/<room_id>/sync`

That gives ops and downstream consumers one lighter-weight way to confirm that
ALTTP seed/runtime settings changed without diffing the entire room snapshot.

Outbound / sent-item view:
- the repo does not currently expose a first-class `item_sent` event
- the closest structured source today is `location_to_item`, `location_to_item_id`, and `location_names` inside the room snapshot/projection payload

## Why the projection cleanup matters

Before the local 2026-05-05 cleanup, read-only protocol calls could re-append room projections, and mutation projections could re-send the full historical event/report lists.

Current behavior is cleaner:
- read-only commands do not append projection noise
- projection stores only receive the new event/report delta for each successful mutation
- room snapshots remain complete so restore/query paths still work
