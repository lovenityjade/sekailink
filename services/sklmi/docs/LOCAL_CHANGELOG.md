# Local Changelog

## 2026-06-25

- Split SKLMI bridge runtime helper logic into
  `api_bridge_runtime_helpers.hpp`. Room metadata descriptions, room item
  tracker events, read snapshots, context matching, and FireRed dynamic
  read/write helpers are now separate from the session classes, keeping
  `api_bridge_runtime.cpp` under the 800-line maintenance limit.

## 2026-06-17

- Added passive Archipelago client wrapper metadata to SKLMI bridge manifests:
  - `archipelago_client_wrapper.enabled`
  - `game_key`, `game`, `platform`, `world`
  - `wrapper`, `module`, `client_file`, `status`
  - `requires`
- Added manifest validation for wrapper family names and required entry points.
- Added runtime trace event `archipelago_client_wrapper` so wrapper intent is
  visible in SKLMI JSONL logs and bug-report log tails.
- Added `archipelago_client_wrapper` to SKLMI bug-report `app_info` for easier
  filtering/debugging in Nexus/Discord reports.
- Added `sekailink_sklmi_archipelago_client_wrapper_manifest_smoke` to protect
  the wrapper metadata parse/validation contract.

## 2026-06-11

- Added automatic SKLMI runtime bug-report submission for tracker initialization,
  manifest loading, memory provider connection, tracker snapshot publication,
  and room/session start or tick failures using the shared
  `/api/client/bug-report` payload contract.
- SKLMI bug reports now include the trace log tail, runtime mode, bridge ID,
  linkedworld ID, core profile, and player alias when available.

## 2026-06-10

- Added the first generic BizHawk/Sekaiemu runtime manifests for non-SNES
  starter targets:
  - `tloz.phase1.json` for NES `system_ram` checks.
  - `ladx.phase1.json` for GB/GBC `system_ram` checks.
  - `firered.phase1.json` for GBA `gba_system_bus` checks and room-controlled
    receive queue delivery.
- Extended SKLMI manifest checks with `dynamic_source:
  firered_saveblock1_flag`, matching Sekaiemu's existing FireRed logic:
  overworld-gated reads, SaveBlock1 pointer resolution, then dynamic flag byte
  evaluation.
- Extended SKLMI room-controlled injections with `dynamic_source:
  firered_item_queue`, matching the existing Sekaiemu FireRed receive queue:
  item, progress, pending, and display fields are written only when the game is
  in a safe overworld state and the queue is not busy.
- Added `sekailink_sklmi_generic_bizhawk_manifest_smoke`, which loads the real
  manifests and proves:
  - NES TLoZ check detection.
  - GB/GBC LADX check detection.
  - GBA FireRed dynamic flag detection plus room item delivery into the AP
    receive queue.
- Revalidated the targeted SKLMI path with:
  - `ctest --test-dir /tmp/sklmi-generic-bizhawk-build -R
    'sekailink_sklmi_(smoke|legacy_manifest_smoke|generic_bizhawk_manifest_smoke|runtime_socket_smoke|unix_socket_smoke)$'
    --output-on-failure`

## 2026-05-24

- Reduced SKLMI runtime frame pressure by batching manifest check reads into
  compact memory snapshots before evaluating rules. The active ALTTP contract
  no longer performs one Unix-socket round trip per check on every tick.
- Throttled routine bridge/room state persistence to once per second, while
  still saving immediately for emitted checks, applied items, reset, and stop.
- Replaced per-tick `manifest_tick_ok` trace spam with a low-frequency
  heartbeat so Sekaiemu's tracker trace reader does not inherit a disk/log
  backlog during live runs.
- Revalidated the full SKLMI test suite with `ctest --test-dir build
  --output-on-failure` (`12/12` passing).

## 2026-05-21

- pivoted the active BETA-3 room language toward Archipelago packets instead of
  the earlier SekaiLink-only room-server command vocabulary
- added `ArchipelagoTransport` and `ArchipelagoRoomClient` to the public SKLMI
  API as the protocol boundary for the upcoming WebSocket transport
- added `TcpWebSocketArchipelagoTransport` as the first native AP WebSocket
  transport for plain `ws://` MultiServer-style connections
- added AP packet handling for:
  - `RoomInfo` -> send `Connect`
  - `Connected` -> persist team, slot, and slot data metadata
  - `LocationChecks` outbound packets from memory-derived location events
  - `ReceivedItems` inbound packets into room-controlled item deliveries
- added `sekailink_sklmi_archipelago_room_client_smoke` to lock the first AP
  contract without requiring a live WebSocket server yet
- added `sekailink_sklmi_archipelago_websocket_smoke` with a local fake
  WebSocket server proving handshake, masked client sends, AP `Connect`,
  `LocationChecks`, and inbound `ReceivedItems`
- fixed AP packet processing so one WebSocket text frame containing multiple
  command objects is handled command-by-command instead of only reading the
  first object in the array
- added `sekailink_sklmi_archipelago_live_probe`, a native diagnostic executable
  for connecting SKLMI's AP WebSocket transport to a real local/live
  MultiServer without requiring an emulator memory socket
- added `sekailink_sklmi_archipelago_runtime_live_probe`, a native diagnostic
  executable that runs the real SKLMI runtime against a fake Sekaiemu-compatible
  Unix memory socket and a real AP/MultiworldGG server
- added native `GetDataPackage` handling for the active AP game so incoming
  `ReceivedItems` can be resolved from item IDs to human-readable item names
- validated the native AP bridge against a real local MultiworldGG server using
  `AP_78856210104802680998.zip`; see
  [ARCHIPELAGO_LIVE_PROBE_2026-05-21.md](ARCHIPELAGO_LIVE_PROBE_2026-05-21.md)
- exposed the AP path in `sekailink_sklmi_runtime` through `--mode archipelago`
  with `--ap-host`, `--ap-port`, `--ap-game`, `--ap-slot-name`, `--ap-password`,
  `--ap-uuid`, and `--ap-tags`
- added a first `manifests/alttp.phase1.json` bridge fixture so Sekaiemu can
  launch the native SKLMI companion with an explicit ALTTP-shaped memory
  contract during the Core/Sekaiemu/SKLMI/AP heartbeat phase
- kept existing offline and SekaiLink game-server smokes passing so the pivot is
  additive and non-destructive
- promoted the active ALTTP runtime manifest from the two-check heartbeat fixture
  to the broader LinkedWorld-derived contract:
  - `286` memory-backed ALTTP location checks
  - `145` room-controlled item actions
- added `scripts/sync_alttp_runtime_manifest.sh` so `manifests/alttp.phase1.json`
  can be regenerated from
  `sekailink-linkedworld-alttp/bridge/sklmi.phase1.json` while preserving the
  Sekaiemu runtime bridge identity (`alttp-phase1`)
- changed room-sync persistence for reported locations to use the canonical AP
  `location_id` instead of the local semantic `event_key`; this keeps
  `*.room-sync.state` aligned with `LocationChecks` and server-side location IDs
- updated ALTTP runtime smokes to lock the canonical room-sync format
- revalidated the full ALTTP manifest through:
  - `ctest --test-dir build --output-on-failure` (`12/12` passing)
  - `sekailink_sklmi_archipelago_runtime_live_probe` against local
    MultiworldGG/AP
  - a real headless Sekaiemu + bsnes session with patched
    `AP_78856210104802680998_P2_Jade-ALTTP.aplttp`, where writing the Link's
    House bit through Sekaiemu's runtime memory socket produced AP
    `LocationChecks [59836]` and server dispatch
    `Recovery Heart to Jade-SoH (Link's House)`
- fixed the first manual ALTTP loopback bug: AP `ReceivedItems` now preserve
  source `player` and `location`, and `RoomSynchronizedRuntimeSession`
  suppresses self-origin items whose location has already been reported by the
  local ROM. This prevents local ALTTP items from being injected a second time
  while still allowing remote items to flow from AP -> SKLMI -> Sekaiemu.
- added smoke coverage proving self-origin AP items are acknowledged and
  persisted as `self_origin_suppressed` without writing memory or emitting a
  local `item_received` event.

## 2026-05-05

- preserved arbitrary `meta|...` entries in `OfflineRoomClient` so seeded room
  metadata survives later check/item persistence
- added `RoomClient::metadata_snapshot()` so runtime room sync can consume room
  metadata without changing existing authority contracts
- persisted merged runtime/room metadata into `*.room-sync.state` for downstream
  tracker/settings readers
- added smoke coverage for:
  - offline room metadata preservation
  - offline runtime propagation into `room-sync.state`
  - game-server runtime propagation into `room-sync.state`
- documented the metadata flow in [ROOM_METADATA_FLOW.md](ROOM_METADATA_FLOW.md)
- fixed `RoomSynchronizedRuntimeSession` so generic `room_controlled` actions
  can apply bounded multi-write sequences instead of assuming a single direct
  write
- added runtime JSONL traces for:
  - provider memory metadata seen at connect time
  - merged room metadata readiness for seed/settings inspection
  - successful room-item application mode/details (`direct` vs `write_sequence`)
- extended the ALTTP runtime runner smoke as one concrete proof fixture for the
  new metadata and `write_sequence` diagnostics
- documented one ALTTP-shaped alignment study against Archipelago + SNI in
  [ALTTP_REFERENCE_ALIGNMENT.md](ALTTP_REFERENCE_ALIGNMENT.md)
- clarified in [MANIFEST_CONTRACT.md](MANIFEST_CONTRACT.md) that multi-step
  writes are ordered logical deliveries, not hardware-atomic promises
- updated `GameServerRoomClient` so modern Link Room hosts can be reached by
  hostname instead of IPv4 literals only
- added split control/runtime room port support for staging against private Link
  Room deployments such as `link.sekailink.com:28080/28081`
- documented private Link Room staging in
  [PRIVATE_LINK_ROOM_STAGING.md](PRIVATE_LINK_ROOM_STAGING.md)
- added non-destructive staging helpers in:
  - [scripts/sklmi_link_room_stage.sh](../scripts/sklmi_link_room_stage.sh)
  - [scripts/sklmi_link_room_private.env.example](../scripts/sklmi_link_room_private.env.example)
- `GameServerRoomClient::poll_pending_items()` now preserves optional
  `event_key`, `tracker_semantic_id`, and `mapped_value` fields from room
  deliveries so room-controlled injections can match semantic item identities
  without depending only on `item_name`
- enriched `apply_room_item:no_matching_injection_rule` traces with the incoming
  room item identity plus the available room-controlled injection candidates
- fixed `sekailink_sklmi_runtime` so `RoomSynchronizedRuntimeSession` persists
  the runtime-owned `*.room-sync.state` file under `--runtime-state` instead of
  accidentally reusing `--room-state`
- added `sekailink_sklmi_alttp_runtime_game_server_smoke` as a live
  game-shaped proof that the generic room-controlled flow works through
  `tracker_semantic_id` and split control/runtime room ports
- clarified the private Link Room live delivery contract and the room-controlled
  matching order in [PRIVATE_LINK_ROOM_STAGING.md](PRIVATE_LINK_ROOM_STAGING.md)
- added live item flow traces for operators:
  - `room_item_pending`
  - `room_item_applied`
  - `room_item_acknowledged`
- enriched `GameServerRoomClient` ticket handling so live room metadata now
  flows back into runtime metadata:
  - `room_id`
  - `slot_name`
  - `player_alias`
  - `seed_id`
  - `seed_hash`
  - `tracker_pack`
  - `tracker_variant`
  - `slot_data`
- corrected `sekailink_sklmi_runtime` so the tracker-facing `room.state` file
  is now the live metadata surface consumed by `Sekaiemu`, while bridge state
  remains under `--runtime-state`
## 2026-05-21

- Improved Archipelago room item responsiveness for realtime tests:
  - `RoomSynchronizedRuntimeSession` now drains inbound room items before the
    manifest memory scan and again after reporting checks.
  - self-origin AP items are deferred until their local check has been reported,
    preserving the duplicate-item suppression behavior.
  - This targets the observed ALTTP test delay where a full check scan could
    postpone admin-injected items by several seconds.
- Added the clean-room `earthbound.phase1.json` runtime manifest for the second
  SNES heartbeat:
  - two early Onett checks;
  - one room-controlled `Toothbrush` injection;
  - synchronized from the clean EarthBound LinkedWorld bridge contract.
