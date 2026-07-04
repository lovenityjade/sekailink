# SekaiLink Runtime Lab TUI

Purpose: test the runtime stack without going through Client Core, Nexus, live generators, or the public servers.

Local command:

```bash
skl-runtime-lab
```

Raw one-shot command:

```bash
skl-runtime-lab --remote runtime-doctor
```

## Windows Workbench

- SSH host: `sekailink-windows`
- Windows repo: `D:/SekaiLink/repos/sekailink-canonical`
- Runtime lab state: `D:/RuntimeLab`
- Worker: `tools/windows-worker/sekai-worker-msys.sh`

The Linux TUI calls the Windows worker through SSH, then the worker executes the runtime helper inside MSYS2.

Current lab split:

- Windows is the host/server/generator machine.
- Linux is the test/runtime machine.
- `launch` / `plan` still prepare the old Windows-side runtime chain.
- `launch-local` / `plan-local` are the preferred commands for runtime testing: they start or locate the Windows AP room server, translate its `127.0.0.1:<port>` endpoint to the Windows LAN address, then build or launch the Linux bootchain locally.

The local TUI config lives at:

```text
~/.config/sekailink-runtime-lab/config.json
```

For this private lab machine, the Windows SSH password is stored there so each command can run unattended.

## Useful Commands

Inside `skl-runtime-lab`:

```text
doctor
status
list nes
list gb_gbc
generate nes
generate-game super_mario_land_2
generate-game metroid_zero_mission
generate-game metroid_fusion
generate-game a_link_to_the_past
rom-scan
rom-import
roms
tracker-scan
tracker-import
trackers
seeds all
server start nes
server game super_mario_land_2
server game metroid_zero_mission
server stop nes
server stop gb_gbc
server stop gba
launch mega_man_2 --family nes
launch super_mario_land_2 --family gb_gbc
launch super_mario_land_2 --family gb_gbc --auto-server
plan-local super_mario_land_2
launch-local super_mario_land_2 --run --with-emu --with-tracker
logs --limit 3 --tail 2000
mark super_mario_land_2 stable "GB bridge confirmed"
```

`plan-local <game_key>` is the safest first step. It starts the matching Windows room server when needed, but only prints the Linux bootchain plan.

`launch-local <game_key> --run --with-emu --with-tracker` starts the Linux wrapper, Sekaiemu, and PopTracker against the Windows-hosted AP server.

`launch` without `--run` only prints the Windows-side bootchain plan. Use it only when intentionally testing from Windows.

If a ROM was imported with `rom-import`, `launch` automatically passes it to the bootchain. No manual ROM import through Client Core is needed for lab tests.

If a PopTracker pack was imported with `tracker-import`, `launch` automatically passes it to PopTracker with `--pack`, plus the AP host and slot.

For local Linux testing, `plan-local` and `launch-local` use the local ROM and tracker scans directly. The Windows copies remain useful for the host/generator setup, but Linux does not need to launch the Windows runtime.

## Prepared Seeds

As of 2026-06-22:

- NES family seed: generated successfully.
- Isolated `super_mario_land_2`: generated successfully.
- Isolated `metroid_zero_mission`: generated successfully.
- Isolated `metroid_fusion`: generated successfully.
- Isolated `a_link_to_the_past`: generated successfully after setting the ALTTP base ROM on the Windows lab.
- ROM registry: 32 ROMs imported into `D:/RuntimeLab/roms/by-game` and registered in `D:/RuntimeLab/status/roms.json`.
- Tracker registry: 19 PopTracker packs imported into `D:/RuntimeLab/trackers/by-game` and registered in `D:/RuntimeLab/status/trackers.json`.

## Known Notes

- GB/GBC family generation currently fails because `Wario Land` calls deprecated `Utils.get_options()` during generation. Use isolated game generation to avoid one broken world blocking the family.
- The local AP server warns that `_speedups` is unavailable and falls back to pure Python `LocationStore`. This is not fatal, but it may affect performance.
- The Windows Python portable now includes `requests`, `urllib3`, `charset_normalizer`, `idna`, and `certifi`, because some APWorld imports need them during MultiServer startup.
- No Runtime Lab MultiServer should be left running after tests. Check with `status` and stop with `server stop <family>`.
- `rom-import` intentionally skips `.rvz` and only imports explicit filename matches to avoid false positives.
- `tracker-import` only imports explicit filename matches. Trackerless games remain valid runtime targets; they simply launch without PopTracker until a pack exists.
