# Changelog

## 2026-06-25

- Froze NES as a validated BETA-3 runtime system after successful Client Core
  and multiworld tests. NES should now be treated as a known-good generic
  BizHawk-style bridge reference unless a reproducible regression appears.
- Registered The Legend of Zelda in the Sekaiemu SKLMI bridge registry so its
  existing `tloz.phase1.json` manifest is used instead of falling back to the
  legacy profile socket bridge, which could fail on long Linux save paths.
- Split the runtime memory server into focused memory utility and socket utility
  modules. The server source is now below the 800-line maintenance limit while
  keeping the BizHawk-compatible protocol and GBA/GB domain behavior unchanged.
- Added the first generic Sekaiemu Runtime Debug window, replacing the old
  bridge terminal title and view with Readable/RAW tabs, runtime event
  filtering, redacted report copying, and low-level SKLMI trace/companion log
  ingestion without adding game-specific debug logic.
- Routed SDL input to the separate Runtime Debug window so its tabs, filters,
  clear action, and copy-report action can be used without leaking clicks into
  the emulator, tracker, or runtime menu.
- Added a test command input to Runtime Debug. `give <slot> <item>` is
  translated to the same Archipelago admin chat command used by `skl-room`
  (`!admin /send ...`), while `say <message>` sends normal AP chat through the
  existing SKLMI chat bridge/command-log path.
- Runtime Debug `give` now mirrors `skl-room` admin login behavior by reading
  `SEKAILINK_AP_ADMIN_PASSWORD`, sending `!admin login` before `!admin /send`,
  and redacting the password from Readable/RAW logs and copied reports.
- Expanded Runtime Debug into a small `skl-room`-style console with relevant
  debug commands (`help`, `status`, `players`, `items`, `give`, `say`,
  `ap-say`, `hint`, `collect`, `remaining`, `admin`) plus Tab autocomplete for
  commands, slot names, item names, and known location names from the active
  runtime snapshot.
- Stabilized the ESC runtime menu by removing the unstable ImGui tab selection
  loop, replacing it with a deterministic SekaiLink-styled tab strip, fixing
  the inverted Advanced mode toggle, and preventing menu mouse clicks from
  leaking into the emulator/tracker underneath.
- Fixed Runtime Debug check progress so it falls back to
  `checked_locations + missing_locations` when the client snapshot does not
  provide an explicit `summary.total`, preventing misleading `0/0` progress.
- Expanded Runtime Debug readable summaries to keep human-friendly AP events
  together with raw IDs: checks now show location names plus location IDs,
  received items show item names plus raw item/location/player numbers, and
  memory bridge reads/writes show address, size, and byte previews.
- Added tracker pipeline visibility to Runtime Debug. It now tails the tracker
  command log for commands sent to the tracker and records tracker snapshot
  responses with revision, check count, item count, active map, and active tab.
- Added bounded low-level BizHawk-compatible memory socket tracing for READ,
  WRITE, and GUARD requests. The trace includes domain, address, size, and
  resolution status so GBA/GB/NES core bridge failures can be diagnosed at the
  generic memory layer instead of game by game.
- Surfaced the main `sekaiemu.log` path in Runtime Debug and tail only the
  relevant runtime/memory/connection/error lines there. The Debug window can
  now show `[sekaiemu-memory]` bridge traffic without drowning users in core
  log spam.
- Aligned the bundled Mega Man 3 Archipelago runtime client with the generated
  `0.1.7` patch format. MM3 patches were validating as `0.1.7` while the local
  client expected `0.1.8`, so the client connected but never started its RAM
  watcher or sent location checks.
- Fixed Mega Man 2 and Mega Man 3 check deduplication in the local
  Archipelago runtime clients. The watchers now consider both server-confirmed
  `checked_locations` and locally queued `locations_checked`, preventing repeated
  `LocationChecks` spam for the same pickup while keeping detection generic.
- Guarded Mega Man 2 and Mega Man 3 outgoing checks against the room's
  `server_locations`. Pickups disabled by the current seed options are no
  longer reported as if they were valid checks, which avoids silent server
  ignores such as MM3 sending `Needle Man Stage - Weapon Energy 1` when only
  the enabled consumable locations are present in the room.
- Replaced deprecated `Utils.get_options()` ROM setting lookups in Zelda II,
  Wario Land, Donkey Kong Country, and Mega Man X3 runtimes with the current
  Archipelago settings API so patch/launch no longer trips on updated AP
  runtime warnings/errors.
- Fixed the Zelda II AP runtime client so received items are written to RAM
  before side-scroll state filtering, NPC checks can be reported outside
  side-scroll scenes, and outgoing checks are deduplicated/guarded against the
  room's active `server_locations`.
- Added a Zelda II-only WRAM fallback that resolves its cartridge WRAM accesses
  through `WRAM`, `SRAM`, `Battery RAM`, or NES `System Bus` without changing
  the shared NES core bridge used by Zelda 1, Mega Man 2, and Mega Man 3.

## 2026-06-21

- Disabled the internal Sekaiemu tracker runtime for normal host launches.
  Legacy tracker CLI flags are still parsed for compatibility, but they no
  longer map into `HostOptions`; BETA-3 uses external PopTracker instead.

## 2026-06-20

- Fixed the runtime memory server TCP reader so fragmented JSON-line requests
  are buffered per client until complete. This resolves Super Mario Land 2
  watcher timeouts when the AP client sends large batched `System Bus` reads
  for coinsanity/midway/level checks, and keeps the fix generic for other
  BizHawk-compatible clients.
- Added local reference copies of Zunawe's `bhc-substitutes` mGBA and PJ64
  connectors under `third_party/bhc-substitutes` to compare Sekaiemu's
  BizHawk-compatible bridge against known emulator substitute scripts.
- Matched the GBA bridge more closely to the mGBA substitute connector by
  treating `System Bus` as the authoritative GBA access path and mirroring
  high GBA save/SRAM addresses such as `0x0E01FFFE` into smaller libretro
  save RAM regions, including the 32 KiB save RAM case seen in live mGBA tests.
- Added smoke coverage for mirrored 32 KiB GBA save RAM reads/writes through
  both `System Bus` and `SRAM`, then rebuilt and staged the local Linux
  Sekaiemu runtime binary.
- Made battery save loading tolerant when a core exposes a larger save RAM
  region than the existing `.sav` file. Smaller GBA saves now load into memory
  and pad the remaining save RAM instead of being skipped entirely.
- Fixed the generic GBA memory bridge used by BizHawk-wrapper Archipelago
  clients so `EWRAM`, `IWRAM`, `System Bus`, `bus`, and `gba_system_bus`
  resolve correctly through mGBA/libretro memory.
- Added GBA SRAM mapping through `System Bus` at `0x0E000000`, including the
  Fusion receive counter addresses at `0x0E01FFFE` and `0x0E01FFFF`, without
  changing the already-working MZM ROM/RAM bridge path.
- Added GBA ROM bus reads through `System Bus` at `0x08000000`, which generic
  BizHawk-style GBA clients use to validate headers and seed symbols.
- Rebuilt and staged the local Linux Sekaiemu runtime binary after the GBA
  bridge fix.
- Added a descriptor-backed fallback for GBA relative domains so clients that
  ask for `IWRAM:offset` or `EWRAM:offset` still work when mGBA/libretro
  exposes those regions through the `System Bus` memory map instead of one
  contiguous system RAM block.
- Revalidated Metroid Fusion live through Client Core: Sekaiemu exposed Morph
  Ball state in `IWRAM`, the AP client sent `LocationChecks` for `Main Deck --
  Quarantine Bay`, and the server returned `ReceivedItems`.
- Fixed BizHawk protocol error responses from the runtime memory server so
  failed `READ`/`WRITE` requests return the upstream-compatible `err` field
  instead of `value`. This prevents generic BizHawk AP clients such as Metroid
  Fusion from crashing their watcher on a transient or guarded memory read
  failure.
- Added a smoke assertion covering the `READ` error payload shape used by
  upstream Archipelago BizHawk clients.
- Rebuilt and staged the local Linux Sekaiemu runtime binary after the BizHawk
  protocol error payload fix.
- Added a generic BizHawk-compatible TCP memory bridge path for Sekaiemu on
  Linux, matching the Archipelago BizHawk client connector port range used by
  upstream clients.
- Added read-only ROM domains for the runtime memory bridge, including
  headerless `PRG ROM` exposure for NES clients such as The Legend of Zelda.
- Updated the Linux bundled Sekaiemu runtime binary so BizHawk-wrapper
  Archipelago clients can connect to Sekaiemu through the same memory protocol
  used by `connector_bizhawk_generic.lua`.

## 2026-06-13

- Revalidated the Dear ImGui Sekaiemu runtime integration from a clean local
  build directory.
- Verified the full Sekaiemu smoke suite after the ImGui runtime/menu/layout
  preview work, including ALTTP tracker bundle rendering, preview rendering,
  launch options, save states, frontend settings, and the BizHawk-compatible
  memory protocol smoke.

## 2026-06-12

- Started the Sekaiemu Dear ImGui runtime migration with vendored ImGui
  v1.91.9b and SDL2/OpenGL3 backend integration.
- Added an OpenGL ImGui draw callback path so Sekaiemu can render native menu
  and tracker panels directly in the runtime frame instead of rasterizing them
  through the old overlay canvas.
- Added a SekaiLink-styled ImGui runtime menu covering main actions, settings,
  input bindings, core options, bridge status, sync info, and save-state slots.
- Added a first generic ImGui tracker surface backed by the existing tracker
  runtime state, including tabs, map selection, info panels, live feed, loading,
  and error states.
- Added a real `--layout-preview` mode for opening Sekaiemu without ROM/core,
  showing a retro no-cartridge preview surface plus the offline tracker layout
  in windowed or fullscreen mode.
- Added an ALTTP pack-driven ImGui preview bridge that renders the existing
  native tracker pack into an ImGui texture, letting the lab compare tracker
  layout direction before the tracker is fully rewritten as native ImGui
  widgets.
- Polished the ImGui layout preview with crisp nearest-neighbor tracker pack
  rendering, ALTTP-specific offline preview state, realistic checks/items, and
  keyboard shortcuts for split/PIP/toggle tracker layout comparison.

## 2026-06-11

- Added automatic Sekaiemu runtime bug-report submission for patch preparation,
  libretro initialization, and non-zero runtime exits using the shared
  `/api/client/bug-report` payload contract.
- Sekaiemu bug reports now include the active runtime log tail, game/core
  context, player alias, and linkedworld/AP game metadata when available.
- Fixed a Windows/MSYS2 build break in the tracker runtime smoke by passing
  libarchive a UTF-8 narrow archive path instead of a wide filesystem path.

## 2026-06-10

- Verified live runtime memory socket exposure for NES/FCEUmm, GBA/mGBA, and
  GB/GBC/Gambatte with real local ROM launches.
- Restricted the `gba_system_bus` memory domain to GBA sessions only, while
  keeping descriptor-backed `System Bus` available for NES and GB/GBC generic
  integration.

## 2026-06-06

- Added an explicit tracker loading state so Sekaiemu shows
  "Loading tracker..." until tracker data is ready, then either renders the
  tracker or shows a clear tracker error panel.
- Hardened tracker initialization failures so a broken tracker pack reports
  `TRACKER ERROR` in the overlay instead of aborting the whole emulator launch.
- Kept the last valid tracker map as a fallback when auto-follow temporarily
  loses its map/zone hint, preventing ALTTP from snapping back to the default
  map after leaving a dark-world context.
- Completed tracker checks now disappear from native map pins and pack-layout
  pins, including their click targets.
- Window sizing now avoids shrinking an already larger user-resized Sekaiemu
  window, reducing streamer-hostile snap-back when tracker/sidebar state changes.
- SKLMI companion runtime exit errors now include the companion log path so
  connection/runtime checkups point directly to the diagnostic file.

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
