# Next Tracker Design Pass

This is the next focused work slice for the ALTTP tracker.

## Start Here

Generate a full preview board:

```bash
SEKAIEMU_TRACKER_PREVIEW_MODE=full \
SEKAIEMU_TRACKER_PREVIEW_OUT_DIR=/tmp/sekaiemu-tracker-preview-full \
tools/render_alttp_tracker_preview.sh
```

Open:

`/tmp/sekaiemu-tracker-preview-full/alttp-tracker-contact-sheet.png`

## Current Validated Baseline

- Sekaiemu + ALTTP + SKLMI + Archipelago server path works.
- ALTTP tracker loads the LinkedWorld `default.bundle`.
- PopTracker-adapted map assets render with correct colors.
- Audio is stable after audio queue pacing.
- Tracker work is throttled to avoid competing with emulation.
- Headless preview generation is available and covered by tests.

## What To Redesign First

- Reduce debug-dashboard density.
- Keep map and important item/check feed visible.
- Collapse session/seed metadata into a compact status strip.
- Move deep settings/detail panels behind a debug/detail mode.
- Use item icons from normalized bundle metadata instead of tiny text badges.
- Make map pins visually meaningful:
  - unchecked
  - checked
  - current/nearby
  - important
  - unavailable/unknown

## Do Not Do Yet

- Do not rewrite the tracker runtime.
- Do not embed PopTracker runtime code.
- Do not make the emulator parse relaxed PopTracker JSON-like files directly.
- Do not widen scope beyond ALTTP tracker presentation until the ALTTP surface
  is comfortable for a community test.

## Verification Loop

After a layout change:

```bash
cmake --build /tmp/sekaiemu-libretro-spike-beta3-build --target tracker_preview_render sekaiemu_libretro_spike -j2
SEKAIEMU_TRACKER_PREVIEW_MODE=full tools/render_alttp_tracker_preview.sh
ctest --test-dir /tmp/sekaiemu-libretro-spike-beta3-build --output-on-failure -R 'tracker_(runtime|overlay|alttp|preview)'
```

Only launch the live emulator after the headless preview looks acceptable.
