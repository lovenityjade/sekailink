# SekaiLink Client Plan (Electron + React)

This document defines the step-by-step plan for building a single client that automates
emulators, mods, patching, and connection flow across all games.

## 0) Goals & Constraints
- One client (Electron + React) that automates everything end-to-end.
- User supplies ROM/ISO/BIOS/keys as required.
- We vendor emulator/mod sources in `third_party/` and build them as part of our release.
- Webhost remains the source of truth for lobbies, rooms, YAMLs, and game metadata.

## 1) Define the Game Runtime Contract (Schema)
Create a single normalized schema per game that describes how the client should:
- validate assets
- patch or install mods
- launch the emulator/game
- connect the client/AP/SNI

Minimum schema fields:
- `game_id`, `display_name`
- `platform` (snes, gba, gc, ps2, pc, etc.)
- `emulator_family` (bizhawk, retroarch, dolphin, pcsx2, duckstation, ppsspp, cemu, ryujinx/eden, etc.)
- `core_id` (only for libretro/RetroArch)
- `patch_mode` (ap_patch, external_patcher, mod_loader, none)
- `required_assets` (rom/iso/keys/firmware/rel/seed/etc.)
- `launch_steps` (ordered actions)
- `connect_steps` (AP/SNI/CLI commands)
- `notes` (manual fallbacks)

## 2) Build the Driver Layer (Family-based)
Create a driver per emulator family:
- `BizHawkDriver`
- `RetroArchDriver`
- `DolphinDriver`
- `PCSX2Driver`
- `DuckStationDriver`
- `PPSSPPDriver`
- `CemuDriver`
- `RyujinxDriver` (fallback to Eden)
- `AzaharDriver`
- `mGBA/Snes9x/Mesen/bsnes` (per-game selection)
- `ScummVM/DOSBox/OpenGOAL/OpenRCT2/gzDoom` (specialized)

Each driver implements:
- `prepare(ctx)` → write config, install mods/patches
- `launch(ctx)` → start emulator/game with proper CLI
- `connect(ctx)` → inject server/slot/password (AP/SNI/mod config)
- `verify(ctx)` → validate assets and dependencies

## 3) Add Core-based Support (Hybrid Model)
Use a core-first model only for RetroArch/libretro.
- `CoreDriver` handles libretro cores.
- `core_id` in schema maps to specific core.
- Config layered: global RetroArch config → core config → game overrides.

Standalone emulators stay driver-first.

## 4) Central Config Manager (Per Emulator + Per Game)
Create a config system that:
- generates emulator configs from templates
- supports user overrides
- writes to the emulator’s expected file layout

Suggested structure:
```
profiles/
  default/
    emulators/
      bizhawk/EmuHawk.cfg
      dolphin/Dolphin.ini
      retroarch/retroarch.cfg
    cores/
      snes9x.cfg
    mods/
      bepinex/config/
      smapi/config.json
    games/
      twilight_princess/overrides.json
```

## 5) Assets Manager
Central service to track user-provided assets and build outputs:
- ROM/ISO/BIOS/keys/firmware
- patch files
- mod files and profiles
- emulator binaries

Functions:
- `importAsset(game_id, type, path)`
- `validateAsset(game_id, type)`
- `resolvePaths(game_id)`

## 6) Patch Pipeline (Unified)
Implement a standardized patch pipeline:
- AP patch files (`.ap*` extensions)
- External patchers (Twilight Princess, etc.)
- Mod loader installs (BepInEx/SMAPI/Fabric/Forge)

Output:
- A per-game runnable output folder
- A ready-to-launch config + assets set

## 7) Auto-Connect Layer
Single service to inject:
- `server:port`
- `slot`
- `password`

Targets:
- AP clients
- PopTracker (CLI with AP args, plus SNI later)
- SNI connections where required
- Mod configs (Unity/BepInEx/etc.)

## 8) Game Registry (Auto-generated)
Generate a registry by parsing the guides and metadata:
- `WebHostLib/static/generated/docs`
- YAML metadata
- manual overrides for exceptions

Registry should map each game to:
- driver/core
- patch mode
- required assets
- connection method

### 8.1 Registry Validation (Schema)
Add a validation step that checks the generated registry against
`client/schema/game_runtime.schema.json`.
- Run validation in CI and during local dev.
- Fail fast if any entry is missing required fields.

### 8.2 Driver Mapping Pass (Normalization)
Create a mapping pass that converts raw guide output into the final contract:
- `hints.emulators` → `driver.family`
- RetroArch-only games → set `driver.core_id`
- Guide keywords → `patch_mode`
- Inject `required_assets` (ROM/ISO/BIOS/keys/etc.)
- Manual overrides for edge cases (external patchers, special clients)

## 9) UI Integration (Electron + React)
Reuse existing web UI patterns for:
- Lobby list + join
- Room info + server port
- YAML selection
- “Play” flow that runs the driver pipeline

UI is a front-end for the driver pipeline and config manager.

## 10) Build & Packaging
Build per OS:
- Bundle emulator binaries
- Bundle mod loader assets
- Include PopTracker + packs

Pipeline:
- `scripts/fetch_third_party.sh`
- Build emulators
- Package Electron app

## 11) Phased Implementation (All Families)
1) BizHawk + RetroArch + PopTracker
2) Dolphin + PCSX2 + DuckStation
3) Mod loaders (BepInEx/SMAPI/Fabric/Forge)
4) External patchers (Twilight Princess)
5) Remaining special tools (ScummVM, DOSBox, OpenGOAL, etc.)

## 12) Twilight Princess External Patcher (Required)
Twilight Princess requires external assets (REL loader + seed) and save injection.
We vendor:
- `third_party/tools/TP_SaveFileHacker`
- `third_party/tools/TP_Randomizer`
- `third_party/tools/TP_Randomizer_Web_Generator`
- `third_party/tools/TP_GeckoPatcher`

Automation flow is documented in `CLIENT_EXTERNAL_PATCHERS.md`.

---
This plan is the source of truth for client integration work. Update it whenever
we add new drivers, patchers, or support flows.
