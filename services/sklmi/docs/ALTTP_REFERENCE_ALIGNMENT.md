# SKLMI ALTTP Reference Alignment

Status: analysis for runtime preparation
Date: 2026-05-05

## Scope

This note compares the current `sklmi` ALTTP runtime shape against the
behavioral reference used by the ALTTP ecosystem:

- Archipelago ALTTP client behavior
- SNI device/runtime memory behavior

The goal is alignment and gap visibility only. No reference code is copied into
`sklmi`.

This note is a game-shaped audit of the generic runtime. It is not an ownership
claim that `sklmi` should become the home of canonical ALTTP bridge content.
That content belongs in the ALTTP `LinkedWorld` repo.

## Behavioral Reference Taken From Archipelago

The Archipelago ALTTP client establishes a few stable expectations that matter
for a coherent run:

- item delivery is save-memory based rather than an abstract queue
  - a progress counter is advanced
  - the delivered item id is written
  - the sending player/owner byte is written
- location checking is memory-driven
  - underworld, overworld, NPC, misc, and shop checks are derived from memory
    state rather than from opaque RPCs
- ROM identity matters
  - the client validates the active ROM identity before trusting memory-derived
    checks
- tracker-facing metadata matters
  - the effective slot/seed context is tied to ROM/session identity rather than
    to raw memory alone
- optional writeback exists for remote-owned checks
  - when the run allows collect propagation, already-known remote checks can be
    reflected back into save data
- scouting is a memory handshake
  - scout request/reply bytes are part of the ALTTP loop, not just out-of-band
    server metadata

For the currently used offsets, the practical mapping is:

- `system_ram + 0xF4D0`
  - receive progress counter
- `system_ram + 0xF4D2`
  - received item id
- `system_ram + 0xF4D3`
  - received item sender/player id

That is exactly the shape now modeled by the `sklmi` ALTTP smoke manifest.

## Behavioral Reference Taken From SNI

SNI adds two important runtime constraints:

- ordered batched writes are normal
- hardware-level atomicity must not be assumed for WRAM writes

That means a correct `sklmi` ALTTP bridge should:

- preserve write order for one logical item delivery
- trace step-level write failures clearly
- avoid pretending that one manifest `writes` array is a single atomic device
  transaction

## What `sklmi` Already Covers Well

Current `sklmi` behavior is in a good place for a first real ALTTP runtime pass:

- ALTTP item delivery can be represented as a bounded multi-write sequence
  against `system_ram`
- room-controlled deliveries are kept outside the generic bridge tick path
- ALTTP checks can be emitted from memory watch rules with canonical ids or
  explicit `event_key` values
- offline room metadata can carry:
  - `world_id`
  - `seed_id`
  - `seed_hash`
  - `slot_data`
- merged runtime metadata persists into `*.room-sync.state` for tracker/settings
  readers
- runtime JSONL now exposes:
  - provider memory metadata
  - merged room metadata readiness
  - item application mode/details for ordered ALTTP write sequences

## What Is Still Missing For A Fully Coherent ALTTP Run

These are the main remaining gaps relative to the Archipelago + SNI reference:

- full ALTTP check coverage
  - current smoke proves a few representative checks, not the full underworld /
    overworld / NPC / misc / shop coverage expected in practice
- explicit ROM identity validation
  - `sklmi` does not yet verify an ALTTP ROM marker/name before trusting
    memory-derived checks or metadata
- explicit collect/writeback semantics
  - there is no ALTTP-specific equivalent yet for remote-owned checked-location
    writeback into save state
- scout request/reply handshake
  - `sklmi` does not currently model the ALTTP scout bytes used by the
    Archipelago client loop
- richer tracker/session binding
  - `seed_id`, `seed_hash`, and `slot_data` are preserved, but there is no
    stronger runtime guard yet that confirms they still match the active game
    memory source
- provider-level grouped IO
  - `sklmi` can execute ordered write sequences, but the memory provider API is
    still step-based rather than exposing an explicit grouped `MultiRead` /
    `MultiWrite` concept

## Practical Interpretation

Today `sklmi` is ready for:

- ALTTP-shaped memory reads
- ALTTP-shaped ordered item writes
- tracker/settings metadata propagation
- useful runtime diagnostics when preparing a real run

Today `sklmi` is not yet a drop-in replacement for the full Archipelago ALTTP
client loop because it still lacks ROM validation, scout handshakes, collect
writeback semantics, and broad location-table coverage.

## Recommended Next Safe Steps

- add more ALTTP watch coverage from declarative manifests and smoke fixtures
- add a bounded ROM identity read/validation path before enabling check reports
- decide whether collect propagation belongs in the generic contract or in a
  higher ALTTP-specific room policy layer
- decide whether scout bytes should be represented as declarative actions/checks
  or handled by a separate bounded handshake path
