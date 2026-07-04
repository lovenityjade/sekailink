# Generic Bootchain E2E Preparation

Date: 2026-06-19

## Goal

Prepare a real-time E2E launch path for any enabled generic game from
`runtime/game-registry/archipelago-clients.json`.

Generic currently means:

- `sni`
- `bizhawk`

These wrappers cover the immediate NES, SNES, GB/GBC, and GBA style test pool.

## New entry point

```bash
python3 runtime/bootchain_supervisor.py
```

This sits above `runtime/wrapper_supervisor.py`.

It resolves:

```text
game registry -> wrapper family -> wrapper supervisor command
              -> optional Sekaiemu command
              -> optional PopTracker command
              -> session log root
```

It does not assume ALTTP.

## List generic candidates

```bash
python3 runtime/bootchain_supervisor.py list-generic
```

## Pick a deterministic random candidate

```bash
python3 runtime/bootchain_supervisor.py plan \
  --random-generic \
  --random-seed live-candidate \
  --server 127.0.0.1:38281 \
  --slot TestSlot
```

Example result from the smoke environment:

```text
Mega Man 3 -> wrapper=bizhawk -> platform=NES
```

## Plan a specific game

```bash
python3 runtime/bootchain_supervisor.py plan \
  --game-key pokemon_emerald \
  --server HOST:PORT \
  --slot PlayerName \
  --rom /path/to/generated.gba
```

The plan reports readiness fields:

- `wrapper_supervisor`
- `python`
- `generic_wrapper`
- `sekaiemu`
- `core`
- `rom`
- `server`
- `slot`

If `rom`, `core`, and `sekaiemu` are true, the plan also includes a concrete
`sekaiemu_command`.

## Launch wrapper-only diagnostic

This is useful before opening emulator windows:

```bash
python3 runtime/bootchain_supervisor.py launch \
  --game-key pokemon_emerald \
  --server HOST:PORT \
  --slot PlayerName \
  --max-runtime 30
```

This starts:

```text
bootchain_supervisor -> wrapper_supervisor -> Archipelago client wrapper
```

The wrapper will still require the expected emulator-side connector for full
gameplay sync. This diagnostic proves the AP client process can start, log, and
attempt connection under SekaiLink supervision.

## Launch with Sekaiemu command prepared

```bash
python3 runtime/bootchain_supervisor.py launch \
  --game-key mega_man_3 \
  --server HOST:PORT \
  --slot PlayerName \
  --rom /path/to/generated.nes \
  --launch-sekaiemu
```

This starts Sekaiemu only when:

- a matching core exists for the platform;
- the ROM path exists;
- `sekaiemu_libretro_spike` is present.

The wrapper still runs as a separate process. The emulator-side connector must
match the wrapper family:

- `bizhawk`: Sekaiemu must expose the generic BizHawk-compatible Lua/socket
  behavior expected by Archipelago's BizHawk client.
- `sni`: Sekaiemu must expose or be bridged to an SNI websocket endpoint.

## Launch with PopTracker

```bash
python3 runtime/bootchain_supervisor.py launch \
  --game-key alttp \
  --server HOST:PORT \
  --slot PlayerName \
  --rom /path/to/generated.sfc \
  --tracker-pack /path/to/poptracker-pack.zip \
  --launch-sekaiemu \
  --launch-tracker
```

Tracker launch is optional. BETA-3 can first validate wrapper + emulator sync,
then add PopTracker Edition to the same session id.

## Logs

Bootchain session logs live under:

```text
runtime/logs/bootchain/<session-id>/
```

Wrapper flight recorder logs live under:

```text
runtime/logs/bootchain/<session-id>/wrapper/<session-id>.jsonl
```

## Validation

Smoke test:

```bash
python3 runtime/tests/bootchain_supervisor_smoke.py
```

Expected:

```text
bootchain_supervisor_smoke_ok
```

The smoke validates:

- generic candidate list is populated;
- deterministic random selection works;
- wrapper command is generated;
- test bootchain launches `wrapper_supervisor`;
- `wrapper_supervisor` launches a controlled fixture wrapper;
- lifecycle JSONL events return to the bootchain.

## Current boundary

This connects the generic bootchain process layer. It does not yet prove live
memory synchronization for every game.

The next real-time test must choose one generic game and validate:

```text
AP room -> wrapper supervisor -> game client wrapper
        -> Sekaiemu connector -> ROM memory/checks/items
        -> optional PopTracker Edition
```

Recommended first non-ALTTP live candidates:

- `mega_man_3` for NES/BizHawk generic.
- `pokemon_emerald` for GBA/BizHawk generic.
- `earthbound` for SNES/SNI path if the SNI bridge is the focus.
