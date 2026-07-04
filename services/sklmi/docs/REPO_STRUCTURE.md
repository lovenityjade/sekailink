# SKLMI Repo Structure

Status: current orientation note
Date: 2026-05-05

## Purpose

This note describes the structure that this clean-room repo should preserve so
multiple agents can work safely without re-opening migration debates every time.

## What `sklmi` Owns

Runtime code that is generic across games:

- memory-provider abstraction
- Unix socket memory transport
- manifest parsing and validation
- bridge sessions and runtime sessions
- room-client integration points
- Archipelago packet translation for runtime room communication
- native plain-WebSocket transport for Archipelago `ws://` room connections
- trace/event emission
- local bridge and room-sync persistence

Public API surface:

- `include/sekailink_sklmi/api.hpp`

Implementation:

- `src/api_*.cpp`
- `src/runtime_*.cpp`
- `src/unix_socket_memory_provider.cpp`

Validation:

- `tests/`

## What Is Not Runtime Product Code

- `docs/`
  - design notes, handoff, migration history
- `fixtures/`
  - reserved for test fixtures only
- `source-snapshots/`
  - reserved for extracted references or comparison inputs only

These directories should stay safe to prune, refresh, or repopulate without
changing the runtime boundary.

## Manifest Boundary

Current runtime behavior accepts two inputs:

- preferred: a `LinkedWorld` document with an embedded `sklmi` section
- compatibility: a legacy standalone bridge manifest

Inside `sklmi`, manifests are data inputs, not product modules.

That means:

- the parser and validator belong here
- representative test manifests belong here when embedded in tests/fixtures
- permanent game catalog content should migrate out of this repo

## Future Outbound Split

The following should leave `sklmi` or stay outside it as the ecosystem hardens:

- canonical `LinkedWorld` manifests per game
- repo-level contract governance shared across drivers
- cross-repo schema publication and version policy
- game-specific check/action libraries

Probable destinations:

- `LinkedWorld` repos for per-game authored content
- `Contracts` repos for shared schema, compatibility notes, and policy

`sklmi` should consume those artifacts, not become their long-term home.

## Test Boundary

Tests in this repo should prove one of four things:

- API-level smoke coverage
- manifest compatibility and validation behavior
- runtime/session ownership behavior
- integration proofs for supported transport and room flows
- protocol translation behavior, especially Archipelago packet semantics

Tests should avoid becoming a shadow home for large game-specific datasets unless
those datasets are the smallest artifact that proves a generic runtime contract.

## Safe Change Rules For Multiagent Work

- Edit only inside this repo.
- Prefer changes in `include/`, `src/`, `tests/`, and current-orientation docs.
- Treat historical phase docs as reference material unless the current boundary
  would be misleading without a correction.
- Do not re-introduce permanent ownership of game manifests just because tests
  still embed small JSON examples.
- Keep the parser restrictive; if a manifest starts acting like code, move the
  design discussion to contracts instead of widening runtime semantics here.
