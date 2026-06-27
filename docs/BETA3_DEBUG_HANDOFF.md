# SekaiLink BETA-3 Debug Handoff

Date: 2026-06-04

This is the handoff for the next agent working on SekaiLink BETA-3 debug. Read this
before touching code. The main risk is mixing old repos/old runtime artifacts with the
canonical tree. That already happened once and caused tracker layout/runtime regressions.

## Absolute Source Of Truth

Use only:

`/home/thelovenityjade/SekaiLink/canonical`

Current GitHub remote:

`https://github.com/lovenityjade/sekailink-canonical.git`

Current commit at handoff:

Check current git `HEAD` in the canonical repo. This handoff has been updated through
the 2026-06-04 route-query hotfix release.

Current Client Core version:

`0.3.1-prebeta3.20260604.5`

Lobby readiness/profile hotfix on 2026-06-04T18:05Z:

- Root cause: Link Chat API accepted `local_ready_*` presence fields from Client Core but
  did not persist or return them from `chat_presence`; lobby sync generation could see a
  player as "not reported readiness" even after their local setup had reported state.
- Client Core `0.3.1-prebeta3.20260604.5` also keeps the cached identity as the sidebar
  fallback so an installed desktop session does not show generic `Joueur` while authenticated.
- The lobby room now matches the current player against normalized identity aliases
  (`user_id`, Discord id, display/global/username) before deciding whether a member is local
  or remote.
- Deployed Link Chat API on `link.sekailink.com`; live `chat_presence` includes `role`,
  `ready`, `local_ready_known`, `local_ready`, and `local_ready_note`.
- Pushed `.5` Linux/Windows update bundles and full installers to the public CDN under
  `/var/www/sekailink.com/public/downloads/client/prebeta3/20260604`.
- Updated `/opt/sekailink/link/chat-api/config/client_release_latest.json` and restarted
  `sekailink-chat-api.service`; public `release-latest` for linux-x64 and win32-x64
  returns `0.3.1-prebeta3.20260604.5`.

Route-query login hotfix on 2026-06-04T10:28Z:

- Root cause: installed Electron clients could append a stale desktop token as
  `?token=...`; Nexus Identity compared `/me` and `/login` before stripping query
  strings, so `/me?token=...` and `/login?token=...` returned `route_not_found`.
- Client Core `0.3.1-prebeta3.20260604.4` removes the URL token fallback and uses the
  `Authorization` header only.
- Nexus Identity now normalizes query strings before early auth route checks.
- Local `sekailink_identity_service_smoke` passed with explicit `/me?token=...` and
  `/login?token=...` regression checks.
- Deployed Nexus service on `nexus.sekailink.com`; public checks return `401` for
  `/api/identity/me?token=...` and `/api/identity/login?token=...`, not 404.
- Pushed `.4` Linux/Windows update bundles and full installers to the public CDN under
  `/var/www/sekailink.com/public/downloads/client/prebeta3/20260604`.
- Updated `/opt/sekailink/link/chat-api/config/client_release_latest.json` and restarted
  `sekailink-chat-api.service`; public `release-latest` for linux-x64 and win32-x64
  returns `0.3.1-prebeta3.20260604.4`.
- Bootstrapper archives did not need republishing; they query `release-latest` and will
  now install/update to `.4`.

Post-handoff CDN update on 2026-06-04T09:21Z:

- Pushed `0.3.1-prebeta3.20260604.3` to the public CDN under
  `/var/www/sekailink.com/public/downloads/client/prebeta3/20260604`.
- Updated the active Chat API release manifest at
  `/opt/sekailink/link/chat-api/config/client_release_latest.json`.
- Restarted `sekailink-chat-api.service`; public checks for linux-x64 and win32-x64
  returned version `0.3.1-prebeta3.20260604.3`.

Bootstrapper MVP on 2026-06-04T09:45Z:

- Published Windows/Linux bootstrapper archives under
  `/var/www/sekailink.com/public/downloads/client/bootstrapper/20260604`.
- Stable public aliases:
  - `https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-windows.zip`
  - `https://sekailink.com/downloads/client/bootstrapper/latest/SekaiLink-bootstrapper-linux.tar.gz`
- Linux bootstrapper was tested end-to-end in `/tmp` with `--no-launch`; it downloaded
  the `linux-x64` client bundle, verified SHA-256, extracted, wrote install-state, and
  a second dry-run reported `needsUpdate=0`.
- Windows dry-run still needs validation from the Windows agent or an interactive SSH
  session; non-interactive SSH to the Windows box was denied.

Forbidden as active sources:

- `/home/thelovenityjade/Projects/Sekaiemu-Libretro-Spike-Codex`
- `/home/thelovenityjade/DevSSD/sekailink-beta-3-final`
- `/home/thelovenityjade/SekaiLinkDev`
- `/home/thelovenityjade/DevSSD/_sekailink_quarantine`
- `/home/thelovenityjade/DevSSD/sekailink-legacy-quarantine-2026-05-17`

Those paths may be used only for historical comparison. Do not build, launch, package, or
copy runtime artifacts from them unless Jade explicitly asks for a recovery comparison.

Canonical docs already present:

- `docs/SOURCE_OF_TRUTH.md`
- `docs/NO_LEGACY_POLICY.md`
- `docs/RUNTIME_BOUNDARY.md`
- `docs/BACKUP_INDEX.md`

## Repository Map

Important canonical paths:

- `apps/client-core`: Electron/Vite SekaiLink Client Core.
- `apps/sekaiemu`: libretro/OpenGL emulator and tracker renderer.
- `services/sklmi`: local companion tracker/runtime process.
- `services/nexus`: identity/user/profile/database-backed game config service source.
- `services/link-social`: chat/lobby/API layer source.
- `services/link-room`: room runtime surface source.
- `services/worlds`: generation service source.
- `services/evolution`: distribution/update/assets service source.
- `linkedworlds/alttp`: ALTTP showcase LinkedWorld assets and tracker bundle source.
- `runtime`: packaged local runtime used by Client Core releases.

Do not confuse `runtime/` with `apps/runtime/`. `apps/runtime/` was an old accidental
packaging target and must not be used. Packaging was fixed so `electron-builder.yml` and
`scripts/prepare-native-deps.mjs` use repo-root `runtime/`.

## Runtime Boundary

BETA-3 architecture is:

- Client Core is the user-facing launcher/controller.
- Sekaiemu is visual/emulation only.
- SKLMI owns tracker logic.
- Servers provide identity, lobbies, generation, rooms, distribution, and assets.

Sekaiemu must not calculate tracker logic during emulation. It must not run PopTracker Lua,
`RunFrameHandlers`, `CanReach`, `SnapshotObjects`, or pack logic in normal BETA-3 runtime.
Sekaiemu may render only draw-ready snapshots and send rare user commands.

SKLMI owns:

- PopTracker-compatible pack load.
- Lua/pack behavior.
- AP/live room events.
- items, stages, toggles, counters.
- pins, maps, accessibility.
- callbacks and commands.
- `tracker.snapshot.json` publication.

Sekaiemu owns:

- libretro core execution.
- audio, video, input.
- memory exposure to SKLMI.
- reading `tracker.snapshot.json` only when `mtime/size` changes.
- rendering tracker snapshots with cached assets/textures.
- user interactions as commands to SKLMI.

Worst acceptable behavior: tracker is late.

Never acceptable: audio/emulation waits on tracker work.

## Client Core Flow

At startup:

1. `apps/client-core/src/components/AuthGate.tsx` loads runtime env.
2. API default is now `https://sekailink.com` in `apps/client-core/src/services/api.ts`.
3. It checks updates via:
   - `/api/client/version`
   - `/api/client/release-latest`
4. It checks auth via `/api/identity/me`.
5. It loads lobbies/friends from Nexus/Link.

User flow:

1. User logs in with SekaiLink account.
2. User enters/creates a lobby.
3. User adds seed configs.
4. Seed config data lives in Nexus/database, not in user YAML files.
5. When generation starts, server converts DB config into generator-compatible YAML.
6. All player YAMLs for a sync are zipped together.
7. Worlds runs the background generator and returns output to Link.
8. Link hosts/provides the generated room package.
9. Client Core downloads the generated package.
10. Client Core materializes the patched ROM from the user's configured base ROM.
11. Client Core resolves/downloads tracker pack if needed.
12. Client Core launches Sekaiemu with runtime paths, room info, tracker pack, snapshot path,
    command log path, and live server connection details.

Important naming rule:

- Under the hood, SekaiLink uses `user_id`.
- User-facing chat/logs should show username.
- The old "slot" concept should not be exposed as the main user identity.
- Generated backend slot names may be compact because of generator slot-name limits
  (example: `thelov-alin-c061`), but UI/chat should say `thelovenityjade`.
- If one user has multiple seeds of the same game in a sync, distinguish them with
  game name and increment (`{1}`, `{2}`, etc.) in user-facing item logs.

Expected human item log format:

`Jade (A Link to the Past {1}) sends Boomerang to Raifu (Twilight Princess)`

For ordinary chat messages, show only username, not username plus game. Game context is for
item transfer/session messages.

The new redesign is the source of truth. If the legacy client has a feature not present in
the redesign, treat it as nonexistent unless Jade asks to restore it.

There is no global chat in the current redesign.

## Sekaiemu

Canonical source:

`apps/sekaiemu`

Packaged binary:

`runtime/bin/sekaiemu_libretro_spike`

Key areas:

- `apps/sekaiemu/src/libretro_host_runloop.cpp`: main emulation loop.
- `apps/sekaiemu/src/audio_output.cpp`: audio output and underrun-sensitive work.
- `apps/sekaiemu/src/opengl_video_backend.cpp`: hardware video backend.
- `apps/sekaiemu/src/opengl_overlay_renderer.cpp`: overlay path.
- `apps/sekaiemu/src/tracker_snapshot_io.cpp`: snapshot reader.
- `apps/sekaiemu/src/tracker_asset_resolver.cpp`: asset lookup.
- `apps/sekaiemu/src/tracker_raster_image.cpp`: image decode.
- `apps/sekaiemu/src/tracker_pack_layout_renderer.cpp`: tracker layout renderer.
- `apps/sekaiemu/src/tracker_overlay_renderer.cpp`: tracker renderer.
- `apps/sekaiemu/src/tracker_map_context_menu.cpp`: map tab/context menu.
- `apps/sekaiemu/src/tracker_pin_context_menu.cpp`: pin/check context menu.
- `apps/sekaiemu/src/libretro_host_tracker_interactions.cpp`: click/interaction bridge.
- `apps/sekaiemu/src/runtime_chat_overlay.cpp`: in-emulator chat overlay.
- `apps/sekaiemu/src/runtime_menu*`: ESC menu/settings/save slots/controls/sync info.
- `apps/sekaiemu/src/sklmi_companion_runtime.cpp`: launch/monitor SKLMI.

Modes that must remain:

- side-by-side tracker.
- separate tracker window.
- toggle screen tracker.

Rendering requirements:

- Use hardware/OpenGL.
- Pixel art should be crisp/point filtered.
- No software-render fallback as default for BETA-3.
- Tracker assets/textures must be cached.
- No PNG/image decode per frame.
- No JSON parse per frame; read snapshot only on `mtime/size` changes.

Current tracker UI expectation:

- Large map and item grid only.
- No old "Summary", "Details", "Recent", "Live", "Settings" panels in the visible tracker.
- Right-click/context map menu should list only tabs/maps with real map images/pins.
- Automapping should change map based on current game location, not only when a check event fires.
- Pins support colors/accessibility and hover names.
- Pin click/check interactions and item clicks should go through SKLMI commands.

Recent visual/debug notes:

- A previous boot-screen animation from `/home/Projects/sekaiemu-bios-boot` was tried and removed.
  It ran at about 1 FPS without audio, then too fast after hardware attempts. Current state should
  be pre-boot-screen integration.
- F1 should pause/show shortcut keys.
- There should be a fullscreen shortcut.
- Debug HTML console should not be shown in release builds.
- Battery/load-at-boot was fixed shortly before release; if saves do not persist, inspect
  patched ROM identity, save path derivation, and generated package naming.

## SKLMI

Canonical source:

`services/sklmi`

Packaged binary:

`runtime/bin/sekailink_sklmi_runtime`

Current hotfix runtime SHA256:

`5ba0a44827cba76d21ed07adb0f12c5b4358c3b77e08ff8821f8c8e7cb17f4c5`

Key files:

- `services/sklmi/src/runtime_main.cpp`: runtime entry/options wiring.
- `services/sklmi/src/runtime_options.cpp`: CLI options.
- `services/sklmi/src/api_archipelago.cpp`: live room/AP-style client transport.
- `services/sklmi/src/api_bridge_runtime.cpp`: room-synchronized runtime session.
- `services/sklmi/src/api_room.cpp`: room client abstractions.
- `services/sklmi/src/api_manifest.cpp`: bridge manifest loading.
- `services/sklmi/src/tracker_headless_runtime.cpp`: tracker runtime core.
- `services/sklmi/src/tracker_headless_runtime_*.inc`: tracker model/state/snapshot/commands.
- `services/sklmi/src/tracker_poptracker_eval.lua`: PopTracker compatibility evaluator.
- `services/sklmi/src/tracker_poptracker_eval_parts/*.lua`: split Lua runtime pieces.

Important CLI/options:

- `--tracker-pack <path>`
- `--tracker-variant <name>`
- `--tracker-snapshot <path>`
- `--tracker-command-log <path>`
- `--tracker-assets-root <path>`

Channel contract:

- SKLMI writes `tracker.snapshot.json` atomically.
- Sekaiemu reads only when `mtime/size` changes.
- User commands go into `tracker.commands.jsonl`.
- JSONL is for rare commands, not full state every frame.

Recent critical bug fixed:

"Checks do not pass to server" was caused by stale local SKLMI state. Local files said a
location was already reported, while the live room server said `checked_locations=[]`.

Fix in commit `99ae88b`:

- `api_archipelago.cpp` now captures `checked_locations` from `Connected`/room updates.
- `api_bridge_runtime.cpp` reconciles local `reported_checks_` with server metadata.
- In room-synchronized runtime, live room `checked_locations` is authoritative.
- A local stale `reported|...` no longer suppresses re-reporting if the server does not know
  that check.
- Room-synchronized sessions no longer use old bridge state as authority for check reporting.

Tests added/updated:

- `services/sklmi/tests/sklmi_archipelago_room_client_smoke_main.cpp`
- `services/sklmi/tests/sklmi_room_sync_smoke_main.cpp`

Passed before release:

- SKLMI targeted tests: 2/2.
- Full SKLMI tests: 15/15.
- Runtime runner smoke with packaged runtime: `sklmi_alttp_runtime_runner_smoke_ok`.

Build/test commands used:

```bash
cmake -S services/sklmi -B /tmp/sekailink-sklmi-checks-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sekailink-sklmi-checks-build -j4
ctest --test-dir /tmp/sekailink-sklmi-checks-build --output-on-failure
/tmp/sekailink-sklmi-checks-build/sekailink_sklmi_alttp_runtime_runner_smoke runtime/bin/sekailink_sklmi_runtime
```

## PopTracker Packs

Directive:

Do not hardcode ALTTP into SKLMI. ALTTP is the showcase/validation driver only.

Pack truth for ALTTP showcase:

`linkedworlds/alttp/tracker/default.bundle`

Important files inside it:

- `manifest.json`
- `map-pin-metadata.json`
- `autotabbing-hints.json`
- `item-icon-metadata.json`
- `item-slots.complete.json`
- `location-groups.complete.json`
- `dungeon-progress.complete.json`
- `settings-metadata.json`
- `slot-data.complete.json`
- `room-metadata.complete.json`
- `surface.complete.json`
- `tracker-flow.v1.json`
- `maps/*.ppm`
- `poptracker-adapted/`

Runtime generated/cached bundles:

- `runtime/tracker-bundles/alttp-default`
- `runtime/tracker-bundles/alttp-linkedworld-default`
- `runtime/tracker-bundles/alttp-native`

Rule:

If a zip/archive/bundle disagrees with `linkedworlds/alttp/tracker/default.bundle`, rebuild from
the directory bundle. The directory bundle is the source, generated archives are artifacts.

SKLMI supports directory packs and archive packs through libarchive. Archives may be extracted
near runtime/snapshot state. Asset roots should resolve through:

1. explicit `--tracker-assets-root`
2. snapshot `assets_root`
3. snapshot `status.assets_root`
4. `--tracker-pack` directory
5. legacy fallback bundle only if necessary

Compatibility goal:

SekaiLink must understand PopTracker-compatible packs as packs, not as ad hoc ALTTP data. If
logic/pins/items are wrong, study PopTracker behavior and reproduce the necessary compatibility
semantics in SKLMI. Avoid manual one-off fixes that only make one ALTTP screen look right.

Known PopTracker-related history:

- Several pin placement/logical errors came from trying to infer the pack instead of honoring
  pack metadata/runtime behavior.
- Tooltips temporarily caused lag and were removed/refined; current minimum hover behavior is
  check name.
- Grouped pins can represent multiple checks and need partial/mixed color state.
- Entrance shuffle pins should not be mixed into normal maps for BETA-3; entrance shuffle is
  not supported until later versions.
- Dungeon items, keys, maps, compasses, big keys, prizes, and crystals must come from pack/room
  data, not hardcoded ALTTP tables in SKLMI.

## Server Roles

SSH aliases are in `/home/thelovenityjade/.ssh/config`.

Do not write secrets into docs/commits. Do not reset databases. Ask Jade before destructive or
schema-risk server changes unless she has explicitly authorized that exact deploy.

Servers:

- `nexus-vps` / `nexus-root-vps`: Nexus identity, users, profiles, database-backed game/seed
  config, seed preset/admin services.
- `link-vps` / `link-root-vps`: Link public API, chat/lobby, room server, webhost, CDN path for
  current client downloads, public `sekailink.com` API.
- `worlds-vps` / `worlds-root-vps`: Worlds generation service. Uses the background generator for
  BETA-3; do not reinvent the generator for this release.
- `evolution-vps` / `evolution-root-vps`: Evolution distribution/assets/update role, including
  shared tracker pack hosting direction.
- `pulse-vps`: Pulse assistant/config helper. Pulse was reported down during late testing; do not
  block ALTTP live launch on Pulse, but investigate if Easy config needs it.

Public endpoints validated at release hotfix:

- `https://sekailink.com/api/client/version?channel=test&platform=linux-x64`
- `https://sekailink.com/api/client/release-latest?channel=test&platform=linux-x64`
- `https://sekailink.com/api/client/release-latest?channel=test&platform=win32-x64`

Live room host seen in SKLMI traces:

- `link.sekailink.com`
- room ports are generated dynamically, examples seen: `38290`, `38295`

Do not present the background generator/client branding in user-facing UI. User-facing language
should be SekaiLink/Sync/Room terminology.

## Release/Update Deployment

Current live version:

`0.3.1-prebeta3.20260604.5`

CDN directory on Link:

`/var/www/sekailink.com/public/downloads/client/prebeta3/20260604`

Release manifest consumed by API:

`/opt/sekailink/link/chat-api/config/client_release_latest.json`

Public copy in CDN dir:

`/var/www/sekailink.com/public/downloads/client/prebeta3/20260604/sekailink-client-release-20260604.json`

Service to restart after manifest replacement:

`sekailink-chat-api.service`

Current live hotfix artifacts:

- `SekaiLink-client-0.3.1-prebeta3.20260604.5-linux-x64.zip`
  - SHA256: `21f90ea260f92ed47cef5b5c8ddd96dafd5014b8cb9443b875b0e55fddd65b0d`
- `SekaiLink-client-0.3.1-prebeta3.20260604.5-win-x64.zip`
  - SHA256: `eda622aec83b3535f19483363a58d8a52be76627d3e9f563b1e7c1045201de7a`
- `SekaiLink-client-0.3.1-prebeta3.20260604.5.AppImage`
  - SHA256: `f356eb06cdcc68d94da6703f7e61ef867a15d2ea4b03e15a2ee371e80983aaa8`
- `SekaiLink-client-0.3.1-prebeta3.20260604.5.exe`
  - SHA256: `17ad1479e4d7d8cb164c8d2a713688d10e965170751f0fffcca70ce48c37734f`

Packaging commands:

```bash
cd /home/thelovenityjade/SekaiLink/canonical/apps/client-core
npm run electron:pack
npm run electron:pack:win
SEKAILINK_RELEASE_VERSION=0.3.1-prebeta3.20260604.5 \
SEKAILINK_RELEASE_CHANNEL=test \
SEKAILINK_RELEASE_BUILD=release \
SEKAILINK_RELEASE_DATE=20260604 \
npm run electron:pack:update-bundles
```

Important packaging fix:

- `apps/client-core/electron-builder.yml` must use `../../runtime`.
- `apps/client-core/scripts/prepare-native-deps.mjs` must use repo-root `runtime`.
- Packaged `release/*/resources/runtime` should be about the same size as repo `runtime`
  (188 MB at handoff), not 11 MB.
- Verify `release/*/resources/runtime/bin/sekailink_sklmi_runtime` hash matches repo runtime.
- Verify `release/*/resources/runtime/ap/Patch.py` exists.
- Verify `release/*/resources/runtime/cores/bsnes_mercury_performance_libretro.so` exists.

Expected harmless build warnings at handoff:

- Vite large chunk warning.
- `stage-ap-runtime` warns if `SEKAILINK_AP_SOURCE_DIR` is missing, but `runtime/ap` already
  exists in canonical. This is not a blocker unless `runtime/ap` is missing/incomplete.
- `third_party` BizHawk/Mono/PopTracker paths are missing in canonical and electron-builder logs
  warnings. Current ALTTP showcase uses packaged `runtime`, not those `third_party` folders.
  If a future feature needs them, fix deliberately, not by copying from legacy paths silently.
- Default Electron icon warning may appear.

## Debug Logs And State Paths

Client logs:

`/home/thelovenityjade/.config/sekailink-client/logs/`

Sekaiemu logs:

`/home/thelovenityjade/.config/sekailink-client/logs/sekaiemu/sekaiemu.log`

Common SKLMI runtime state for ALTTP:

`/home/thelovenityjade/.config/sekailink-client/sekaiemu/alttp/saves/sklmi/alttp-phase1/`

Important files in that tree:

- `trace.jsonl`
- `runtime/tracker.logic.log`
- `runtime/tracker.snapshot.json`
- `runtime/tracker.commands.jsonl`
- `runtime/alttp.phase1.bridge.state`
- `runtime/alttp_phase1.room-sync.state`
- `room.state`

When debugging check reporting:

1. Inspect `trace.jsonl` for `location_checked`.
2. Inspect room metadata for `checked_locations`.
3. Inspect `alttp_phase1.room-sync.state` for `reported|...`.
4. If server `checked_locations=[]` but local state says `reported|...`, the hotfix should now
   reconcile and re-report. If it does not, check `api_bridge_runtime.cpp`.
5. Check live chat/item flow for self-origin suppression. A self-origin item may be suppressed
   visually but the check still must reach the room server.

Example trace fields from the fixed flow:

- `room_client_ready`
- `room_metadata_ready`
- `checked_locations`
- `location_checked`
- `room_item_pending`
- `room_item_self_origin_suppressed`
- `item_received`

## Important User-Facing Design State

Client Core redesign is the source of truth:

- Sidebar: Home, Library, Lobbies, Settings.
- No global chat.
- Friends/notifications must come from Nexus, not placeholders.
- Lobby page must show real players/configs, lobby chat, lobby status, generate/launch.
- Add seed config:
  - Easy uses Pulse-style guided questions.
  - Advanced opens APWorld-derived form.
- Library stores/manages seed configs per game.
- ALTTP is showcase for Pre-BETA3.

Known UI preferences from Jade:

- The redesign should be copied 1:1 for layout, CSS, buttons, modals, icons, font, spacing.
- Do not "improve" colors/styles unless asked.
- Use modals for long operations:
  - Joining Lobby.
  - Saving to server.
  - Pulse is thinking.
  - Generation progress.
  - Errors with clear message and code.
- If generation completes, Launch should be very obvious in the success modal.
- If a generated sync already exists, do not regenerate it.
- If generation requires a local ROM and the user's ROM is missing, generation should fail early
  with a clear system chat/modal reason.

## Current Feature/Behavior Status

Working at handoff:

- Live login/auth with `https://sekailink.com`.
- Live lobbies load.
- Generated ALTTP package launches Sekaiemu.
- Tracker pack loads.
- Pins display.
- Automapping was reported working after fixes.
- Check reporting to server was visually confirmed by Jade before rebuild.
- Public update `.2` is live.
- GitHub canonical is pushed.

Potential follow-up/debug items:

- Pulse was reported down before release build; verify Easy config path if it matters for public
  testers.
- Ensure installed clients successfully auto-update from `.1` to `.2` and relaunch.
- Verify Windows package on a Windows machine, especially patching and packaged Python/AP runtime.
- Verify Linux AppImage fresh install path, not only dev launch.
- Some warnings remain around absent `third_party` packaged resources. Do not ignore if testing a
  path that depends on BizHawk/Mono/PopTracker external binaries.
- User-facing logs should be SekaiLinkified: avoid raw compact slot names and old client names.
- If chat messages show game names for ordinary chat, correct to username only.
- If item messages show raw IDs/slot names, map to username/game/inc in Client Core or SKLMI
  metadata.
- Check item/pin click latency. It was acceptable when routed through SKLMI, but watch for
  multi-second stalls.
- Watch for tracker clicks impacting emulation FPS/audio; never block audio on tracker commands.
- Verify bottle/boomerang/bow/arrow/key initial states against actual seed slot data before
  assuming a renderer bug.
- Dungeon item row alignment was repeatedly mentioned; verify from TH downward.
- Manual dungeon prize markers should be clickable eventually, but avoid spoilers unless a game
  setting enables pre-filled prizes.

## Build And Test Commands

SKLMI:

```bash
cmake -S services/sklmi -B /tmp/sekailink-sklmi-checks-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sekailink-sklmi-checks-build -j4
ctest --test-dir /tmp/sekailink-sklmi-checks-build --output-on-failure
```

Client Core:

```bash
cd apps/client-core
npm run build
npm run electron:dev
npm run electron:pack
npm run electron:pack:win
npm run electron:pack:update-bundles
```

Sekaiemu visual/e2e:

Use the canonical test scripts under:

`apps/sekaiemu/tests/e2e/alttp_chest/`

The exact manual script historically used:

`apps/sekaiemu/tests/e2e/alttp_chest/run_alttp_manual_check_session.sh`

Do not use the old script path under `/home/thelovenityjade/Projects/...`.

## Server Deploy Safety

General rule:

- Read first.
- Backup before replacing config.
- Prefer atomic `install`/rename.
- Restart only the service needed.
- Verify `systemctl is-active`.
- Verify public endpoint with `curl`.
- Never reset databases unless Jade explicitly asks.

For release manifest on Link:

1. Upload artifacts to:

   `/var/www/sekailink.com/public/downloads/client/prebeta3/YYYYMMDD`

2. Backup:

   `/opt/sekailink/link/chat-api/config/client_release_latest.json`

3. Install new manifest:

   `/opt/sekailink/link/chat-api/config/client_release_latest.json`

4. Restart:

   `systemctl restart sekailink-chat-api.service`

5. Verify:

```bash
curl -fsS 'https://sekailink.com/api/client/release-latest?channel=test&platform=linux-x64'
curl -fsS 'https://sekailink.com/api/client/release-latest?channel=test&platform=win32-x64'
curl -fsS 'https://sekailink.com/api/client/version?channel=test&platform=linux-x64'
```

## Do Not Regress These Decisions

- Canonical repo is the only active repo.
- Do not use legacy/quarantine active code.
- Client Core uses live SekaiLink services.
- Nexus/database-backed settings are source of truth; user YAML files are generated as an output
  for the background generator.
- Worlds uses the background generator for BETA-3. Do not rebuild a native generator right now.
- Do not hardcode ALTTP in SKLMI.
- Do not move PopTracker logic back into Sekaiemu.
- Do not parse tracker JSON every frame.
- Do not decode images every frame.
- Do not let tracker rendering/commands stall audio.
- Keep side-by-side, separate window, and toggle modes.
- Keep hardware/OpenGL rendering.
- Keep PopTracker pack compatibility as a first-class goal.
- User-facing identity is username; under the hood is user_id.
- Do not expose old slot-name weirdness except where required by generator protocol.

## Practical First Steps For Next Agent

1. Start in `/home/thelovenityjade/SekaiLink/canonical`.
2. Run `git status --short`. If dirty, understand changes before touching anything.
3. Read:
   - `docs/SOURCE_OF_TRUTH.md`
   - `docs/NO_LEGACY_POLICY.md`
   - this file.
4. Check current release:

```bash
curl -fsS 'https://sekailink.com/api/client/version?channel=test&platform=linux-x64'
```

5. If debugging runtime, launch from Client Core, not old scripts, unless doing isolated Sekaiemu
   reproduction.
6. If debugging tracker logic, inspect SKLMI trace/snapshot first. Sekaiemu should be a renderer.
7. If fixing server behavior, identify which server owns it:
   - identity/config: Nexus.
   - lobby/chat/API/release latest: Link.
   - generation: Worlds.
   - distribution/assets/tracker pack hosting: Evolution.
   - easy assistant/config helper: Pulse.
8. After fixes, run focused tests, then the relevant e2e visual test with Jade.

End of handoff.
