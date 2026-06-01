# Generic Generator Contract

Date: 2026-05-10

## Role

`Worlds` is the official generator.

It does not own permanent game knowledge. It consumes `LinkedWorld` modules and produces a concrete seed package that every runtime component can trust.

The core unit is a multiworld generation job, not a single-game seed. A job is
assembled from one or more Nexus config versions, one slot per participating
player/run, and one LinkedWorld generation surface per slot. ALTTP solo is only
the first validation case; the contract must stay valid for mixed rooms such as
ALTTP + Earthbound + Mega Man + Pokemon.

The native implementation is capability-gated: every game can be discovered, but a seed can only be generated when its `LinkedWorld` generation surface declares enough information to validate options, build the item pool, solve logic, place items, emit patch instructions, and emit room contracts without external tools.

`can_native_generate` is the final conjunction of those gates for every slot in
the room. A missing generation item pool or missing item-pool construction
rules must keep the affected slot, and therefore the whole room seed package,
out of native generation even when tracker item metadata is present.

## Archipelago Reference Policy

Archipelago and MultiworldGG are priority behavioral references for the
SekaiLink generator. The goal is to learn the proven invariants behind their
world lifecycle, item pools, fill algorithms, accessibility checks,
sphere/replay validation, patch outputs, and room/client contracts before we
invent alternate behavior.

This permission is intentionally stronger than the earlier clean-room posture:
developers should inspect upstream behavior closely enough to reproduce the
same safety model, edge-case handling, and test expectations in SekaiLink terms.

That permission does not make them source dependencies.

Rules:

- Preserve the generic `LinkedWorld` boundary. Any game-specific behavior
  learned from Archipelago must become LinkedWorld-owned data, declared
  capabilities, or a game-owned native surface behind the LinkedWorld.
- Do not copy implementation code 1:1.
- Do not introduce Python as a final production dependency for native
  generation.
- Do not bypass licenses by mechanically translating large source bodies.
- Prefer clean-room C++ implementations of the same concepts and invariants:
  deterministic item pools, constrained fill, reachability/sphere replay,
  patch-contract emission, and multiworld package consistency.
- ALTTP may be the first validation target, but it must never become a branch
  inside the generic Worlds path.

In short: match the behavior and safety model where appropriate; keep the
implementation, ownership, and package contract SekaiLink-native.

## Canonical Multiworld Flow

The canonical flow is:

1. Nexus stores one or more immutable config versions for the room.
2. The generation request maps each participating player/run to a slot.
3. Each slot references exactly one Nexus config version.
4. Each slot resolves exactly one `LinkedWorld` generation surface from its
   selected game/config version.
5. `Worlds` loads all requested `LinkedWorld` surfaces for the room.
6. `Worlds` asks those surfaces to validate options and provide generation data.
7. `Worlds` creates one shared placement graph across all slots.
8. `Worlds` emits one immutable seed package for the entire room.

The shape is therefore:

```text
Nexus config versions -> slots -> LinkedWorld generation surfaces -> one seed package
```

This is true for solo, same-game multiworld, and mixed-game multiworld. A
single-slot ALTTP seed is still represented as a one-slot room package; it is
not a special generator mode.

## Ownership Boundaries

`LinkedWorld` owns game knowledge.

That includes:

- option schema and option validation rules
- item and location catalogs
- logic and reachability data
- item pool construction rules and generated item pool refs
- placement constraints
- patch or launch artifact policy
- SKLMI, tracker, and Link Room contract metadata for that game

`Worlds` owns orchestration and packaging.

That includes:

- loading the requested `LinkedWorld` generation surfaces
- enforcing capability gates
- assigning slots to players and config versions
- coordinating cross-slot placement
- writing the canonical seed package files
- hashing and auditing the emitted artifacts
- refusing generation when a requested `LinkedWorld` cannot prove support

`Worlds` must not hardcode ALTTP, Earthbound, Mega Man, Pokemon, or any other
game-specific behavior into the generic generation path. Game-specific native
code may exist behind a `LinkedWorld` generation surface, but the generic
multiworld generator only consumes declared capabilities and data.

## Patch Contract Policy

Every generated slot has a patch contract, even when that slot does not need a
binary patch file.

Archipelago and MultiworldGG are priority behavioral references for patch
semantics. SekaiLink should match the safety model and user-visible package
expectations where they are relevant, while keeping the implementation
SekaiLink-native. Do not copy upstream code into the native generator, do not
bypass licenses, and do not require Python for final native generation.

The generic generator must branch on declarative `patch.mode`, not on game id,
file extension, emulator family, or launcher name. This keeps patched games
such as ALTTP and server-port games such as Twilight Princess or Ship of
Harkinian in the same seed-package model.

The minimal per-slot shape is:

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "mode": "artifact",
  "artifact_kind": "apdelta",
  "artifact_extension": ".aplttp",
  "base_asset": {
    "required": true,
    "validation": {
      "hash_algorithm": "md5",
      "accepted_hashes": ["03a63945398191337e896e5771f77173"]
    }
  },
  "emission": {
    "package_path": "patches/slot-1.aplttp",
    "per_slot": true
  },
  "runtime_metadata": {
    "server": "",
    "player": 1,
    "player_name": "Jade"
  }
}
```

Known mode semantics:

- `artifact`: Worlds emits or imports a per-slot patch artifact inside the
  single room seed package. ALTTP `.aplttp` is the proof target for this mode.
- `none`: no binary patch is produced. The slot still receives a
  `slot_manifest` patch contract so clients and servers do not guess.
- `server_dispatch`: the runtime receives seed state from a server-port or
  live service API instead of applying a local patch artifact. The contract
  must name the dispatch target and required room/session fields.
- `contract_only`: Worlds emits only metadata/runtime contracts for the slot.
  This is useful for server-port games whose executable or data package is
  installed separately and whose seed behavior is driven by room contracts.
- `external_import`: an external generator was used temporarily and its output
  has been normalized into this package. This mode is not native generation
  unless the LinkedWorld also proves the same gates as any other slot.

These strings are a versioned vocabulary, not game-specific switches. Adding a
new mode requires updating this contract and tests, but it must still describe
transport semantics rather than a named game.

Worlds carries the selected `patch.mode` into `seed_manifest.linkedworlds[]`,
`slot_manifest.<slot>.json`, and the per-slot patch contract as `patch_mode` so
downstream runtimes can route artifact, server-dispatch, no-artifact, and
contract-only slots without inspecting game-specific patch payloads.

`can_emit_patch` means "can emit the declared patch contract for this slot".
For `artifact`, it also means the artifact can be produced or imported and
hashed. For `none`, `server_dispatch`, and `contract_only`, it means Worlds can
write an explicit no-artifact contract with enough launch/runtime metadata for
the consumer to proceed safely.

The seed package remains singular. A mixed room with one ALTTP artifact slot and
one server-port slot still produces one `seed_manifest.json`, one
`placements.json`, one room contract, and one `slot_manifest.<slot>.json` plus
one `patch` contract per slot. It must not degrade into independent per-game
seeds.

## AP/MultiworldGG Behavior Translation

SekaiLink patch contracts should preserve these behavioral expectations from
the AP-style patch ecosystem:

- A patch artifact is a per-player or per-slot container, not just a raw binary
  diff.
- The artifact carries enough metadata to identify the target game, player,
  player display name, and optional server/connect target.
- The artifact declares its accepted input/base checksum and expected output
  extension.
- The artifact declares a versioned patch procedure or an equivalent native
  operation plan.
- Patch application produces a runtime-loadable output file, while the seed
  package remains the durable source of truth.
- Unknown procedure steps, unsupported artifact versions, checksum mismatches,
  or missing source assets are hard failures, not best-effort guesses.

SekaiLink-native translation:

- `patch.mode=artifact` carries those expectations directly in
  `patch_contracts/slot-<slot>.json` and, when applicable, in the artifact
  manifest.
- `patch.mode=none`, `server_dispatch`, and `contract_only` still carry player,
  slot, room, config, and dispatch metadata so no runtime guesses what to do.
- Native generation may import AP/MultiworldGG output for validation, but a
  final native path must emit the SekaiLink seed package and patch contracts
  without invoking Python-only patchers.
- License-sensitive upstream behavior is translated as contract semantics and
  test expectations, not copied code.

## Minimal LinkedWorld Generation Refs

The next minimal `LinkedWorld` generation contract should expose explicit refs
for the three generation surfaces that Worlds needs before native placement can
be trusted:

- `item_pool_ref`: points to generated item-pool data or deterministic
  item-pool construction rules for the active Nexus config version.
- `logic_rules_ref`: points to the logic and reachability rules consumed by the
  solver.
- `placement_rules_ref`: points to placement constraints consumed by the placer,
  including game-owned restrictions such as rewards, dungeon/key rules, forced
  placements, or filler policy when applicable.

These refs are declarations of available LinkedWorld-owned data, not gate
openers by themselves. A ref may exist before Worlds can use it. The relevant
native gates stay closed until the generic solver, item-pool builder, and placer
actually consume the referenced data and tests prove the behavior for the active
config.

In particular:

- `item_pool_ref` does not open `can_build_item_pool` unless Worlds can build or
  load the actual pool for the active config.
- `logic_rules_ref` does not open `can_solve_logic` unless the solver consumes
  those rules.
- `placement_rules_ref` does not open `can_place_items` unless the placer
  consumes those rules.
- Candidate or declared-only logic must remain non-authorizing. Surfaces marked
  `candidate-not-consumed`, `declared-not-consumed`, `not-authorizing`, or
  equivalent audit blockers are reported as effective gate failures even if a
  future LinkedWorld accidentally flips the boolean capability early.
- Probe/readiness diagnostics must keep capability blockers distinct from
  declared-surface blockers. If `can_solve_logic` or `can_place_items` is still
  false but the LinkedWorld already exposes non-authorizing logic, placement,
  dungeon/key, reward, medallion, or completion contracts, Worlds reports both:
  the capability gate and the specific non-authorized surface.
- Placement contracts may expose `authorizes_native_placement:false` or
  `blocks_can_place_items_until_audited:true`; Worlds treats those as generic
  blockers, not ALTTP-specific exceptions.
- Region traversal placeholders such as declared-but-not-consumed traversal
  predicates remain blockers until the LinkedWorld declares a matching consumed
  edge, with explicit native logic authorization and a replacement requirement
  that no longer contains the placeholder predicate.
- If a consumed replacement edge sets `proof_required:true` or provides
  `proof`/`native_proof`, Worlds requires that proof to be consumed,
  and list every replacement `requires` fact in both `produced_facts` and
  `consumed_facts`. Native logic authorization may be carried by either the
  consumed edge surface or the proof object. The enclosing region graph must
  still authorize native logic before the final region-graph blocker clears.
  The probe reports missing produced and consumed replacement-edge fact counts
  separately.
- Dungeon/key policy authorization is conjunctive for declared native surfaces:
  if a policy exposes `native_placement`, `native_item_pool`,
  `native_dungeon_key_policy`, or `native_key_policy`, each declared flag must
  authorize Worlds before the policy can stop blocking readiness.
- Dungeon/key policy proofs are generic: a policy proof may prove small-key
  pool coverage with `small_key_proof.proved_item_ids`, and a logic binding
  proof may prove self-lock coverage with
  `self_lock_proof.proved_location_ids`. Missing counts are reported in the
  probe and keep `dungeon_key_policy_not_consumed` blocked until complete.
- Progressive item effects may be consumed when the LinkedWorld declares a
  generic level-aware effect with `type:"progressive"` and ordered `stages[]`
  or `levels[]`, where each stage may expose flat `grants[]`. Worlds tracks the
  collected level per receiving slot and effect key during placement and replay.
- Repeated item families may expose `count_grants.thresholds[]` with integer
  `count` and flat `grants[]`. Worlds increments the count per receiving slot
  and effect key during placement and replay. Ambiguous map encodings such as
  `grant_by_count`, `grant_by_level`, or `progressive_grants` stay blocked
  until explicitly implemented and tested.
- Logic and effect surfaces may be direct or wrapped with audit metadata.
  `locations` and `locations.rules` are equivalent rule arrays; `item_effects`
  and `item_effects.effects` are equivalent effect maps.

## Minimal Region Graph Contract

For a generic region-graph solver, `logic_rules_ref` should resolve to a
LinkedWorld-owned region graph that has no game-specific field names in Worlds.
The minimal consumable shape is:

```json
{
  "schema_version": "sekailink-logic-rules-v1",
  "rule_language": "sekailink-expr-v1",
  "regions": [
    {"id": "region.start"},
    {"id": "region.locked"}
  ],
  "starting_regions": ["region.start"],
  "edges": [
    {
      "from_region": "region.start",
      "to_region": "region.locked",
      "requires": {"op": "fact", "id": "fact.can_reach_locked_region"}
    }
  ],
  "location_region_bindings": [
    {"location_id": "location.open", "region": "region.start"},
    {"location_id": "location.locked", "region": "region.locked"}
  ],
  "fact_names": [
    "fact.can_reach_locked_region"
  ]
}
```

The fields mean:

- `starting_regions`: non-empty array of opaque region ids reachable at sphere
  zero for that slot after option validation. Each id must also appear in the
  region declaration vocabulary.
- `regions`, `nodes`, or `region_ids`: the opaque region declaration
  vocabulary. Worlds accepts string ids or objects with `id`, `region_id`, or
  `region`; it does not infer game meaning from those ids.
- `edges`: directed traversal rules between opaque region ids. Each edge must
  have `from_region`, `to_region`, and a `requires` expression in
  `sekailink-expr-v1`; unconditional traversal uses `{"op":"true"}`. Edge
  endpoints must resolve to declared region ids.
- `location_region_bindings`: mapping from every fillable or logic-relevant
  location id to exactly one region id. Location ids must match the ids exposed
  through the location catalog and placement/fillable surfaces, and bound
  regions must resolve to declared region ids.
- `fact_names`: the declared fact vocabulary used by starting state, edge
  requirements, location requirements, and item-effect grants. Worlds treats
  fact names as opaque strings and must not infer game semantics from them. If
  present, every fact referenced by the generic surface must be declared.

The region graph is slot-local unless a future contract explicitly declares a
cross-slot traversal effect. Multiworld item sends still grant facts to the
receiving slot through generic item effects; they do not make one game's region
ids meaningful to another game.

For now, these fields are contract data. They do not open `can_solve_logic` or
`can_place_items` until native C++ traversal consumes `starting_regions`,
declared region ids, `edges`, and `location_region_bindings`, validates all
referenced `fact_names`, and replay proves the same reachability result from
the emitted placements.

The C++ generic generator now understands this shape and derives opaque
`predicate.region.can_reach:<region_id>` facts per slot. A LinkedWorld still
must keep the native gates closed while its graph is marked
`declared-not-consumed` or exposes `authorizes.native_logic_solve=false`.
Those authorization fields are blockers, not hints.

The placer also keeps placement constraints generic. When several reachable
locations can hold the current item, Worlds rejects candidates that would make
the remaining item/location bipartite placement impossible under the declared
item and location tags. This is a static dead-end guard, not game knowledge: it
prevents consuming a unique remaining location needed by another opaque item
family before replay validates region reachability, progressive effects, and
count effects in order.

For large packages, the static dead-end guard is intentionally bounded. Full
bipartite matching on every candidate is too expensive for complete game pools
and should not block native package emission once progression placement has
already advanced through reachable spheres. Worlds may therefore run the full
static guard only on small remaining sets, while preserving structural replay
and placement-constraint validation for the whole package.

When all remaining items are non-advancement items and no additional reachable
sphere can be opened, Worlds may switch to `post_logic_fill`. This is a generic
late-fill policy, not an ALTTP exception. It only applies after progression
items have been placed or exhausted, it must still satisfy placement tag
constraints, and every such placement must be marked with
`"post_logic_fill": true` in `placements.json` and counted in `audit.json` and
`seed_manifest.json`. This policy is especially important for future
server-dispatch or no-patch games where the package contract drives runtime
state but no local patch artifact exists.

Replay currently runs for the whole package and emits
`full-logic-replay-passed` only after count, progressive, placement-constraint,
and non-`post_logic_fill` reachability checks complete. If a future native pass
needs a large-package replay summary for performance, it must use an explicit
audit status such as `structure-and-placement-constraints-passed-large-package`
and document which replay phase was intentionally summarized instead of
silently pretending a costly validation ran.

If no candidate can be placed, Worlds reports bounded generic diagnostics with
remaining item and location counts, reachable and blocked location samples,
unplaceable item samples, and whether a static item/location matching still
exists. These diagnostics name opaque ids and declared `requires` expressions
only; they do not infer ALTTP or any other game semantics.

The generic probe is the dry-run readiness surface for LinkedWorld authors. It
reports blockers grouped by capability, item-pool, solver, placer, patch, and
surface phases; patch-mode expectations for `artifact`, `contract_only`,
`server_dispatch`, `none`, and `external_import`; and the multiworld package
invariant that the generated item pool must match the effective fillable
location count for the slot before Worlds can authorize native generation.

LinkedWorlds may add `native_probe_contract_index` on generic logic,
placement, or item-pool contract surfaces. The index may be a flat
`contracts[]` array or grouped by surface keys such as `region_graph`,
`location_refinements`, `location_rule_segmentation`, `dungeon_key_policy`, or
`placement`; individual contracts may also declare `surface_id`,
`surface_ref`, `surface`, or `family`. Worlds executes those contracts
natively during probe: it builds generic checks from the declared location,
logic, refinement, segmentation, region graph, and placement surfaces, injects
declared `facts`, `produced_facts`, and `consumed_facts`, derives region
reachability, and evaluates `requires`, `expected_facts`,
`expected_reachable_regions`, `expected_reachable_locations`, and
`expected_placeable_items`. The probe exposes `native_probe_contract_count`,
`native_probe_pass_count`, `native_probe_fail_count`, `native_probe_by_surface`,
and failure samples. Failing contracts keep readiness blocked via
`native_probe_contract_failed`; passing contracts do not flip capabilities by
themselves.

Worlds may consume optional per-location refinement surfaces named
`location_refinements`, `per_location_refinements`, or
`location_rule_refinements`, and segmented rule surfaces named
`location_rule_segmentation`, only when the surface is explicitly marked
consumable and authorizes native logic solve or native location reachability.
Consumed refinements and segments add opaque `requires` expressions to matching
location ids; non-authorizing or candidate surfaces remain blockers rather than
quietly opening native generation.

Those optional surfaces may also provide native proof through `proof` or
`native_proof` when `proof_required:true` is set. A proof must be consumed,
match the declared `fact_graph.id` when both ids are present, and list every
referenced refinement fact in both `produced_facts` and `consumed_facts`.
Native logic authorization may be carried by either the refinement surface or
the proof object. Segmentation proofs may additionally declare
`coverage.expected_location_count`, `coverage.covered_location_count`, and
`coverage.uncovered_location_ids`; incomplete coverage keeps
`location_rule_segmentation_not_authorizing` blocked. The probe reports exact
missing proof counts for location refinements, per-location refinements,
location-rule refinements, segmentation facts, and segmentation coverage.

`locations.rules` is also reported as a refinement-readiness surface in the
probe. Worlds accepts either `id` or `location_id` for matching, but rejects
ambiguous primary rules: duplicate location rules, conflicting duplicate rules,
catalog `requires` plus rule `requires` without an explicit additive merge
policy, and refinement or segment facts not declared in `fact_names` are
generic blockers.

## YAML Boundary

YAML is an import/export compatibility format only.

YAML may be used to:

- import legacy player settings into Nexus
- export a human-readable snapshot for debugging or external tools
- compare against upstream randomizer fixtures during validation

YAML must not be treated as:

- the authoritative runtime config store
- the canonical seed package format
- a side channel that bypasses Nexus config versions
- a reason for `Worlds` to learn game-specific option semantics

At generation time, `Worlds` consumes Nexus config versions or an equivalent
imported projection that preserves the same version identity. Once imported,
the durable truth is the Nexus config version and the generated seed package,
not the source YAML file.

## Inputs

`Worlds` consumes:

- `generation_request`
- `room_id`
- `seed_id` or entropy source
- `players`
- `player_options`
- `config_version_id` per slot, stored in Nexus
- `linkedworld_id` per slot, resolved from the selected game/config version
- `LinkedWorld` manifest and generation surface
- patch policy declared by the LinkedWorld
- Nexus-visible user and room identity metadata

The generator must read player options from Nexus config versions or an
equivalent imported projection. YAML files are an import/export compatibility
format, not the authoritative runtime input.

## Outputs

`Worlds` produces one immutable seed package.

The package must contain:

- `seed_manifest.json`
- `room_manifest.json`
- `slot_manifest.<slot>.json`
- `placements.json`
- `checks.json`
- `items.json`
- `tracker_seed_state.json`
- `sklmi_seed_contract.json`
- `link_room_seed_contract.json`
- `patch_contracts/slot-<slot>.json`
- `patches/` or launch artifacts only when the slot's `patch.mode` needs them
- `audit.json`

## Seed Manifest

`seed_manifest.json` is the durable top-level truth.

Required fields:

- `schema_version`
- `seed_id`
- `created_at`
- `generator_version`
- `generation_algorithm`
- `generation_scope`
- `rng_seed_commitment`
- `room_id`
- `players`
- `slot_count`
- `linkedworlds`
- `linkedworld_count`
- `config_versions`
- `artifact_hashes`
- `nexus_projection_keys`

## Room Manifest

`room_manifest.json` is the Link Room-facing truth.

Required fields:

- `room_id`
- `seed_id`
- `generation_scope`
- `slots`
- `slot_count`
- `linkedworld_count`
- `config_versions`
- `location_count`
- `item_count`
- `event_surface`
- `send_receive_rules`
- `client_log_labels`
- `completion_rules`
- `goal_rules`

## Slot Manifest

Each `slot_manifest.<slot>.json` is the client-facing truth.

Required fields:

- `slot_id`
- `user_id`
- `display_name`
- `linkedworld_id`
- `player_options`
- `config_version_id`
- `launch_artifact`
- `patch_artifact`
- `patch_contract_ref`
- `tracker_seed_state`
- `sklmi_contract_ref`
- `link_room_contract_ref`

`patch_artifact` may be null when `patch.mode` is `none`,
`server_dispatch`, or `contract_only`. `patch_contract_ref` is always required
and points at the slot's explicit patch contract.

Before runtime launch, package verifier requirements in
`PACKAGE_VERIFIER_REQUIREMENTS.md` must prove every slot ref exists, every
`patch_contract_ref` exists and is hashed, `patch_artifact` is required only for
`artifact` mode, and non-artifact modes cannot invoke the `.aplttp` patcher.
The generic generator enforces these package invariants against the in-memory
package file map before writing files. JSON-only package entries may use the
legacy string sha256 hash during the transition; future materialized binary
artifacts must use the object hash contract that identifies raw-byte hashing.

## Placements

`placements.json` maps generated logic:

- location id
- location name
- owning slot
- receiving slot
- item id
- item name
- classification
- advancement flag
- progression sphere when available
- source LinkedWorld reference

Placements are global to the multiworld. A location owned by one slot may
receive an item for another slot, and Link Room must be able to distribute that
result without re-running game logic.

The item pool is a generation surface, not the same thing as tracker item
icons. `LinkedWorld` should expose an explicit `item_pool_ref` or equivalent
item-pool construction rules. Tracker-facing `items_ref` may be used for UI
metadata, but it must not be mistaken for the final fillable item pool of a
real seed. If a game has 286 fillable checks and 145 tracker item definitions,
that is not a valid placement pool until the LinkedWorld also declares filler,
pre-placements, rewards, dungeon/key constraints, and every other rule needed
to make the pool match the seed's fillable locations.

For new or updated LinkedWorld generation surfaces, `can_build_item_pool` should
be true only when at least one of these is true:

- the `LinkedWorld` exposes a generated item pool for the active config
- the `LinkedWorld` exposes deterministic construction rules that build that
  pool from Nexus config versions and game-owned data

Worlds also verifies the actual surface requirements. If a legacy LinkedWorld
has `can_build_item_pool=true` but exposes only tracker-facing metadata such as
icons, labels, actions, or display classifications,
`missing_native_generation_reasons` must include `missing_generation_item_pool`.
If an `item_pool_ref` exists but expands to an empty `items[]`,
`missing_native_generation_reasons` must include `empty_generation_item_pool`.
In both states `can_native_generate` must stay false, even if location catalogs,
patch metadata, SKLMI contracts, tracker bindings, or declarative logic and
placement refs are otherwise present.

## Runtime Contracts

`sklmi_seed_contract.json` is the seed-specific runtime view for SKLMI.

It may reference the canonical LinkedWorld bridge, but it can also include seed-specific values such as slot id, player id, enabled events, Nexus config version, and tracker metadata bindings.

`link_room_seed_contract.json` is the seed-specific room view for Link Room.

It must include the exact location and item distribution rules Link Room needs to distribute items without guessing.

## Import Rule

If an external generator is used temporarily, its output must be imported and normalized into this seed package shape before a run is considered valid.

External randomizer ROMs or patches are not valid runtime truth on their own.

For mixed multiworld validation, every imported external output must still be
normalized into the same one-room seed package. Multiple independent single-game
patches are not a Sekailink multiworld unless the shared placements, slot
manifests, and Link Room contract agree.

## Native C++ Surface

Implemented entry points:

- `load_linkedworld_generation_surface(root)` reads `manifest/manifest.json` and the declared `generation-ir.json`.
- `missing_required_generation_capabilities(capabilities)` returns the exact missing gates.
- `generate_seed_package_from_linkedworlds(request, surfaces)` writes the immutable multi-slot package shape.
- TCP command `submit_seed_package` loads LinkedWorld roots, runs the generic package generator, and returns the package manifest/hash.

Capability gates:

- `can_validate_options`
- `can_build_item_pool`; the LinkedWorld must expose item-pool construction
  rules or a generated item pool for the active Nexus config version, not just
  tracker item metadata.
- `can_solve_logic`
- `can_place_items`
- `can_emit_patch`
- `can_emit_room_contract`
- `external_tools_required` must be empty for native generation.
- `unsupported_options` must be empty for the requested option set.

Patch contract coverage:

- `sekailink_generic_generation_smoke` should include at least one artifact
  slot and one no-artifact/server-dispatch style slot before this contract is
  considered complete.
- Package verifier coverage should prove every `slot_manifest` ref exists and
  is hashed, every `patch_contract_ref` exists and is hashed, and future binary
  artifact hashes use raw bytes.
- A slot with `patch.mode=none`, `server_dispatch`, or `contract_only` must not
  fail only because no file exists under `patches/`.
- A slot with `patch.mode=artifact` must fail if the declared artifact cannot be
  emitted, imported, hashed, or referenced from the slot manifest.

Smoke coverage:

- `sekailink_generic_generation_smoke` proves multiple complete dummy LinkedWorlds package into one multi-slot seed successfully.
- The same smoke proves an incomplete LinkedWorld is refused with `missing_generation_capability` instead of being guessed or hardcoded.
- `sekailink_generation_server_smoke` proves the TCP service path can package a generic LinkedWorld through `submit_seed_package`.
- `sekailink_generic_generation_probe` reports real LinkedWorld gates and catalog counts.

## Non-Goals

`Worlds` must not:

- become an emulator
- read live memory
- implement SKLMI memory behavior
- own long-term game knowledge that belongs in LinkedWorld
- hide generation truth only inside opaque ROM bytes
