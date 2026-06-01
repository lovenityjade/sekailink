# SKLMI Phase 4 Handoff

Status: in progress
Date: 2026-04-24

## Current Position

We are at the start of Phase 4 of SKLMI:

- runtime-side proof is completed
- real Sekaiemu runtime socket path is validated
- EarthBound proof through `sekailink_runtime_snes` is validated
- the next chantier is the client/server bridge layer

This means the runtime side is no longer the blocker.
The active problem is now:

- how SKLMI synchronizes checks and pending items with a room/offline authority
- without pushing protocol logic back into emulator frame-sensitive code

## What Was Intentionally Chosen

The Phase 4 implementation direction is:

- keep SKLMI modular
- add a generic `RoomClient` abstraction
- start with an `OfflineRoomClient` proof
- keep the synchronization loop outside rendering/frame paths
- later swap or extend the same contract toward real SekaiLink/AP room authority

Reason:

- we need a stable offline/local proof first
- we do not want to freeze SKLMI around a temporary network protocol
- a file-backed/local authority is enough to validate the room-sync contract

## Code State At Pause

The following API changes were started in:

- `include/sekailink_sklmi/api.hpp`
- `src/api.cpp`

### Added or being added

- `RoomItem`
- `RoomClient`
- `OfflineRoomClient`
- `RoomSynchronizedRuntimeSession`
- `InjectRule.room_controlled`
- `BufferedForwardingEventSink`

### Intent of these additions

`RoomClient`
- abstract room authority
- report checked locations
- poll pending items
- acknowledge applied items

`OfflineRoomClient`
- file-backed room authority for the first full offline proof
- stores checked locations
- stores pending items
- stores consumed items

`RoomSynchronizedRuntimeSession`
- wraps a `BridgeSession`
- forwards bridge-emitted check events to a `RoomClient`
- polls pending room items
- applies item injections through memory writes
- persists bridge state and room sync state separately

## Important Constraint

For SNES behavior, continue to use SNI as the reference model:

- memory provider boundary stays clean
- reconnect stays outside frame-sensitive loops
- room sync must not live inside emulator render timing
- no regression into Lua-style per-game network logic

## Likely Next Steps

1. Finish compiling the new API additions cleanly.
2. Add a Phase 4 smoke test:
   - fake or file-backed room state
   - one reported check
   - one pending item delivered and acknowledged
3. Validate persistence:
   - restart session
   - ensure already-reported checks are not re-reported
   - ensure already-applied items are not re-applied
4. Document the room state file format once stabilized.
5. Only then decide whether the next adapter should target:
   - local SekaiLink room authority
   - or AP-compatible room transport

## What Must Not Happen

- do not move room sync into emulator frame code
- do not make the room adapter SNES-specific
- do not skip the offline/local proof
- do not bind SKLMI directly to Core UI concerns

## Resume Command Context

The last active files before pause were:

- `/home/thelovenityjade/DevSSD/SekaiLinkDev/sekailink-sklmi/include/sekailink_sklmi/api.hpp`
- `/home/thelovenityjade/DevSSD/SekaiLinkDev/sekailink-sklmi/src/api.cpp`
- `/home/thelovenityjade/DevSSD/SekaiLinkDev/docs/roadmap/SKLMI_ROADMAP.md`

The next thing to do is:

- finish the Phase 4 `RoomClient` / offline sync implementation and get it building
