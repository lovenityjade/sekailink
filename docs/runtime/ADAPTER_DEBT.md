# Runtime Adapter Debt

Date: 2026-06-26

This file tracks runtime-adapter work that must not be forgotten while
SekaiLink wraps upstream Archipelago clients.

## DKC Barrel Count UI

Status: deferred, not removed.

The Donkey Kong Country clients expose a barrel count label through the
upstream Archipelago Kivy UI (`kvui`). SekaiLink runs these clients headless, so
loading `kvui` from the wrapper path can crash or disconnect the runtime when
Kivy is not installed.

Current adapter behavior:

- DKC1 and DKC2 skip the Kivy label when `ctx.ui` is unavailable.
- This prevents headless Sekaiemu/SNI sessions from crashing.
- The gameplay value is still read/written by the client; only the old
  Archipelago desktop UI label is skipped.

Follow-up:

- Re-expose the barrel count through SekaiLink-owned runtime state instead of
  Kivy.
- Preferred display targets:
  - Sekaiemu bridge terminal readable tab;
  - Sekaiemu debug window;
  - tracker/overlay state, if useful.
- Suggested event shape:

```json
{
  "type": "runtime_state",
  "source": "archipelago_client",
  "game": "Donkey Kong Country",
  "barrel_count": 3
}
```

Rule:

Do not reintroduce direct `kvui` dependency in SekaiLink headless paths. If a
wrapped client needs a UI-only value, emit it as runtime/debug state and let
Sekaiemu/SKLMI decide how to display it.

## Secret of Mana / Secret of Evermore Web AP Client

Status: upstream browser client wrapped. Secret of Mana is confirmed
bilateral-functional in SekaiLink runtime tests.

`som.apworld` provides world, options, generation, and patching, but it does not
ship an `AutoSNIClient` handler like the confirmed SNES worlds do. Secret of
Evermore ships its client separately as a browser/WASM application. The SoM
documentation points users at the same browser-client flow.

Current adapter behavior:

- The upstream `black-sliver/ap-soeclient` repository is staged in
  `third_party/upstream/ap-soeclient`.
- SekaiLink has a dedicated `web_ap_client` runtime family instead of routing
  SoM/SoE through the generic Python SNI wrapper.
- Autolaunch starts Sekaiemu, starts the local SNI bridge, then opens the
  Evermizer AP client in a hidden Electron `BrowserWindow` with
  `#server=<room>`.
- Browser client output is mirrored into runtime logs and Sekaiemu system chat
  for diagnostics.

Boundary rule:

Do not fold SoM/SoE behavior into the generic SNI wrapper unless we replace the
browser client with a real native adapter. The browser client remains the owner
of game-specific memory semantics for this runtime path.

Follow-up:

- Build or vendor a local browser-client bundle so release builds do not depend
  on the hosted Evermizer page.
- Add explicit health/status probes for SNES, Game, and AP connection colors.
- Fix Secret of Mana's non-blocking graphics bug on the naming/save screen.
- Verify item receive/send behavior for SoE in a fresh room.

## Shared Runtime Resources / EnergyLink

Status: technique identified, SekaiLink surface still needed.

Mega Man 2 and Mega Man 3 both expose EnergyLink as an accumulator that the
player can use intentionally. This is not just a passive check/item bridge:
the upstream clients register commands such as `pool`, `request`, and
`autoheal`, then translate those commands into AP data-storage updates and RAM
writes.

Current upstream behavior:

- EnergyLink is enabled from the patched ROM metadata.
- The shared pool lives in AP storage under `EnergyLink{team}`.
- Enemy drops can deposit into the shared pool through AP `Set` operations.
- `/pool` reports requestable health, weapon energy, and lives.
- `/request <amount> <type>` queues a local refill, subtracts from the shared
  pool, then writes health/weapon/life values into emulator RAM.
- `/autoheal` automatically pulls from the pool when health is missing.
- MM2 and MM3 still call `ctx.ui.enable_energy_link()` when `ctx.ui` exists,
  but this is guarded and is not required for headless operation.

SekaiLink adapter rule:

Do not move EnergyLink logic out of the upstream client unless we are replacing
the whole client contract. The upstream client remains the source of truth for
which RAM addresses are safe to write and which exchange rates are valid.

SekaiLink should adapt it at the boundary:

1. Observe AP storage updates for `EnergyLink{team}` and local queued refill
   state.
2. Emit normalized runtime/debug state for Sekaiemu and the bridge terminal.
3. Route user commands from Sekaiemu's debug command bar to the existing
   client command processor (`pool`, `request`, `autoheal`) instead of
   reimplementing the exchange math in C++.
4. Show both readable labels and raw values, including AP storage key, pool
   integer, exchange rates, requested type, target RAM address, and write
   result.
5. Keep release builds read-only unless the command surface is explicitly
   enabled for debug/admin use.

Suggested event shape:

```json
{
  "type": "runtime_state",
  "source": "archipelago_client",
  "game": "Mega Man 2",
  "energy_link": {
    "enabled": true,
    "storage_key": "EnergyLink1",
    "pool": 1500000000,
    "requestable": {
      "health": 3,
      "weapon": 6,
      "lives": 0
    },
    "auto_heal": false,
    "queued_refill": null
  }
}
```

Suggested Sekaiemu debug commands:

- `energy pool` -> invoke/report upstream `pool`
- `energy request <amount> <type>` -> invoke upstream `request`
- `energy autoheal` -> invoke upstream `autoheal`

Mega Man target codes to surface in autocomplete:

- MM2: `HP`, `AF`, `AS`, `LS`, `BL`, `QB`, `TS`, `MB`, `CB`, `I1`, `I2`, `I3`,
  `1U`
- MM3: `HP`, `NE`, `MA`, `GE`, `HA`, `TO`, `SN`, `SP`, `SH`, `RC`, `RM`, `RJ`,
  `1U`

Follow-up:

- Add an adapter-level event whenever the client sends an AP `Set` operation
  against `EnergyLink{team}`.
- Add an adapter-level event whenever a queued refill results in a RAM write.
- Add command autocomplete in Sekaiemu's debug window for the EnergyLink
  commands above.
