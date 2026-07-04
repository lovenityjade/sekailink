# SKLMI ALTTP Runtime Recipe

Status: implemented
Date: 2026-05-05

## Goal

Provide one concrete `SKLMI`-local path to exercise an ALTTP runtime loop with:

- offline room metadata seeding
- room-controlled item delivery
- persisted `room-sync.state`
- JSONL trace diagnostics

This is not a full game integration guide. It is the shortest repeatable path to
prove that `SKLMI` can execute an ALTTP-shaped bridge manifest end to end.

This file is a game-shaped proof recipe only. The canonical ALTTP bridge
content, coverage expansion, and live contract belong in the ALTTP
`LinkedWorld` repo, not in `sklmi`.

## Fastest Proof

Configure and build:

```bash
cmake -S . -B build/sklmi
cmake --build build/sklmi --target \
  sekailink_sklmi_runtime \
  sekailink_sklmi_alttp_golden \
  sekailink_sklmi_alttp_runtime_runner_smoke
```

Run the in-process ALTTP golden:

```bash
ctest --test-dir build/sklmi -R '^sekailink_sklmi_alttp_golden$' --output-on-failure
```

Run the runtime CLI proof:

```bash
ctest --test-dir build/sklmi -R '^sekailink_sklmi_alttp_runtime_runner_smoke$' --output-on-failure
```

What the runtime smoke proves:

- two ALTTP location checks are emitted from RAM watches
- one room-controlled ALTTP item is injected through a multi-write action
- seeded offline room metadata survives persistence
- merged runtime metadata is written to `*.room-sync.state`
- runtime JSONL includes startup/configuration traces plus emitted events
- runtime JSONL captures provider metadata (`system`, protocol version, domains)
- runtime JSONL captures the merged room metadata snapshot used for ALTTP seed/settings propagation
- runtime JSONL records that the ALTTP room item was applied through a
  `write_sequence` rather than a direct single write

Tracker runtime note:

- `sekailink_sklmi_runtime` accepts `--tracker-pack` as either an extracted
  bundle directory or a zip-style archive.
- Archive packs are extracted beside `tracker.snapshot.json` under
  `tracker.pack.extracted/`; the published snapshot includes `assets_root` so
  Sekaiemu can resolve item/map images without parsing pack logic.
- `--tracker-assets-root` can override that published asset root when assets
  are staged separately from the pack data.

## Expected Files

The ALTTP runtime smoke creates a temp runtime root and verifies these files:

- `room.state`
  - contains seeded `meta|world_id|...`, `meta|seed_id|...`, `meta|seed_hash|...`, `meta|slot_data|...`
  - contains `checked|0xEA79|Sanctuary`
  - contains `checked|0xE9BC|Link's House`
  - contains `consumed|item-hookshot|item.hookshot|Hookshot|10|10|Hookshot`
- `runtime-state/alttp_runtime_runner.room-sync.state`
  - contains runtime-owned metadata such as `driver_instance_id`, `linkedworld_id`, `core_profile`, `room_mode`
  - mirrors seeded offline metadata
  - records `reported|...` check markers and `applied|item-hookshot|Hookshot`
- `trace.jsonl`
  - contains trace records for `manifest_loaded`, `runtime_config`, `room_client_ready`, `session_start`, `stop_condition`
  - contains `provider_metadata` with the exposed domain contract seen by `SKLMI`
  - contains `room_metadata_ready` so seed/settings metadata presence is visible before the first ALTTP tick
  - contains `room_item_applied` with `mode=write_sequence` and `steps=3` for the ALTTP hookshot proof
  - contains event records carrying `driver_instance_id`, `linkedworld_id`, and `core_profile`

## Manual Runtime Invocation

If you want to drive the runtime binary directly, reproduce the same shape as the
smoke:

```bash
build/sklmi/sekailink_sklmi_runtime \
  --memory-socket /tmp/sekaiemu.sock \
  --bridge-manifest /tmp/alttp-bridge.json \
  --room-state /tmp/alttp-room.state \
  --runtime-state /tmp/alttp-runtime-state \
  --trace-log /tmp/alttp-trace.jsonl \
  --tick-ms 16
```

The important ALTTP characteristics for the manifest are:

- checks on `system_ram`
- ALTTP location identities preserved through `event_key` or canonical ids
- room-controlled item actions for delivered items
- multi-write sequences for inventory/progress updates when one delivered item
  must update more than one memory slot in a fixed order

The current ALTTP path should be treated as ordered rather than hardware-atomic:
`SKLMI` executes each write step sequentially and traces failures per step, but
SNI-style WRAM runtimes may still observe those writes across multiple device
operations or frames.

## Recommended Readback

When an ALTTP run fails, inspect these in order:

1. `trace.jsonl`
2. `room.state`
3. `runtime-state/*.room-sync.state`
4. the memory provider side of the socket runtime

For seed/settings issues, check `room_metadata_ready` first. If `world_id`,
`seed_id`, `seed_hash`, or `slot_data` are missing there, the tracker-facing
metadata path is incomplete before ALTTP item/check logic even starts.

If `trace.jsonl` contains `manifest_loaded` and `provider_connect` but not
`session_start`, the failure is usually in manifest validation or room/session
startup. If `session_start` exists but `reported|...` or `applied|...` markers
never appear, the issue is usually watch coverage, room item matching, or memory
write eligibility.
