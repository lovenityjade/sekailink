# ALTTP Generation Contracts

This document mirrors the machine-readable contract surfaces declared in
`generation/logic.open-no-glitches.v1.json` at
`$.region_graph.edge_audit.missing_generation_contract_surfaces`.

Status:

- schema: `sekailink-linkedworld-generation-contract-surfaces-v1`
- status: `declared-contract-not-consumed`
- native reachability: not authorized
- native logic solve: not authorized
- native placement: not authorized

## Region Traversal Blockers

The `open-no-glitches` region graph currently has `16` edges:

- `3` edges are `audited-not-consumed`
- `13` edges remain `blocked-unresolved-traversal`
- `13` traversal placeholders remain required

No blocked edge can honestly replace
`predicate.region.traversal_declared_not_consumed:*` until its missing
contracts are declared as generic LinkedWorld facts and consumed by Worlds.

## Contract Surfaces

- `dark_world_route_or_portal_state_missing`: declares the need for generic
  dark-world route, portal, mirror-entry, entrance-state, route-variant, and
  per-location traversal facts across `10` blocked edges. The machine-readable
  surface now exposes `contract_roles`, `required_fact_families`,
  `fact_templates`, `route_state_examples`, and
  `location_refinement_requirements` for dark-world access, portal/mirror
  state, and per-location traversal refinements. These templates are not facts
  yet, so the existing placeholders remain required.
- `dungeon_policy_missing`: declares the need for generic dungeon entrance,
  small-key, big-key, intra-dungeon traversal, dark-room, and mechanic policy
  across `8` blocked dungeon edges. The machine-readable surface now exposes
  `fact_templates`, `key_rule_templates`, and `edge_bindings` for Ganon's
  Tower, Ice Palace, Misery Mire, Palace of Darkness, Skull Woods, Swamp
  Palace, Thieves' Town, and Turtle Rock.
- `reward_or_medallion_contract_missing`: declares the need for generic
  crystal/reward count, prize, medallion requirement, and entrance-open facts
  across `3` blocked edges. The machine-readable surface now exposes
  `fact_templates`, `reward_gate_templates`,
  `medallion_requirement_templates`, and `edge_bindings` for Ganon's Tower
  crystal-count gating plus Misery Mire and Turtle Rock medallion entrance
  gating.
- `npc_or_follower_state_missing`: declares the need for generic NPC
  interaction, follower/escort, delayed exchange, dark-world NPC access, route,
  and per-location traversal facts across `1` blocked edge. The
  machine-readable surface now exposes `contract_roles`,
  `required_fact_families`, `fact_templates`, `route_state_examples`, and
  `location_refinement_requirements` for NPC interaction, follower state,
  delayed exchange state, NPC route access, and per-location refinements. These
  templates are not facts yet, so the `world_npc_checks` placeholder remains
  required.

## Consumable Contract Shape

The priority generic traversal surfaces use this shape:

- `contract_roles`: semantic roles a consumer must satisfy before removing a
  placeholder.
- `required_fact_families`: parameterized families that must become declared
  LinkedWorld facts before a blocked edge can use them in `requires`.
- `fact_templates`: example IDs for future facts; each template is marked as
  not consumed.
- `key_rule_templates`: original-dungeon small-key policy, separate-pool
  big-key policy, and blocked self-lock policy for big-key chests.
- `reward_gate_templates`: crystal/reward count gates and dungeon prize
  bindings that still need concrete reward facts.
- `medallion_requirement_templates`: medallion entrance gates parameterized by
  dungeon and medallion slot, without declaring the required medallion value.
- `edge_bindings`: edge ids, target regions, bound location ids, remaining
  blockers, and explicit `placeholder_required=true`.
- `route_state_examples`: sample route variants showing why existing item
  facts are partial only.
- `location_refinement_requirements`: per-location fields required when a
  provisional region is too coarse for one honest edge-level `requires`.

The item pool mirrors key templates under
`$.dungeon_key_policy.key_rule_templates`. These templates do not add small keys
or big keys to the `153` item normal main pool; the existing `3` big-key entries
remain in `separate_key_pool` only.

No blocked edge was promoted in this pass. Existing item facts such as
`item.moon_pearl`, `item.flippers`, `item.magic_mirror`, `item.fire_rod`,
`item.mushroom`, `item.hammer`, `item.flute`, and `cap.has_inactive_flute`
remain partial prerequisites, not complete traversal contracts. The new dungeon,
reward, medallion, and crystal-count templates are also not enough to replace a
placeholder until Worlds consumes concrete route, entrance, reward-value,
medallion-value, key-reachability, and intra-dungeon facts.

## Consumption Rule

These surfaces are declarations only. They do not flip gates, capabilities, or
`authorizes` fields to true. Worlds must consume the contracts generically
before native generation can use them, and `generation-ir.json` remains
untouched by this contract pass.
