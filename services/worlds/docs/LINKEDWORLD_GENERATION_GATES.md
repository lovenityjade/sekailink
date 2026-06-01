# LinkedWorld Generation Gates

Date: 2026-05-10

## Purpose

Worlds must stay generic.

Every game is discovered through a `LinkedWorld` generation surface, but native
seed generation is only allowed when the module declares every required
capability and has no unsupported option gates for the requested seed.

The gate exists at the `LinkedWorld` boundary. Nexus supplies versioned config
snapshots, slots bind those configs to players, and each slot resolves a
`LinkedWorld` generation surface. Worlds then packages all approved slots into
one seed package for the room; it does not add per-game exceptions to make an
incomplete surface pass.

YAML fixtures may help import, export, or validate legacy settings, but they are
not the authoritative source for a native generation request. A request is valid
only when it can be traced back to Nexus config versions or an equivalent
imported projection with durable version identity.

## Required Native Gates

- `can_validate_options`
- `can_build_item_pool`; tracker item definitions alone are not an item pool.
  This gate requires either a real generated item pool for the active config or
  game-owned construction rules that produce one.
- `can_solve_logic`
- `can_place_items`
- `can_emit_patch`
- `can_emit_room_contract`
- `external_tools_required` must be empty.
- `unsupported_options` must be empty for the active request.

If no item-pool surface is declared, `missing_native_generation_reasons` must
include `missing_generation_item_pool`. If an `item_pool_ref` exists but its
expanded `items[]` is empty, `missing_native_generation_reasons` must include
`empty_generation_item_pool`. `can_native_generate` must remain false for the
whole seed request until every slot has a real item pool path.

The next minimal LinkedWorld generation refs are `item_pool_ref`,
`logic_rules_ref`, and `placement_rules_ref`. They are necessary contract hooks,
but they are not sufficient proof that native generation is safe. Worlds may see
those refs before the corresponding native gates open; `can_build_item_pool`,
`can_solve_logic`, and `can_place_items` remain false until the item-pool
builder, solver, and placer consume the referenced surfaces for the active
request.

## ALTTP Current Probe And Gate Interpretation

Command:

```bash
/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_probe \
  /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp
```

Current result:

- `location_count`: `286`
- `tracker_item_count`: `145`
- `generation_item_pool_count`: `0`
- `has_generation_item_pool`: `true`
- `has_logic_rules`: `true`
- `has_placement_rules`: `true`
- `can_validate_options`: `true`
- `can_build_item_pool`: `false`
- `can_emit_patch`: `true`
- `can_emit_room_contract`: `true`
- `can_solve_logic`: `false`
- `can_place_items`: `false`
- `missing_native_generation_reasons`: `can_build_item_pool`,
  `can_solve_logic`, `can_place_items`, `unsupported_options`,
  `empty_generation_item_pool`
- `can_native_generate`: `false`

ALTTP is therefore loaded by Worlds, but intentionally refused for native seed
generation until its C++ solver and item placer are implemented. The current
`tracker_item_count` comes from linked tracker/action metadata and must not be
treated as the final ALTTP generation pool.

The required gate interpretation is stricter than refs alone: with
`generation_item_pool_count=0`, ALTTP does not yet prove that its item-pool
surface is usable for native generation even though `item_pool_ref`,
`logic_rules_ref`, and `placement_rules_ref` now exist. A conforming gate check
must include `empty_generation_item_pool` in refusal details and keep
`can_native_generate=false` even if future work flips solver or placer gates
first.

## Why This Matters

This prevents dangerous failures:

- treating any randomizer ROM as valid room truth
- hardcoding ALTTP behavior inside Worlds
- pretending a LinkedWorld can generate when it only has tracker/runtime data
- treating tracker item icons/actions as the final generation item pool
- allowing other missing gates to hide the fact that `item_pool` is still absent
- letting YAML files bypass Nexus config versioning
- producing several disconnected single-game seeds instead of one room package

## Next Gates For ALTTP

- Move the complete ALTTP option schema into Nexus and keep `generation-ir.json`
  pointing to Nexus config snapshots rather than user YAML files.
- Convert ALTTP logic data into LinkedWorld-owned declarative data or native C++
  algorithms that consume LinkedWorld data.
- Implement the C++ item pool builder and placement solver behind generic Worlds
  interfaces, including filler, rewards, dungeon/key constraints, and any other
  game-owned rules needed to make the pool match fillable locations.
- Keep Enemizer-dependent options disabled until the Enemizer behavior is native.
- Only then flip `can_solve_logic`, `can_place_items`, and the relevant
  `unsupported_options` gates.
