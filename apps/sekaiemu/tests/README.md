# Golden Tests

These tests lock in the first real proof artifacts from the Sekaiemu bring-up work.

Current fixtures:

- `EarthBound`
  - memory dump transition at `0x9C11` and `0x9C6C`
  - location log lines for the two Tracy checks
- `ALTTP`
  - verified injection traces for `Blue Boomerang` and `Hookshot`
  - native bundle smoke for `tracker-bundles/alttp-default`
  - native bundle smoke for `tracker-bundles/alttp-native`
  - LinkedWorld default archive fixture at
    `tracker-bundles/alttp-linkedworld-default`
  - live e2e defaults to the clean-room ALTTP LinkedWorld `default.bundle`,
    including its `poptracker-adapted/` map/assets refs
  - headless preview rendering through `tracker_preview_render`
  - visible map rendering smoke through the native overlay path
  - bundle metadata, live-feed fields, progress panels, and recent event capture checks
  - e2e heartbeat in `tests/e2e/alttp_chest` proving real gameplay chest open
    -> SKLMI `location_checked` -> AP `LocationChecks` -> tracker `local_checked`

Tracker interpretation note:

- these fixtures exercise ALTTP bundles and ALTTP-shaped metadata through the
  generic tracker runtime
- they should not be read as proof that the host itself is game-specific
- the LinkedWorld default archive fixture verifies archive inventory and
  runtime handoff expectations only; the native runtime now accepts extracted
  bundle directories or `.zip` bundles and can decode PPM, PNG, JPG/JPEG, and
  WebP tracker assets
- the runtime now follows referenced bundle JSON files such as
  `tracker_flow_ref`, `item_icon_metadata_ref`, and `map_pin_metadata_ref`
  before rendering
- `sekaiemu_tracker_preview_render_smoke` verifies that the renderer can produce
  a tracker preview image without launching Sekaiemu or opening a window

These tests are intentionally offline and fixture-based for now.
They are meant to catch regressions in the validated proof path while the runtime is still being refactored.

The ALTTP chest e2e is intentionally not part of regular `ctest` because it
requires local ROM, seed, MultiServer, and SKLMI runtime artifacts. Run it
manually when validating the Beta-3 live path:

```bash
tests/e2e/alttp_chest/run_alttp_chest_e2e.sh
```

For human-driven validation, use:

```bash
tests/e2e/alttp_chest/run_alttp_manual_check_session.sh
```

This keeps MultiServer alive while Sekaiemu is open and records the same
SKLMI/server/tracker proof files after the play session.

The scripted ALTTP chest e2e now reuses `/tmp/sekaiemu-links-house-state` when
that calibrated Link's House savestate is present. This keeps the heartbeat
focused on the live path:

- load calibrated savestate
- open Link's House chest
- SKLMI emits `location_checked`
- MultiServer receives `LocationChecks`
- tracker state saves with `local_checked=1` and `recent=1`
