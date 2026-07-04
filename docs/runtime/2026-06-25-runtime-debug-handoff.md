# Runtime Debug Handoff - 2026-06-25

## Stop Point

Work was paused by Jade before continuing deeper into the GBA/runtime debug
path. Do not continue from assumption; wait for the next directive.

## Active Framing

Games are test fixtures. The target is core/system stability:

1. The emulator core exposes memory reliably.
2. SKLMI reads and writes through the generic bridge.
3. The room server receives checks/status.
4. The runtime receives server items/messages and writes them back safely.

Do not solve this as isolated game compatibility unless a generic layer has
already been proven sound.

## Changes Made Before Pause

- `runtime/apclient_common.py`
  - Added generic `.ap*` patch metadata reading from `archipelago.json` or
    `patch.json`.
  - Added structured `connection_refused` events for upstream
    `InvalidSlot`/`InvalidGame`.
  - Added `room_patch_mismatch` diagnostics when room seed and patch seed do
    not match.
  - Stops the wrapper via `ctx.exit_event` on fatal slot/game refusals instead
    of leaving a watcher running with `slot:null`.

- `apps/client-core/electron/main.cjs`
  - Mirrors `room_info`, `room_patch_mismatch`, and `connection_refused` into
    readable Sekaiemu/AP debug chat diagnostics.

- `apps/client-core/CHANGELOG.md`
  - Added 2026-06-25 entries for the wrapper diagnostics.

- Documentation
  - `docs/ACTIVE_DIRECTIVE.md`
  - `docs/runtime/core-system-validation-directive.md`

## Verification Completed

Both syntax checks passed:

```text
python3 -m py_compile runtime/apclient_common.py
node -c apps/client-core/electron/main.cjs
```

## Important Observation

The last MZM runtime trace showed a successful BizHawk-protocol bridge startup
and handler selection, then Archipelago server refusal:

```text
AP -> Connect: thelov-metr-ee09 / Metroid: Zero Mission
ConnectionRefused: InvalidSlot
```

The patch inspected at:

```text
<local-home>/.config/sekailink-client/runtime/downloads/AP_53368305998360213434_P1_thelov-metr-ee09 (2).apmzm
```

contained:

```text
player_name = thelov-metr-ee09
seed_name = 53368305998360213434
game = Metroid Zero Mission
```

This means the observed failure was not yet proof of an mGBA memory exposure
failure. It may have been a room/slot/seed artifact mismatch, stale room, or
server-side session mismatch. The new diagnostics were added to make that
visible on the next run.

## Next Debug Rule

Before touching mGBA, SKLMI, or game-specific code, reproduce with a fresh room
and confirm:

- room seed shown by `RoomInfo`;
- patch seed shown by wrapper metadata;
- slot name sent in `Connect`;
- whether the server reaches `Connected` or refuses before memory validation.

Only after `Connected` succeeds should memory/check/item behavior be treated as
a core bridge issue.
