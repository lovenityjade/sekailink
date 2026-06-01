# Room LinkedWorld Surface: Server-First

Date: `2026-05-17`

`build_linkedworld_room_surface(...)` doit maintenant pouvoir exposer une
surface room-facing utile meme quand un linkedworld:

- n'a pas de tracker actif
- n'a pas de bridge memoire local
- livre sa seed via `patch_mode=server_dispatch`

Champs room-facing attendus:

- `delivery_mode`
- `delivery.patch_mode`
- `delivery.server_dispatch_enabled`
- `delivery.server_dispatch_target`
- `delivery.server_dispatch_transport`
- `delivery.server_dispatch_payload_ref`
- `delivery.server_dispatch_ack_required`

Commande complementaire:

- `apply_seed_contract`
- `sekailink_room_seed_package_dispatch`

`apply_seed_contract` prend le contrat `sekailink-link-room-seed-contract-v1`
emis par Worlds, le stocke dans la room et applique les `config_snapshot.values`
du slot courant comme `slot_data`.

`sekailink_room_seed_package_dispatch` est l'outil filesystem/TCP qui lit
`<package_dir>/link_room_seed_contract.json`, construit l'envelope admin et
peut soit l'imprimer, soit l'envoyer au room server TCP.

But:

- permettre a `Link Room` de raisonner sur une verticale `server-first`
- eviter de supposer que toute room possede un runtime `Sekaiemu + SKLMI`
