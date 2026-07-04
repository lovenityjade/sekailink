# Link Room To Nexus Room-Query

Date: 2026-05-05

## Goal

Describe the live data path that matters now:

- Link Room accepts live room mutations on `link`
- Link Room writes the shared MySQL room projection
- Nexus room-query reads that same projection

## Live chain

```text
ALTTP runtime / wrapper
  -> link room TCP surface
  -> RoomSession mutation
  -> MySQL projection tables
  -> Nexus room-query
```

Private operator inspection remains separate:

```text
link room HTTP admin surface
  -> /rooms
  -> /rooms/<room_id>/summary
  -> /rooms/<room_id>/snapshot
  -> /rooms/<room_id>/sync
  -> /rooms/<room_id>/events
```

## Write side on Link Room

The mutation path in this repo is:

1. protocol envelope enters `handle_protocol_json(...)`
2. room command routes through `handle_room_server_command_with_audit(...)`
3. `RoomSession` mutates room state
4. projection write happens through the configured projection store

For live MySQL projection, the relevant pieces are:

- [src/room_projection_mysql.cpp](<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-link-room/src/room_projection_mysql.cpp:1)
- [src/room_projection_sql.cpp](<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-link-room/src/room_projection_sql.cpp:28)

The MySQL schema currently contains:

- `room_records`
- `room_event_records`
- `client_report_records`

Each row stores:

- `room_id`
- serialized JSON payload in `record_json`

## Read side for Nexus room-query

This repo already includes the query helpers used to read the projection:

- [include/room_projection_query.hpp](<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-link-room/include/room_projection_query.hpp:1)
- [src/room_projection_query.cpp](<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-link-room/src/room_projection_query.cpp:1)

The important read functions are:

- `latest_room_snapshot(...)`
- `latest_room_snapshots(...)`
- `room_events(...)`
- `client_reports(...)`

That is the clean-room contract to keep in mind for Nexus room-query:

- Link Room is the writer
- the shared MySQL projection is the exchange point
- Nexus room-query is the reader

## What should stay true in live

- Link Room stays authoritative for live room mutation order
- MySQL projection stays append-oriented and query-friendly
- Nexus room-query should consume projection state, not verbose logs
- the private HTTP admin surface on `link` remains for operators and debugging

## Metadata that should survive the chain

Room metadata:

- `room_id`
- `game`
- `slot_id`
- `slot_name`
- `slot_alias`

Seed/tracker metadata:

- `seed_name`
- `seed_id`
- `seed_hash`
- `tracker_pack`
- `tracker_variant`
- `slot_data`

Runtime/item/check metadata:

- `runtime_state`
- `received_items`
- `checked_locations`
- `missing_locations`
- room events such as `slot_data_updated`, `item_received`, `location_checked`

## Operator implication

When validating a live issue, check the chain in this order:

1. Link Room accepted the mutation
2. Link Room log/state shows the room changed
3. the shared MySQL projection target is still the expected one
4. Nexus room-query is pointed at that same projection target

## Read-side checklist

Before promoting the private Link Room instance as the ALTTP test target,
confirm the read side still matches the write side:

- Link Room config still says `projection_backend=mysql`
- Link Room config still says `projection_root=sekailink_room_projection`
- the room server env still contains the expected `SEKAILINK_MYSQL_*`
- Nexus room-query is still the intended consumer for the same target
- the target room appears coherently on both:
  - Link Room HTTP admin inspection
  - Nexus room-query

## Verification commands

Verify the Link Room write-side assumptions:

```bash
jq -r '.projection_backend, .projection_root' \
  /opt/sekailink/link/room-server/config/room_server.json

jq . /opt/sekailink/link/room-server/runtime/room_server_state.json
```

Verify the private operator view on Link Room:

```bash
ROOM_HTTP_PORT="$(jq -r '.effective_http_port' /opt/sekailink/link/room-server/runtime/room_server_state.json)"

curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/summary" | jq .

curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/snapshot" | jq .
```

Verify the room-focused log stream before looking at Nexus:

```bash
bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

If the Nexus room-query endpoint is reachable from the host, verify that the
same room is visible there with the expected metadata and counts before
inviting testers.
