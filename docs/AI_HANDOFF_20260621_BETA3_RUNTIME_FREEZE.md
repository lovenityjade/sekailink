# SekaiLink BETA-3 Runtime Handoff - 2026-06-21

This repository is currently in a fragile state. Do not start by adding features.
Start by stabilizing the runtime path and proving one game family at a time.

## Context

SekaiLink BETA-3 is moving toward a compatibility-first design:

- Client Core is the lobby/config/launch UX.
- Bootloader is frozen and considered functional.
- Sekaiemu remains the emulator shell/runtime.
- PopTracker is used as-is for BETA-3 through a SekaiLink Edition/forked UI.
- SKLMI should wrap or bridge Archipelago clients for compatibility first.
- Native generator/client replacement is deferred until a later beta.

The immediate goal before this freeze was to make generic runtime launching work
for NES, SNES, GB/GBC, and GBA through Client Core -> runtime module -> wrapper
client -> AP room server -> Sekaiemu memory bridge.

## Current Emergency State

User reported after the latest changes:

- "tout est brise"
- GBA now reports an error in `CommonClient`
- credits/time are nearly exhausted
- project should be frozen and handed off to another AI agent

I stopped active development and killed the running Client Core dev process.

Last running process group before freeze:

- `npm run electron:dev`
- Vite
- Electron Client Core

They were killed at handoff time.

## Important: Do Not Trust Current Green Checks As Product Health

The following checks passed, but only prove syntax/build smoke:

```bash
node --check apps/client-core/electron/main.cjs
npm --prefix apps/client-core run build
./apps/sekaiemu/build-codex-imgui/launch_options_smoke
```

They do not prove runtime compatibility. Manual launch testing found regressions.

## Latest Confirmed Good/Bad Signals

### Confirmed Good Before Freeze

- ALttP/SNES communication started working again after the raw PopTracker pack
  was no longer used as the SKLMI companion pack.
- User confirmed: "ok la communication fonctionne maintenant."

### Confirmed Bad Before Freeze

- After subsequent changes/checkup, user reported everything broken again.
- GBA reportedly now hits a `CommonClient` error.
- Logs also showed noisy GBA DMA output and runtime exits, but not enough was
  captured to identify the exact `CommonClient` traceback.
- Earlier GBA had been working both directions for at least one test before this
  regression, so treat this as a regression, not an unimplemented feature.

## Critical Design Boundary Found Today

Do not pass a raw PopTracker pack to SKLMI as `--tracker-pack`.

SKLMI expects an adapted SekaiLink tracker bundle that contains files such as:

- `item-slots.complete.json`
- `tracker-flow.v1.json`
- `poptracker-adapted/sekailink-adaptation.json`

Raw PopTracker packs do not contain these files. Passing a raw pack caused:

```text
tracker_pack_missing
tracker_init_failed:tracker_file_open_failed:.../item-slots.complete.json
```

The only adapted bundle currently known locally is:

```text
runtime/tracker-bundles/alttp-linkedworld-default.zip
```

Copies also exist under:

```text
~/.config/sekailink-client/runtime/tracker-bundles/alttp-linkedworld-default.zip
~/.config/sekailink-client/runtime/overlay/runtime/tracker-bundles/alttp-linkedworld-default.zip
```

## Latest Runtime Change That May Need Review

`apps/client-core/electron/main.cjs`

Function:

```js
resolveSekaiemuTrackerPackPath(manifest, roots, options = {})
```

was changed so Sekaiemu/SKLMI launch paths call it with:

```js
{ allowInstalledFallback: false }
```

Intent:

- PopTracker external may use installed raw PopTracker packs.
- SKLMI companion must not receive raw PopTracker packs as fallback.
- SKLMI should receive only adapted bundles or explicit SKLMI tracker paths.

This fixed ALttP but may have side effects if another game relied on a raw pack
being passed into SKLMI. If a game lacks an adapted SKLMI bundle, SKLMI may run
without tracker metadata or fail differently. Investigate per log.

## Files Most Likely Involved In Current Regression

Client Core launch/wrapper path:

- `apps/client-core/electron/main.cjs`
- `apps/client-core/src/services/sessionLaunch.ts`
- `apps/client-core/src/services/roomSessionLaunch.ts`
- `apps/client-core/src/redesign/components/LobbyRoomPage.tsx`
- `apps/client-core/src/services/runtime.ts`

Sekaiemu launch/options:

- `apps/sekaiemu/src/launch_options.cpp`
- `apps/sekaiemu/src/launch_options.hpp`
- `apps/sekaiemu/src/libretro_tracker_host_state.cpp`
- `apps/sekaiemu/src/sklmi_companion_runtime.cpp`
- `runtime/bin/sekaiemu_libretro_spike`
- `runtime/platforms/linux-x64/bin/sekaiemu_libretro_spike`

Archipelago runtime/client wrappers:

- `runtime/commonclient_wrapper.py`
- `runtime/bizhawkclient_wrapper.py`
- `runtime/sniclient_wrapper.py`
- `runtime/moduleclient_wrapper.py`
- `runtime/apclient_common.py`
- `runtime/ap/CommonClient.py`
- `runtime/ap/NetUtils.py`
- `runtime/ap/worlds/_bizhawk/context.py`

Game-specific runtime manifests:

- `runtime/modules/alttp/manifest.json`
- `runtime/modules/metroid_zero_mission/manifest.json`
- `runtime/modules/metroid_fusion/manifest.json`
- `runtime/modules/super_mario_land_2/manifest.json`
- `runtime/modules/the_legend_of_zelda/manifest.json`
- `runtime/modules/mega_man_2/manifest.json`

SKLMI manifests:

- `runtime/sklmi/manifests/alttp.phase1.json`
- `runtime/sklmi/manifests/tloz.phase1.json`
- `runtime/sklmi/manifests/ladx.phase1.json`
- `runtime/sklmi/manifests/firered.phase1.json`

## Useful Log Paths

Client Core / Electron logs:

```text
~/.config/sekailink-client/logs/
```

Sekaiemu logs:

```text
~/.config/sekailink-client/logs/sekaiemu/sekaiemu.log
```

Per-game save/runtime state:

```text
~/.config/sekailink-client/sekaiemu/<game_id>/saves/
```

SKLMI companion logs usually live under:

```text
~/.config/sekailink-client/sekaiemu/<game_id>/saves/<seed-hash>/sklmi/<bridge_id>/companion.log
~/.config/sekailink-client/sekaiemu/<game_id>/saves/<seed-hash>/sklmi/<bridge_id>/trace.jsonl
```

## Suggested First Steps For Next Agent

1. Do not modify code first.
2. Run one clean launch for a previously working GBA game.
3. Capture the exact `CommonClient` traceback.
4. Determine whether the failure is in:
   - AP protocol version mismatch,
   - wrapper startup arguments,
   - missing/changed `CommonClient` import,
   - game module name mismatch,
   - patcher output mismatch,
   - memory bridge connection.
5. Only then patch.

Recommended commands:

```bash
cd /home/thelovenityjade/SekaiLink/canonical
git status --short
tail -200 ~/.config/sekailink-client/logs/sekaiemu/sekaiemu.log
find ~/.config/sekailink-client/sekaiemu -path '*companion.log' -printf '%T@ %p\n' | sort -nr | head -10
find ~/.config/sekailink-client/sekaiemu -path '*trace.jsonl' -printf '%T@ %p\n' | sort -nr | head -10
```

To start Client Core dev:

```bash
cd /home/thelovenityjade/SekaiLink/canonical/apps/client-core
npm run electron:dev
```

## Compatibility Notes From User Testing

Treat these as test reports, not final truth:

- NES worked in tests: The Legend of Zelda, Mega Man 2.
- SNES/ALttP worked again after the SKLMI tracker-pack correction.
- GBA had worked in both directions for at least one test, then regressed.
- GB/SML2 had a difficult debug path but eventually appeared to work after core
  and bridge investigation.
- LADX is special and should not be treated as generic GB/BizHawk.
- N64 and GameCube are not ready for BETA-3 runtime validation.

## Current Architectural Decisions To Preserve

- Do not re-enable Sekaiemu internal tracker for BETA-3.
- Use PopTracker external / SekaiLink Edition for tracker UI.
- Use wrappers for Archipelago clients for BETA-3 compatibility.
- Do not attempt native generation/client replacement now.
- Do not do game-by-game hacks unless a world truly has a non-generic client.
- Prefer fixing generic wrappers and bridge contracts.

## Warning About Worktree

The worktree is very dirty and contains many unrelated changes from many days of
work. Do not revert files casually. Do not run `git reset --hard`.

Before any patch, inspect targeted diffs:

```bash
git diff -- <file>
```

## Last Human Instruction

Freeze the work here and hand off to another AI agent.

