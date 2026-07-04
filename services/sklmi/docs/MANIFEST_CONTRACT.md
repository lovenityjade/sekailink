# SKLMI LinkedWorld Contract

Status: canonical contract in migration
Date: 2026-05-01

## Goal

Define the dedicated `sklmi` section that a `LinkedWorld` exposes to the
generic SKLMI driver.

This contract is intentionally declarative and bounded. It must never become a
script engine or a Lua-equivalent hidden inside JSON.

## Purpose

The contract exists so that:

- a `LinkedWorld` can describe memory-facing behavior without embedding code
- `SKLMI` can stay generic and game-agnostic
- `Sekaiemu` can host multiple independent driver instances cleanly
- server/tracker layers can consume structured events instead of raw memory writes

## Ownership Split

This contract is generic. The authored game content is not.

`SKLMI` owns:

- the schema
- validation behavior
- supported compare operators
- supported bounded write sources
- generic event envelopes and trace names

The `LinkedWorld` owns:

- concrete addresses and domains
- concrete checks and actions
- concrete game-facing ids and labels
- game-specific `event_key` and `mapped_value` conventions
- game-specific coverage decisions

That means `SKLMI` should execute a game-shaped manifest, not become the home
of that game's catalog content.

## Fields

```json
{
  "id": "earthbound",
  "type": "linkedworld",
  "version": "0.1.0-dev",
  "sklmi": {
    "contract_version": "1.0",
    "bridge_id": "earthbound-smoke",
    "driver_instance_id": "earthbound-default",
    "core_profile": {
      "name": "snes_v1",
      "domains": [
        { "id": "WRAM" }
      ]
    },
    "state_file": "earthbound-smoke.state",
    "poll_interval_ms": 33,
    "checks": [
      {
        "domain_id": "WRAM",
        "address": 4,
        "size": 1,
        "compare": "gte",
        "operand_u64": 127,
        "event_type": "location_checked",
        "location_id": 42001,
        "location_name": "Onett - Test"
      }
    ],
    "actions": [
      {
        "domain_id": "WRAM",
        "address": 8,
        "size": 1,
        "value_u64": 1,
        "item_id": 1337,
        "item_name": "Cookie",
        "room_controlled": true
      }
    ]
  }
}
```

## Meaning

- top-level `id`
  - identifies the `LinkedWorld`
- top-level `type`
  - must be `linkedworld` when loading a `LinkedWorld`-embedded `sklmi` block
- `sklmi.contract_version`
  - explicit version of the SKLMI contract
- `sklmi.bridge_id`
  - identifies the bridge ruleset for logging/tracing
- `sklmi.driver_instance_id`
  - default identity for one driver instance
  - may be overridden by the host at launch time
- `sklmi.core_profile`
  - stable core-profile reference plus optional domain declarations
  - when domains are declared, every check and write must use one of those
    domain ids
- `sklmi.poll_interval_ms`
  - desired controlled tick interval
  - must not imply per-frame polling
- `sklmi.state_file`
  - local bridge state snapshot file name
- `sklmi.checks[]`
  - list of watch rules that emit a decoded `event_type`
- `sklmi.actions[]`
  - list of action rules that resolve to one or more writes and emit `item_received`
  - `room_controlled: true` means the rule is reserved for `RoomClient`-driven delivery and is not auto-applied by the manifest bridge tick path
  - when one action uses `writes`, `SKLMI` preserves step order but does not promise hardware-level atomicity across the whole sequence

APWorld-shaped identity fields are preferred when available:

- `location_id`
- `location_name`
- `item_id`
- `item_name`

Legacy standalone bridge manifests remain loadable through a compatibility path.
They are a migration convenience, not a claim that `SKLMI` should permanently
own per-game manifests.

## Watch Rule Fields

- `domain_id`
- `address`
- `size`
- `compare`
- `operand_u64`
- `event_type`
- `event_key`
- `mapped_value`
- `location_id`
- `location_name`

Supported `compare` values:

- `equals`
- `not_equals`
- `greater_than`
- `greater_or_equal`
- `less_than`
- `less_or_equal`
- `mask_any`
- `mask_all`

Supported `event_type` values:

- `location_checked`
- `map_changed`
- `item_received`
- `slot_connected`
- `runtime_reset`
- `trace`
- `error`

If `event_key` is omitted, SKLMI derives it from `location_id` first, then `location_name`.
If `mapped_value` is omitted, SKLMI derives it from `location_name` when present.

## Injection Rule Fields

- `domain_id`
- `address`
- `size`
- `value_u64`
- `event_key`
- `mapped_value`
- `item_id`
- `item_name`
- `room_controlled`
- `writes`

When `writes` is used, each step is intentionally constrained:

- `source: "constant"`
- `source: "current_plus"`
- `source: "current_plus_delta"`

Unknown write sources are rejected during validation. This is deliberate: the
contract may describe bounded memory actions, but it must not become an open
execution language.

Unknown `compare` and `event_type` values are rejected during loading instead
of falling back to a default. A `LinkedWorld` should fail fast when it asks for
behavior outside the generic SKLMI contract.

For SNES runtimes this matches SNI expectations better than pretending that a
multi-step WRAM update is a single atomic commit. A write sequence is one
logical delivery from the bridge point of view, but each step can still fail or
land on a different frame at the device/runtime layer.

If `event_key` is omitted, SKLMI derives it from `item_id` first, then `item_name`.
If `mapped_value` is omitted, SKLMI derives it from `item_name` when present.

## Core Principles

- the `LinkedWorld` describes, but does not program
- no arbitrary code
- no scripts
- no callbacks
- no free-form expressions that turn the contract into a hidden language
- if the schema starts to resemble a scripting language, the design must be revised instead of extended further

## Current Runtime Meaning

For the current runtime:

- non-`room_controlled` actions are local driver-owned writes
- `room_controlled` actions are applied only through `RoomSynchronizedRuntimeSession`
- room authority and room sync state stay outside emulator frame ownership
- events are emitted with driver-instance, LinkedWorld, and core-profile context when available

## SNI Alignment

For SNES-style runtimes, SKLMI should follow the SNI stability model:

- memory access separated from higher-level protocol concerns
- controlled polling instead of frame-loop scripting
- reconnect behavior handled at runtime-session level
- bridge rules loaded as data whenever possible
- multiple games or multiple copies of one game are represented by separate driver instances
- ordered multi-step writes are acceptable; fake atomicity guarantees are not

This means the `LinkedWorld` contract is the rules source, not the networking layer and not a programmable runtime.
