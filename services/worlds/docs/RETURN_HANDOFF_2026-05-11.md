# Return Handoff - 2026-05-11

Audience: Jade and future Codex agents returning after a 2-3 day pause.

Read this first before continuing Worlds/generator work.

## 2026-05-13 Refactor Update

The generic generator refactor was pushed significantly further without changing
the public API or adding game-specific C++ branches.

Current line split:

- `src/generic_generation.cpp`: `2218` lines
- `src/generic_generation_internal.cpp`: `411` lines
- `src/generic_generation_checks.cpp`: `108` lines
- `src/generic_generation_logic_rules.cpp`: `359` lines
- `src/generic_generation_surface_contracts.cpp`: `337` lines
- `src/generic_generation_region_graph.cpp`: `198` lines
- `src/generic_generation_region_analysis.cpp`: `240` lines
- `src/generic_generation_placement.cpp`: `395` lines
- `src/generic_generation_patch_contracts.cpp`: `239` lines

New extracted modules:

- `include/sekailink_server/generic_generation_checks.hpp`
- `include/sekailink_server/generic_generation_logic_rules.hpp`
- `include/sekailink_server/generic_generation_surface_contracts.hpp`
- `include/sekailink_server/generic_generation_region_graph.hpp`
- `include/sekailink_server/generic_generation_region_analysis.hpp`
- `include/sekailink_server/generic_generation_placement.hpp`
- `include/sekailink_server/generic_generation_patch_contracts.hpp`
- matching `.cpp` files under `src/`

The remaining `generic_generation.cpp` is still not tiny, but it is now mostly
the facade/orchestrator plus native-probe execution, readiness assembly,
placement/replay orchestration, and package writing.

Do not re-merge extracted logic into the facade.

## Current Mission

We are no longer trying to ship another prototype. The immediate production
goal is to make Worlds a clean, generic, multiworld generator that is driven by
LinkedWorld data, not by hardcoded game logic.

ALTTP is the first validation target, but ALTTP must remain data in
`sekailink-linkedworld-alttp`. Worlds must not grow ALTTP-specific C++ branches.

## Canonical Direction

- Worlds generates a room seed, not just one game seed.
- A room seed contains one or more slots.
- Each slot has a user/config/game/LinkedWorld.
- Worlds reads each LinkedWorld, builds local checks/items/rules, then produces
  one global multiworld placement surface.
- Cross-slot placement is required: a check in one slot may deliver an item to
  another slot.
- Link-Room later consumes `placements.json` and the room contracts to route
  item sends/receives.
- SKLMI and Sekaiemu must consume contracts. They must not infer seed logic on
  their own.

## Important Reality Check

The current ALTTP LinkedWorld is not complete.

Verified counts:

- Tracker bundle locations: `286`
- AP/MultiworldGG reference `lookup_name_to_id`: `286`
- AP/MultiworldGG reference `location_table`: `235`
- AP/MultiworldGG key drops: `33`
- AP/MultiworldGG shop locations: `37`
- Current LinkedWorld generation fillable set: `153`
- Current item pool: `153`

This means the current generator package proves the generic engine can place
cleanly on the declared subset, but it does not prove a complete ALTTP run.

Do not say “ALTTP complete” until the LinkedWorld generation profile declares
the full intended option-aware check/item surface.

## Generator State

The public API remains in:

- `server/native/sekailink_server_core/include/sekailink_server/generic_generation.hpp`

Stable public functions:

- `load_linkedworld_generation_surface(...)`
- `missing_required_generation_capabilities(...)`
- `missing_required_generation_surface_requirements(...)`
- `generate_seed_package_from_linkedworlds(...)`

The main implementation has been split into internal modules. It can still be
split further, but it is no longer a single generator blob.

Before refactor:

- `generic_generation.cpp`: `4297` lines

After 2026-05-11 refactor:

- `generic_generation.cpp`: `3937` lines
- `generic_generation_internal.cpp`: `402` lines
- `generic_generation_internal.hpp`: `52` lines

After 2026-05-13 refactor:

- `generic_generation.cpp`: `2218` lines
- extracted generator `.cpp` modules: `2287` lines total, excluding CLI smoke/probe mains

## Refactor Already Done

Added:

- `include/sekailink_server/generic_generation_internal.hpp`
- `src/generic_generation_internal.cpp`
- `docs/GENERIC_GENERATOR_REFACTOR_MAP.md`

Moved out of `generic_generation.cpp`:

- file read/write helpers
- SHA-256 helper
- UTC timestamp helper
- required JSON string helper
- LinkedWorld ref resolution
- generation IR path resolution
- JSON ref loading
- catalog expansion from `locations_ref`, `items_ref`, `item_pool_ref`
- logic/placement rule ref loading
- patch manifest expansion
- package hash material helper
- generic JSON helpers:
  - string coercion
  - string arrays/sets
  - truthy flags
  - object status checks
  - JSON recursive string search
  - slot/entity key helpers
  - base instance ID helper

This was behavior-preserving. The public API did not change.

## Refactor Rules

Do not do a giant rewrite.

Refactor in slices, and run checks after every slice.

The intended module order is documented in:

- `docs/GENERIC_GENERATOR_REFACTOR_MAP.md`

Remaining extraction targets:

1. `generic_generation_surface_probe`
   Capability checks, native probe readiness, blocker reports, and probe report
   shaping.

2. `generic_generation_items`
   Item expansion, ownership metadata, progression classification, future
   option-aware pool hooks.

3. `generic_generation_solver`
   Facts, reachability, spheres, requirement evaluation.

4. `generic_generation_replay`
   Final replay validation, progressive/count validation, fail-closed audit
   statuses.

5. `generic_generation_package_writer`
   Manifest assembly, file writes, hashes, package verifier checks.

## Tests / Checks Last Run

Build command:

```bash
cmake --build /tmp/sekailink-worlds-generic-build --target sekailink_generic_generation_package sekailink_generic_generation_smoke sekailink_generic_generation_probe
```

Smoke:

```bash
/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_smoke
```

Result:

```text
sekailink_generic_generation_smoke ok
```

ALTTP probe:

```bash
/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_probe clean-room/repos/sekailink-linkedworld-alttp
```

Important probe values:

- `can_native_generate=true`
- `native_probe_contract_count=5`
- `native_probe_pass_count=5`
- `native_probe_fail_count=0`
- `effective_fillable_location_count=153`
- `generation_item_pool_count=153`
- `location_count=286`
- `patch_mode=artifact`
- `patch_artifact_extension=.aplttp`

Package generation after refactor:

```bash
/usr/bin/time -f elapsed=%E /tmp/sekailink-worlds-generic-build/sekailink_generic_generation_package clean-room/repos/sekailink-linkedworld-alttp /tmp/sekailink-alttp-native-packages seed-alttp-refactor-init deterministic-alttp-native-test 1 1001 Jade 501
```

Result:

```text
package_dir="/tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init"
slot_count=1
linkedworld_count=1
elapsed=1:48.70
```

Package verification:

```bash
jq 'length' /tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init/checks.json /tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init/items.json /tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init/placements.json
```

Result:

```text
153
153
153
```

Audit:

```bash
jq '.replay_validation, .post_logic_fill_count' /tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init/audit.json
```

Result:

```text
"full-logic-replay-passed"
94
```

2026-05-13 post-refactor package:

```bash
/usr/bin/time -f elapsed=%E /tmp/sekailink-worlds-generic-build/sekailink_generic_generation_package clean-room/repos/sekailink-linkedworld-alttp /tmp/sekailink-alttp-native-packages seed-alttp-refactor-final deterministic-alttp-native-test 1 1001 Jade 501
```

Result:

```text
package_dir="/tmp/sekailink-alttp-native-packages/seed-alttp-refactor-final"
slot_count=1
linkedworld_count=1
elapsed=1:47.05
```

Verification:

```text
checks/items/placements = 153 / 153 / 153
replay_validation = "full-logic-replay-passed"
post_logic_fill_count = 94
placement_count = 153
```

Additional 2026-05-13 smokes:

```text
sekailink_generic_generation_smoke ok
sekailink_generic_generation_probe clean-room/repos/sekailink-linkedworld-alttp ok
generation_service_ok=1
generation_server_ok=1
generation_server_service_ok=1
```

Note: `sekailink_generation_server_service_smoke` must be run outside the Codex
network sandbox because it starts a child TCP service. Inside the sandbox it can
fail at service readiness even when the binary works.

Post-logic fill verification:

```bash
jq '[.[] | select(.post_logic_fill == true)] | {count:length, advancement_count:([.[] | select(.advancement == true)]|length)}' /tmp/sekailink-alttp-native-packages/seed-alttp-refactor-init/placements.json
```

Result:

```json
{
  "count": 94,
  "advancement_count": 0
}
```

CTest:

```bash
ctest --test-dir /tmp/sekailink-worlds-generic-build -R "generic|generation" --output-on-failure
```

Result:

```text
No tests were found!!!
```

So the real checks today are the smoke/probe/package binaries above.

## Known Blockers

1. ALTTP LinkedWorld is partial.

   Current profile declares only `153` fillable checks/items. A complete ALTTP
   profile must become option-aware and include the intended full check surface:
   normal locations, key drops, dungeon item policy, shops depending on settings,
   NPC/trade/tablet/economy/event checks, and any excluded/non-randomized
   locations explicitly classified.

2. Patch artifact is not materialized.

   The package emits a patch contract referencing `patches/slot-1.aplttp`, but
   the contract marks it as placeholder. Runtime must fail closed until real
   `.aplttp` bytes exist and are hashed.

3. Package generation is slow.

   Current ALTTP subset package takes about `1:48`. This is likely in
   solver/placer/replay, not LinkedWorld loading. Do not hide this. Future
   profiling should happen after the module split makes timing easier.

4. Multi-slot validation is still pending.

   Current package validation is single-slot ALTTP. We still need a two-slot
   generic test proving cross-slot item placement and contracts.

## Do Not Do

- Do not add ALTTP-specific branches in Worlds.
- Do not mark ALTTP complete while the generation profile is still `153`.
- Do not claim the `.aplttp` exists while the artifact is placeholder.
- Do not rewrite the generator in one pass.
- Do not move logic from LinkedWorld JSON into C++ to make a test pass.

## Recommended Restart Sequence

When work resumes:

1. Read this file.

2. Read:

   - `docs/GENERIC_GENERATOR_REFACTOR_MAP.md`
   - `docs/GENERIC_GENERATOR_CONTRACT.md`
   - `docs/ALTTP_POST_NATIVE_GENERATION_PATCH_HANDOFF.md`

3. Run:

   ```bash
   cmake --build /tmp/sekailink-worlds-generic-build --target sekailink_generic_generation_package sekailink_generic_generation_smoke sekailink_generic_generation_probe
   /tmp/sekailink-worlds-generic-build/sekailink_generic_generation_smoke
   /tmp/sekailink-worlds-generic-build/sekailink_generic_generation_probe clean-room/repos/sekailink-linkedworld-alttp
   ```

4. Continue refactor with the next safe slice:

   - either `generic_generation_surface_probe`
   - or `generic_generation_checks`

5. Only after the generator is cleanly split, start the ALTTP complete profile:

   - derive full intended check surface from AP/MultiworldGG reference
   - keep it in LinkedWorld files
   - update `item-pool` to match fillable count
   - update probe expectations
   - keep Worlds generic

## Last Known Good Mental Model

Worlds is the itinerary builder.

LinkedWorlds are the game manuals.

Nexus stores the selected player settings/config versions.

Link-Room routes the generated placements at runtime.

SKLMI delivers memory events according to contracts.

Sekaiemu emulates, exposes memory, and eventually applies materialized patches.

No component should silently do another component's job.
