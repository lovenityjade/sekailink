# SekaiLink Wrapper Supervisor

Date: 2026-06-19

## Purpose

`runtime/wrapper_supervisor.py` is the process boundary between the SekaiLink
bootchain and upstream Archipelago game clients.

It does not replace Archipelago client logic. It resolves a registered game,
starts the correct SekaiLink wrapper with the bundled Python runtime, supervises
the process, and emits normalized JSONL events for SKLMI or a future bootchain
supervisor.

## Supported wrapper families

The supervisor resolves families from `runtime/game-registry/archipelago-clients.json`.

- `sni`: SNES clients through SNI.
- `bizhawk`: generic BizHawk clients for NES, GB, GBC, GBA, and some N64.
- `oot`: Ocarina of Time N64 client.
- `dolphin`: Dolphin Memory Engine clients.
- `module`: game-specific upstream Archipelago client modules.
- `text`: AP websocket-only client, mainly for generator-only worlds.

There is also a `fixture` wrapper for smoke tests only. It is not used by the
game registry.

## Commands

List enabled clients:

```bash
python runtime/wrapper_supervisor.py list --status enabled
```

Resolve a launch plan:

```bash
python runtime/wrapper_supervisor.py plan --game-key alttp
python runtime/wrapper_supervisor.py plan --game-key ttyd
```

Dry-run a launch:

```bash
python runtime/wrapper_supervisor.py launch --game-key pokemon_emerald --dry-run
```

Launch and supervise:

```bash
python runtime/wrapper_supervisor.py launch \
  --game-key alttp \
  --connect archipelago.example:38281 \
  --slot PlayerName \
  --password optional-password \
  --session-id lobby-slot-runtime
```

The process accepts JSONL commands on stdin:

```json
{"cmd":"status"}
{"cmd":"say","text":"hello"}
{"cmd":"disconnect"}
{"cmd":"shutdown"}
```

Commands that are not handled by the supervisor are forwarded to the wrapped
Archipelago client.

## Output events

The supervisor writes JSONL to stdout and mirrors it to:

```text
runtime/logs/wrapper-supervisor/<session-id>.jsonl
```

Important event names:

- `launch_plan`: dry-run result.
- `launching`: resolved process invocation.
- `started`: child process started.
- `wrapper_event`: JSON event emitted by `apclient_common.py`.
- `wrapper_stdout`: non-JSON stdout line from upstream code.
- `wrapper_stderr`: stderr line from upstream code.
- `command_forwarded`: command sent to wrapped client.
- `status`: supervisor status response.
- `max_runtime_reached`: test/diagnostic timeout reached.
- `exited`: child process ended.

## Python resolution

The supervisor prefers the Windows portable Python when running on Windows:

```text
runtime/tools/python/portable-win/tools/python.exe
```

Otherwise it falls back to `sys.executable`.

The environment is prepared with:

- `SEKAILINK_AP_ROOT=runtime/ap`
- `SEKAILINK_AP_WRAPPER=1`
- `SKIP_REQUIREMENTS_UPDATE=1`
- `KIVY_NO_ARGS=1`
- `KIVY_NO_FILELOG=1`
- `PYTHONPATH=runtime:runtime/ap:<existing>`

This keeps upstream Archipelago clients headless and avoids user-side dependency
installation.

## Current validation

Smoke test:

```bash
python runtime/tests/wrapper_supervisor_smoke.py
```

The smoke test verifies:

- registry has a healthy enabled-client count;
- representative games resolve to expected wrapper families;
- module/dolphin clients receive `--module`;
- dry-run emits a launch plan;
- lifecycle supervision starts, forwards commands, reads wrapper events, and exits.

## Remaining integration work

This is now a working process supervisor, but not yet the complete bootchain.

Next integration steps:

- SKLMI or the runtime bootchain must call the supervisor for the selected slot.
- Client Core must pass the selected game/slot/server/password into the bootchain.
- Sekaiemu must expose the emulator-side connector expected by the chosen wrapper
  family: SNI, BizHawk Lua-compatible socket, Dolphin Memory Engine, or module-specific transport.
- PopTracker Edition should be launched next to the wrapper and associated with
  the same session id.
- Failure events must be surfaced in Client Core/Sekaiemu as user-readable
  launch errors.
