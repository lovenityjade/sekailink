# ALTTP Native Generation Plan

Date: 2026-05-10

## Goal

Make ALTTP produce a clean native `.aplttp` package through Worlds without
hardcoding ALTTP into the generic generator and without copying Archipelago code.

ALTTP is the first validation target. The implementation must leave the same
generic path usable for other LinkedWorld modules.

## Current State

ALTTP loads through `sekailink_generic_generation_probe`:

- `286` locations
- `145` items/actions
- `0` generation item-pool entries
- option defaults referenced
- patch contract referenced
- SKLMI bridge referenced
- tracker bundle referenced
- room metadata referenced
- declarative `item_pool_ref`, `logic_rules_ref`, and `placement_rules_ref`
  referenced, but not yet executable

Native generation is intentionally refused until:

- `can_build_item_pool=true` from a non-empty generated pool or construction
  rules consumed by Worlds
- `can_solve_logic=true`
- `can_place_items=true`
- active `unsupported_options=[]`

## Minimal Supported ALTTP Profile

First native milestone:

- `goal=ganon`
- `mode=open`
- `logic=no_glitches`
- `entrance_shuffle=vanilla`
- `item_pool=normal`
- `item_functionality=normal`
- `enemy_health=default`
- `enemy_damage=default`
- `boss_shuffle=none`
- `enemy_shuffle=false`
- `pot_shuffle=false`
- `bush_shuffle=false`
- `killable_thieves=false`

## Enemizer Gate

The following options stay refused until their behavior is native:

- `boss_shuffle != none`
- `enemy_shuffle != false`
- `enemy_health != default`
- `enemy_damage != default`
- `pot_shuffle != false`
- `bush_shuffle != false`
- `killable_thieves != false`

## C++ Components

Proposed native ALTTP components under Worlds:

- `worlds/alttp/alttp_options`
- `worlds/alttp/alttp_catalog`
- `worlds/alttp/alttp_state`
- `worlds/alttp/alttp_logic`
- `worlds/alttp/alttp_item_pool`
- `worlds/alttp/alttp_dungeons`
- `worlds/alttp/alttp_placer`
- `worlds/alttp/alttp_patch_plan`

These components are game-specific plugins behind the generic Worlds surface.
They may consume ALTTP LinkedWorld data, but the generic generator must not gain
ALTTP-specific branches.

## Milestones

1. Extend ALTTP `generation-ir.json` with refs for complete options, region graph,
   rule refs, dungeon schema, item pool groups, and patch plan inputs.
2. Implement option validation and normalization from Nexus config snapshots.
3. Implement catalog loading for items, locations, regions, dungeons, and rules.
4. Implement minimal vanilla/open/no-glitches reachability.
5. Implement normal item pool creation and progression item classification.
6. Implement restrictive placement and sphere verification.
7. Emit complete `placements.json`, `checks.json`, `items.json`,
   `tracker_seed_state.json`, `sklmi_seed_contract.json`, and
   `link_room_seed_contract.json`.
8. Emit a clean `.aplttp` patch plan and then a patch artifact through the native
   patcher.
9. Run smoke tests against generated package output.
10. Flip ALTTP gates only after tests prove solver, placement, and patch output.
