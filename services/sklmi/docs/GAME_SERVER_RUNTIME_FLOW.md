# SKLMI Game Server Runtime Flow

Status: implemented
Date: 2026-05-01

## Purpose

Describe the current runtime path when `SKLMI` is attached to `SekaiLink Game Server`
instead of the local offline proof backend.

## Runtime Topology

`Sekaiemu`
- exposes the Unix socket memory provider

`SKLMI`
- loads the `LinkedWorld` `sklmi` section
- runs `RoomSynchronizedRuntimeSession`
- uses `GameServerRoomClient`

`SekaiLink Game Server`
- issues runtime session tickets
- validates runtime check events
- exposes pending items
- accepts delivery acknowledgements

## Startup Flow

1. `Sekaiemu` starts and exposes the memory socket.
2. `SKLMI` loads the manifest and connects to memory.
3. `SKLMI` creates `GameServerRoomClient`.
4. `GameServerRoomClient` requests a runtime ticket using:
   - `session_name`
   - `slot_id`
   - `driver_instance_id`
   - `linkedworld_id`
   - `core_profile`
   - the selected control endpoint
5. `SKLMI` starts the bridge session and resumes bridge-local state.

## Tick Flow

For each tick:

1. the bridge evaluates watch rules
2. new `location_checked` events are forwarded to `SekaiLink Game Server`
3. pending items are polled from `SekaiLink Game Server`
4. matching `room_controlled` injection rules are applied to memory
5. acknowledged items are marked back to the server
6. bridge state and room-sync cache are persisted locally

## Endpoint Modes

`SKLMI` supports both:

- one shared room port via `--room-port`
- split ports via `--room-control-port` and `--room-runtime-port`

It also supports:

- runtime ticket issuance at connect time
- pre-issued runtime session tokens when ticket issuance should stay outside the
  runtime process

## Important Boundary

`SKLMI` does not become the room authority.
`SKLMI` also does not become a game-specific runtime.

`SKLMI`:
- detects checks
- applies delivered items
- caches local sync state
- executes whichever concrete check/action set the loaded `LinkedWorld`
  declares

`SekaiLink Game Server`:
- decides whether a check is accepted
- owns item delivery state
- owns runtime session ticketing

The loaded `LinkedWorld`:

- decides concrete memory addresses
- decides concrete check/item identities
- decides which room-controlled rules exist for one game

The live room/server side may also consume `LinkedWorld` metadata for
session-facing concerns, but it still does not replace the raw memory boundary.
Raw memory reads and writes remain between `Sekaiemu` and `SKLMI`.

## Local Persistence

Two local files still matter:

- bridge state:
  - emitted checks
  - injected non-room items
  - bridge-local timing state
- room-sync cache:
  - reported check keys
  - locally applied remote item ids

Even in `sekailink_game_server` mode, the room-sync cache remains local and is not
the authority source.

## Validation

Current proof coverage:

- `sklmi_game_server_room_client_smoke`
- `sklmi_runtime_game_server_smoke`

These prove:

- runtime ticket acquisition
- check forwarding
- pending item polling
- delivery acknowledgement
- real memory injection through the `Sekaiemu`-style Unix socket path
