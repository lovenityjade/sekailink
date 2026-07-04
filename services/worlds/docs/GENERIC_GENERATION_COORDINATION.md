# Generic Generation Coordination

This note pins the current Beta-3 direction so future agents do not drift back
into ALTTP-specific generation code.

## Prime Rule

Worlds is generic. A game becomes generatable only when its LinkedWorld provides
machine-readable generation contracts that Worlds can consume without hidden game
knowledge.

ALTTP is the first proof target, not a special path.

## Current State

- The generic generator can load LinkedWorld generation surfaces, expand counted
  item pools, evaluate `sekailink-expr-v1`, consume item effects, apply
  placement constraints, validate replay, and traverse declared region graphs.
- ALTTP declares a complete normal item pool of `153` main-pool items.
- ALTTP declares a separate dungeon-key policy outside the main item pool.
- ALTTP still has unresolved region traversal placeholders and non-consumed
  dungeon-key policy contracts.
- ALTTP declares `4` missing generation contract surfaces for route/portal,
  dungeon policy, reward/medallion, and NPC/follower state. They are
  `declared-contract-not-consumed` and must remain blockers.
- Native generation gates must remain closed until those contracts are consumed
  by generic Worlds logic or a clearly isolated LinkedWorld adapter surface.

## Non-Negotiable Gates

- Do not set `can_solve_logic=true` until every required location, item effect,
  region traversal, dungeon/key rule, reward gate, and profile-dependent rule is
  consumed by the native generation path.
- Do not set `can_place_items=true` until fillable locations, reserved
  locations, preplacements, item constraints, dungeon keys, and replay
  validation are all consumed.
- Do not mark `authorizes_native_placement=true` or
  `authorizes.native_logic_solve=true` while a surface is
  `declared-not-consumed`, `candidate-not-consumed`, or placeholder-backed.
- Do not hardcode ALTTP rules in `generic_generation.cpp`.

## Agent Work Split

- LinkedWorld agents may add better contracts, facts, bindings, and docs inside
  game repos.
- Worlds agents may add generic validators, counters, blockers, and consumers.
- A gate may open only when both sides agree through data and tests: the
  LinkedWorld declares the contract, and Worlds proves it consumes the contract.

## Current ALTTP Blockers

- Dark-world route / portal / mirror-entry state.
- Provisional region groups that are too coarse for honest per-region requires.
- Dungeon entrance, intra-dungeon, small-key, and big-key policy.
- Reward, crystal count, and medallion contracts.
- NPC, follower, escort, and interaction state.

These blockers are not failures. They are the remaining contract surface needed
to make ALTTP generation honest and reusable.
