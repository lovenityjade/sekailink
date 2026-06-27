# SekaiLink PopTracker Runtime

Status: BETA-3 compatibility path
Date: 2026-06-16

## Decision

For BETA-3, SekaiLink integrates PopTracker as-is for tracker logic, Lua,
pack loading, layout behavior, autotracking, and variant handling.

SekaiLink-specific changes are intentionally limited to:

- UI chrome suitable for Sekaiemu/SekaiLink embedding.
- Silent runtime launch.
- Runtime status reporting.
- Lightweight health/control commands from Sekaiemu.

This runtime must not fork pack semantics or reimplement PopTracker logic.

## Source Location

The SekaiLink runtime fork lives at:

```text
third_party/upstream/poptracker-sekailink/
```

It is based on the local PopTracker reference source:

```text
/home/thelovenityjade/Projects/reference-repos/PopTracker
```

Upstream reference:

```text
https://github.com/black-sliver/PopTracker
```

## Runtime Launch

Sekaiemu should launch the SekaiLink runtime build with:

```text
sekailink-poptracker \
  --sekailink-runtime \
  --sekailink-status-file <path/status.json> \
  --sekailink-control-file <path/control.json> \
  --pack-variant <variant> \
  --ap-host <ws://host:port> \
  --ap-slot <slot> \
  --ap-pass-env SEKAILINK_AP_PASS \
  --ap-autoconnect \
  <path/to/pack-or-pack.zip>
```

The variant should be selected by Sekaiemu or Client Core before launch. The
runtime still lets PopTracker select and load the variant internally.

`--sekailink-runtime` also suppresses PopTracker's normal console path and
removes the top chrome controls that Sekaiemu/Client Core now own.

When AP arguments are provided, the runtime uses PopTracker's existing
Archipelago backend. It does not replace AP logic or patch pack Lua.

## Status Contract

The status file is JSON:

```json
{
  "schema": "sekailink.poptracker.status.v1",
  "runtime": "sekailink-poptracker",
  "state": "running",
  "detail": "",
  "frame": 1234,
  "healthy": true,
  "pid": 12345,
  "pack_loaded": true,
  "pack": {
    "path": "...",
    "uid": "...",
    "version": "...",
    "variant": "tracker_horizontal",
    "title": "...",
    "game": "..."
  }
}
```

Expected states:

- `starting`
- `window_ready`
- `waiting_for_pack`
- `tracker_loaded`
- `running`
- `ap_connect_requested`
- `ap_connect_failed`
- `ap_backend_unavailable`
- `reload_requested`
- `force_reload_requested`
- `shutdown_requested`
- `control_error`

Sekaiemu should treat the runtime as unhealthy if:

- the process exits unexpectedly;
- the status file does not update for the configured timeout;
- `healthy` is false;
- `state` is `control_error` repeatedly.

## Control Contract

Sekaiemu writes JSON to the control file. PopTracker polls the file mtime.

Supported commands:

```json
{ "command": "ping" }
{ "command": "health" }
{ "command": "reload" }
{ "command": "force_reload" }
{ "command": "quit" }
```

`ping` and `health` force an immediate status write with detail `pong`.

`reload` and `force_reload` call PopTracker's existing reload path. They do not
change pack logic.

`quit` requests a clean shutdown.

## Client Core IPC

Client Core exposes the runtime control surface through Electron:

```text
tracker:launch
tracker:stop
tracker:runtimeStatus
tracker:runtimeCommand
```

`tracker:launch` returns the runtime files when `sekailink-poptracker` is used:

```json
{
  "ok": true,
  "pid": 12345,
  "runtime": "sekailink-poptracker",
  "statusFile": "...",
  "controlFile": "..."
}
```

If only upstream `poptracker` is available, Client Core falls back to it without
passing SekaiLink runtime flags.

## Build Notes

The Makefile now allows:

```text
make native CONF=RELEASE EXE_NAME=sekailink-poptracker
```

Submodules must be initialized before building:

```text
git submodule update --init --recursive
```

Windows builds should be done from the Windows/MSYS2 build box.

## Non-Goals

This runtime does not:

- change PopTracker Lua behavior;
- change pack JSON loading;
- change map/item logic;
- replace AP/SNI autotracking internally;
- implement a native SekaiLink tracker;
- own Archipelago connection state.

Sekaiemu/SKLMI own the user-facing connection state and should decide what to
show in the surrounding SekaiLink UI.
