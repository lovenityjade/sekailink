# Source Of Truth

Date: 2026-06-01

This file exists because BETA-3 had two active Sekaiemu trees and the runtime started mixing
old and new tracker behavior. From this point forward, use:

- Canonical repo: `<local-home>/SekaiLink/canonical`
- Sekaiemu source: `apps/sekaiemu`
- Client Core source: `apps/client-core`
- SKLMI source: `services/sklmi`
- ALTTP showcase LinkedWorld source: `linkedworlds/alttp`

## Imported Sources

- `apps/sekaiemu` came from the project tree that contained the validated tracker layout:
  `<local-home>/Projects/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike`
- `apps/client-core`, `runtime`, services, and LinkedWorld assets came from:
  `<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos`

## Backup

Full backups were written before consolidation:

`<local-home>/mnt/gdrive/Backups/SekaiLink/sekailink-pre-canonical-20260601-135453`

The backup contains `SHA256SUMS.txt`, and the checksum verification passed before this
canonical tree was created.

## Tracker Pack Rule

The complete PopTracker-compatible pack source is the directory bundle:

`linkedworlds/alttp/tracker/default.bundle`

Zip artifacts are generated outputs. If a zip/bundle in `runtime/tracker-bundles` disagrees
with the directory bundle, rebuild it from `linkedworlds/alttp/tracker/default.bundle`.

The Evolution publish artifact was regenerated from that directory bundle during the canonical
import, and its manifest now points at a zip containing the PopTracker Lua logic files.
