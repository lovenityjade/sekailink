# Room Server Archipelago Alignment

Date: 2026-05-05

## Goal

Explain how `sekailink-link-room` is moving toward an ALTTP live integration
shape inspired by the official Archipelago protocol, without copying code or
claiming wire compatibility.

Reference direction:

- `RoomInfo`
- `Connected`
- `ReceivedItems`
- `LocationChecks`
- `RoomUpdate`
- `Sync`

Official protocol reference:

- `https://alwaysintreble.github.io/Archipelago/network%20protocol.html`

## What now aligns better

Room and seed metadata:

- `seed_name`
- `seed_id`
- `seed_hash`
- `tracker_pack`
- `tracker_variant`
- `slot_data`

Runtime identity and liveness:

- `runtime_kind`
- `runtime_session_name`
- `driver_instance_id`
- `linkedworld_id`
- `core_profile`
- heartbeat timing and counters

Check state:

- `checked_locations`
- `missing_locations`
- `record_check`

Received item state:

- monotonic `item_index`
- incremental replay from `from_item_index`
- `next_received_item_index`

## Current room-server entrypoints

TCP commands:

- `runtime_heartbeat`
- `runtime_disconnect`
- `set_seed_metadata`
- `sync_room`

HTTP inspection:

- `GET /rooms/<room_id>/snapshot`
- `GET /rooms/<room_id>/summary`
- `GET /rooms/<room_id>/sync`
- `GET /rooms/<room_id>/events`

## Important differences from Archipelago

This repo still does not implement:

- WebSocket transport
- AP login handshake
- AP datapackages
- AP scout/hint packets
- AP chat / `PrintJSON`

That is intentional for now. The current target is a same-host room authority
with enough metadata, item replay, check sync, and runtime visibility to support
a first external ALTTP test safely.
