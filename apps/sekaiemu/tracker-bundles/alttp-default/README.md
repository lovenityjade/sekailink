# ALTTP Default Tracker Bundle

This native bundle gives `sekaiemu_libretro_spike` a real ALTTP tracker surface
without depending directly on PopTracker at runtime. The visual goal is to
capture the same kind of side-by-side readability for tests while staying on a
repo-owned native renderer and bundle contract.

It should still be read as one ALTTP bundle artifact consumed by the generic
tracker host, not as proof of ALTTP-specific host code.

Current scope:

- split-screen or separate-window tracker rendering
- bundled dungeon maps for a first real ALTTP presentation
- item/status counts and seed metadata driven by the native tracker runtime
- direct display of `seed_id`, `slot_id`, `world_instance_id`, tracker pack,
  tracker variant, and selected `slot_data` fields when the room state exposes them
- metadata cards meant to stay readable during live ALTTP test sessions
- a test-facing side-by-side layout with explicit header, session strip, item
  flow, map panel, and non-breaking empty states

Current metadata emphasis:

- session identity:
  - `slot_id`
  - `slot_name`
  - `player_alias`
  - `seed_id`
  - `world_instance_id`
  - `room_id`
- tracker identity:
  - `tracker_pack`
  - `tracker_variant`
  - active map/tab/display state
- progress:
  - checked count
  - missing count
  - received count
  - local received count
- seed settings:
  - `slot_data.mode`
  - `slot_data.goal`
  - `slot_data.difficulty`
  - `slot_data.weapons`
  - `slot_data.name`

Recommended `room.state` metadata for a private Link Room run:

- `meta|world_id|...`
- `meta|linkedworld_id|alttp`
- `meta|room_id|...`
- `meta|slot_id|...`
- `meta|slot_name|...`
- `meta|player_alias|...`
- `meta|seed_id|...`
- `meta|seed_hash|...`
- `meta|tracker_pack|alttp-default`
- `meta|tracker_variant|side-by-side`
- `meta|slot_data|{...}`

Current limitations:

- no full icon-for-icon PopTracker parity yet
- no automatic zone-follow bindings yet
- the `items` tab is still a native summary view, not a full icon grid
- this bundle is a practical runtime surface for ALTTP testing, not the final
  complete LinkedWorld-owned tracker package
