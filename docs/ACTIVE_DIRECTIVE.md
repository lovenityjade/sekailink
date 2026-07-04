# Active Directive

Date: 2026-06-25

Primary objective:

**Stabilize the runtime pipeline one core/system at a time.**

Primary runtime directive:

Games are validation fixtures, not the architectural target. We use specific
games to test each core, but the goal is generic stability:

- the emulator core exposes memory reliably;
- SKLMI can read and write that memory through the generic bridge;
- the Archipelago room server receives checks/status from the runtime;
- the runtime receives server items/messages and writes them back safely.

Do not attack individual games first. If a game fails, use it to identify which
core/system layer regressed. Fix the generic core bridge, launch contract,
memory domain mapping, or server communication path before considering a
game-specific exception.

SekaiLink is currently on hiatus. Do not treat any release as imminent. Do not
push builds, deploy updates, delete work, move directories, or restart large
architecture changes unless Jade explicitly asks for that specific action.

Current focus:

- Reduce cognitive load around the repository.
- Document what exists, what is experimental, and what is safe to ignore.
- Preserve work without moving or deleting it.
- Separate product decisions from emotional urgency.
- Keep the public site in hiatus mode.

Current runtime investigation, 2026-06-26:

- SNES/SNI is now validated and frozen for BETA-3. Confirmed fixtures include
  ALttP, DKC, DKC2, EarthBound, KDL3, Lufia II Ancient Cave, Mega Man X2,
  Secret of Mana, SMZ3, Super Mario World, and Super Metroid.
- Do not reopen generic SNES/SNI work unless a new reproducible regression log
  is attached. Future SNES failures should be treated as module-level adapter
  debt, tracker/UI work, ROM requirement cleanup, or game-specific APWorld
  behavior.
- Secret of Mana is bilateral-functional, but keeps a non-blocking graphics bug
  on the naming/save screen. Do not mark SoM incompatible for that visual issue.
- Current `core-test-snes` generated ROMs exposed an SNI/FXPak memory mapping
  bug: AP SNI clients use linear FXPak ROM/SRAM/WRAM addresses, while
  Sekaiemu's `System Bus` domain can expose CPU-bus semantics. The bridge must
  prefer ROM/WRAM/SRAM domains for those FXPak ranges and use System Bus only
  as a fallback. Smoke tests now connect for ALttP, DKC, and EarthBound using
  seed `03532246439531845580`.
- DKC write path was validated headless with `runtime/tools/snes_sni_smoke.py
  --write-probe`: SNI `PutAddress` reaches Sekaiemu, writes SRAM, and reads
  back the changed byte. If a received DKC item is not applied in-game, inspect
  the AP client receive guard/state (`game_state`, brightness, recv index,
  current map/level) before changing the generic bridge again.
- Live logs after the bridge fix show EarthBound sending checks and receiving
  items through WRAM writes, and Super Metroid sending Morph Ball plus receiving
  Energy Tank through SRAM writes. DKC connects and receives the AP
  `ReceivedItems` package, but no DKC memory write follows the item event. Treat
  DKC as a game-state/client-guard investigation, not proof that SNES/SNI is
  broken.
- DKC1 and DKC2 both had receive handling gated behind map/game-state checks.
  That can deadlock world unlock items because the item needed to open the map
  may be waiting behind the same guard. Their receive handlers now run after ROM
  / save validation but before the `game_state` early return. DKC3 is different:
  it is save-file gated and writes active save WRAM, so do not move its receive
  handling earlier unless a live DKC3 test proves the save gate is the blocker.
- DKC1/DKC2 early receive must stay conservative: before `game_state` is stable,
  only persistent `unlock_data` SRAM writes are allowed, and no SFX/WRAM/trap
  side effects should run. Currency, traps, and sound playback must remain
  queued until the normal runtime state is stable; otherwise entering a level
  can black-screen after the jingle.
- DKC-family checks must use the canonical Archipelago client helper
  `ctx.check_locations(...)`, not raw `LocationChecks` packets. This applies to
  DKC1, DKC2, and DKC3 so local trace output cannot falsely imply the room
  server accepted a check.
- DKC1/DKC2 persistent completion flags may be scanned while the player is on
  the overworld/map, but in-level sanity checks must remain gated behind a
  stable level state. DKC3 remains save-file gated.

Frozen validated runtime systems:

- NES is frozen as validated on 2026-06-25 after successful Client Core runtime
  and multiworld tests. Do not keep changing the NES core bridge unless a clear
  regression is reproduced. Use NES as a known-good reference while validating
  the remaining systems.
- SNES/SNI is frozen as validated on 2026-06-26 after successful bilateral
  runtime tests across normal SNI clients, Snes9x/Cx4-style modules, and the
  SoM web AP client path. Do not keep changing the generic SNES/SNI bridge
  unless a clear regression is reproduced.

Frozen until explicitly reopened:

- Client Core release work.
- Bootloader release work.
- Server/CDN update pushes.
- Native tracker replacement.
- New game compatibility expansion that is not tied to core/system validation.

Product/runtime decisions:

- Ocarina of Time must not be implemented twice. Do not treat the legacy N64
  OOT APWorld/runtime path as a normal SekaiLink target. The intended SekaiLink
  path is Ship of Harkinian only; if a player wants the original-feeling OOT
  experience, expose that later through SoH settings instead of adding a second
  native OOT runtime lane.

Allowed cleanup:

- Add documentation that clarifies current state.
- Add non-destructive inventories.
- Run read-only checks and smoke tests.
- Propose cleanup steps before executing them.

Rule:

If a task feels like it could reshape the product, pause and write the decision
down first. SekaiLink should be resumed as a small, understandable system, not
as an emergency.
