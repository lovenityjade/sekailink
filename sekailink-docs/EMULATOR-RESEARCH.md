# EMULATOR-RESEARCH.md

## Scope
Goal: identify emulator CLI support from **local repo evidence** (docs shipped in `third_party/emulators/`).
This is not an upstream audit; missing CLI here does **not** mean the emulator lacks CLI.

## Notes
- `GAMES-SUPPORTED.md` does not exist in this repo.
  - Supported list: `sekailink-docs/SUPPORTED-GAMES.md`
  - Per‑game setup: `sekailink-docs/GAMES-SETUP.md`
- PopTracker CLI integration is already documented in `sekailink-docs/POPTRACKER.md`.

## CLI Docs Found (local repo evidence)
- BizHawk
  - `third_party/emulators/BizHawk/README.md`
    - Section: “Passing command-line arguments”
- Eden
  - `third_party/emulators/Eden/docs/user/CommandLine.md`
  - `third_party/emulators/Eden/docs/Options.md`
- MAME
  - `third_party/emulators/MAME/docs/source/commandline/commandline-index.rst`
  - `third_party/emulators/MAME/docs/source/commandline/commandline-all.rst`
- ScummVM
  - `third_party/emulators/ScummVM/doc/docportal/advanced_topics/command_line.rst`

## Emulators With No CLI Docs Found (local repo)
These may still support CLI; it just isn’t documented in the vendored tree.
- Azahar
- BizHawk-2.10-linux-x64 (binary bundle docs)
- bsnes
- Cemu
- DeSmuME
- Dolphin
- DOSBox-Staging
- DuckStation
- gzdoom
- melonDS
- Mesen
- mGBA
- OpenEmu
- OpenGOAL
- OpenRCT2
- PCSX2
- PPSSPP
- RetroArch
- Ryujinx
- Snes9x
- VisualBoyAdvance-M

## MVP-Relevant Emulators (from SUPPORTED-GAMES.md)
Focus for MVP automation:
- BizHawk (CLI docs found locally)
- Dolphin (CLI docs not found locally)
- snes9x-rr (Snes9x tree present; CLI docs not found locally)
- Project64 (not present in `third_party/emulators/`)

## Follow‑Up (if needed)
- Run binaries with `--help` where available to discover flags.
- Check upstream docs for: Dolphin, snes9x‑rr, Project64.
- Decide which CLI flags are sufficient for “setup wizard” automation (ROM load, full screen, lua auto‑load, config path).

## Hard Check (local binaries)
- Executables found:
  - `third_party/emulators/BizHawk-2.10-linux-x64/EmuHawkMono.sh`
- Attempted:
  - `EmuHawkMono.sh --help` failed because `mono` is not installed in this environment.
  - Script produced `EmuHawkMono_last*.txt` log in the BizHawk folder.
- No runnable binaries were found for Dolphin, Snes9x, RetroArch, or Project64 in the repo.
- Next step (if approved): install Mono or build emulators to run `--help` checks.

### Update after installing Mono
- Installed `mono-complete` so BizHawk can run.
- `EmuHawkMono.sh --help` still fails due to missing X display:
  - Error: `Could not open display (X-Server required. Check your DISPLAY environment variable)`
- This environment has no X server; need X/Wayland session or headless support to get `--help`.

## BizHawk CLI Output (provided)
Provided CLI output for `EmuHawkMono.sh --help` (summary of useful flags):
- `--chromeless` (no GUI chrome)
- `--fullscreen`
- `--config <config>`
- `--lua <lua>` (implies `--luaconsole`)
- `--mmf <mmf>` (memory‑mapped file IPC for Lua `comm.mmf*`)
- `--socket-ip/--socket-port` (+ `--socket-udp`) for Lua `comm.socket*` IPC
- `--url-get/--url-post` for Lua `comm.http*` IPC
- `--open-ext-tool-dll <dll>` (auto-open an External Tool)
- `--userdata <k1:v1;...>` (key/value payload)

## Dolphin Build Status (local)
- CMake requires additional dependencies (installed: cmake, libXi-devel, libXrandr-devel, libudev-devel, libevdev-devel).
- Build failed because Dolphin submodules are missing and **cannot be fetched** (no network access in this environment).
- Submodule error: `Externals/Qt` gitlink points to commit `495517af...` which could not be fetched.
