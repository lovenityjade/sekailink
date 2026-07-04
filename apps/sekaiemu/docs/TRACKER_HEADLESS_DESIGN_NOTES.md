# Tracker Headless Design Notes

These notes capture the current ALTTP tracker state while working without a
graphical session.

## Current Proof

- Live Sekaiemu can load the ALTTP LinkedWorld `default.bundle`.
- The bundle loader consumes referenced metadata and the adapted PopTracker map
  assets instead of the old placeholder maps.
- The Light World map renders with correct colors.
- The tracker no longer competes with emulation/audio every frame.
- SNES audio pacing is stable after using the audio queue as the runtime clock.
- Headless previews can be generated without launching Sekaiemu.
- Bundle audit notes are captured in `docs/ALTTP_TRACKER_BUNDLE_AUDIT.md`.

## Generate Previews

```bash
tools/render_alttp_tracker_preview.sh
```

Outputs:

- `/tmp/sekaiemu-tracker-preview/alttp-light-world.png`
- `/tmp/sekaiemu-tracker-preview/alttp-dark-world.png`
- `/tmp/sekaiemu-tracker-preview/alttp-items.png`
- `/tmp/sekaiemu-tracker-preview/alttp-tracker-contact-sheet.png`

Useful overrides:

- `SEKAIEMU_TRACKER_PREVIEW_WIDTH=1920`
- `SEKAIEMU_TRACKER_PREVIEW_HEIGHT=1080`
- `SEKAIEMU_TRACKER_PREVIEW_OUT_DIR=/tmp/sekaiemu-tracker-preview-1080`
- `SEKAIEMU_TRACKER_PREVIEW_BUNDLE=/path/to/default.bundle`
- `SEKAIEMU_TRACKER_PREVIEW_MODE=full` to render all current ALTTP tabs
- `SEKAIEMU_TRACKER_PREVIEW_STATE=/path/to/saves/tracker/default.bundle/state.json`
  to render a real persisted tracker state
- `SEKAIEMU_TRACKER_PREVIEW_CONTACT_SHEET=0` to skip the contact sheet
- `SEKAIEMU_TRACKER_PREVIEW_CONTACT_TILE=4x` to change the contact sheet grid

## Audit Bundle Data

```bash
tools/audit_alttp_tracker_bundle.sh
```

Default output:

`/tmp/sekaiemu-tracker-audit/alttp-tracker-bundle-audit.md`

## Design Pass Targets

Detailed next-step checklist:

- `docs/NEXT_TRACKER_DESIGN_PASS.md`

- Make the side-by-side layout feel like a real play overlay, not a debug
  dashboard.
- Keep the game readable first, tracker second.
- Make map, pins, item inventory, and recent item/check feed visible without
  needing the details panels open.
- Turn the current metadata chips into a compact debug/status row.
- Move deeper seed/settings metadata behind a detail or debug mode.
- Use pack assets for item icons wherever the bundle provides them.
- Make pin state obvious: unchecked, checked, unavailable, important, and
  current-zone hint.

## Current Boundaries

- This is not a full embedded PopTracker runtime yet.
- The renderer consumes adapted pack data/assets through the LinkedWorld bundle.
- Some adapted PopTracker files are not strict JSON, so runtime consumption
  should prefer normalized strict bundle metadata for BETA-3.
- The current layout is still debug-heavy and should be redesigned before the
  community test.
- The headless renderer is the fastest loop for layout iteration; use the live
  emulator only after a preview looks acceptable.
