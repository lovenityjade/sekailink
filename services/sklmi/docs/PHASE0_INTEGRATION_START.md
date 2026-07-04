# SKLMI Phase 0 / Integration Start

This phase targets the new `Sekaiemu` runtime directly.

## Immediate contract

`Sekaiemu` exposes a local runtime memory socket:

- default path: `save_dir/runtime/sekaiemu-memory.sock`
- optional override: `--memory-socket <path>`

The transport is a newline-delimited Unix socket protocol:

1. `VERSION`
2. JSON arrays containing:
   - `SYSTEM`
   - `DOMAINS`
   - `READ`
   - `WRITE`

## Memory contract

`SKLMI` consumes the memory surface exposed by `Sekaiemu`'s `MemoryDomainRegistry`.

Canonical phase-0 domains:

- `system_ram`
- `save_ram`
- `video_ram`

Where additional descriptor-backed addrspaces exist, they may also be exposed,
but Phase 0 integration is intentionally pinned to the canonical domains above.

## Migration rule

- `ProfileBridge` is legacy transition code.
- `bridge_adapters` remain temporary legacy adapters.
- New check detection and injection proofs must move through `SKLMI`.

## Scope lock

Phase 0 is complete only when:

- `EarthBound` passes through `SKLMI`
- `ALTTP` passes through `SKLMI`
- no new games were added to get there

## Status

Phase 0 is now validated on the current worktree:

- `EarthBound` golden passes through `SKLMI`
- `ALTTP` golden passes through `SKLMI`
- the real `Sekaiemu` runtime proof passes through the Unix socket memory path

The next step is not to add games.
The next step is to continue replacing legacy bridge behavior with `SKLMI` while
keeping `bridge_adapters` only as temporary compatibility shims.
