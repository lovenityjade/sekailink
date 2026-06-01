# ALTTP Native Bundle

This bundle is the repo-owned native ALTTP tracker fixture for the libretro spike.
It is an ALTTP example running through the generic tracker host path, not proof
of an ALTTP-specific host architecture.
It now targets side-by-side test readability first: visible map, visible room and slot identity, and stable starter progress state.

What it proves:

- ALTTP can ship a tracker bundle directly in this repo
- the runtime can resolve tabs, maps, zone bindings, and metadata panels from local bundle metadata
- the native overlay renderer can show a visible side-by-side surface instead of status text only
- test launches can expose room, seed, slot, `slot_data`, and recent tracker events without depending on PopTracker runtime embedding
- the host can stay bundle-driven while LinkedWorld / `SKLMI` metadata carry the ALTTP-specific meaning

Current maps:

- `lightworld`
- `hyrule-castle`
- `hyrule-sewers`

Current tabs:

- `overview`
- `items`
- `map-lightworld`
- `map-castle`
- `map-sewers`
- `settings`
- `slot`

Current zone bindings:

- `alttp.overworld` -> `lightworld`
- `alttp.links_house` -> `lightworld`
- `alttp.sanctuary` -> `lightworld`
- `alttp.secret_passage` -> `hyrule-castle`
- `alttp.castle` -> `hyrule-castle`
- `alttp.hyrule_castle` -> `hyrule-castle`
- `alttp.sewers` -> `hyrule-sewers`

Current info panels:

- `session`
- `progress`
- `live-feed`
- `tracker-state`
- `slot-info`
- `seed-meta`
- `room-meta`
- `runtime-meta`
- `journal-meta`
- `settings-core`
- `settings-rules`

Visual direction:

- inspired by the readability goals of PopTracker and ALTTP map packs
- implemented here with repo-owned metadata and simple native map assets only
- no third-party tracker source is copied into this repo

This bundle is still intentionally starter-oriented.
It is aligned with `profiles/alttp-starter.profile` and external test prep, not with full ALTTP tracker coverage yet.

Current live-data assumptions:

- room and player identity come from `room.state` / snapshot keys such as `room_id`, `slot_name`, and `player_alias`
- rules and settings come from `seed_metadata.slot_data.*`
- `received_items` and `checked_locations` may arrive either as simple ids or as structured objects with names
- the visible side column prioritizes `session`, `progress`, `live-feed`, and `tracker-state` before the deeper rules panels
- the bundle now tags panels with generic `surface` / `priority` metadata so the host can sort summary vs. deeper cards without hard-coded ALTTP panel ids
- the `DETAILS` area is now meant to spend its space on deeper ALTTP cards such as `slot-info`, `seed-meta`, `room-meta`, `runtime-meta`, and seed settings instead of repeating the summary panels
- when the metadata surface grows beyond the available space, the native renderer now keeps the layout readable and exposes `+N MORE PANELS` / `+N MORE EVENTS` instead of collapsing the whole column
- the runtime now derives a few tracker-facing live values from the snapshot itself:
  - `last_received_label`
  - `last_received_from`
  - `last_check_label`
  - `check_progress`
  - `completion_percent`
- the native recent-event strip merges live trace events with snapshot-backed
  last item/check rows when one side of the feed has not arrived through the
  live trace yet
