# SKLMI Bridge Manifests

This directory contains BETA-3 bridge fixtures consumed by the native SKLMI
runtime when Sekaiemu starts it as an embedded companion.

These files are not the final canonical game database. They are narrow runtime
contracts used to prove the heartbeat:

`Core -> Sekaiemu -> SKLMI -> Archipelago room`

For BETA-3, the manifest tells SKLMI which emulator memory ranges to watch,
which Archipelago locations to report, and which room-delivered items can be
written back into memory. Full game coverage should be added deliberately, with
tests, without moving protocol ownership into Sekaiemu or Core.

## ALTTP Runtime Manifest

`alttp.phase1.json` is synchronized from the ALTTP LinkedWorld contract:

`../sekailink-linkedworld-alttp/bridge/sklmi.phase1.json`

Run `scripts/sync_alttp_runtime_manifest.sh` from the SKLMI repo after updating
the LinkedWorld ALTTP bridge contract. The script preserves the Sekaiemu runtime
bridge identity (`alttp-phase1`) while importing the broader ALTTP check/action
surface from the LinkedWorld repo.

## EarthBound Runtime Manifest

`earthbound.phase1.json` is the narrow SNES heartbeat manifest synchronized from:

`../sekailink-linkedworld-earthbound/bridge/sklmi.phase1.json`

It intentionally covers only two early Onett checks and one room-controlled
item write (`Toothbrush`). Its purpose is to prove the same
`Sekaiemu -> SKLMI -> Archipelago` path on a second SNES game before expanding
game coverage.
