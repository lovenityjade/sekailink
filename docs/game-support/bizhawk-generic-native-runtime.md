# Generic BizHawk Contract In Sekaiemu/SKLMI

Status: native pre-realtime-test integration
Date: 2026-06-06

## Rule

SekaiLink should not introduce a new Lua communication dependency for runtime
memory integration. The Archipelago `connector_bizhawk_generic.lua` protocol is
used as a compatibility contract to absorb into native Sekaiemu/SKLMI behavior.

Lua remains allowed for PopTracker and explicit exceptions. This document is not
an authorization to route game runtime sync through Lua.

## Native Coverage

Sekaiemu's runtime memory server now accepts ordered JSON request lists matching
the generic BizHawk connector shape:

- `VERSION`
- `PING`
- `SYSTEM`
- `PREFERRED_CORES`
- `HASH`
- `MEMORY_SIZE`
- `DOMAINS`
- `GUARD`
- `LOCK`
- `UNLOCK`
- `READ`
- `WRITE`
- `DISPLAY_MESSAGE`
- `SET_MESSAGE_INTERVAL`

`LOCK` / `UNLOCK` are native runtime state now. While locked, the runtime memory
server continues polling, but the libretro core does not advance frames.

## Domain Compatibility

Sekaiemu now normalizes common BizHawk memory-domain names in the native memory
resolver:

- `System Bus`, `system_bus`, `bus`
- `RAM`, `WRAM`, `system_ram`
- `Battery RAM`, `SRAM`, `save_ram`
- `VRAM`, `video_ram`
- `gba_system_bus`

Virtual bus domains are exposed when libretro memory maps are available. This
allows APWorld-style absolute addresses, such as GBA `0x02000000` system-bus
addresses, to resolve through native libretro descriptors.

## Scope Boundary

This work does not revive the old Electron launcher flow that directly
orchestrated BizHawk + Lua + AP + PopTracker as the main architecture. Electron
may still contain active client code, but that historical launcher path is not
the target runtime design for Beta 3.

The intended path is:

```text
APWorld evidence -> generated/native runtime adapter -> Sekaiemu memory server
-> SKLMI provider/session -> Room server
```

## Validation

Automated checks run locally:

```text
runtime_memory_server_bizhawk_protocol_smoke
sekailink_sklmi_runtime_socket_smoke
sekailink_sklmi_runtime_runner_smoke
sekailink_sklmi_runtime_game_server_smoke
sekailink_sklmi_alttp_golden
sekailink_sklmi_earthbound_golden
sekaiemu_libretro_spike build
```

The new Sekaiemu smoke validates the native protocol without real Lua, without
SNI, and without a ROM by using a fake libretro memory map.

## Remaining Realtime-Test Requirements

Before claiming a game is supported, run one live game through the full path:

1. Launch a BizHawk-generic candidate through Sekaiemu with the relevant core.
2. Confirm native `DOMAINS` output includes the domains expected by the APWorld.
3. Confirm `GUARD`, `READ`, and `WRITE` work on real core memory.
4. Confirm SKLMI can report one check to the room server.
5. Confirm SKLMI can receive and inject one item from the room server.
6. Confirm ALTTP/SNI behavior is unchanged in a live smoke.

Recommended first candidates remain `mega_man_3` or `super_mario_land_2`.
