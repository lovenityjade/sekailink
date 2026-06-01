# Changelog

## 2026-05-18

- Added a server-first seed package path for LinkedWorlds using `patch.mode=server_dispatch`.
- Kept native generation gates strict: `server_dispatch` can emit a room-facing package without pretending to support item-pool build, logic solve, or item placement.
- Added `config_snapshot` passthrough from Nexus-style slot requests into seed manifests, slot manifests, and Link Room contracts.
- Added `sekailink_generic_generation_package` support for a config snapshot JSON argument.
- Added smoke coverage for server-first packages with zero local checks/items/placements and a per-slot dispatch contract.
- Hardened `server_dispatch` contracts to require `target`, `transport`, and `payload_ref`.
- Confirmed the existing generic generation smoke remains green after the server-first split.
- Confirmed the TCP generation server smoke passes outside the sandbox; sandboxed local TCP bind is restricted in this environment.

## 2026-05-10

- Added the native C++ generic LinkedWorld generation foundation.
- Added capability gating so Worlds refuses incomplete LinkedWorld generation surfaces instead of hardcoding game behavior.
- Added immutable seed package output files for Nexus, Link Room, SKLMI, Sekaiemu tracker, patches, and audit projection.
- Added `sekailink_generic_generation_smoke` to prove successful package generation and clean refusal of incomplete modules.
- Added `sekailink_generic_generation_probe` to inspect real LinkedWorld generation gates and catalog counts.
- Added `docs/LINKEDWORLD_GENERATION_GATES.md` to pin the ALTTP probe result and prevent false native-generation claims.
- Added `submit_seed_package` to the Worlds TCP service and covered it in `sekailink_generation_server_smoke`.
- Added `docs/ALTTP_NATIVE_GENERATION_PLAN.md` to pin the native ALTTP solver/placer milestones.
- Added `docs/ALTTP_REFERENCE_PATCH_VALIDATION.md` after generating, applying, and headless-booting a reference ALTTP `.aplttp` through Sekaiemu.
- Clarified that Worlds generates one multiworld from multiple Nexus config versions and strengthened the generic smoke around two slots/two LinkedWorld surfaces.
- Documented the canonical `Nexus config versions -> slots -> LinkedWorld generation surfaces -> one seed package` flow.
- Clarified that YAML is import/export compatibility only, `LinkedWorld` owns game logic, and Worlds must not hardcode any game in the generic generator.
- Split generation item pools from tracker item metadata in the generic C++ path and documented why tracker item definitions are not enough to claim native placement.
- Updated the LinkedWorld probe output to report tracker item count and generation item pool count separately.
- Added `missing_native_generation_reasons` so missing item-pool surfaces block native generation even when legacy capability flags are overly optimistic.
- Clarified that `can_build_item_pool` requires a real generated item pool or LinkedWorld-owned construction rules; tracker items alone must keep `can_native_generate=false`.
- Documented the next minimal LinkedWorld generation refs: `item_pool_ref`, `logic_rules_ref`, and `placement_rules_ref`, and clarified that refs do not open gates until solver/placer code consumes them.
- Recentered the JSON third-party header required by the native Worlds build under this clean repo.
- Documented the generic generator contract and the rule that ALTTP is the first validation target, not a special generator path.
- Added counted item-pool expansion in the generic loader so LinkedWorlds can declare compact pools with `count` while Worlds places concrete item instances.
- Added generic `sekailink-expr-v1` fact evaluation, item effects, location requirements, fillable-location filtering, and tag-based placement constraints.
- Updated the generic generation smoke to prove a locked location opens only after a placed item grants the required fact, and that impossible logic fails with `unsolved_logic_or_placement_constraints`.
- Added audit fields `solver_mode=generic-fact-sphere-v1` and `placement_algorithm=deterministic-constraint-sphere-v1` to generated seed packages.
- Made slot resolution strict on `linkedworld_id` with `game_key` validation so multiple LinkedWorld variants of one game cannot be mixed accidentally.
- Added generic consumption of LinkedWorld-declared preplacements, reserved locations, `fillable_locations_ref` sources, and explicit fillable-location count mismatch gating.
- Extended the generation probe with explicit fillable-location counts so ALTTP now reports `153` expected fillable locations and `0` audited explicit locations instead of implying tracker checks are fillable.
- Added generic replay validation after placement and hardened it to cross-check placements against source `checks.json` and `items.json`, including contiguous placement indexes, unique locations/items, known references, placement constraints, and fact replay.
- Extended `sekailink_generation_server_smoke` to verify generated `audit.json` reports `replay_validation=passed` and that placements carry replay metadata.
- Confirmed Worlds still treats ALTTP logic and placement candidate metadata as non-authorizing LinkedWorld data; `can_native_generate` remains false until the LinkedWorld opens its gates.
- Added generic effective gate blockers for candidate/non-consumed logic, unaudited placement surfaces, non-authorizing placement contracts, and unsupported progressive item-effect encodings.
- Extended `sekailink_generic_generation_smoke` and `sekailink_generic_generation_probe` so these blockers are tested and reported without hardcoding ALTTP behavior.
- Added generic support for LinkedWorld-declared progressive item effects with staged/level grants, including replay validation.
- Taught the generic generator to consume both direct and wrapped rule surfaces: `item_effects` or `item_effects.effects`, plus `locations` or `locations.rules`.
- Extended the probe with declared rule/effect counters so candidate history and partially consumable LinkedWorld surfaces are visible separately.
- Added generic `count_grants.thresholds` support for repeated item families, with deterministic placement and replay validation coverage.
- Extended the generation probe with location segmentation and region traversal binding-plan counters so non-consumed LinkedWorld progress is visible without opening native gates.
- Added generic region graph consumption for `starting_regions`, `edges`, and `location_region_bindings`, including replay validation and gate blockers for non-authorizing graphs.
- Updated the generation probe to compute remaining region traversal placeholder edges from the graph itself, not from optional audit metadata.
- Extended the generation probe with LinkedWorld-owned dungeon-key policy counters so separate non-main-pool key models are visible without being consumed by Worlds.
- Added generic LinkedWorld-declared progressive item effects with ordered `stages[]`/`levels[]` grants, including solver placement state, replay validation, and smoke coverage for second-level unlocks.
- Documented the minimal generic region graph contract for LinkedWorld surfaces: `starting_regions`, `edges`, `location_region_bindings`, and `fact_names`, with smoke fixture coverage that preserves the declared fields without opening native solver gates.
- Hardened generic region graph validation so incoherent surfaces fail cleanly when starting regions are empty, edge/binding regions are undeclared, or declared `fact_names` omit referenced facts.
- Hardened generic contract-surface gates for `region_graph.edge_audit.edge_blocker_requirements`, `dungeon_key_policy`, and `dungeon_key_policy_binding` so declared-not-consumed or non-authorizing metadata remains visible in the probe and blocks native generation.
- Extended the generic probe and smoke coverage for `region_graph.edge_audit.missing_generation_contract_surfaces` so LinkedWorld-declared missing route/dungeon/reward/NPC contracts remain visible and block native generation until consumed.
