# Native Tracker Bundles

This directory contains repo-owned tracker bundle fixtures for the native
libretro spike. These are test fixtures, not the current live ALTTP tracker
package.

Architectural intent:

- the libretro host is a generic tracker consumer
- bundles declare tracker-facing maps, tabs, panels, and visibility hints
- LinkedWorld / bridge metadata provide the game-specific meanings and live state
- these fixtures should not be read as proof that the host is hard-coded for one game

Current live ALTTP bundle:

- `<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle`
  - preferred BETA-3 ALTTP tracker package
  - includes the strict LinkedWorld runtime metadata plus `poptracker-adapted/`
    maps/assets
  - used by the live ALTTP e2e/manual scripts and by the headless preview tool

Fixture scope:

- stabilize the bundle contract in-tree
- provide a first visible map path without depending on PopTracker runtime embedding
- keep assets simple enough for smoke tests and rapid iteration

Current bundles:

- `alttp-native`
  - legacy in-repo ALTTP example fixture for regression tests
  - includes starter tabs, three visible maps, and room/slot/progress-oriented metadata panels
  - meant for split-screen, separate-window, and toggle-screen verification
- `alttp-default`
  - older contract/rendering fixture for tests
  - useful for fast smoke coverage without the large external LinkedWorld bundle
- `alttp-linkedworld-default`
  - documentation/fixture for validating the external LinkedWorld ALTTP
    `tracker/default.zip` enriched with `poptracker-adapted`
  - records archive refs, size/hash, expected paths, and runtime expectations
    without copying the large archive into this repo
  - Sekaiemu now accepts either the extracted `tracker/default.bundle`
    directory or the packaged `tracker/default.zip`

Bundle notes:

- the current native image path still keeps small `.ppm` fixtures for fast tests
- the runtime can also decode linked bundle `.png`, `.jpg/.jpeg`, and `.webp`
  assets through SDL2_image
- these bundles are fixtures and UI proofs, not final art packs
- `poptracker-adapted` assets in the LinkedWorld ALTTP archive are static
  PNG/JPG/XCF/JSON references; this repo still avoids PopTracker, Lua, and SNI
  runtime execution
- several adapted PopTracker files are JSON-like but not strict JSON; runtime
  code should consume normalized LinkedWorld metadata unless/until a tolerant
  normalizer is introduced
