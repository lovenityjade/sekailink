# SKLMI Phase 1 / Runtime Ownership

Status: implemented
Date: 2026-04-30

Current BETA-3 correction, 2026-05-21:

`GameServerRoomClient` remains as compatibility/test coverage, but the active
BETA-3 direction is now Archipelago-first. SKLMI should speak Archipelago room
packets for player runtime communication, then let SekaiLink Core make that
flow approachable. Do not continue expanding the custom SekaiLink game-server
protocol unless a later plan explicitly reactivates it.

## Goal

Turn `SKLMI` from a library/test-only surface into the first real runtime process
that can execute migrated `LinkedWorld` bridge behavior through one generic
runtime, validated at the time with `EarthBound` and `ALTTP` proof fixtures.

## What Phase 1 Adds

- a real runtime executable:
  - `sekailink_sklmi_runtime`
- offline room authority:
  - `OfflineRoomClient`
- game-server room authority:
  - `GameServerRoomClient`
- Archipelago room packet boundary:
  - `ArchipelagoTransport`
  - `TcpWebSocketArchipelagoTransport`
  - `ArchipelagoRoomClient`
- room-synchronized runtime ownership:
  - `RoomSynchronizedRuntimeSession`
- representative migrated proof manifests and fixtures used to validate the
  generic runtime boundary during the phase
- newline-delimited JSON trace/event logs

## Ownership Boundary

`Sekaiemu`
- runs the core
- exposes memory through the Unix socket runtime surface

`SKLMI`
- interprets loaded manifest checks
- applies loaded manifest room-controlled items
- persists bridge state
- persists room-sync state
- forwards check events into room authority
- for BETA-3, translates those events and deliveries through Archipelago packet
  semantics (`Connect`, `LocationChecks`, `ReceivedItems`)

`LinkedWorld`
- owns the concrete game-specific addresses, checks, actions, and identities
- remains the long-term home for per-game bridge content

## Runtime Contract

CLI:

```bash
sekailink_sklmi_runtime \
  --memory-socket <path> \
  --bridge-manifest <path> \
  --room-state <path> \
  --runtime-state <dir> \
  --trace-log <path> \
  [--mode offline|sekailink_game_server|archipelago] \
  [--room-host <host>] \
  [--room-port <port>] \
  [--room-session-name <name>] \
  [--room-slot-id <id>] \
  [--room-control-channel core|admin] \
  [--room-control-auth-token <token>] \
  [--room-runtime-auth-token <token>] \
  [--room-runtime-session-token <token>] \
  [--ap-host <host>] \
  [--ap-port <port>] \
  [--ap-path <path>] \
  [--ap-game <game>] \
  [--ap-slot-name <slot>] \
  [--ap-password <password>] \
  [--ap-uuid <uuid>] \
  [--ap-tags AP,SekaiLink,SKLMI] \
  [--tick-ms <ms>] \
  [--max-ticks <count>]
```

Behavior:

- `--mode offline`
  - authority is file-backed through `OfflineRoomClient`
- `--mode sekailink_game_server`
  - authority is remote through `GameServerRoomClient`
  - `SKLMI` first requests a runtime ticket, then uses runtime commands for checks/items/acks
- `--mode archipelago`
  - `ArchipelagoRoomClient` owns AP packet translation
  - `TcpWebSocketArchipelagoTransport` owns the first native `ws://` transport
  - `--ap-host` and `--ap-port` target the Archipelago MultiServer endpoint
  - `--ap-game`, `--ap-slot-name`, `--ap-password`, `--ap-uuid`, and `--ap-tags`
    form the AP `Connect` packet
  - the custom game-server mode should be treated as compatibility while this
    path is wired into `sekailink_sklmi_runtime`
- room authority state and room-sync state are separate concepts
- the bridge state file is resolved from the manifest `state_file` inside `--runtime-state`
- `room_controlled` injection rules are not auto-applied by `ManifestBridgeSession`
- those rules are applied only when a room item is delivered

## Validation

Phase 1 is validated in-tree with:

- existing `EarthBound` and `ALTTP` goldens still passing
- `sklmi_room_sync_smoke`
- `sklmi_runtime_runner_smoke`
- `sklmi_game_server_room_client_smoke`
- `sklmi_runtime_game_server_smoke`
- `sklmi_archipelago_room_client_smoke`
- `sklmi_archipelago_websocket_smoke`

This phase does not add new games and does not replace `ProfileBridge` for
non-migrated games.

It also should not be read as a claim that `SKLMI` permanently owns
`EarthBound` or `ALTTP` bridge content. Those games were phase fixtures used to
prove the generic runtime.
