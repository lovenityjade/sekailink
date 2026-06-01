# sklmi

`sklmi` is the clean-room memory-interface and runtime bridge for SekaiLink.

Its ownership is intentionally narrow:

- generic memory provider APIs
- manifest decoding and validation for the `sklmi` contract
- runtime sessions that poll memory, emit events, and apply bounded writes
- room synchronization clients and runtime glue
- smoke and proof tests for the runtime boundary

What does not belong here long-term:

- game-specific `LinkedWorld` content
- rich gameplay logic
- social/orchestration layers
- open-ended scripting inside manifests
- canonical cross-repo contract governance

## Repo Map

- `include/sekailink_sklmi/`
  - public API surface for providers, sessions, room clients, events, and manifests
- `src/`
  - runtime/library implementation only
- `tests/`
  - in-tree smoke, golden, and runtime proof coverage
- `docs/`
  - contract notes, runtime flow, and migration handoff material
- `fixtures/`
  - reserved for test-only data; not runtime product code
- `source-snapshots/`
  - reserved for migration/reference snapshots; not runtime product code

## Runtime Boundary

`sklmi` owns the generic bridge loop:

- load a bridge manifest or `LinkedWorld`-embedded `sklmi` section
- connect to a memory provider
- evaluate watch rules
- emit structured events
- apply bounded injection rules
- persist bridge-local state
- optionally synchronize delivered items through a room client

`sklmi` does not own:

- emulator internals
- game-specific rule authoring as a permanent in-repo asset set
- room authority decisions
- `LinkedWorld` packaging and lifecycle

## Contract Direction

The preferred input is a `LinkedWorld` document with an embedded `sklmi` block.
Legacy standalone bridge manifests still load through a compatibility path, but
that path exists to keep migration moving, not to define the final boundary.

See:

- `docs/MANIFEST_CONTRACT.md`
- `docs/LINKEDWORLD_OWNERSHIP_BOUNDARY.md`
- `docs/REPO_STRUCTURE.md`
- `EXTRACTION_STATUS.md`

## Build And Test

Library and runtime targets are defined in `CMakeLists.txt`.

Primary targets:

- `sekailink_sklmi`
- `sekailink_sklmi_runtime`

Representative tests:

- `sekailink_sklmi_smoke`
- `sekailink_sklmi_legacy_manifest_smoke`
- `sekailink_sklmi_runtime_runner_smoke`
- `sekailink_sklmi_runtime_game_server_smoke`

## Reading Order For A New Agent

1. `README.md`
2. `docs/LINKEDWORLD_OWNERSHIP_BOUNDARY.md`
3. `docs/REPO_STRUCTURE.md`
4. `EXTRACTION_STATUS.md`
5. `docs/MANIFEST_CONTRACT.md`
6. the specific runtime or test file you need
