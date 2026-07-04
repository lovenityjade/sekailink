# Generic Generator Refactor Map

Status: major split complete, behavior-preserving.

If returning after a pause, read `RETURN_HANDOFF_2026-05-11.md` first. It
contains the latest validated state, commands, blockers, and restart sequence.

The generic generator must stay data-driven. Game-specific knowledge belongs in
LinkedWorld data files, Nexus config records, patch manifests, and runtime
contracts. Worlds may interpret declared generic contracts, but it must not
grow `if game == ...` logic for ALTTP or any other game.

## Current Public Facade

The public API remains:

- `load_linkedworld_generation_surface(...)`
- `missing_required_generation_capabilities(...)`
- `missing_required_generation_surface_requirements(...)`
- `generate_seed_package_from_linkedworlds(...)`

Callers should continue to include `sekailink_server/generic_generation.hpp`.
Internal modules may change as the implementation is split.

## Extracted Modules

- `generic_generation_internal.hpp/cpp`
  File I/O, hashing, timestamps, JSON coercion/helpers, LinkedWorld ref
  resolution, catalog expansion, rule ref loading, patch manifest expansion,
  and package hash material.

- `generic_generation_checks.hpp/cpp`
  Fillable-location filtering from placement rules and declared sets.

- `generic_generation_logic_rules.hpp/cpp`
  Logic expression evaluation, starting facts, rule lookup/normalization,
  refinement/segmentation rule expansion, item effect views, and fact/grant
  collection.

- `generic_generation_surface_contracts.hpp/cpp`
  Generic consumed/candidate status checks, native authorization checks,
  native proof handling, proof fact validation, and contract blockers.

- `generic_generation_region_graph.hpp/cpp`
  Region edge identity, consumed edge matching, placeholder replacement,
  effective region graph construction, and unresolved placeholder detection.

- `generic_generation_region_analysis.hpp/cpp`
  Region declaration validation, invalid-region diagnostics, location-region
  bindings, and derived region reachability facts.

- `generic_generation_placement.hpp/cpp`
  Location/item enrichment, placement item constraints, deterministic ordering,
  static matching guard, progressive/count grants, and placement diagnostics.

- `generic_generation_patch_contracts.hpp/cpp`
  Patch mode detection, per-slot patch contract refs/artifact refs, patch
  manifest normalization, and generated package contract verification.

The public facade remains in `generic_generation.cpp`; it now owns orchestration,
readiness/report assembly, native probe execution, final placement/replay calls,
and package writing.

## Remaining Split Targets

Split in this order, with tests after every slice:

1. `generic_generation_surface_probe`
   Capability checks, native probe readiness, surface blockers, and report
   shaping. The CLI probe can then become a thin executable over this module.

2. `generic_generation_items`
   Item expansion, item ownership metadata, progression classification, and
   option-aware pool construction hooks.

3. `generic_generation_solver`
   Fact graph, reachability, sphere opening, requirement evaluation, and replay
   fact derivation.

4. `generic_generation_replay`
   Final replay validation, progressive/count validation, and fail-closed audit
   statuses.

5. `generic_generation_package_writer`
   Manifest assembly, per-slot outputs, package file writes, package hash, and
   verifier contract checks.

## Refactor Rules

- Do not change JSON output shape unless a test and document are updated in the
  same patch.
- Do not add game-specific branches in Worlds.
- Keep `GenerationPackageRequest` and `GenerationPackageResult` stable until
  the first full ALTTP package and a two-slot multiworld package both pass.
- Every extracted module must be buildable independently through
  `sekailink_server_core`.
- After each slice, run at minimum:
  - `sekailink_generic_generation_smoke`
  - `sekailink_generic_generation_probe <linkedworld-root>`
  - one package generation for the current ALTTP LinkedWorld fixture

## Known Non-Goals For This Refactor

- This refactor does not make ALTTP complete by itself.
- This refactor does not materialize `.aplttp` bytes.
- This refactor does not hardcode AP behavior into C++.

The next content milestone remains an option-aware ALTTP LinkedWorld generation
profile with the full intended check/item surface. This refactor exists so that
work can happen without turning Worlds into another legacy blob.
