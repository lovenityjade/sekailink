# Tracker BETA-3 Runtime Boundary

`BETA-3` keeps `SKLMI` as a companion process launched by `Sekaiemu`.

Runtime ownership for tracker logic is split like this:

- `SKLMI` owns PopTracker-compatible runtime behavior:
  - pack loading
  - Archipelago state ingestion
  - Lua / pack logic
  - item progression
  - pin accessibility colors
  - snapshot publication
- `Sekaiemu` owns visual tracker presentation only:
  - bundle-backed assets and map rasters
  - snapshot file reads
  - throttled command-log writes
  - passive rendering in split, toggle, or separate window modes

`PIP` mode remains a legacy enum value for compatibility with older saved
settings, but it is not part of the active `BETA-3` presentation surface.
Old `PIP` values are treated as tracker-toggle behavior.

Active `BETA-3` file handoff:

- `tracker.snapshot.json`
  - written atomically by `SKLMI`
  - polled by `Sekaiemu` at low frequency
  - parsed only when `mtime` or `size` changes
  - last valid snapshot stays active if a newer file is corrupt
- `tracker.commands.jsonl`
  - append-only
  - written only on user tracker actions
  - intended for item clicks, pin clicks, tab changes, and similar commands

Current `Sekaiemu` behavior in this worktree:

- `--tracker-pack <path>` now activates the visual tracker when `SKLMI`
  provides the snapshot, without requiring the legacy `--tracker-bundle`
- `--tracker-snapshot <path>` enables snapshot-first tracker updates
- `--tracker-command-log <path>` enables command emission back to `SKLMI`
- `--tracker-bundle <path>` remains the visual fallback for assets and maps
- when snapshot mode is active, `Sekaiemu` stops consuming tracker room/trace state as the primary source of truth
- live tracker rendering resolves images through a host asset resolver; the
  renderer should not parse packs or run PopTracker logic to find assets
- the asset resolver accepts an explicit `--tracker-assets-root`, a directory
  `--tracker-pack`, and the `assets_root` published by the trusted local SKLMI
  snapshot; this keeps pack parsing out of the render path while preserving
  image lookup for extracted archives
- tracker polling/autosave is isolated in host-state helpers and remains
  throttled; slow or corrupt snapshots must not block emulation/audio

Non-goal for this phase:

- absorbing `SKLMI` into Client Core now

That consolidation is intentionally deferred until both `Sekaiemu` and `SKLMI`
are stable enough that the boundary is no longer doing active debugging work for
us.
