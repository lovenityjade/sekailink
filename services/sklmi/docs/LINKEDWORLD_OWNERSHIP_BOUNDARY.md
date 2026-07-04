# SKLMI LinkedWorld Ownership Boundary

Status: current boundary note
Date: 2026-05-05

## Goal

State plainly what belongs to the generic `sklmi` runtime and what belongs to a
game-owned `LinkedWorld`.

This note exists because recent ALTTP proof work can otherwise look like
`sklmi` is turning into an ALTTP runtime. That is not the intended boundary.

## What `sklmi` Owns

`sklmi` is the generic plug-and-play runtime layer.

It owns:

- manifest parsing and validation for the `sklmi` contract
- generic watch evaluation
- generic bounded write execution
- runtime sessions and room-synchronized sessions
- room client transport behavior
- generic runtime traces and emitted event envelopes
- local bridge state and room-sync persistence
- host/runtime CLI overrides such as socket path, room endpoints, and tick rate

It does not own per-game authored content.

## What A `LinkedWorld` Owns

The `LinkedWorld` is the game-shaped source of truth.

It owns:

- memory addresses and domains used by one game
- the concrete list of checks and actions
- game-facing identifiers:
  - `location_id`
  - `location_name`
  - `item_id`
  - `item_name`
  - `event_key`
  - `mapped_value`
- coverage policy:
  - which checks are declared
  - which received items are injectable
  - which semantics stay out of scope for one pass
- game-specific metadata meaning:
  - `seed_id`
  - `seed_hash`
  - `slot_data`
  - goal/settings interpretation
- game-specific reference notes and extraction status

If a field changes because one game needs a different memory rule, that belongs
in the `LinkedWorld` manifest, not in `sklmi` runtime code.

## What The Room Service Owns

The room service or game server owns:

- session creation and ticketing
- delivery queue authority
- acknowledgement authority
- accepted check policy
- room/session metadata issuance

`sklmi` consumes that room state. It does not become the authority.

The room layer may also read `LinkedWorld` content for live-session concerns
such as session identity, compatibility, or tracker-facing session semantics.
That still does not make the room layer the owner of raw memory reads and
writes; raw memory remains the `Sekaiemu` + `sklmi` boundary.

## How To Read Recent ALTTP Work

ALTTP-specific docs and tests inside `sklmi` are proof fixtures for the generic
runtime, not a claim that `sklmi` owns ALTTP as product logic.

Interpret them like this:

- acceptable inside `sklmi`:
  - a small ALTTP smoke manifest embedded in a test
  - a runtime recipe that proves generic ordered writes
  - an alignment note that checks generic semantics against one external
    reference
- should live in `sekailink-linkedworld-alttp` instead:
  - canonical ALTTP bridge content
  - ALTTP coverage expansion
  - ALTTP item/check prioritization
  - ALTTP live contract notes

## Displacement Rule

When deciding where new work belongs:

- if the change adds or clarifies generic manifest semantics, it belongs in
  `sklmi`
- if the change adds or clarifies ALTTP addresses, item ids, check lists,
  tracker semantics, or live coverage, it belongs in the ALTTP `LinkedWorld`
  repo
- if the change defines authority, delivery lifecycle, or room ticketing, it
  belongs in the room/server layer

## Trace Interpretation

`sklmi` trace event names should stay generic:

- `manifest_loaded`
- `provider_metadata`
- `room_metadata_ready`
- `room_item_pending`
- `room_item_applied`
- `room_item_acknowledged`

Game-specific strings may appear inside those traces only because they come from
the loaded manifest or room metadata, not because the trace type itself is
game-specific.
