# ALTTP LinkedWorld Default Archive Fixture

This fixture documents how Sekaiemu can validate the LinkedWorld ALTTP
`tracker/default.zip` artifact that is enriched with `poptracker-adapted`.
It intentionally does not copy the archive or extracted assets into this repo.

Canonical source checked on 2026-05-10:

- archive: `<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip`
- extracted bundle: `<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle`
- fixture: `archive-fixture.json`

## Runtime Path

Sekaiemu accepts either the extracted bundle directory or the packaged zip file.
The extracted directory remains useful while editing bundle contents directly:

```sh
--tracker-bundle <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.bundle
```

For packaged installs, pass the archive directly:

```sh
--tracker-bundle <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip
```

The runtime materializes zip bundles into a local cache before loading. Do not
duplicate the 69 MiB zip or 110 MiB extracted `poptracker-adapted` tree inside
this repo.

## Validated Archive Facts

- `sha256`: `18b5967d539295229e4c1cac273269dc8efb235d9ad1c30489efd2f1ac161d54`
- archive size: `71,801,240` bytes (`69M` via `du -h`)
- zip summary: `1,628` files, `111,117,277` bytes uncompressed, `71,412,906` bytes compressed
- root native map images: `10` PPM files under `maps/`
- poptracker-adapted static image/data inventory: `911` PNG, `9` JPG, `616` XCF, `60` JSON files
- runtime surfaces intentionally absent: `scripts/`, Lua execution, PopTracker runtime embedding, SNI embedding

The archive contains both the native Sekaiemu root bundle files and the
adapted static PopTracker data:

- `manifest.json`
- `surface.complete.json`
- `location-groups.complete.json`
- `item-slots.complete.json`
- `dungeon-progress.complete.json`
- `room-metadata.complete.json`
- `slot-data.complete.json`
- `tracker-flow.v1.json`
- `item-icon-metadata.json`
- `map-pin-metadata.json`
- `settings-metadata.json`
- `autotabbing-hints.json`
- `maps/*.ppm`
- `poptracker-adapted/manifest.json`
- `poptracker-adapted/sekailink-adaptation.json`
- `poptracker-adapted/items/*.json`
- `poptracker-adapted/layouts/*.json`
- `poptracker-adapted/locations/*.json`
- `poptracker-adapted/maps/maps.json`
- `poptracker-adapted/images/**/*.png`

## Smoke Commands

Archive integrity:

```sh
unzip -t <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip
```

Archive summary:

```sh
sha256sum <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip
stat -c '%s bytes' <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip
zipinfo -t <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip
```

Native Sekaiemu tracker regression suite:

```sh
ctest --test-dir build --output-on-failure
build/tracker_alttp_bundle_smoke .
build/tracker_alttp_native_bundle_smoke .
```

Runtime archive loading is covered by `tracker_runtime_smoke`, which creates a
temporary zip bundle and loads it through `TrackerBundle::LoadFromPath`.

Targeted archive entry checks:

```sh
zipinfo -1 <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip manifest.json
zipinfo -1 <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip maps/light_world.ppm
zipinfo -1 <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip poptracker-adapted/sekailink-adaptation.json
zipinfo -1 <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip poptracker-adapted/images/items/Hookshot.png
zipinfo -1 <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp/tracker/default.zip poptracker-adapted/images/maps/lttp_lightworld.png
```

## Runtime Boundaries

- Archive loading: `--tracker-bundle` can be either a directory or `.zip`.
- Image formats: native raster loading supports PPM/P3/P6 plus PNG, JPG/JPEG,
  and WebP through SDL2_image.
- Runtime scope: this fixture deliberately does not validate PopTracker Lua,
  SNI, or PopTracker runtime behavior.
