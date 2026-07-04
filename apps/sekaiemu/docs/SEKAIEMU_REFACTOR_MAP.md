# Sekaiemu Refactor Map

This document is the working map for keeping `Sekaiemu` maintainable while the
`BETA-3` tracker boundary moves PopTracker runtime work into `SKLMI`.

## Non-Negotiable Boundary

Production `Sekaiemu` is an emulator and renderer. It may read throttled
snapshots, write rare command-log events, resolve already-declared assets, and
draw the tracker. It must not run PopTracker Lua, compute `CanReach`, evaluate
pack logic, or rebuild heavy tracker snapshots on the emulation/audio path.

`SKLMI` owns the PopTracker-compatible runtime: pack loading, Lua callbacks,
Archipelago events, item progression, pin colors, location grouping, and the
stable `tracker.snapshot.json` publication.

## File Budget

Target source file size is under 500 lines.

Temporary exceptions are allowed only while a file is actively being split, but
new work should not add responsibilities to files already over the target.

There are currently no tracked `src/*.cpp` or `src/*.hpp` files above the
500-line target.

## Module Ownership

`libretro_host.cpp` should become orchestration glue only. The host may sequence
startup, shutdown, and frame ticks, but subsystem details should live in smaller
controllers.

Tracker presentation belongs in modules with "tracker" names and draw-only
responsibilities:

- `tracker_snapshot_io.*`: low-frequency atomic snapshot reader
- `tracker_asset_resolver.*`: host-side image lookup/cache for declared assets
- `tracker_window_presenter.*`: SDL-only tracker separate-window presentation
- `tracker_raster_image.*`: PPM/SDL_image raster decoding for declared tracker assets
- `tracker_overlay_renderer.*`: canvas composition and style
- `tracker_overlay_style.*`: shared panel colors and drawing primitives
- `tracker_overlay_*_sections.*`: draw-only panel body sections
- `tracker_overlay_json.*`: JSON/text helpers for render-state projection
- `tracker_pack_layout_engine.*`: pure layout reference, sizing, margins, and map-target helpers
- `tracker_pack_layout_json.*`: JSON/comment/path helpers for adapted pack layouts
- `tracker_pack_layout_model.*`: private pack layout renderer model structs
- `tracker_pack_layout_renderer.*`: temporary snapshot/layout drawing adapter

The temporary pack layout renderer exists to bridge old local assets and new
SKLMI snapshot payloads. Its direction is smaller, not smarter: every piece of
semantic pack/runtime behavior should migrate to `SKLMI` snapshots.

## Refactor Order

1. Move small, low-risk helpers out of `libretro_host.cpp`.
2. Split tracker host presentation from tracker state/snapshot pumping.
3. Split `tracker_pack_layout_renderer.cpp` into loader, visual resolver,
   layout engine, and painter files.
4. Split `tracker_runtime.cpp` into bundle loading, image loading, snapshot
   normalization, and local UI/persistence.
5. Remove or quarantine Sekaiemu-side PopTracker projections once `SKLMI`
   publishes equivalent draw-ready data.

## Completed First Pass

- Extracted host tracker asset resolution from `libretro_host.cpp`.
- Extracted the SDL separate tracker window into `tracker_window_presenter.*`.
- Extracted tracker raster image decoding from `tracker_runtime.cpp`.
- Extracted adapted pack layout JSON helpers from `tracker_pack_layout_renderer.cpp`.
- Extracted the pack layout model structs into `tracker_pack_layout_model.hpp`.
- Extracted the pack layout engine into `tracker_pack_layout_engine.*`.
- Split `tracker_overlay_renderer.cpp` into renderer, style, map sections, and detail sections.
- Grouped tracker runtime/render source lists in `CMakeLists.txt` so the next splits update one source set.
- Documented that `PIP` is legacy-only for `BETA-3`; old `PIP` settings render through toggle behavior.
- Extracted pack layout document loading, visual state resolution, and widget
  painting from `tracker_pack_layout_renderer.cpp`.
- Extracted overlay pack metadata, snapshot helpers, and session-state mapping
  from `tracker_overlay_render_state.cpp`.
- Split `tracker_runtime.cpp` into bundle archive/loading, legacy visual
  PopTracker fallback, ingestion, persistence, snapshot value helpers, and view
  state resolution.
- Split host tracker presentation, metadata pumping, tracker host state, menu
  presentation/actions, frame automation, frame dump, save helpers, memory tools,
  bridge helpers, I/O callbacks, and video host helpers out of
  `libretro_host.cpp`.
- Split the remaining host implementation into lifecycle, callbacks, runloop,
  and action modules; `libretro_host.cpp` is now a thin public facade.
- Split input menu/config helpers from `input_state.cpp`.
- Split OpenGL viewport/shader helpers and overlay rendering from
  `opengl_video_backend.cpp`.
- Split core-option utility helpers from `core_option_manager.cpp`.

The next high-value cuts should be behavioral seams rather than line-count
triage: keep carving tracker draw code toward snapshot-only consumers, and move
any remaining runtime semantics to `SKLMI` snapshots.

## Verification

After every extraction:

```bash
cmake --build build-codex-tracker -j4
ctest --test-dir build-codex-tracker --output-on-failure -R '(tracker_(runtime|overlay_renderer|alttp_bundle|alttp_native_bundle|preview_render|snapshot_io)_smoke|launch_options_smoke)'
```

For visual tracker changes, also render the headless preview before launching a
live emulator session.
