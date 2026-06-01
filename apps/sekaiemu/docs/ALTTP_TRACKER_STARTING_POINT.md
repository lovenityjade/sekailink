# ALTTP Tracker Starting Point

This is the current Beta-3 tracker pilot. Keep the scope on ALTTP until this is
usable for a real community test.

## What Works

- Sekaiemu launches ALTTP with the LinkedWorld `default.bundle`.
- The bundle loader now follows the bundle refs for PopTracker-adapted assets,
  tracker flow, item metadata, map pins, settings, and autotabbing hints.
- The visible maps now come from `poptracker-adapted/` where the pack provides
  them, instead of the old placeholder fixture maps.
- Tracker polling/rendering is throttled separately from emulation frames to
  avoid audio/video contention.
- The tracker can render in split-screen, picture-in-picture, toggle-screen, or a
  separate window.
- SKLMI `location_checked` trace events update the tracker live feed.
- SKLMI received-item events now get display names from the preceding
  `room_item_pending item_name=...` trace, so the panel can show `Hookshot`
  instead of only `10`.
- The tracker reads room/session metadata from `room.state` when SKLMI provides
  it, with trace fallback for sparse live runs.

## Run

Use the human-driven ALTTP session:

```bash
tests/e2e/alttp_chest/run_alttp_manual_check_session.sh
```

Use the reproducible heartbeat:

```bash
tests/e2e/alttp_chest/run_alttp_chest_e2e.sh
```

The heartbeat uses `/tmp/sekaiemu-links-house-state` when that calibrated
Link's House savestate is available. If it is missing, the script falls back to
creating a savestate from boot using the current input script, which may require
recalibration when boot timing changes.

Important hotkeys:

- `F8` cycles tracker display mode.
- `F9` toggles tracker visibility / tracker screen.
- `F10` cycles tracker tabs.
- `F11` toggles automatic map follow.

## Headless Preview

When no graphical session is available, render tracker previews without
launching Sekaiemu:

```bash
tools/render_alttp_tracker_preview.sh
```

The script builds `tracker_preview_render` when needed, renders the current
LinkedWorld `default.bundle`, and writes previews to:

- `/tmp/sekaiemu-tracker-preview/alttp-light-world.png`
- `/tmp/sekaiemu-tracker-preview/alttp-dark-world.png`
- `/tmp/sekaiemu-tracker-preview/alttp-items.png`
- `/tmp/sekaiemu-tracker-preview/alttp-tracker-contact-sheet.png`

Useful overrides:

- `SEKAIEMU_TRACKER_PREVIEW_WIDTH=1920`
- `SEKAIEMU_TRACKER_PREVIEW_HEIGHT=1080`
- `SEKAIEMU_TRACKER_PREVIEW_OUT_DIR=/tmp/my-preview`
- `SEKAIEMU_TRACKER_PREVIEW_BUNDLE=/path/to/default.bundle`
- `SEKAIEMU_TRACKER_PREVIEW_MODE=full`
- `SEKAIEMU_TRACKER_PREVIEW_STATE=/path/to/state.json`
- `SEKAIEMU_TRACKER_PREVIEW_CONTACT_SHEET=0`
- `SEKAIEMU_TRACKER_PREVIEW_CONTACT_TILE=4x`

## Proof Files

After closing Sekaiemu, inspect:

- `/tmp/sekaiemu-alttp-manual-check-session/live/saves/sklmi/alttp-phase1/trace.jsonl`
- `/tmp/sekaiemu-alttp-manual-check-session/live/multiserver.log`
- `/tmp/sekaiemu-alttp-manual-check-session/live/saves/tracker/default.bundle/state.json`

The reproducible heartbeat writes the same proof shape to:

- `/tmp/sekaiemu-alttp-chest-e2e/live/saves/sklmi/alttp-phase1/trace.jsonl`
- `/tmp/sekaiemu-alttp-chest-e2e/live/multiserver.log`
- `/tmp/sekaiemu-alttp-chest-e2e/live/saves/tracker/default.bundle/state.json`

Expected current proof:

- SKLMI emits `location_checked` for `Link's House` / canonical id `59836`.
- MultiServer receives `LocationChecks [59836]`.
- The tracker autosave/shutdown line reports `local_checked=1` and `recent=1`.

## Current Boundary

This is not a full embedded PopTracker runtime yet. It is the first usable
Sekaiemu tracker surface backed by the ALTTP LinkedWorld bundle and its adapted
PopTracker pack data/assets: visible maps, item/check feed, tabs, room metadata,
and the same live SKLMI/AP data path.
