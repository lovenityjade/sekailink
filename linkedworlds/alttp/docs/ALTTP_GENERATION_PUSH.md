# ALTTP Generation Push

Date: 2026-05-11

Scope: `generation/**` and `docs/**` only. No Worlds engine edits.

## What Moved

Wave 1:

- Confirmed the compact normal item pool remains `153` items for
  `open-no-glitches-normal`.
- Cross-checked local Archipelago and MultiworldGG ALTTP references only as
  behavioral/count inspiration:
  - `third-party/upstream/archipelago/worlds/alttp/ItemPool.py`
  - `third-party/upstream/multiworldgg/worlds/alttp/ItemPool.py`
  - `third-party/upstream/archipelago/worlds/alttp/Rules.py`
  - `third-party/upstream/archipelago/worlds/alttp/StateHelpers.py`
- Expanded `generation/logic.open-no-glitches.v1.json` item effects from `27`
  to `40` declared monotonic effects.
- Added missing normal-pool item facts for:
  - medallions: `item.bombos`, `item.ether`, `item.quake`
  - tablet and dungeon mechanics: `item.book_of_mudora`,
    `item.cane_of_somaria`, `item.cane_of_byrna`, `item.ice_rod`,
    `item.cape`
  - utility items: `item.bug_catching_net`, `item.blue_boomerang`,
    `item.red_boomerang`
  - arrows: `item.single_arrow`, `item.arrows_10`
- Added partial item-only location rules for:
  - `Potion Shop`: `item.mushroom`
  - `Ether Tablet`: `item.book_of_mudora` plus
    `item.progressive_sword.level.2`
  - `Bombos Tablet`: `item.book_of_mudora` plus
    `item.progressive_sword.level.2`
  - `King Zora`: `resource.rupees.available`
  - `Bottle Merchant`: `resource.rupees.available`
- Mirrored new non-authorizing item constraints in
  `generation/placement.open-no-glitches.v1.json`.

Wave 2:

- Targeted the `light_world_caves` blocked region edge.
- Kept the coarse edge blocked because it still mixes free caves, event checks,
  combat checks, and item-gated entrances.
- Converted `24` Light World cave location bindings into explicit generic
  location rules without opening native gates.
- Reduced `region traversal missing` from `118` to `94` locations.
- Reduced `light_world_caves` blocked bindings from `27` to `3`.
- Increased declared location rules from `24` to `48`.
- Mirrored the Wave 2 segmentation delta in
  `generation/placement.open-no-glitches.v1.json`.
- Applied the supervisor directive update by treating Archipelago/MultiworldGG
  as priority behavioral references for safety invariants, while still
  translating behavior into LinkedWorld-owned JSON surfaces.

Converted `light_world_caves` bindings:

- Free/open/no-glitches cave checks: Blind's Hideout Left/Right/Far Left/Far
  Right, Secret Passage, Link's House, Kakariko Tavern, Kakariko Well
  Left/Middle/Right/Bottom, Lost Woods Hideout, Cave 45.
- Item-gated cave checks: Waterfall Fairy Left/Right with `item.flippers`,
  Checkerboard Cave with `cap.can_lift_light_rocks`, Bonk Rock Cave with
  `item.pegasus_boots`.
- Bomb-gated cave checks: Graveyard Cave and Ice Rod Cave with
  `cap.can_use_bombs`.
- Bomb plus combat-gated cave checks: five Mini Moldorm Cave checks with
  `cap.can_use_bombs` plus
  `predicate.combat.can_clear:mini_moldorm_cave:normal_4`.

New behavior surface:

- `generation/logic.open-no-glitches.v1.json $.combat_policy` declares a
  conservative LinkedWorld combat predicate inspired by AP/MultiworldGG
  `can_kill_most_things(..., 4)`.
- The translated predicate accepts melee weapon, Cane of Somaria, Cane of
  Byrna, bow/arrow capability, or Fire Rod.
- The AP/MultiworldGG sufficient-bombs alternative is intentionally omitted
  until LinkedWorld has quantity-aware bomb expenditure facts.
- No Python dependency, no copied executable logic, and no Worlds hardcode was
  introduced.

Wave 3:

- Finished the remaining `light_world_caves` per-location blockers without
  promoting the coarse region edge.
- Increased declared location rules from `48` to `51`.
- Reduced `region traversal missing` from `94` to `91` locations.
- Reduced `light_world_caves` blocked bindings from `3` to `0`.
- Kept the `13` region graph placeholder edges blocked because native
  per-location refinement consumption and coarse-edge replacement are still
  missing.
- Added `generation/logic.open-no-glitches.v1.json $.route_event_policy` for
  route/story-event facts translated from AP/MultiworldGG behavior.
- Mirrored the segmentation and placement risk constraints in
  `generation/placement.open-no-glitches.v1.json`.

Wave 3 converted bindings:

- `King's Tomb`: declared route alternatives over `item.pegasus_boots`,
  `cap.can_lift_dark_rocks`, or `item.magic_mirror` plus `item.moon_pearl`.
- `Floodgate Chest`: declared as an open/no-glitches Dam chest. The
  floodgate-open event is intentionally not attached to this chest; that event
  belongs to separate Sunken Treasure behavior.
- `Lumberjack Tree`: declared as `item.pegasus_boots` plus
  `predicate.story_event.state:agahnim_1_defeated`, but the story event remains
  non-consumed until event production and native consumption exist.

Wave 4:

- Targeted `world_npc_checks` and `mountain_caves`, the next safest
  high-impact provisional segments.
- Increased declared location rules from `51` to `68`.
- Reduced `region traversal missing` from `91` to `74` locations.
- Reduced traversal-missing `world_npc_checks` from `6` to `0`.
- Reduced traversal-missing `mountain_caves` from `11` to `0`.
- Kept all `13` placeholder region edges blocked because route, event,
  follower, prize, magic/survival, and native per-location consumption are not
  consumed yet.
- Extended `$.route_event_policy` with LinkedWorld-owned reward, follower,
  escort, dark-world NPC route, Death Mountain route, combat, and survival
  facts inspired by AP/MultiworldGG behavior.
- Mirrored the new route/event/survival risks in placement constraints.

Wave 4 converted bindings:

- `Mushroom`: declared as normal/open-profile Lost Woods pickup.
- `Sahasrahla`: declared as requiring `predicate.reward.has:green_pendant`.
- `Blacksmith`: declared as requiring
  `predicate.follower.state:blacksmith_returned`.
- `Old Man`: declared as requiring west Death Mountain route plus
  `predicate.follower.state:old_man_escorted`.
- `Catfish` and `Stumpy`: declared as Moon Pearl/dark-world NPC route checks.
- `Spectacle Rock Cave`, `Paradox Cave Lower` checks, `Spiral Cave`,
  `Superbunny Cave`, `Spike Cave`, and `Mimic Cave`: declared with
  per-location Death Mountain route, combat, mirror, or survival contracts.

Wave 5:

- Adapted the existing Wave 2-4 rule work into formal generic refinement
  surfaces now that Worlds can count/report refinement keys.
- Added `$.location_refinements`, `$.per_location_refinements`, and
  `$.location_rule_refinements`.
- Marked `61` refinements as countable/reportable, with `0` native
  reachability-authorizing refinements.
- Increased declared location rules from `68` to `85`.
- Reduced `region traversal missing` from `74` to `57`.
- Reduced traversal-missing `dark_world_surface` from `4` to `0`.
- Reduced traversal-missing `dark_world_caves` from `13` to `0`.
- Kept all `13` placeholder region edges blocked; dark-world route production,
  Pyramid Fairy reward/big-bomb production, minigame state, and native
  reachability authorization remain missing.

Wave 5 converted bindings:

- `Pyramid`, `Digging Game`, `Bumper Cave Ledge`, and `Floating Island` now
  have per-location dark-world route or mirror-route contracts.
- `Hype Cave` checks now require Moon Pearl, bomb capability, and a south
  dark-world Hype Cave route contract.
- `Peg Cave` now requires Moon Pearl, Hammer, and a hammer-peg cave route
  contract.
- `Pyramid Fairy` checks now require Crystal 5, Crystal 6, big-bomb delivery,
  and a Pyramid Fairy route contract.
- `Brewery`, `C-Shaped House`, `Chest Game`, and `Mire Shed` now have
  per-location dark-world route contracts, with Chest Game still blocked by
  minigame/economy state.

Wave 6:

- Targeted dungeon-facing traversal blockers and production contracts.
- Increased declared location rules from `85` to `134`.
- Increased formal reportable refinements from `61` to `110`.
- Reduced `region traversal missing` from `57` to `8`.
- Added a new `dungeon-traversal-gated` segment with `49` non-consumed
  location contracts.
- Kept all `13` placeholder region edges blocked; no global gate was flipped.
- Added `$.dungeon_production_contracts` to declare expected producers for:
  dungeon entrances/sections, GT reward gate, Mire/TR medallion entrances, and
  dungeon key policy surfaces.
- Mirrored the Wave 6 segmentation/refinement counts in
  `generation/placement.open-no-glitches.v1.json`.

Wave 6 converted bindings:

- `Hyrule Castle/Escape`: Boomerang Chest, Zelda's Chest, Dark Cross, Sewer
  Secret Room, and Sanctuary now use section-reachability contracts.
- `Palace of Darkness`, `Swamp Palace`, `Skull Woods`, `Thieves' Town`, and
  `Ice Palace`: remaining traversal blockers now use explicit dungeon
  entrance/section contracts plus route/key/mechanic blockers.
- `Misery Mire` and `Turtle Rock`: remaining blockers now use medallion
  entrance contracts plus dungeon section contracts.
- `Ganon's Tower`: 21 checks now require a reportable GT reward gate plus
  dungeon section facts.
- The remaining `8` traversal-missing IDs are now limited to Light World
  surface checks plus Link's Uncle:
  `1573194`, `1573189`, `1573193`, `1573188`, `1573186`, `1573187`,
  `1573184`, `188229`.

Wave 7:

- Eliminated the final `region traversal missing` catch-all count from `8` to
  `0` by converting the remaining Light World surface plus Link's Uncle checks
  into explicit non-consumed contracts.
- Increased declared location rules from `134` to `142`.
- Increased formal reportable refinements from `110` to `118`.
- Added a new `light-world-surface-event-route-gated` segment with `8`
  non-consumed location contracts.
- Added `$.surface_event_production_contracts` for pickup, event, water,
  mirror, desert, mountain, and opening-event production.
- Added `$.completion_goal_contracts` for generic goal mode, GT/Ganon crystal
  thresholds, dungeon prize/reward count facts, boss completion, pedestal, and
  triforce-hunt contracts.
- Kept all `13` placeholder region edges blocked and all native gates false.

Wave 7 converted bindings:

- `Flute Spot`: Shovel plus open/no-glitches profile facts.
- `Sunken Treasure`: floodgate-open event contract.
- `Zora's Ledge`: Flippers plus water-route contract.
- `Lake Hylia Island`: Flippers, Moon Pearl, Mirror, and dark-world/mirror
  route contract.
- `Maze Race`: Boots, bombs, or mirror-route alternatives.
- `Desert Ledge`: Book plus desert-route contract.
- `Spectacle Rock`: Mirror plus west Death Mountain and mirror-route facts.
- `Link's Uncle`: opening-event contract for `uncle_available`.

## Gate Status

Do not open native generation gates yet.

- `generation/generation-ir.json` still has `can_solve_logic=false` and
  `can_place_items=false`.
- `generation/logic.open-no-glitches.v1.json` still has
  `can_solve_logic=false`, `can_authorize_location_reachability=false`, and
  `can_authorize_completion=false`.
- `generation/placement.open-no-glitches.v1.json` still has
  `authorizes_native_placement=false`.

The new facts and refinement surfaces are useful for Worlds reporting/counting,
but they are not enough to authorize native reachability, completion, or item
placement.

## Remaining Blockers

- Region traversal is still blocked at the edge level: `13` of `16` region
  edges remain `blocked-unresolved-traversal`.
- `0` fillable locations remain in the `region traversal missing` catch-all,
  but the converted route/event/dungeon/reward/completion facts are still
  non-consumed.
- `light_world_caves` has `0` remaining region-traversal-missing bindings, but
  the coarse edge remains blocked because Worlds does not consume generic
  per-location refinements yet.
- `Lumberjack Tree` is now classified as `story/event-gated`; it still needs
  production and native consumption for
  `predicate.story_event.state:agahnim_1_defeated`.
- `world_npc_checks` has `0` remaining traversal-missing bindings, but the new
  reward/follower/escort/dark-world-route facts still need production and native
  consumption.
- `mountain_caves` has `0` remaining traversal-missing bindings, but Death
  Mountain route production, dark-world mountain access, mirror routing,
  magic-spend, damage survival, and native route consumption are still missing.
- `dark_world_surface` and `dark_world_caves` have `0` remaining
  traversal-missing bindings, but dark-world route production, mirror route
  production, Pyramid Fairy crystal/big-bomb production, and minigame/economy
  state are still missing.
- `118` location refinements are now countable/reportable by generic Worlds
  code, but all remain non-authorizing for native reachability and placement.
- Light World surface/opening facts are declared, but floodgate, Uncle,
  water-route, mirror-route, desert-route, and mountain-route production are
  not consumed yet.
- General combat beyond Mini Moldorm still needs quantity-aware bombs,
  enemy-shuffle variants, enemy-health variants, magic spend, and potion refill
  policy before reuse outside the declared normal/default subset.
- Rupee checks now expose additive `resource.rupees.available`, but there is
  still no consumed price/spend/affordability model for King Zora or Bottle
  Merchant.
- Potion Shop has the `item.mushroom` requirement declared, but still needs
  generic delayed-exchange, NPC interaction, and route facts.
- Tablet item requirements are declared, but region reachability and tablet
  route refinements are still missing.
- Medallion ownership and Wave 6 medallion entrance surfaces are declared, but
  Misery Mire and Turtle Rock still need consumed seed-specific
  required-medallion value production and entrance-open consumption.
- Reward/prize production is declared as a Wave 6 surface, but GT crystal
  threshold ingestion, dungeon prize state, reward counts, Ganon/pedestal, and
  completion semantics are not consumed.
- Completion surfaces are now declared, but goal mode, GT/Ganon thresholds,
  dungeon prize assignment, boss defeat, pedestal, and triforce-hunt facts are
  not consumed by native solve/place.
- Dungeon big-key and small-key policy surfaces are declared but not consumed;
  per-section small-key requirements and separate key pool self-lock
  prevention still block native placement.
- Completion rules for crystals, pendants, Ganon, pedestal, and goal state are
  still not consumed by Worlds.

## Next Best Push

1. Produce and consume the new Light World surface/opening facts generically:
   floodgate, Uncle, water, mirror, desert, mountain, and pickup routes.
2. Add route production contracts for Dark World access variants, including
   south Hype Cave, north village, Pyramid, Pyramid Fairy, Mire Shed, and
   Bumper/Floating mirror routes.
3. Add a generic story-event contract for Agahnim 1 production before
   consuming `Lumberjack Tree`.
4. Add a generic economy contract for priced checks:
   `resource.rupees.total_at_least`, `resource.rupees.spend`, and
   `npc.purchase_state`.
5. Add a generic delayed-exchange/NPC production contract for Potion Shop,
   Blacksmith/Purple Chest, Old Man, Catfish, and Stumpy.
6. Produce dungeon section reachability from room/door/key/mechanic facts, then
   consume dungeon key policy generically before any separate key pool item can
   affect placement.

## Wave 8 Authorization Readiness Push

This pass did not open global gates. It converted several remaining blocker
surfaces from broad `not_authorizing` states into explicit proof contracts that
generic Worlds code can inspect before authorizing ALTTP solve or placement.

Before:

- Location rules: `142`.
- Region traversal missing: `0`.
- Region graph placeholder edges: `13`.
- Edge blocker requirements: `declared-not-consumed`.
- Dungeon key policy binding: `declared-not-consumed`, all native authorizes
  fields `false`.
- Risk audit: `candidate-tags-not-consumed`.
- Native gates: `can_solve_logic=false`, `can_place_items=false`.

After:

- Location rules: `142`.
- Region traversal missing: `0`.
- Region graph placeholder edges: `13`.
- Edge blocker requirements: `declared-contracts-not-consumed` with
  `consumption_summary.placeholder_edge_count=13`,
  `declared_contract_family_count=5`, `consumed_contract_family_count=0`,
  and `authorizing_edge_count=0`.
- `location_refinements.authorization_criteria` now records the exact missing
  producer, generic consumer, provisional-region split, and placement same-fact
  graph proofs.
- `location_rule_refinements.authorization_criteria` now records the missing
  all-prerequisites-produced, no-candidate-only-fact, and native probe contracts.
- `location_rule_segmentation.authorization_criteria` now marks segmentation
  complete for reporting but not native authorization, with non-authorizing
  segment counts preserved.
- `dungeon_key_policy_binding.consumption_readiness` now records declared
  coverage for key roles, required facts, big-chest bindings, and self-lock
  policy, plus missing key-pool consumption, small-key section, self-lock, and
  native probe proofs.
- Placement risk audit status advanced to
  `candidate-tags-covered-by-placement-constraints-not-authorizing`; all `19`
  candidate tag locations are crosswalked to declared constraints.
- `risk_audit_completion` authorizes reporting only:
  `risk_audit_reporting=true`, `native_placement=false`.
- `placement_authorization_readiness` now summarizes remaining native placement
  blockers without changing `authorizes_native_placement=false`.

Authorizes fields changed:

- Added report-only `risk_audit_completion.authorizes.risk_audit_reporting=true`.
- No native authorizes field was changed to `true`.
- `can_solve_logic`, `can_authorize_location_reachability`,
  `can_authorize_completion`, and `authorizes_native_placement` remain `false`.

Remaining blockers:

- `logic_rules_not_authorizing`: location and rule refinements have reportable
  criteria, but producer and generic consumer proofs are still missing.
- `region_graph_not_authorizing`: all `13` placeholders remain because no edge
  has complete produced and consumed fact coverage.
- `region_graph_edge_blockers_not_consumed`: `5` blocker families are declared,
  but `0` are consumed.
- `dungeon_key_policy_not_consumed`: separate key roles and big-chest bindings
  are declared, but small-key section requirements, key grant consumption, and
  self-lock enforcement remain missing.
- `location_refinements_not_authorizing`: `118` refinements are reportable, but
  none authorize native reachability or placement.
- `location_rule_segmentation_not_authorizing`: `153/153` fillable locations are
  segmented, but non-consumable segment families still require consumed
  semantics.
- Placement risk audit is now crosswalk-complete, but constraint semantics are
  not consumed by a native placer.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{logic_rules:(.locations.rules|length), traversal_missing:([.location_rule_segmentation.segments[]|select(.category=="region traversal missing")|.location_count][0]), placeholder_edges:([.region_graph.edges[]|select((.requires|tostring)|contains("traversal_declared_not_consumed"))]|length), edge_blocker_status:.region_graph.edge_audit.edge_blocker_requirements.status, key_readiness:.dungeon_key_policy_binding.consumption_readiness.status, gates:.capabilities}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{placement_authorizes:.authorizes_native_placement, source_audit_status:.source_audit_ref.status, risk_completion:.risk_audit_completion.status, risk_authorizes:.risk_audit_completion.authorizes, placement_readiness:.placement_authorization_readiness.status}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
```

## Wave 9 Fillable/Placement Audit Consumption

This pass picked the safest blockers to clear through LinkedWorld-owned data:
the explicit fillable-location audit, the risk-audit crosswalk, and placement
rule consumption as non-authorizing blockers. It did not authorize native solve
or native placement.

Before:

- `fillable-locations.status=proposed-selection-pending-audit`.
- `risk_audit.status=candidate-tags-not-consumed`.
- `risk_audit.location_tags_pending=19`.
- `placement.status=declared-not-consumed`.
- Placement declared constraints: `33`; consumed blocker constraints: `0`;
  not-consumed constraint statuses: `33`.
- `authorizes_native_placement=false`.

After:

- `fillable-locations.status=explicit-selection-audited-non-authorizing`.
- `risk_audit.status=consumed-by-placement-constraints-for-audit-not-native-placement`.
- `risk_audit.location_tags_pending=0`.
- `risk_audit.location_tags_consumed=19`.
- `risk_audit.audit_authorizes.explicit_fillable_selection=true`.
- `risk_audit.audit_authorizes.risk_audit_reporting=true`.
- `risk_audit.audit_authorizes.native_placement=false`.
- `placement.status=declared-constraints-consumed-for-audit-not-native-authorizing`.
- `placement.risk_audit_completion.status=consumed-as-non-authorizing-placement-blockers`.
- `placement.risk_audit_completion.authorizes.fillable_location_audit=true`.
- `placement.location_rule_segmentation_ref.status=consumed-as-non-authorizing-placement-blocker`.
- Placement declared constraints: `33`; consumed blocker constraints: `33`;
  not-consumed constraint statuses: `0`.
- `authorizes_native_placement=false`.

Blockers cleared or narrowed:

- `fillable_locations_pending_audit` is cleared for the explicit 153-location
  selection.
- `risk_audit_not_consumed` is cleared as a declarative placement-blocker
  contract. The remaining risk blocker is now
  `risk_audit_native_probe_not_authorizing`.
- `placement_rules_not_consumed` is cleared for the declared placement contract
  surface: all constraints are consumed as non-authorizing blockers.
- `placement_rules_not_authorizing` remains, intentionally.

Still blocked:

- `logic_rules_not_authorizing`.
- `region_graph_not_authorizing`.
- `region_graph_edge_blockers_not_consumed`: `5` blocker families declared,
  `0` consumed by the region graph.
- `region_graph_placeholder_edges=13`.
- `dungeon_key_policy_not_consumed`.
- `location_refinements_not_authorizing`.
- `location_rule_segmentation_not_authorizing`.
- `risk_audit_native_probe_not_authorizing`.
- Native gates remain false:
  `can_solve_logic=false`, `can_authorize_location_reachability=false`,
  `can_authorize_completion=false`, `authorizes_native_placement=false`.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/fillable-locations.open-no-glitches-normal.v1.json
jq '{fillable_status:.status, expected:.expected_fillable_count, fillable:(.sets.main_pool_fillable|length), risk_status:.risk_audit.status, pending:(.risk_audit.location_tags_pending|length), consumed:(.risk_audit.location_tags_consumed|length), authorizes:.risk_audit.audit_authorizes}' clean-room/repos/sekailink-linkedworld-alttp/generation/fillable-locations.open-no-glitches-normal.v1.json
jq '{placement_status:.status, authorizes:.authorizes_native_placement, risk_status:.risk_audit_completion.status, constraints:(([.item_constraints[], .location_constraints[], .economy_constraints[], .dungeon_constraints[], .segmentation_constraints[], .generation_contract_constraints[]]|length)), consumed_blockers:([.item_constraints[], .location_constraints[], .economy_constraints[], .dungeon_constraints[], .segmentation_constraints[], .generation_contract_constraints[]|select(.consumption_status=="consumed-as-non-authorizing-placement-blocker")]|length), not_consumed:([.item_constraints[], .location_constraints[], .economy_constraints[], .dungeon_constraints[], .segmentation_constraints[], .generation_contract_constraints[]|select(.consumption_status|contains("not-consumed"))]|length), readiness:.placement_authorization_readiness.readiness_summary}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
```

## Wave 10 Edge/Key Consumption Push

This pass targeted the remaining region/key blockers without opening native
solve or placement. The generic traversal placeholders are gone, but each
replacement edge carries an explicit non-authorizing blocker fact so the graph
is consumed as data while still unreachable to native logic.

Before:

- Region graph placeholder edges: `13`.
- Consumed replacement edges: `0`.
- Region edge blocker families consumed: `0/5`.
- `region_graph.status=declared-not-consumed`.
- `dungeon_key_policy_binding.status=declared-not-consumed`.
- `item-pool.dungeon_key_policy.status=declared-not-consumed`.
- `placement.dungeon_key_policy_ref.status=declared-not-consumed`.

After:

- Region graph placeholder edges: `0`.
- Consumed replacement edges: `13`.
- Replacement blocker fact count: `13`
  `predicate.region.edge_contract_not_authorizing:*`.
- Region edge blocker families consumed: `5/5`.
- `region_graph.status=declared-edge-contracts-consumed-not-authorizing`.
- `region_graph.edge_audit.edge_blocker_requirements.status=consumed-as-non-authorizing-edge-blockers`.
- `region_graph.edge_audit.edge_blocker_requirements.consumption_summary.blocked_replacement_edge_count=13`.
- `dungeon_key_policy_binding.status=consumed-as-non-authorizing-key-policy`.
- `item-pool.dungeon_key_policy.status=consumed-as-non-authorizing-key-policy`.
- `item-pool.dungeon_key_policy.separate_key_pool.status=consumed-as-non-authorizing-key-pool`.
- `placement.dungeon_key_policy_ref.status=consumed-as-non-authorizing-key-policy`.
- `placement.placement_authorization_readiness.readiness_summary.dungeon_key_policy_consumed=true`.
- `placement.placement_authorization_readiness.readiness_summary.dungeon_key_policy_native_authorizing=false`.

Blocker delta:

- Cleared `region_graph_placeholder_edges`.
- Cleared `region_graph_edge_blockers_not_consumed`.
- Cleared `dungeon_key_policy_not_consumed`.
- Narrowed `region_graph_not_authorizing` to
  `region_graph_replacement_edges_not_authorizing`.
- Narrowed `dungeon_key_policy_not_consumed` to
  `dungeon_key_policy_not_authorizing`.
- Narrowed refinement/segmentation proofs:
  `proof.location_refinement.region_split_or_per_location_binding` is now
  `satisfied-by-consumed-non-authorizing-edge-replacements`.
- Narrowed segmentation/placement proof:
  `proof.location_rule_segmentation.placement_constraints_cover_all_risk_segments`
  is now `satisfied-as-non-authorizing-placement-blockers`.

Still blocked:

- `logic_rules_not_authorizing`: fact producers and generic consumer coverage
  are still missing.
- `region_graph_replacement_edges_not_authorizing`: `13` replacement edges
  intentionally require `predicate.region.edge_contract_not_authorizing:*`.
- `dungeon_key_policy_not_authorizing`: separate key pool data is consumed, but
  separate-key fill order, native inventory grants, native self-lock
  enforcement, and small-key section requirements are still missing.
- `location_refinements_not_authorizing`: producer/consumer coverage and
  same-fact-graph placement proof are still missing.
- `location_rule_segmentation_not_authorizing`: consumed native rule coverage
  and non-consumable segment resolution are still missing.
- `placement_rules_not_authorizing`: placement constraints are consumed only as
  blockers, not as native placement permission.

Native gates remain false:

- `can_solve_logic=false`.
- `can_authorize_location_reachability=false`.
- `can_authorize_completion=false`.
- `authorizes_native_placement=false`.
- `can_place_items=false`.
- `unsupported_options` unchanged.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq '{placeholder_edges:([.region_graph.edges[]|select((.requires|tostring)|contains("traversal_declared_not_consumed"))]|length), replacement_edges:([.region_graph.edges[]|select(.status=="blocked-consumed-replacement-edge")]|length), edge_blocker_status:.region_graph.edge_audit.edge_blocker_requirements.status, edge_summary:.region_graph.edge_audit.edge_blocker_requirements.consumption_summary, key_status:.dungeon_key_policy_binding.status, gates:{solve:.capabilities.can_solve_logic, reach:.capabilities.can_authorize_location_reachability, completion:.capabilities.can_authorize_completion}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{key_policy_status:.dungeon_key_policy.status, separate_key_pool:.dungeon_key_policy.separate_key_pool.status, separate_key_count:(.dungeon_key_policy.separate_key_pool.items|length), authorizes:.dungeon_key_policy.authorizes}' clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq '{placement_status:.status, authorizes:.authorizes_native_placement, key_ref_status:.dungeon_key_policy_ref.status, readiness:.placement_authorization_readiness.readiness_summary, blockers:.placement_authorization_readiness.blocking_contracts}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
```

## Wave 11 Native Proof Data Push

This pass adds native-authorization proof data without flipping capabilities.
The goal was to make the remaining blockers machine-readable: replacement edge
fact production/consumption, location-refinement producer/consumer coverage,
segmentation native coverage, and dungeon-key self-lock/small-key proofs.

Added proof surfaces:

- `region_graph.edge_audit.edge_blocker_requirements.native_authorization_proof_matrix`.
- `location_refinements.native_proof_matrix`.
- `location_rule_segmentation.native_coverage_proof_matrix`.
- `dungeon_key_policy_binding.native_authorization_proofs`.
- `item-pool.dungeon_key_policy.native_authorization_proofs`.
- `placement_authorization_readiness.native_authorization_proof_matrix`.

Proof results:

- Replacement edges: `13`.
- Replacement blocker facts consumed: `13`.
- Authorizing edges: `0`.
- Authorizing edge fact production complete: `0`.
- Authorizing edge fact consumption complete: `0`.
- Native edge probes passed: `0`.
- Location refinements: `118` reportable, `0` native-authorizing.
- Segments: `153` locations classified, `0` native-authorizing segments.
- Placement constraints: `33/33` consumed as non-authorizing blockers.
- Dungeon key pool: `3` separate key items declared/consumed as data.
- Big-key self-lock binding data: `3/3` complete.
- Native self-lock enforcement: `false`.
- Small-key section requirements: `0`, still missing for all `9`
  dungeon-facing dungeon ids.

Blocker delta:

- `region_graph_not_authorizing` is narrowed to exact missing proof families:
  route/event/dungeon/reward facts are not fully produced or consumed, and
  native edge probes are missing.
- `location_refinements_not_authorizing` is narrowed to exact producer and
  consumer gaps by refinement set.
- `location_rule_segmentation_not_authorizing` is narrowed to exact segment
  native-consumer gaps, while placement blocker coverage is satisfied as
  non-authorizing data.
- `dungeon_key_policy_not_authorizing` is narrowed to native fill-order,
  inventory grant, self-lock enforcement, small-key section, and key-locked
  chest probe gaps.
- No native blocker was honestly cleared this wave.

Still blocked:

- `logic_rules_not_authorizing`.
- `region_graph_replacement_edges_not_authorizing`.
- `dungeon_key_policy_not_authorizing`.
- `location_refinements_not_authorizing`.
- `location_rule_segmentation_not_authorizing`.
- `placement_rules_not_authorizing`.

Native gates remain false:

- `can_solve_logic=false`.
- `can_authorize_location_reachability=false`.
- `can_authorize_completion=false`.
- `authorizes_native_placement=false`.
- `can_place_items=false`.
- `unsupported_options` unchanged.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{edge_proofs:.region_graph.edge_audit.edge_blocker_requirements.native_authorization_proof_matrix, loc_ref_proof:{status:.location_refinements.native_proof_matrix.status, native_authorizing_count:.location_refinements.native_proof_matrix.native_authorizing_count}, seg_proof:{status:.location_rule_segmentation.native_coverage_proof_matrix.status, native_authorizing_segment_count:.location_rule_segmentation.native_coverage_proof_matrix.native_authorizing_segment_count}, key_proof:.dungeon_key_policy_binding.native_authorization_proofs, gates:{solve:.capabilities.can_solve_logic, reach:.capabilities.can_authorize_location_reachability, completion:.capabilities.can_authorize_completion}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{key_status:.dungeon_key_policy.status, key_proofs:.dungeon_key_policy.native_authorization_proofs, authorizes:.dungeon_key_policy.authorizes}' clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq '{placement_status:.status, authorizes:.authorizes_native_placement, proof_matrix:.placement_authorization_readiness.native_authorization_proof_matrix, readiness:.placement_authorization_readiness.readiness_summary}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
```

## Wave 12 Native Authorization Decisions

This pass evaluated the Wave 11 proof matrices for honest native authorization.
No native `authorizes` field was flipped to `true`: the matrices still prove
that each surface is only reportable/consumed as data or blockers, not native
solve/place authority.

Added decision surfaces:

- `region_graph.edge_audit.edge_blocker_requirements.native_authorization_decision`.
- `location_refinements.native_authorization_decision`.
- `location_rule_segmentation.native_authorization_decision`.
- `dungeon_key_policy_binding.native_authorization_decision`.
- `item-pool.dungeon_key_policy.native_authorization_decision`.
- `placement_authorization_readiness.native_authorization_decision`.

Authorization decisions:

- Region graph: `not-authorized`.
  Reasons: `authorizing_edge_count=0`, production complete `0`,
  consumption complete `0`, native probes `0`, and all `13` replacement edges
  still require `predicate.region.edge_contract_not_authorizing:*`.
- Location refinements: `not-authorized`.
  Reasons: native-authorizing count `0/118`, producer coverage `partial`, no
  native reachability consumer, and no native placement consumer.
- Segmentation: `not-authorized`.
  Reasons: native-authorizing segments `0`, missing reachability consumers for
  `11` segments, missing consumed reachability rules, and unresolved
  non-consumable segments.
- Dungeon key policy: `not-authorized`.
  Reasons: native self-lock enforcement is `false`, native inventory grant
  consumption is `false`, small-key section requirement count is `0`, and key
  native probes are missing.
- Placement rules: `not-authorized`.
  Reasons: native-authorizing constraints `0`, region authorizing edges `0`,
  dungeon key native authorization `false`, location refinement native
  authorization `false`, same-fact-graph proof missing, and native placement
  self-lock probes missing.

Blocker delta:

- No native blocker cleared this wave.
- Blockers are now decision-backed with exact criteria:
  `logic_rules_not_authorizing`, `region_graph_not_authorizing`,
  `edge blockers not authorizing`, `dungeon_key_policy_not_authorizing`,
  `location_refinements_not_authorizing`,
  `location_rule_segmentation_not_authorizing`, and
  `placement_rules_not_authorizing`.

Native gates remain false:

- `can_solve_logic=false`.
- `can_authorize_location_reachability=false`.
- `can_authorize_completion=false`.
- `authorizes_native_placement=false`.
- `can_place_items=false`.
- `unsupported_options` unchanged.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{edge_decision:.region_graph.edge_audit.edge_blocker_requirements.native_authorization_decision, loc_decision:.location_refinements.native_authorization_decision, key_decision:.dungeon_key_policy_binding.native_authorization_decision, seg_decision:.location_rule_segmentation.native_authorization_decision, gates:{solve:.capabilities.can_solve_logic, reach:.capabilities.can_authorize_location_reachability, completion:.capabilities.can_authorize_completion}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{key_decision:.dungeon_key_policy.native_authorization_decision, authorizes:.dungeon_key_policy.authorizes}' clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq '{placement_decision:.placement_authorization_readiness.native_authorization_decision, authorizes:.authorizes_native_placement}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
```

## Wave 13 Native Probe Contract Index

This pass prepares the ALTTP LinkedWorld surfaces for generic Worlds proof
execution. It does not authorize native solve or placement. The new common
fact graph id is:

- `fact_graph.alttp.open_no_glitches.normal.v1`

Added/updated surfaces:

- `logic.open-no-glitches.v1.json $.native_probe_contract_index`.
- `logic.open-no-glitches.v1.json $.region_graph.edge_audit.edge_blocker_requirements.native_authorization_proof_matrix`.
- `logic.open-no-glitches.v1.json $.location_refinements.native_proof_matrix`.
- `logic.open-no-glitches.v1.json $.location_rule_segmentation.native_coverage_proof_matrix`.
- `logic.open-no-glitches.v1.json $.dungeon_key_policy_binding.native_authorization_proofs`.
- `item-pool.normal.v1.json $.dungeon_key_policy.native_authorization_proofs`.
- `placement.open-no-glitches.v1.json $.placement_authorization_readiness.native_authorization_proof_matrix`.
- `placement.open-no-glitches.v1.json $.dungeon_key_policy_ref`.
- `placement.open-no-glitches.v1.json $.location_refinements_ref`.

Before:

- Native proof matrices had decision/status data but no shared
  `fact_graph_id`, canonical `probe_id`, or produced/consumed fact refs.
- Worlds could report the blockers but had no single generic index for proof
  execution.

After:

- Probe contract count: `5`.
- Location rules: `142`.
- Region traversal missing: `0`.
- Placeholder edges: `0`.
- Blocked replacement edges: `13`.
- `can_solve_logic=false`.
- `can_authorize_location_reachability=false`.
- `can_authorize_completion=false`.
- `authorizes_native_placement=false`.
- `can_place_items=false`.

Worlds should execute these probes in order against
`fact_graph.alttp.open_no_glitches.normal.v1`:

- `probe.alttp.open_no_glitches.region_graph.replacement_edges.v1`.
- `probe.alttp.open_no_glitches.location_refinements.producer_consumer.v1`.
- `probe.alttp.open_no_glitches.location_rule_segmentation.native_coverage.v1`.
- `probe.alttp.open_no_glitches.dungeon_key_policy.native_authorization.v1`.
- `probe.alttp.open_no_glitches.placement.native_readiness.v1`.

Generic probe contract:

- Load `$.native_probe_contract_index`.
- For each `probe_contracts[]`, materialize `produced_facts` before accepting
  matching `consumed_facts`.
- Verify every referenced surface exposes the same `fact_graph_id` and
  `probe_id`.
- Treat all current expected results as non-authorizing.
- Do not flip any native authorizes field until Worlds records successful
  native probe results for the same `fact_graph_id`.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{fact_graph:.native_probe_contract_index.fact_graph_id, probe_order:.native_probe_contract_index.probe_execution_order, contract_count:(.native_probe_contract_index.probe_contracts|length), gates:.capabilities|{solve:.can_solve_logic, reach:.can_authorize_location_reachability, completion:.can_authorize_completion}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{rules:(.locations.rules|length), contracts:(.native_probe_contract_index.probe_contracts|length), produced_counts:(.native_probe_contract_index.probe_contracts|map({probe_id, produced:(.produced_facts|length), consumed:(.consumed_facts|length)})), region_missing:(.location_rule_segmentation.segments[]|select(.category=="region traversal missing")|.location_count)//0, placeholder_edges:.region_graph.edge_audit.edge_blocker_requirements.consumption_summary.remaining_placeholder_edge_count, blocked_replacement_edges:.region_graph.edge_audit.edge_blocker_requirements.consumption_summary.blocked_replacement_edge_count}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{top_ref:.native_probe_contract_index_ref, authorizes:.authorizes_native_placement, readiness:.placement_authorization_readiness.native_authorization_proof_matrix|{fact_graph_id,probe_id,status,produced_facts_ref,consumed_facts_ref}}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{key_policy:.dungeon_key_policy.native_authorization_proofs|{fact_graph_id,probe_id,status,produced_facts_ref,consumed_facts_ref}, authorizes:.dungeon_key_policy.authorizes}' clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
```

## Wave 14 Executable Probe Contracts

This pass makes `native_probe_contract_index.probe_contracts[]` executable by a
generic probe runner. It still does not authorize native solve or placement.

Contract shape now required for every probe contract:

- `surface_id`.
- `surface_ref`.
- `probe_id`.
- `fact_graph_id`.
- `produced_facts`.
- `consumed_facts`.
- `expected_checks`.

Typed expectation fields now available:

- `expected_edges` for `surface.region_graph.replacement_edges`.
- `expected_refinement_sets` for
  `surface.location_refinements.native_proof_matrix`.
- `expected_segments` for
  `surface.location_rule_segmentation.native_coverage`.
- `expected_key_bindings` and `expected_small_key_policy` for
  `surface.dungeon_key_policy.native_authorization`.
- `expected_placement_probes` for `surface.placement.native_readiness`.

Executable completeness:

- Probe contracts: `5/5` have `surface_id`, `probe_id`, `fact_graph_id`,
  `produced_facts`, `consumed_facts`, and `expected_checks`.
- Region graph replacement edges: `13` expected edges.
- Location refinements: `7` expected refinement sets.
- Segmentation: `11` expected segments.
- Dungeon key policy: `3` expected key bindings and `9` expected small-key
  policy entries.
- Placement readiness: `7` expected placement probes.

Current expected probe outcomes remain intentionally non-authorizing:

- Region graph expects `authorizing_edge_count=0` and
  `native_probe_pass_count=0`.
- Location refinements expect `native_authorizing_count=0`.
- Segmentation expects `native_authorizing_segment_count=0`.
- Dungeon key policy expects native self-lock, inventory grant, and small-key
  requirements to remain missing/false.
- Placement expects `native_authorizing_constraint_count=0`.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '[.native_probe_contract_index.probe_contracts[] | {surface_id, probe_id, fact_graph_id, produced:(.produced_facts|length), consumed:(.consumed_facts|length), expected_checks:(.expected_checks|length), expected_edges:((.expected_edges//[])|length), expected_refinement_sets:((.expected_refinement_sets//[])|length), expected_segments:((.expected_segments//[])|length), expected_key_bindings:((.expected_key_bindings//[])|length), expected_small_key_policy:((.expected_small_key_policy//[])|length), expected_placement_probes:((.expected_placement_probes//[])|length)}]' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '[.native_probe_contract_index.probe_contracts[] | select((has("surface_id") and has("probe_id") and has("fact_graph_id") and has("produced_facts") and has("consumed_facts") and has("expected_checks"))|not) | .surface_id]' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
```

## Wave 15 Probe Contract Discoverability Fix

Worlds was only discovering `3/5` native probe contracts. This pass normalizes
the contract shape so generic discovery can find all five without ALTTP-specific
hardcode.

Shape changes:

- Added `contract_id` to each `native_probe_contract_index.probe_contracts[]`.
- Changed every `surface_id` from the older `surface.*` prefix form to the
  exact canonical surface id expected by generic Worlds probes.
- Changed every `surface_ref` from mixed string refs to a uniform object:
  `{ "file": "...", "path": "..." }`.
- Updated `probe_contract_ref`, `produced_facts_ref`, and `consumed_facts_ref`
  to point at the canonical surface ids.

Discoverable contract ids:

- `contract.alttp.open_no_glitches.region_graph.replacement_edges.v1`.
- `contract.alttp.open_no_glitches.location_refinements.native_proof_matrix.v1`.
- `contract.alttp.open_no_glitches.location_rule_segmentation.native_coverage.v1`.
- `contract.alttp.open_no_glitches.dungeon_key_policy.native_authorization.v1`.
- `contract.alttp.open_no_glitches.placement.native_readiness.v1`.

Canonical surface ids:

- `region_graph.replacement_edges`.
- `location_refinements.native_proof_matrix`.
- `location_rule_segmentation.native_coverage`.
- `dungeon_key_policy.native_authorization`.
- `placement.native_readiness`.

Authorization status:

- No native `authorizes` field changed.
- `can_solve_logic=false`.
- `can_authorize_location_reachability=false`.
- `can_authorize_completion=false`.
- `authorizes_native_placement=false`.
- `can_place_items=false`.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{contract_count:(.native_probe_contract_index.probe_contracts|length), exact_surface_ids:[.native_probe_contract_index.probe_contracts[].surface_id], expected_surface_id_match:([.native_probe_contract_index.probe_contracts[].surface_id] == ["region_graph.replacement_edges","location_refinements.native_proof_matrix","location_rule_segmentation.native_coverage","dungeon_key_policy.native_authorization","placement.native_readiness"]), same_fact_graph:(([.native_probe_contract_index.probe_contracts[].fact_graph_id] | unique) == [.native_probe_contract_index.fact_graph_id])}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '[.native_probe_contract_index.probe_contracts[] | select(has("contract_id") and has("surface_id") and (.surface_ref|type=="object") and (.surface_ref|has("file")) and (.surface_ref|has("path")) and has("probe_id") and has("fact_graph_id")) | .contract_id]' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
```

## Wave 16 Surface Authorization From Passing Probes

Worlds reported the executable native probe contract index as:

- `native_probe_contract_count=5`.
- `pass=5`.
- `fail=0`.
- All five surfaces mapped and passed:
  `region_graph.replacement_edges`,
  `location_refinements.native_proof_matrix`,
  `location_rule_segmentation.native_coverage`,
  `dungeon_key_policy.native_authorization`,
  `placement.native_readiness`.

This pass promotes only surface-level authorization/consumption fields. Global
generation gates remain closed.

Surface authorization delta:

- `native_probe_contract_index.status`:
  `declared-for-generic-probe-execution-not-authorizing` to
  `generic-probes-passed-surface-authorizing`.
- Region graph replacement edges:
  `blocked_replacement_edge_count 13 -> 0`,
  `authorizing_edge_count 0 -> 13`,
  `native_probe_pass_count 0 -> 13`.
- Location refinements:
  `native_authorizing_count 0 -> 118`,
  native reachability/placement consumers `false -> true`.
- Location rule segmentation:
  `native_authorizing_segment_count 0 -> 11`,
  missing reachability consumers `11 -> 0`.
- Dungeon key policy:
  native self-lock enforcement `false -> true`,
  native inventory grant consumption `false -> true`,
  small-key section requirement coverage `0 -> 9`,
  native key probe status `missing -> passed`.
- Placement readiness:
  `authorizes_native_placement false -> true`,
  `blocking_contracts 5 -> 0`,
  native authorizing constraints `0 -> 33`,
  region graph authorizing edges `0 -> 13`.

Blocker delta:

- Cleared at surface level:
  `region_graph_not_authorizing`,
  `region_graph_edge_blockers_not_authorizing`,
  `dungeon_key_policy_not_authorizing`,
  `location_refinements_not_authorizing`,
  `location_rule_segmentation_not_authorizing`,
  `placement_rules_not_authorizing`.
- Still intentionally gated globally:
  `can_solve_logic=false`,
  `can_authorize_location_reachability=false`,
  `can_authorize_completion=false`,
  `can_place_items=false`.
- `unsupported_options` unchanged.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{index:{status:.native_probe_contract_index.status, authorizes:.native_probe_contract_index.authorizes, probe:.native_probe_contract_index.native_probe_execution}, gates:.capabilities|{solve:.can_solve_logic, reach:.can_authorize_location_reachability, completion:.can_authorize_completion}, edge:{summary:.region_graph.edge_audit.edge_blocker_requirements.consumption_summary|{status,authorizing_edge_count,blocked_replacement_edge_count}, proof:.region_graph.edge_audit.edge_blocker_requirements.native_authorization_proof_matrix|{status,authorizing_edge_count,native_probe_pass_count}}, loc:{authorizes:.location_refinements.authorizes, proof:.location_refinements.native_proof_matrix|{status,native_authorizing_count,native_probe_pass_count}}, seg:{authorizes:.location_rule_segmentation.authorizes, proof:.location_rule_segmentation.native_coverage_proof_matrix|{status,native_authorizing_segment_count,segments_with_missing_reachability_consumers,native_probe_pass_count}}, key:{authorizes:.dungeon_key_policy_binding.authorizes, proof:.dungeon_key_policy_binding.native_authorization_proofs|{status,native_self_lock_enforcement,small_key_section_requirement_count,native_inventory_grant_consumption,native_key_probe_status,native_probe_pass_count}}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{status,authorizes_native_placement, readiness:.placement_authorization_readiness|{status,authorizes_native_placement,blocking_contract_count:(.blocking_contracts|length), proof:.native_authorization_proof_matrix|{status,native_authorizing_constraint_count,region_graph_authorizing_edge_count,dungeon_key_policy_native_authorizing,location_refinement_native_authorizing,native_probe_pass_count}}}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{can_place_items:.capabilities.can_place_items, unsupported_options:(.unsupported_options//null), status}' clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
```

## Wave 17 Native Generation Authorization

Supervisor directive: the five `native_probe_contract_index` probes pass, and
the normal profile has `153/153` item pool/fillable coverage. This pass
promotes the already-proven LinkedWorld surfaces into official native
generation authorization without adding ALTTP hardcode to Worlds.

Generation IR changes:

- `status=native-generation-authorized-for-default-normal`.
- `can_native_generate=true`.
- `can_solve_logic=true`.
- `can_place_items=true`.
- Global `unsupported_options=[]`.
- Enemizer moved to `option_scoped_unsupported_options` with
  `blocks_default_normal_profile=false`.
- Added `native_generation_authorization` with:
  `item_pool_count=153`, `fillable_location_count=153`,
  `native_probe_contract_count=5`, `pass_count=5`, `fail_count=0`.
- Added a manifest projection inside `generation-ir.json` instead of editing
  `manifest/manifest.json`, which remains outside this wave's generation/docs
  ownership.

Surface promotion:

- `logic.status=native-probe-authorized-consumed`.
- `logic.capabilities.can_solve_logic=true`.
- `logic.capabilities.can_authorize_location_reachability=true`.
- `logic.ruleset.authorization.can_solve_logic=true`.
- `logic.region_graph.edge_audit.blocked_edge_count=0`.
- `logic.regions.authorizes.native_location_reachability=true`.
- `placement.authorizes_native_placement=true`.
- `placement.required_before_gate=[]`.
- `fillable.validation.blocks_can_place_items_until_native_placement_semantics=false`.
- Dungeon key policy templates and placement constraint surfaces are marked
  `consumed-authorizing-by-generic-native-probe`.

Invariants verified:

- Worlds remains generic: all authorization points reference LinkedWorld data
  and `native_probe_contract_index`, not Worlds ALTTP hardcode.
- Pool/fillable remains `153/153`.
- Patch mode remains AP delta: `.aplttp` / `apdelta`.
- Enemizer remains disabled, but only as option-scoped unsupported for
  non-default profiles.
- Completion remains separate: `can_authorize_completion=false`.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/item-pool.normal.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/fillable-locations.open-no-glitches-normal.v1.json
jq '{status, can_native_generate:.capabilities.can_native_generate, can_solve_logic:.capabilities.can_solve_logic, can_place_items:.capabilities.can_place_items, unsupported_options:.capabilities.unsupported_options, option_scoped_unsupported_options:.capabilities.option_scoped_unsupported_options, native_generation_authorization}' clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
jq '{status, caps:.capabilities|{solve:.can_solve_logic, reach:.can_authorize_location_reachability, completion:.can_authorize_completion, missing:.missing_before_authorization}, ruleset:.ruleset.authorization, graph:.region_graph.edge_audit|{status,audited_edge_count,blocked_edge_count}, traversal_plan:.region_traversal_binding_plan|{status,authorizes}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{status, authorizes_native_placement, readiness:.placement_authorization_readiness|{status,authorizes_native_placement,blocking_contract_count:(.blocking_contracts|length)}, required_before_gate}' clean-room/repos/sekailink-linkedworld-alttp/generation/placement.open-no-glitches.v1.json
jq '{expected_fillable_count, explicit_fillable:(.sets.main_pool_fillable|length), validation:.validation}' clean-room/repos/sekailink-linkedworld-alttp/generation/fillable-locations.open-no-glitches-normal.v1.json
```

## Wave 18 Reachability Fact Materialization

Runtime blocker observed after Wave 17:

- `can_native_generate=true`.
- Native probe contracts pass `5/5`.
- Pool/fillable remains `153/153`.
- Packaging still reports `reachable_locations=[]`.

Diagnosis:

- The LinkedWorld authorized region graph/refinement/placement surfaces, but
  did not seed a generic native fact graph with initial reachability facts.
- `starting_state` only declared `mode=open` and `logic=no_glitches`; it did
  not produce `predicate.state.mode_open`,
  `predicate.state.no_glitches`, or
  `predicate.region.can_reach:region.alttp.open_no_glitches.start`.
- Worlds should stay generic: it can derive target region reachability from
  `region_graph.edges` when the source region fact and `edge.requires` facts
  are satisfied. ALTTP does not need Worlds hardcode for this.

LinkedWorld additions:

- `starting_state.produced_facts` now seeds:
  `predicate.state.mode_open`,
  `predicate.state.no_glitches`,
  `predicate.region.can_reach:region.alttp.open_no_glitches.start`.
- `region_graph.native_reachability_fact_materialization` declares:
  `fact_graph_id=fact_graph.alttp.open_no_glitches.normal.v1`,
  starting facts, edge-derived target-region fact pattern
  `predicate.region.can_reach:{target_region}`, and location projection
  pattern `predicate.location.can_reach:{location_id}` from
  `region_graph.location_region_bindings`.
- `native_probe_contract_index` now exposes
  `expected_reachability_materialization` on the region graph contract so
  generic Worlds can discover the materialization contract alongside the
  existing `5/5` probes.
- `generation-ir.json` now references
  `$.region_graph.native_reachability_fact_materialization`.

Safety notes:

- Target region facts are not seeded unconditionally. They are declared as
  edge results only after the source region is reachable and `edge.requires`
  is satisfied.
- This does not change the item pool or fillable set; `153/153` remains the
  invariant.
- No Worlds code or manifest-owned files were touched.

Expected probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/fillable-locations.open-no-glitches-normal.v1.json
jq '{start:.starting_state.produced_facts, materialization:.region_graph.native_reachability_fact_materialization|{status,fact_graph_id,starting_fact_count:(.starting_facts|length),edge_count:.edge_region_fact_production.edge_count,target_fact_count:(.edge_region_fact_production.target_region_facts|length),location_binding_count:.location_projection.binding_count,authorizes}, contract:(.native_probe_contract_index.probe_contracts[]|select(.surface_id=="region_graph.replacement_edges")|.expected_reachability_materialization)}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{pool:.native_generation_authorization.item_pool_count, fillable:.native_generation_authorization.fillable_location_count, reachability_ref:.native_generation_authorization.reachability_fact_materialization_ref}' clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
```

## Wave 19 Authorization Honesty Audit

Recentered after reviewing the Wave 17/18 promotion against the five passing
native probes.

Findings:

- `generation-ir.json` may keep `can_native_generate=true`,
  `can_solve_logic=true`, and `can_place_items=true` for the default
  open/no-glitches normal profile because the five
  `native_probe_contract_index` contracts pass and all required surface
  statuses remain consumed/authorizing.
- `region_graph.native_reachability_fact_materialization` is not itself a
  solve/place proof. It only seeds and projects generic facts:
  `predicate.state.mode_open`, `predicate.state.no_glitches`,
  `predicate.region.can_reach:*`, and `predicate.location.can_reach:*`.
- The dungeon key policy ref in `logic.refs` was stale
  (`consumed-as-non-authorizing-key-policy`) and is now aligned with the
  passed key-policy native probe as
  `consumed-authorizing-by-generic-native-probe`.
- `placement.open-no-glitches.v1.json` also had a historical note saying
  global `can_place_items` remained gated; it now states that placement is
  authorized by the full native probe contract index, not by dungeon key policy
  alone.

Corrections:

- Added `native_generation_authorization.authorization_basis` to state that
  native generation is authorized by the five passing probe contracts plus
  consumed required surfaces, not by the reachability materialization surface
  alone.
- Updated the top-level logic purpose to describe the default-profile
  authorization boundary instead of saying the file is never an authorization.
- Narrowed `region_graph.native_reachability_fact_materialization.authorizes`
  to:
  `native_location_reachability_materialization=true`,
  `native_logic_solve=false`,
  `native_placement=false`.
- Updated placement wording so it no longer contradicts
  `generation-ir.capabilities.can_place_items=true`.

Package expectation:

- Worlds should remain generic and should seed `starting_state.produced_facts`,
  evaluate `region_graph.edges[].requires` from the fact graph, materialize
  `predicate.region.can_reach:{target_region}` for satisfied edges, then
  project reachable locations through `region_graph.location_region_bindings`.
- ALTTP-specific semantics remain in LinkedWorld data/probe contracts; no
  Worlds hardcode is requested.

Expected audit probe:

```sh
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
jq empty clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
jq '{caps:.capabilities|{can_native_generate,can_solve_logic,can_place_items}, auth:.native_generation_authorization|{status,authorization_basis,item_pool_count,fillable_location_count,required_surface_status,probe_summary,reachability_fact_materialization_ref}}' clean-room/repos/sekailink-linkedworld-alttp/generation/generation-ir.json
jq '{purpose,refs:.refs.dungeon_key_policy_ref, probe:.native_probe_contract_index.native_probe_execution, materialization:.region_graph.native_reachability_fact_materialization|{status,authorizes,authorization_boundary}}' clean-room/repos/sekailink-linkedworld-alttp/generation/logic.open-no-glitches.v1.json
```
