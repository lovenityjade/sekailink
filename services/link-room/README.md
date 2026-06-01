# sekailink-link-room

Role:

- room live gameplay
- sync session
- administration de room

Surface active extraite:

- `include/`
- `src/`
- `docs/`
- `deploy/`

Surface snapshot:

- `source-snapshots/monolith-core/`

Reference:

- `../../../docs/repo-contracts/sekailink-link-room.md`

Docs locales utiles:

- `docs/server-link.md`
- `docs/room-server-live-test.md`
- `docs/room-server-live-deployment-checklist.md`
- `docs/room-server-alttp-live-runbook.md`
- `docs/room-server-archipelago-alignment.md`
- `docs/room-server-first-external-test.md`
- `docs/room-server-private-live-staging.md`
- `docs/room-server-nexus-room-query-chain.md`
- `docs/room-server-field-test-readiness-2026-05-21.md`
- `docs/room-sklmi-tracker-flow.md`
- `deploy/link/room-server/README.md`
- `CHANGELOG.local.md`

Surface LinkedWorld live:

- `apply_linkedworld_surface` attache a une room la surface session officielle
  lue depuis le LinkedWorld: metadata room/seed, presentation, refs runtime et
  resume bridge.
- `pending_items` expose ensuite les semantiques item room-facing
  (`event_key`, `mapped_value`, `tracker_semantic_id`) quand elles sont
  disponibles dans le bridge LinkedWorld.
- Link Room ne consomme pas ces donnees pour ecrire en memoire; SKLMI reste le
  consommateur memory-side.
- `apply_seed_contract` attache a la room le contrat `link_room_seed_contract.json`
  produit par Worlds et rend les settings du slot visibles comme `slot_data`.
- `snapshot_room`, `sync_room` et `issue_ticket` exposent maintenant
  `seed_contract` et `seed_contract_summary`.
- `sekailink_room_seed_package_dispatch` lit un dossier package Worlds et
  construit/envoie automatiquement la commande `apply_seed_contract`.

Build clean-room minimal:

```sh
cmake -S . -B /tmp/sekailink-link-room-build
cmake --build /tmp/sekailink-link-room-build \
  --target sekailink_room_server_service \
           sekailink_room_server_tcp_cli \
           sekailink_room_linkedworld_surface_smoke \
           sekailink_room_seed_package_dispatch \
           sekailink_room_seed_package_dispatch_smoke \
           sekailink_room_seed_package_dispatch_tcp_smoke
/tmp/sekailink-link-room-build/sekailink_room_linkedworld_surface_smoke
/tmp/sekailink-link-room-build/sekailink_room_seed_package_dispatch_smoke
/tmp/sekailink-link-room-build/sekailink_room_seed_package_dispatch_tcp_smoke
```

Note: ce build utilise seulement la surface clean-room active et les headers
tiers vendores par `sekailink-worlds` pour `nlohmann/json.hpp`.

Dispatcher package Worlds:

```sh
/tmp/sekailink-link-room-build/sekailink_room_seed_package_dispatch \
  --package-dir /tmp/sekailink-soh-cycle1-package/seed-soh-cycle1 \
  --room-id room-linkedworld-soh-1 \
  --print
```

Pour envoyer au service TCP au lieu d'imprimer:

```sh
/tmp/sekailink-link-room-build/sekailink_room_seed_package_dispatch \
  --package-dir /tmp/sekailink-soh-cycle1-package/seed-soh-cycle1 \
  --room-id room-linkedworld-soh-1 \
  --host 127.0.0.1 \
  --port 28081 \
  --auth-token "$SEKAILINK_ROOM_ADMIN_TOKEN"
```
