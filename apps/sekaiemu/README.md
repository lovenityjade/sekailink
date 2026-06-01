# Sekaiemu Runtime Worktree

This worktree started as the isolated proof project for the `Sekaiemu-libretro` viability spike.
It is now being evolved into the first production-oriented `Sekaiemu` runtime foundation.

Scope:

- one frontend host
- one console first
- one libretro core first
- one game first
- no Core integration
- native tracker foundation in progress
- no online

Original target:

- console: `SNES`
- core: `bsnes-mercury`
- game: `EarthBound (USA)`

Current spike status:

- `SNES` validated on `EarthBound` and `A Link to the Past`
- `NES` validated on `The Legend of Zelda`
- `GBA` validated on `Pokemon FireRed`
- `GB/GBC` partially validated on `Links Awakening DX Beta`
- `N64` explored with `Ocarina of Time`, but intentionally deferred as a later-stage integration problem

Decision note:

- `../../docs/SEKAIEMU_MVP_STATUS_AND_SCOPE.md`

## What This Project Proves

The goal is not to clone RetroArch.

The goal is to prove that we can hold a minimal libretro host ourselves:

- load a libretro core dynamically
- feed libretro callbacks
- load a game
- open a window
- handle input
- run a stable loop

If that works cleanly, we can decide whether it is realistic to keep `Sekaiemu` in the `BETA 3` conversation before locking it to `BETA 4`.

## Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

Linux build prerequisites are documented in:

- `README_BUILD_LINUX.md`

## Run

```bash
./build/sekaiemu_libretro_spike /path/to/core_libretro.so /path/to/game.sfc
```

Named-argument launch is also supported:

```bash
./build/sekaiemu_libretro_spike \
  --core /path/to/core_libretro.so \
  --game /path/to/game.rom \
  --system-dir ./system \
  --save-dir ./saves \
  --profile ./profiles/example.profile \
  --sklmi-runtime /path/to/sekailink_sklmi_runtime \
  --sklmi-manifest-dir /path/to/sekailink-sklmi/manifests \
  --tracker-pack /path/to/poptracker_pack_or_zip \
  --tracker-snapshot ./saves/sklmi/alttp-phase1/runtime/tracker.snapshot.json \
  --tracker-command-log ./saves/sklmi/alttp-phase1/runtime/tracker.commands.jsonl \
  --tracker-bundle /path/to/tracker_bundle_or_default.zip \
  --tracker-state ./saves/tracker/state.json \
  --log-dir ./logs
```

Tracker bundle example using the current ALTTP LinkedWorld bundle:

```bash
./build/sekaiemu_libretro_spike \
  --profile ./profiles/alttp-starter.profile \
  --tracker-bundle /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle \
  --tracker-state ./saves/tracker/default.bundle/state.json \
  /path/to/bsnes_mercury_performance_libretro.so \
  "/path/to/Zelda no Densetsu - Kamigami no Triforce (Japan).sfc" \
  ./system \
  ./saves
```

With the first memory/game profile:

```bash
./build/sekaiemu_libretro_spike \
  --profile ./profiles/earthbound-starter.profile \
  /path/to/bsnes_mercury_performance_libretro.so \
  /path/to/EarthBound.sfc \
  ./system \
  ./saves
```

Controls right now:

- `Arrow keys` = D-pad
- `Z` = B
- `X` = A
- `A` = Y
- `S` = X
- `Q` = L
- `W` = R
- `Enter` = Start
- `Right Shift` = Select
- `F1` = Reset
- `F2` = save battery-backed game save (`.sav`)
- `F3` = load battery-backed game save (`.sav`)
- `F5` = dump current memory regions to `save_dir/dumps/`
- `F6` = save savestate (`.state`)
- `F7` = load savestate (`.state`)
- `F8` = cycle tracker display mode
- `F9` = toggle tracker screen visibility / primary screen
- `F10` = cycle visible tracker tab
- `F11` = toggle automatic map follow
- `Esc` = open/close the Sekaiemu runtime menu

Native controller notes:

- SDL game controllers are detected at runtime and can be switched from the `Input` page
- input bindings are per-core and persist to:
  - `save_dir/input-config/<core_name>.cfg`
- each binding row stores both keyboard and controller input in the same config file
- `BIND ALL` captures one logical control after another, which is the fastest way to validate a fresh pad
- analog directions use SDL controller axis bindings and are especially relevant for N64 or other analog-aware cores

## Launch Surface

`Sekaiemu` now has a separated launch surface for both CLI and internal API use.

Current launch-related source files:

- [launch_options.hpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/launch_options.hpp)
- [launch_options.cpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/launch_options.cpp)
- [launcher.hpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/launcher.hpp)
- [launcher.cpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/launcher.cpp)
- [logger.hpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/logger.hpp)
- [logger.cpp](/home/thelovenityjade/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/src/logger.cpp)

Behavior:

- CLI and API now share the same launch validation path
- user-facing launch errors are separated from technical details
- by default, runtime logs are written to:
  - `save_dir/logs/sekaiemu.log`
- `--log-dir` can override the log directory explicitly
- bridge and tracker-adjacent runtime artifacts are easiest to inspect from:
  - `save_dir/logs/`
  - `save_dir/input-config/`
  - `save_dir/dumps/`
- `--help` prints usage without trying to start the runtime

## Runtime Menu

`Sekaiemu` now has an in-app runtime menu opened with `Esc`.

Behavior:

- opening the menu pauses emulation
- the menu is shown as a translucent overlay on top of the current frame
- closing the menu resumes emulation

Menu contents currently include:

- resume
- reset core
- save battery
- load battery
- save state
- load state
- quit Sekaiemu
- core settings page with tooltips, pending edits, and explicit apply/default/close actions
- input page with per-core keyboard/controller bindings
- bridge page with active bridge ownership/runtime status and companion restart action

Menu navigation:

- `Esc` = close menu
- `Tab` = switch between `Main`, `Core Settings`, and `Input`
- `Up` / `Down` = move selection
- `Left` / `Right` = change setting value
- `Enter` = activate selected action

Bridge page behavior:

- shows whether the active game is owned by `SKLMI` or by the legacy bridge path
- shows the current runtime memory socket, manifest, room state, trace log, and companion log
- if the `SKLMI` companion process stops unexpectedly, the emulator stays alive and the bridge page shows the last error
- `RESTART BRIDGE` restarts the `SKLMI` companion process without rebooting the core

Practical input/log workflow:

- open `Input` from the runtime menu to confirm which SDL pad is active before testing
- if a pad feels wrong, switch controller first, then use `BIND ALL`, then retest immediately
- inspect `save_dir/input-config/<core_name>.cfg` when you need to verify the persisted keyboard/controller mapping
- inspect `save_dir/logs/sekaiemu.log` first for launch/runtime failures
- inspect the bridge page when ALTTP metadata or tracker state do not look current

## Tracker Foundation

`Sekaiemu libretro` now owns the first native tracker foundation directly in this repo.

Current foundation status:

- bundle-driven tracker runtime
- persisted `tracker_state`
- seed metadata application
- zone-to-map switching
- native visible map rendering from repo-owned bundle assets
- bundle-defined slot, seed, settings, and tracker metadata panels
- snapshot-aware live item/check metadata resolution for richer side-by-side ALTTP runs
- recent tracker event capture for native rendering and smoke coverage
- local non-authoritative toggles
- 4 active display modes:
  - `split-screen`
  - `separate-window`
  - `pip-overlay`
  - `toggle-screen`
- native host rendering path:
  - overlay rendering for `split-screen`, `pip-overlay`, and `toggle-screen`
  - dedicated SDL window for `separate-window`
  - hotkey-driven mode switching and tab cycling

Current launch surface:

- `--tracker-pack <path>` passes the pack path through to the `SKLMI` companion
- `--tracker-variant <name>` passes the preferred pack variant through to `SKLMI`
- `--tracker-snapshot <path>` enables snapshot-first tracker updates in `Sekaiemu`
- `--tracker-command-log <path>` enables append-only tracker command writes back to `SKLMI`
- `--tracker-assets-root <path>` passes an extracted asset root through to `SKLMI`
- `--tracker-bundle <path>` accepts either an extracted tracker bundle directory
  or a `.zip` tracker bundle archive
- `--tracker-state <path>`
- `--patch <file.aplttp>`
- `--base-rom <path/to/base.rom>`

Current scope:

- the tracker runtime/state contract now lives at the `Sekaiemu` / `SKLMI`
  boundary instead of inside the emulation loop
- the live libretro host now owns tracker loading, snapshot recovery, autosave,
  command-log writes, and rendering
- `SKLMI` remains the runtime owner for PopTracker-compatible logic in `BETA-3`
- the host-side tracker path is intentionally generic:
  - it consumes `--tracker-bundle`, persisted tracker state, and an external
    snapshot
  - zipped tracker bundles are materialized into a local cache before loading
  - it does not hard-code ALTTP-specific tracker logic into the host itself
  - ALTTP is currently the most advanced in-repo bundle and live example, not a
    special host architecture
- current renderer is intentionally native, but current ALTTP live runs consume
  the LinkedWorld `default.bundle` and its `poptracker-adapted/` pack assets,
  metadata refs, maps, and pins instead of the old placeholder fixture
- tracker runtime polling is throttled separately from the 60 FPS emulation loop:
  snapshot reads are polled every 10 frames at most and only parsed when file
  size or `mtime` changes, while tracker presentation is refreshed every 30
  frames, so the tracker cannot compete with audio/video each frame
- the preferred ALTTP example bundle for current internal live runs now lives in:
  - `/home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle`
- the older contract fixture remains available in:
  - `tracker-bundles/alttp-default/`
- the runtime can now materialize an ALTTP `.aplttp` patch directly into:
  - `save_dir/patched/`
- the runtime now prefers `tracker.snapshot.json` as the tracker truth surface
  when provided by `SKLMI`
- `room.state` and selected `trace.jsonl` records remain fallback metadata
  surfaces for non-snapshot or migration scenarios
- structured `received_items` / `checked_locations` snapshot arrays are now
  consumed directly when present, so the tracker can keep showing last item,
  last check, and accurate journal counts even when live events are sparse
- snapshot `items`, `pins`, `summary`, `status.pack`, and `status.variant` are
  also consumed directly when present so item stages and pin colors can come
  from `SKLMI`
- live `item_received` events are enriched from the adjacent SKLMI
  `room_item_pending item_name=...` trace so the panel can show the item name
  even when the runtime event itself only carries an Archipelago item id
- when `room.state` is still sparse, the runtime now falls back to selected
  `trace.jsonl` records (`room_client_ready`, `room_metadata_ready`,
  `slot_connected`) so the tracker can still recover room/seed/slot identity
- old non-libretro Sekaiemu trees are now explicitly legacy-only

Tracker runtime example using the current ALTTP fixture:

```bash
./build/sekaiemu_libretro_spike \
  --profile ./profiles/alttp-starter.profile \
  --core /usr/lib64/libretro/bsnes_mercury_performance_libretro.so \
  --patch /path/to/AP_Seed.aplttp \
  --base-rom "/path/to/Zelda no Densetsu - Kamigami no Triforce (Japan).sfc" \
  --tracker-bundle /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle \
  --system-dir ./system \
  --save-dir ./saves \
  --log-dir ./logs
```

That path now does all of the following in one launch:

- validates the patch archive
- validates the base ROM checksum
- materializes the patched ROM locally
- preserves aspect ratio instead of stretching fullscreen output
- boots the libretro runtime
- loads the selected tracker bundle
- renders the active ALTTP map in the native tracker panel
- exposes slot, seed, live-feed, deep settings, and tracker-state metadata in the panel itself
- is now aligned with private-room `slot_data.*` and room/session identity from
  `room.state`
- keeps the side-by-side panel readable even before all live metadata or map state has arrived
- starts the runtime memory socket
- starts the ALTTP `SKLMI` companion when configured

Architectural note:

- the command above is an ALTTP example of a generic tracker host path
- the host loads a bundle plus snapshot surfaces and renders whatever that
  bundle declares
- ALTTP-specific semantics such as zone bindings, panel fields, `slot_data`
  paths, and room-facing labels belong to the ALTTP bundle or its LinkedWorld /
  `SKLMI` metadata, not to special-case host code
- `Sekaiemu` should not become the production owner of PopTracker Lua or pack
  reachability logic in `BETA-3`
- the intended stabilization note for this split lives in:
  - `docs/TRACKER_BETA3_RUNTIME_BOUNDARY.md`

Current ALTTP fixture bundle coverage:

- maps:
  - `lightworld`
  - `hyrule-castle`
  - `hyrule-sewers`
- tabs:
  - `overview`
  - `items`
  - `map-lightworld`
  - `map-castle`
  - `map-sewers`
  - `settings`
  - `slot`
- metadata panels:
  - `session`
  - `progress`
  - `live-feed`
  - `tracker-state`
  - `slot-info`
  - `seed-meta`
  - `room-meta`
  - `runtime-meta`
  - `settings-core`
  - `settings-rules`

Implementation note:

- the side-by-side layout is inspired by PopTracker readability and ALTTP pack behavior, but implemented here with repo-owned native rendering code only
- the renderer/runtime pair stays bundle-driven:
  - panels come from bundle manifests
  - panel placement is driven by manifest metadata such as `surface` and `priority`, so broader LinkedWorld metadata can grow the visible surface without host-side per-game panel lists
  - runtime metadata comes from snapshot, seed metadata, and runtime context
  - game-specific meaning is expected to arrive through LinkedWorld-owned
    metadata and bindings
- `seed_id`, `slot_id`, `world_instance_id`, tracker pack/variant, and `slot_data` fields are surfaced directly in the panel when present
- `slot_name`, `player_alias`, and `room_id` are also surfaced when the room metadata provides them
- the current renderer emphasizes a readable title/header, a seed/session strip, visible item flow, a readable map panel, deeper detail cards, and non-breaking empty states
- deeper cards can now expand into room/runtime metadata and degrade gracefully with `+N MORE PANELS` / `+N MORE EVENTS` markers when the metadata surface becomes larger than the current side column
- the native bundle now resolves structured snapshot arrays for `received_items` and `checked_locations`, including named object entries when the room state provides them
- the current ALTTP pilot runbook lives in `docs/ALTTP_TRACKER_STARTING_POINT.md`

Private Link Room integration path:

- see `ALTTP_PRIVATE_ROOM_LINK_FLOW.md`
- the tracker does not need direct Link Room networking in this repo
- the required handoff is a maintained `room.state` file produced by the active
  `SKLMI` companion
- selected `trace.jsonl` records are also consumed as a bounded fallback when
  live runs have not yet propagated full metadata into `room.state`

Input page behavior:

- input bindings are stored per core in:
  - `save_dir/input-config/<core_name>.cfg`
- the active SDL game controller can be selected from the input page
- each control can be rebound individually by selecting it and pressing `Enter`
- `BIND ALL` starts a sequential capture flow for the whole control set
- when capture is active, the next pressed keyboard key or selected controller input is assigned
- these bindings affect only the loaded core/game input path, not Sekaiemu's own runtime shortcuts
- the persisted config stores:
  - `selected_controller_guid=<guid>`
  - one `.keyboard=` line and one `.controller=` line per logical control

Core settings behavior:

- setting changes are staged first inside the menu
- `Apply` writes the config and pushes live updates to the core when supported
- options whose metadata mentions restart/reset requirements are marked in the menu
- those options are still saved by `Apply`, but may require a core restart to fully take effect
- `Default` stages the default values for all options
- `Close` discards any unapplied edits and closes the menu

Save data paths:

- battery saves:
  - `save_dir/battery/<game_stem>.sav`
- savestates:
  - `save_dir/states/<game_stem>.state`

Battery-backed save behavior:

- battery saves are no longer auto-loaded on boot
- battery load remains manual through the runtime menu or `F3`
- battery save remains manual through the runtime menu or `F2`
- in addition, `Sekaiemu` now watches exposed `SAVE_RAM` memory and writes the `.sav` file automatically after the memory changes and then settles for a short debounce window
- if the runtime closes while a battery-backed save is still dirty, `Sekaiemu` flushes the pending `.sav` on shutdown

## Core Settings

`Sekaiemu` now persists libretro core settings as first-class runtime artifacts.

Generated paths:

- per-core config:
  - `save_dir/core-config/<core_name>.cfg`
- per-core schema/metadata:
  - `save_dir/core-config/<core_name>.options.txt`
- optional per-game override:
  - `save_dir/core-config/<core_name>__<game_name>.cfg`

Behavior:

- the runtime captures full option metadata exposed by the core
- defaults are written to the per-core config file
- metadata and valid values are written to the schema file
- recommended runtime overrides are applied where needed
  - example: `mupen64plus-next` gets `gliden64` + `hle`
- config files are loaded automatically at core startup
- applied changes are saved back to disk through the runtime menu
- config files are hot-reloaded through libretro variable updates when the core supports it

This gives us a real `Sekaiemu` equivalent of core settings without copying the `RetroArch` UI model.

## Portability Notes

The runtime no longer depends on an absolute include path into a local `RetroArch` checkout
just to build.

Current portability work:

- vendored minimal `libretro` API header in `extern/libretro/include`
- Linux dependency guide in `README_BUILD_LINUX.md`
- vendored licensing notes in `LICENSES/`

Still intentionally system-provided:

- `SDL2`
- `OpenGL`

## Profile Bridge

When `--profile` is supplied, `Sekaiemu` loads the bridge profile metadata for the active game.

For non-migrated games, the host then enables a tiny local bridge:

- watches the profile's memory range every frame
- accepts local IPC connections over:
  - `save_dir/bridge/sekaiemu.sock`
- emits bridge events over that socket
- accepts incoming bridge commands over that socket

The current profile intentionally covers only a small `EarthBound` starter slice.
That is enough to prove the model:

- host stays generic
- profile owns the offsets/bit mappings
- bridge behavior is driven by data, not game-specific code in the runtime

The old file-based proof transport:

- `save_dir/bridge/inbox/*.cmd`
- `save_dir/bridge/outbox.log`

has now been replaced by a product-facing local IPC layer.

For migrated games currently owned by `SKLMI`:

- `EarthBound`
- `A Link to the Past`

the local `ProfileBridge` is no longer the active bridge path. `Sekaiemu` uses the
profile only to identify the game/bridge mapping, then launches the external
`SKLMI` runtime and leaves check detection and item delivery to it.

Companion runtime resolution:

- explicit CLI path:
  - `--sklmi-runtime <path>`
- explicit manifest directory:
  - `--sklmi-manifest-dir <dir>`
- environment fallback for the runtime:
  - `SEKAILINK_SKLMI_RUNTIME`
- environment fallback for manifests:
  - `SEKAILINK_SKLMI_MANIFEST_DIR`
  - or `SEKAILINK_SKLMI_ROOT/manifests`

Runtime artifacts for migrated bridges are stored under:

- `save_dir/sklmi/<bridge_id>/room.state`
- `save_dir/sklmi/<bridge_id>/runtime/`
- `save_dir/sklmi/<bridge_id>/trace.jsonl`
- `save_dir/sklmi/<bridge_id>/companion.log`

## Runtime Memory Surface

`Sekaiemu` now exposes a generic runtime memory socket intended for `SKLMI`.

Behavior:

- default socket path:
  - `save_dir/runtime/sekaiemu-memory.sock`
- optional CLI override:
  - `--memory-socket <path>`
- transport:
  - newline-delimited Unix socket protocol
- commands currently exposed:
  - `VERSION`
  - `SYSTEM`
  - `DOMAINS`
  - `READ`
  - `WRITE`

Canonical memory domains exposed for integration:

- `system_ram`
- `save_ram`
- `video_ram`

Additional descriptor-backed addrspaces may also be exposed when the loaded core
provides them, but the canonical domains above are the intended stable surface
for early `SKLMI` integration.

This keeps `Sekaiemu` responsible for runtime hosting and memory exposure while
leaving multiworld interpretation to `SKLMI`.

## Runtime Architecture

The host is no longer treated as one giant experimental file.

Current subsystems now include:

- `audio_output.*`
  - SDL audio device ownership and core sample-rate matching
- `video_backend.*`
  - common rendering backend contract
- `software_video_backend.*`
  - validated software-rendered path
- `opengl_video_backend.*`
  - first real hardware-rendered path
- `vulkan_video_backend.*`
  - runtime placeholder and future backend slot
- `input_state.*`
  - keyboard input state and polling
- `runtime_loop.*`
  - SDL event pumping, pause/menu frame loop, and runtime orchestration
- `memory_domain_registry.*`
  - memory-map descriptor ownership and memory-domain resolution
- `libretro_environment.*`
  - separated libretro environment callback negotiation and frontend capability responses
- `libretro_session.*`
  - frontend/session lifecycle, core loading, content loading, and shutdown plumbing
- `profile_bridge.*`
  - profile-driven check watching, command handling, and game-bridge orchestration
- `libretro_core_loader.*`
  - libretro core loading and symbol resolution
- `core_option_manager.*`
  - libretro core setting definitions, persistence, schema export, and hot-reload
- `save_state_manager.*`
  - battery save and savestate persistence for cores
- `bridge_ipc.*`
  - local Unix-socket transport for runtime bridge commands/events
- `bridge_adapters/*`
  - temporary per-game injection adapters kept outside the host
- `launch_*` and `logger.*`
  - CLI/API launch surface, validation, and runtime logging

This is the direction of travel for production `Sekaiemu`:

- host/runtime responsibilities stay in `Sekaiemu`
- game interpretation and multiworld behavior stay out of the host
- rendering/audio/core loading become dedicated subsystems instead of growing the host monolith

## Notes

- This runtime is still in transition from spike to product.
- The software path is validated and must remain a first-class fallback.
- `OpenGL` hardware rendering is now a real part of the runtime architecture.
- `Vulkan` is intentionally present in the structure before its full implementation.
- `SKLMI` remains a separate layer of responsibility.

## Tests

Offline golden tests are now present under:

- `tests/golden/earthbound_golden.sh`
- `tests/golden/alttp_golden.sh`

They currently lock in:

- the confirmed `EarthBound` WRAM check transitions
- the confirmed `ALTTP` injection traces
