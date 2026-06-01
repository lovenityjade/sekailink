# Changelog

## 2026-05-10

- Added `TrackerBundle::LoadFromPath` so Sekaiemu can consume either extracted
  tracker bundle directories or packaged `.zip` tracker bundles.
- Materialized zipped tracker bundles into a local cache with unsafe archive
  path protection before loading manifests and assets.
- Switched the libretro host tracker initialization to the generic path loader,
  unblocking `--tracker-bundle .../tracker/default.zip` for LinkedWorld
  packages.
- Locked the path with smoke coverage for directory loading, generated zip
  loading, and the external ALTTP LinkedWorld `tracker/default.zip` artifact.
- Updated tracker fixture docs to reflect current PNG/JPG/WebP image loading
  and direct archive support.

## 2026-05-05

- Made the native recent-event feed merge live trace rows with snapshot-backed
  last item/check rows when one event type has not reached the live trace yet,
  keeping ALTTP side-by-side testing readable during partial room sync.
- Extended the generic tracker panel contract with bundle-declared `surface` / `priority` metadata so the host can sort summary vs. detail cards without relying on ALTTP-specific panel ids.
- Expanded the ALTTP native bundle's deeper card surface with `room-meta` and `runtime-meta`, and made the side-by-side renderer degrade cleanly with `+N MORE PANELS` / `+N MORE EVENTS` markers when a run exposes more metadata than the current column can display.
- Realigned tracker docs to state the intended architecture explicitly: `Sekaiemu` remains a generic tracker host consuming bundle + LinkedWorld / `SKLMI` metadata, while ALTTP is the current richest in-repo example rather than a hard-coded host path.
- Prepared the ALTTP native tracker surface for fuller runs by consuming structured `received_items` / `checked_locations` snapshot arrays instead of treating the side-by-side UI as a phase-1 demo only.
- Added derived live tracker fields such as `last_received_label`, `last_received_from`, `last_check_label`, and snapshot-backed progress counts so ALTTP can stay readable even when only room-state sync is available.
- Extended the ALTTP native bundle with a `live-feed` panel and reworked the side-by-side item flow to surface last item, sender, and last check more clearly.
- Reworked the `DETAILS` area so it now spends its space on deeper ALTTP cards like `slot-info`, `seed-meta`, and seed settings instead of repeating summary panels already visible elsewhere.
- Strengthened tracker smokes to cover object-array snapshot ingestion, snapshot-backed progress text, and native ALTTP live-feed panel resolution.
- Tightened the ALTTP native tracker side-by-side layout so session chips, progress, and tracker-state stay readable in the narrow live panel.
- Reordered the ALTTP native info panels to prioritize `session`, `progress`, and `tracker-state` before deeper slot/rules details.
- Aligned the ALTTP native bundle with real private-room snapshot keys such as `room_id`, `slot_name`, `player_alias`, and `seed_metadata.slot_data.*`.
- Added derived tracker session fields for progress text such as `check_progress`, `known_total_count`, and `completion_percent`.
- Added `tracker_alttp_native_bundle_smoke` to lock the native ALTTP bundle against regressions in room identity, `slot_data`, and renderability.
- Improved ALTTP room metadata ingestion so tracker runtime sessions now capture
  `linkedworld_id`, `room_id`, `slot_name`, and `player_alias` in addition to
  seed-facing metadata from `room.state`.
- Added concise runtime logging when tracker room metadata changes, making it
  easier to debug private Link Room handoff during live ALTTP runs.
- Extended the ALTTP default tracker bundle and smoke coverage to show the extra
  room identity fields directly in the session panel.
- Added `ALTTP_PRIVATE_ROOM_LINK_FLOW.md` to document the precise private-room
  handoff path between Link Room, `SKLMI`, and the native tracker runtime.
- Added a small `trace.jsonl` fallback path for tracker metadata recovery when
  live `room.state` only exposes a minimal sentinel and the richer metadata is
  still only visible through `room_client_ready`, `room_metadata_ready`, or
  `slot_connected`.
- Added native launch support for `--patch <file.aplttp>` plus `--base-rom <path>`.
- Materialized ALTTP Archipelago patches directly inside `save_dir/patched/`.
- Validated the generated patched ROM against the previously known ALTTP output.
- Preserved video aspect ratio in the software backend instead of stretching fullscreen output.
- Added an ALTTP tracker bundle for realtime testing under `tracker-bundles/alttp-native/`.
- Re-exported the ALTTP tracker maps to portable pixmap so the native runtime can load them without Lua or external tracker tooling.
- Made tracker bundle raster loading resilient so unsupported map assets no longer abort the full runtime.
- Restored `tracker_overlay_renderer.cpp` as a real source file and revalidated the renderer/bundle smoke coverage.
- Improved tracker rendering so the native panel can show a real map image from the bundle in addition to runtime state and seed metadata.
- Added room-state metadata ingestion from `SKLMI` into the tracker runtime.
- Verified a real ALTTP runtime session where `seed_id`, `slot_id`, and `world_instance_id` reached the persisted tracker state.
- Persisted `cached_seed_metadata` and `cached_server_snapshot` so `slot_data` and tracker-facing settings survive shutdown for triage.
- Expanded the ALTTP native bundle with castle and sewers map coverage, slot and seed info panels, and tracker-side progress metadata for test launches.
- Reworked the native tracker panel into a more readable side-by-side ALTTP surface with map focus, session chips, summary metrics, metadata panels, and recent event history.
- Strengthened the tracker smoke tests so they now validate bundle metadata resolution and live recent-event capture in addition to map visibility.
- Clarified the native controller and mapping workflow in the README, including where controller bindings and logs are persisted for test sessions.
- Shifted the ALTTP test-facing docs and smoke coverage toward `tracker-bundles/alttp-default`, with clearer side-by-side metadata emphasis around `seed_id`, `slot_id`, `world_instance_id`, and `slot_data`.
- Improved the ALTTP side-by-side tracker surface again with clearer header/title treatment, a visible item-flow section, bundle-backed progress metadata, and safer empty-state messaging when live data is still sparse.
- Revalidated the libretro build and the full `ctest` suite after the runtime changes.
# 2026-05-21

- Switched live ALTTP tracker runs from the old `alttp-native` placeholder
  fixture to the clean-room LinkedWorld `default.bundle`.
- Made `TrackerBundle::LoadFromDirectory` follow referenced bundle files such
  as `tracker_flow_ref`, `item_icon_metadata_ref`, `map_pin_metadata_ref`, and
  `poptracker_adaptation_ref` before rendering.
- Replaced placeholder ALTTP map rasters with available
  `poptracker-adapted/` pack maps during bundle loading.
- Fixed SDL tracker map color channels by uploading RGBA canvas bytes through
  `SDL_PIXELFORMAT_ABGR8888`.
- Throttled tracker metadata polling and presentation rendering so tracker UI
  work no longer runs every emulation frame.
- Fixed SNES audio pacing by letting the queued audio buffer act as the runtime
  clock instead of allowing runaway frames followed by buffer clears.
- Added `tracker_preview_render`, a headless tracker renderer that produces
  preview images without launching Sekaiemu.
- Added `tools/render_alttp_tracker_preview.sh`, which renders Light World,
  Dark World, and Items previews to `/tmp/sekaiemu-tracker-preview`.
- Extended the preview script with `SEKAIEMU_TRACKER_PREVIEW_MODE=full` to
  render all current ALTTP tracker tabs for design review.
- Extended `tracker_preview_render` and the preview script with
  `SEKAIEMU_TRACKER_PREVIEW_STATE` so persisted live tracker state can be
  rendered headlessly.
- Added automatic preview contact sheets so all rendered ALTTP tracker tabs can
  be reviewed from one PNG while working headless or from mobile.
- Added `sekaiemu_tracker_preview_render_smoke` so tracker preview rendering is
  covered by `ctest`.
- Added `tools/audit_alttp_tracker_bundle.sh` and documented the current ALTTP
  bundle inventory: `1610` files, `1536` adapted images, `38` strict JSON
  files, and `22` relaxed JSON-like pack files.
- Marked the in-repo `tracker-bundles/` entries as regression fixtures while
  the live BETA-3 ALTTP tracker package remains the external LinkedWorld
  `default.bundle`.
- Added the official ALTTP chest e2e heartbeat under
  `tests/e2e/alttp_chest`.
- The heartbeat creates a Link's House savestate, launches MultiServer,
  reloads Sekaiemu with SKLMI and the native tracker, opens the Link's House
  chest through real input, and verifies `LocationChecks [59836]`,
  `reported|59836|Link's House`, and tracker `local_checked=1`.
- Added `run_alttp_manual_check_session.sh` for human-driven ALTTP validation.
  It keeps MultiServer alive while Sekaiemu is open, starts from the known
  Link's House state, and records SKLMI/server/tracker proof files after manual
  checks.
- Captured the first manual ALTTP triage lesson: if MultiServer exits before
  SKLMI connects, gameplay still updates SRAM/savestates, but no
  `LocationChecks` or tracker progress can be produced. The manual helper now
  avoids that launch trap.
