# Room Server SoH Vertical

Date: `2026-05-17`

`Link Room` redevient actif dans le cycle 1 uniquement pour la verticale
`SoH` serveur-first.

Ce que `Link Room` doit consommer:

- metadata de seed
- metadata de slot
- linkedworld id
- contrat de dispatch room-facing

Ce que `Link Room` ne doit pas supposer:

- aucun patch ROM local
- aucune dependance `Sekaiemu`
- aucune dependance `SKLMI`
- aucun tracker disponible au cycle 1

But du cycle:

- prouver qu'une room peut etre preparee et decrite proprement a partir
  d'un linkedworld `server-first` et des artefacts `Worlds`

Etat valide le 2026-05-18:

- le repo clean-room compile son smoke de surface sans utiliser le snapshot monolithe comme build root
- `apply_linkedworld_surface` accepte une surface SoH `server_dispatch`
- `apply_seed_contract` accepte le `link_room_seed_contract.json` produit par Worlds
- la surface expose `delivery_mode=server_dispatch`
- la surface expose `delivery.server_dispatch_target=link_room`
- la surface expose `delivery.server_dispatch_transport=room_contract`
- la surface expose `delivery.server_dispatch_payload_ref=link_room_seed_contract.json`
- la surface garde `tracker_pack=null`
- le contrat de seed est expose dans `snapshot_room`, `sync_room`, `issue_ticket`
  et les projections
- les `config_snapshot.values` du slot deviennent le `slot_data` de la room
- le dispatcher de package Worlds est valide en local-protocol et en TCP loopback
- le service live Link Room a ete redeploye sur `link.sekailink.com` le
  2026-05-18 avec backup dans
  `/opt/sekailink/link/room-server/backups/20260518T1834Z`
- la room live `room-linkedworld-soh-live-official` a applique
  `seed-soh-cycle1` avec 111 entrees `slot_data`, puis a ete visible dans la
  projection MySQL `sekailink_room_projection`

Commandes locales:

```sh
cmake -S /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-link-room \
  -B /tmp/sekailink-link-room-cycle1-build

cmake --build /tmp/sekailink-link-room-cycle1-build \
  --target sekailink_room_server_service \
           sekailink_room_server_tcp_cli \
           sekailink_room_linkedworld_surface_smoke \
           sekailink_room_seed_package_dispatch \
           sekailink_room_seed_package_dispatch_smoke \
           sekailink_room_seed_package_dispatch_tcp_smoke

/tmp/sekailink-link-room-cycle1-build/sekailink_room_linkedworld_surface_smoke
/tmp/sekailink-link-room-cycle1-build/sekailink_room_seed_package_dispatch_smoke
/tmp/sekailink-link-room-cycle1-build/sekailink_room_seed_package_dispatch_tcp_smoke
```

Limite volontaire:

- `sekailink_room_seed_package_dispatch` ouvre maintenant un dossier package
  Worlds depuis le filesystem et construit la commande `apply_seed_contract`
- le service Link Room lui-meme reste transport-neutre: il recoit une commande
  JSON; le dispatcher est l'outil qui lit le package et l'envoie/imprime
- le smoke TCP demarre une room locale, envoie le package par socket, puis
  relit `sync_room` pour confirmer `seed_contract_summary` et `slot_data`

Payload minimal:

```json
{
  "channel": "admin",
  "command": {
    "cmd": "apply_seed_contract",
    "room_id": "room-linkedworld-soh-1",
    "seed_contract": {
      "schema_version": "sekailink-link-room-seed-contract-v1",
      "generation_scope": "multiworld",
      "room_id": "room-seed-soh-cycle1",
      "seed_id": "seed-soh-cycle1",
      "checks_ref": "checks.json",
      "items_ref": "items.json",
      "placements_ref": "placements.json",
      "slots": [
        {
          "slot_id": 2,
          "user_id": 1001,
          "display_name": "Jade SoH",
          "game_key": "oot_soh",
          "linkedworld_id": "oot_soh",
          "config_version_id": 7001,
          "config_snapshot": {
            "schema_version": "sekailink-config-snapshot-v1",
            "game_key": "oot_soh",
            "linkedworld_id": "oot_soh",
            "values": {
              "starting_age": "child"
            }
          }
        }
      ],
      "config_versions": []
    }
  }
}
```

Dispatcher local depuis un package Worlds:

```sh
/tmp/sekailink-link-room-cycle1-build/sekailink_room_seed_package_dispatch \
  --package-dir /tmp/sekailink-soh-cycle1-package/seed-soh-cycle1 \
  --room-id room-linkedworld-soh-1 \
  --print
```

Dispatcher TCP vers une instance live:

```sh
/tmp/sekailink-link-room-cycle1-build/sekailink_room_seed_package_dispatch \
  --package-dir /tmp/sekailink-soh-cycle1-package/seed-soh-cycle1 \
  --room-id room-linkedworld-soh-1 \
  --host 127.0.0.1 \
  --port 28081 \
  --auth-token "$SEKAILINK_ROOM_ADMIN_TOKEN"
```
