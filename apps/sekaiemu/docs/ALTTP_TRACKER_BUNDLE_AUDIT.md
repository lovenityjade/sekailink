# ALTTP Tracker Bundle Audit

Date: 2026-05-21

Bundle audited:

`/home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle`

## Snapshot

- Bundle files: `1610`
- Adapted pack image files: `1536`
- Strict JSON files: `38`
- Relaxed/non-strict JSON-like files: `22`
- PopTracker-adapted map entries: `56`
- Current strict LinkedWorld manifest maps: `10`
- Current strict LinkedWorld tabs: `16`
- Current strict item icon metadata groups: `9`
- Current strict map pin metadata layers: `20`

## What Is Ready

- The strict bundle manifest is loadable by Sekaiemu.
- The strict bundle refs are loadable:
  - `tracker-flow.v1.json`
  - `item-icon-metadata.json`
  - `map-pin-metadata.json`
  - `settings-metadata.json`
  - `autotabbing-hints.json`
  - `poptracker-adapted/sekailink-adaptation.json`
- `poptracker-adapted/maps/maps.json` is strict JSON and provides real map
  image paths.
- The headless renderer can produce usable previews from the current bundle.
- Current map replacement coverage includes Light World, Dark World, Light Death
  Mountain, Hyrule Castle Escape, Eastern Palace, Desert Palace, Tower of Hera,
  and dark-world dungeon tabs that point at PopTracker dungeon maps.

## Important Finding

Several adapted PopTracker pack files are not strict JSON as consumed by
`nlohmann::json` / `jq`. Examples seen during audit include some files under:

- `poptracker-adapted/items/`
- `poptracker-adapted/layouts/`
- `poptracker-adapted/locations/`

This does not block the current tracker because the strict LinkedWorld metadata
files already exist. It does affect the next step if we want to consume
PopTracker pack files more directly instead of relying only on pre-normalized
bundle refs.

## Design Implication

For BETA-3, the safest route is:

- keep the runtime consuming strict LinkedWorld bundle metadata;
- keep PopTracker pack data/assets inside the bundle as source material;
- add a normalization/export step for any PopTracker pack files we need at
  runtime;
- avoid parsing relaxed PopTracker JSON-like files directly in the live
  emulator path until we have a tolerant parser or verified normalized output.

## Next Tracker Work

- Normalize item icon metadata from the adapted pack into strict runtime-ready
  item slots.
- Normalize location/pin data from adapted pack locations into strict
  `map-pin-metadata.json`.
- Redesign the side-by-side layout using generated previews before live testing.
- Keep `tracker_preview_render` as the fast iteration loop.

## Regenerate This Audit

```bash
tools/audit_alttp_tracker_bundle.sh
```

Default output:

`/tmp/sekaiemu-tracker-audit/alttp-tracker-bundle-audit.md`
