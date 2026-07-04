# Core/System Validation Directive

Date: 2026-06-25

This document is the priority reference for Sekaiemu/SKLMI runtime debugging.

## Directive

We do not stabilize games one by one as isolated targets. We stabilize cores and
systems one by one, using games as fixtures.

A game test is successful only when the full generic pipeline works:

1. The selected emulator core exposes memory through Sekaiemu.
2. SKLMI reads from and writes to that memory through the generic bridge.
3. The Archipelago client/server receives checks, status, and room events.
4. Server-sent items/messages are received by the runtime.
5. The runtime applies received data back into emulated memory safely.

## Debugging Rule

When a game fails, classify the failure at the lowest generic layer first:

- core memory exposure;
- memory domain naming or address translation;
- SKLMI bridge startup;
- SKLMI read/write contract;
- Archipelago wrapper connection;
- server protocol/version compatibility;
- item/check queueing;
- game-specific APWorld behavior.

Only add a game-specific fix after the generic layer has been proven correct and
the APWorld itself requires unique handling.

## Current GBA Fixture

Metroid: Zero Mission is a regression fixture for the GBA/mGBA bridge because it
was previously used to validate Sekaiemu memory exposure. If MZM fails after it
previously passed, treat that as a core/runtime regression until proven
otherwise.

The intended GBA pipeline is:

```text
mGBA libretro core
  -> Sekaiemu memory socket/BizHawk protocol endpoint
  -> generic BizHawk protocol bridge
  -> Archipelago wrapper/client
  -> room server
```

Current BETA-3 note: GBA manifests are not on the embedded SKLMI companion path
yet. They use the generic Archipelago/BizHawk wrapper path above. If GBA fails,
inspect the memory socket and wrapper logs first; do not assume the SKLMI
companion process is involved unless the manifest is explicitly switched to
`client_mode: "sklmi"`.

The target is not "fix MZM"; the target is "GBA/mGBA exposes memory and supports
generic SKLMI/AP read-write flow again."

Current 2026-06-26 GBA note:

- Metroid: Zero Mission and Metroid Fusion are the only GBA fixtures that have
  recently shown plausible runtime behavior, but both still need clean retests
  before freezing the GBA lane.
- Final Fantasy Tactics Advance is temporarily unavailable because it connects
  but item/check transactions are hard to validate manually; it needs a
  dedicated tester familiar with mission rewards.
- Pokemon FireRed/LeafGreen must use the generic Archipelago BizHawk wrapper
  path. Do not attach the old `firered-starter.profile` legacy bridge to normal
  Client Core launches; that path can fail before runtime readiness and is not
  the current GBA compatibility target.
- The Archipelago wrapper may pass an explicit scoped `--client-module` for GBA
  BizHawk clients only. This is a GBA stabilization guardrail so custom APWorld
  client handlers load deterministically without changing frozen NES/SNES/GB
  behavior.
- Pokemon FireRed/LeafGreen server/client compatibility was corrected on
  `worlds-vps` on 2026-06-26 by updating only
  `/opt/sekailink-generate/worlds/pokemon_frlg` from apworld `1.0.2` to `1.0.4`.
  Existing rooms generated before that update remain incompatible and must be
  regenerated; do not diagnose those old rooms as runtime connection failures.

## Frozen Validated Systems

### SNES

Status: frozen validated on 2026-06-26.

Result: the SNES/SNI runtime lane is established for BETA-3. The validated
pattern is:

```text
SNES libretro core
  -> Sekaiemu memory socket
  -> local SNI websocket bridge
  -> upstream Archipelago SNI/web client
  -> room server
```

Confirmed fixtures:

- A Link to the Past
- Donkey Kong Country
- Donkey Kong Country 2
- EarthBound
- Kirby's Dream Land 3
- Lufia II Ancient Cave
- Mega Man X2
- Secret of Mana
- SMZ3
- Super Mario World
- Super Metroid

Operational rule: do not continue changing the generic SNES/SNI bridge during
BETA-3 stabilization unless a clear regression is reproduced with logs. Future
SNES work should be treated as module-level adapter debt, tracker/UI work, ROM
requirement cleanup, or game-specific APWorld behavior.

## SNES Core Notes

Mega Man X2 confirmed the MMX/Cx4-style Snes9x lane as bilateral functional.
Mega Man X3 remains an optional deferred validation target: on 2026-06-26,
bsnes-mercury loaded `cx4.data.rom` and exited cleanly, but produced a fully
black framebuffer at frame 600 for both vanilla USA MMX3 and the AP-patched
MMX3 ROM. Snes9x produced non-black framebuffer output for both ROMs with the
same Sekaiemu host.

Secret of Mana is confirmed bilateral through the Evermizer browser AP client
path. It keeps a non-blocking graphics debt on the naming/save screen: the
screen can appear black, but gameplay proceeds after Start input and the
AP/runtime lane remains valid. Secret of Evermore should use the same web-client
family but still needs its own validation.

Do not globally replace the SNES core because several SNES fixtures are already
frozen on the existing path. Treat this as a module-level core selection until a
lower-level bsnes-mercury/Cx4 host issue is fixed.

### NES

Status: frozen validated on 2026-06-25.

Result: NES runtime tests are considered conclusive, including multiworld item
flow. Do not continue changing the NES core bridge during BETA-3 stabilization
unless a new regression is reproduced with logs.

Validated fixtures:

- The Legend of Zelda
- Mega Man 2
- Mega Man 3
- Zelda II: The Adventure of Link

Operational rule: use NES as the known-good generic BizHawk-style memory bridge
reference while continuing validation for GB/GBC, GBA, N64, and GameCube/Wii.
