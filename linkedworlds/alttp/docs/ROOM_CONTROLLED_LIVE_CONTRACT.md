# ALTTP Room-Controlled Live Contract

Status: bounded live contract note
Date: 2026-05-05

## Goal

Document the smallest stable live contract between:

- Link Room runtime deliveries
- `SKLMI` room-controlled matching
- this repo's phase 1 ALTTP manifest

This is intentionally narrow. It exists to stabilize the current ALTTP real-time
test path, not to redesign the global room protocol.

## Current Phase 1 Reality

The current bridge file still lives at a legacy `phase1` path, but its current
content covers:

- two location checks
  - `Sanctuary`
  - `Link's House`
- a broader runtime-core room-controlled item set declared in
  `bridge/sklmi.phase1.json`

That means:

- the bridge is no longer limited to one tiny sample
- matching still follows the same `SKLMI` rules and the same bounded write model
- other ALTTP items still need explicit manifest `actions[]` entries before
  they can be injected by `SKLMI`

## Hookshot Canonical Identity

The current canonical phase 1 Hookshot identity is:

- `item_id`: `10`
- `item_name`: `Hookshot`
- `event_key`: `item.hookshot`
- `mapped_value`: `Hookshot`

These fields now align across:

- `bridge/sklmi.phase1.json`
- `tracker/runtime-bindings.phase1.json`
- the current `SKLMI` room-controlled matching logic

## Live Delivery Payload Expected By SKLMI

For a room-controlled item delivery, the runtime-facing room payload should
include:

- required:
  - `delivery_id`
- preferred stable identity:
  - `item_id`
  - or `event_key`
  - or `tracker_semantic_id`
- preferred display/mapping hint:
  - `mapped_value`
- optional:
  - `item_name`

For the current Hookshot live path, this minimal payload is sufficient:

```json
{
  "delivery_id": 7,
  "tracker_semantic_id": "item.hookshot",
  "mapped_value": "Hookshot"
}
```

On the `SKLMI` side, operators should now expect this trace sequence for one
supported live delivery:

- `room_item_pending`
- `room_item_applied`
- `room_item_acknowledged`

Pour la surface tracker, la session live devrait aussi rendre visible au moins
une identite de room/seed/slot par:

- `room.state`
- ou, temporairement, `trace.jsonl`

## Matching Order In SKLMI

`SKLMI` currently matches one live room delivery to one `room_controlled`
manifest action in this order:

1. `item_id`
2. `item_name`
3. `event_key` or `tracker_semantic_id`
4. `mapped_value`

This is enough for the current bounded set and for more phase-1-style ALTTP
items too, as long as each item action carries the same four identity axes:

- `item_id`
- `item_name`
- `event_key`
- `mapped_value`

## Do We Need A Contract Change For More ALTTP Items?

Not for normal ALTTP progression items.

For the next items, the useful work is declarative expansion of `actions[]`, not
a new schema. Each new room-controlled item should add:

- canonical Archipelago `item_id`
- stable semantic `event_key`
- readable `mapped_value`
- bounded write plan in `writes[]`

For the exact prioritized runtime-core set selected in this repo, see:

- [ALTTP_RUNTIME_CORE_INVENTORY.md](ALTTP_RUNTIME_CORE_INVENTORY.md)

## When A Contract Extension Would Actually Be Needed

Only extend the contract if one future ALTTP item needs behavior outside the
current bounded model, for example:

- unsupported write sources beyond `constant` / `current_plus` /
  `current_plus_delta`
- one delivery that must branch conditionally on live memory state in a way not
  representable by the current bounded write steps
- composite item semantics that cannot be expressed as one declarative item
  action

That is not the case for standard single-item phase-1 progression injections.

## Current Metadata Reality

Etat acceptable pendant la run de test:

- `room.state` peut encore etre partiellement rempli
- les traces `room_client_ready`, `room_metadata_ready`, `slot_connected`
  peuvent servir de fallback d'exploitation

Etat cible plus propre:

- les metadata room/seed/slot doivent finir dans `room.state` sans depender des
  traces pour l'affichage tracker
