# Runtime Freeze And Generator Pivot

Date: 2026-05-10

## Decision

`Sekaiemu`, `SKLMI`, and the current runtime-facing `Link Room` surface are now treated as frozen integration bases.

They are not deleted, abandoned, or considered final forever. They are simply no longer the correct place to solve seed truth.

## Why

A randomizer ROM by itself is not enough for a complete SekaiLink run.

The room server needs the exact generation truth:

- which seed was generated
- which users and slots exist
- which player options were used
- which checks exist for this seed
- which item is placed at each location
- which patch or launch artifact belongs to each player
- which tracker settings and metadata apply
- which runtime contracts SKLMI and Link Room must expose during play

`Sekaiemu` cannot infer that truth from memory.
`SKLMI` cannot invent that truth from a manifest.
`Link Room` cannot safely guess that truth from a random patched ROM.

`Worlds` must produce it.

## Frozen Responsibilities

`Sekaiemu` remains responsible for:

- libretro emulation
- patch application when given an official patch artifact
- tracker display
- memory exposure
- controller/video/runtime UX

`SKLMI` remains responsible for:

- loading a LinkedWorld memory contract
- reading and writing exposed memory
- emitting and receiving runtime events
- staying plug-and-play across LinkedWorlds

`Link Room` remains responsible for:

- live room connectivity
- item/check/event distribution
- readable room logs
- runtime session state
- consuming generation truth from Worlds/Nexus

## New Critical Path

The next product-critical system is `Worlds` as a generic LinkedWorld-first generator.

Required flow:

1. Client or operator submits a generation request that references Nexus config
   versions.
2. `Worlds` maps the requested players/runs into slots.
3. Each slot resolves one `LinkedWorld` generation surface from its selected
   game/config version.
4. `Worlds` loads all requested `LinkedWorld` surfaces for the room.
5. `Worlds` validates player options against the owning `LinkedWorld` schema.
6. `Worlds` generates one shared multiworld placement graph from those options
   and a seed source.
7. `Worlds` emits one complete seed package for the room.
8. `Worlds` publishes durable generation truth to Nexus.
9. Link Room consumes the room/session portion of the seed package.
10. Client downloads each slot artifact.
11. Sekaiemu launches only from the generated artifact.
12. SKLMI and Link Room use matching contracts from the same seed package.

The input chain is always:

```text
Nexus config versions -> slots -> LinkedWorld generation surfaces
```

The output is always one seed package for the room, not one disconnected seed
per player. YAML may be imported into Nexus or exported for debugging, but it is
not the canonical generation input.

## Hard Rule

No test that claims to be a full run may use an arbitrary randomizer ROM.

A full run must start from a `Worlds` seed package or from an explicitly imported external seed package that has been normalized into the same contract.

`Worlds` must also stay game-agnostic. If a game needs logic, item pools,
placement rules, patch behavior, or room contract metadata, that knowledge must
come from the relevant `LinkedWorld` generation surface rather than from
hardcoded branches in the generic generator.

Tracker item metadata is not an item pool. A native generation request must
remain refused when a slot has tracker items but no generated `item_pool` or
LinkedWorld-owned item-pool construction rules. That refusal is part of the same
generic flow: Nexus config versions -> slots -> LinkedWorld generation surfaces
-> one seed package.
