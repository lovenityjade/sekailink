# SekaiLink Runtime Lab

The Runtime Lab is a local compatibility harness.  Linux runs the TUI, the
Windows PC does the heavy work through MSYS2, and `D:/RuntimeLab` stores all lab
state.

`D:/SekaiLink` remains reserved for Windows client/build work.

## Linux TUI

```bash
skl-runtime-lab
```

Useful commands:

```text
init
doctor
list all
yamls gba
generate gba
plan metroid_zero_mission
mark metroid_zero_mission stable checks-items-ok
logs
```

## Windows worker commands

The TUI calls:

```bash
./tools/windows-worker/sekai-worker-msys.sh lab runtime-doctor
./tools/windows-worker/sekai-worker-msys.sh lab runtime-generate gba
```

The lab helper creates:

```text
D:/RuntimeLab/apworlds
D:/RuntimeLab/yamls
D:/RuntimeLab/seeds
D:/RuntimeLab/roms
D:/RuntimeLab/patches
D:/RuntimeLab/trackers
D:/RuntimeLab/logs
D:/RuntimeLab/status
D:/RuntimeLab/reports
D:/RuntimeLab/tmp
```

## First-pass strategy

Generate and test by family first:

- `nes`
- `snes`
- `gb_gbc`
- `gba`
- `n64`
- `gamecube`

The lab intentionally avoids the live Nexus/Link/Worlds services and avoids the
Client Core until the runtime is stable.
