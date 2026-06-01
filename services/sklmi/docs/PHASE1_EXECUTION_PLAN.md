# SKLMI Phase 1 Execution Plan

Status: completed
Date: 2026-04-23

## Goal

Turn `sekailink-sklmi/` into the active MVP chantier before freezing the final LinkedWorld contract.

## Why SKLMI First

- SKLMI is the center of emulator-backed multiworld runtime behavior.
- Without it, LinkedWorld manifests would be defined around a moving bridge.
- Sekaiemu should adapt once to a stable memory interface, not multiple times to temporary ad-hoc bridges.
- The stability lesson from SNI is more important right now than package polish.

## Phase 1 Deliverables

- Public API header for:
  - memory provider
  - runtime session
  - event sink
  - linkedworld bridge
  - trace/log record format
- Fake memory provider usable in tests.
- One smoke binary proving:
  - manifest bridge load
  - memory watch
  - check event
  - item injection
  - reconnect event path
  - reset event
- Repo-local build path with CMake.

## Immediate Next Steps

1. Add local bridge state serialization.
2. Define the first data-driven rule format beyond simple equality.
3. Pick the first simple proof target:
   - `Earthbound`
   - or `ALTTP`
4. Start Sekaiemu integration only after the simple SKLMI proof is frozen.

## Completion Notes

Completed in Phase 1:

- public API header for `MemoryProvider`, `RuntimeSession`, `EventSink`, `LinkedWorldBridge`, and trace format
- fake memory provider with writable memory domains and endian-aware unsigned access helpers
- manifest contract loader for simple checks and injections
- runtime session exercising `start -> tick -> reset -> reconnect -> stop`
- smoke binary passing with output `sklmi_smoke_ok`

Kept deliberately out of Phase 1:

- local bridge state persistence
- richer conditional expressions
- real Sekaiemu memory provider integration
- network/client transport

## Deliberate Non-Goals

- No Lua execution layer.
- No server transport layer inside the first smoke.
- No Sekaiemu renderer integration yet.
- No Wind Waker or SoH as first generic proof.
