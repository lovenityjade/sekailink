# SoH Server-Port Vertical

Date: `2026-05-17`

Cycle 1 pivote vers `SoH` comme verticale pilote.

Pourquoi:

- eviter qu'un patcher ROM local ALTTP bloque encore la reprise
- valider la generisation de `Worlds` sur un linkedworld `server-first`
- forcer la separation entre patch local, contrat serveur et metadata de seed

Regles:

- `Worlds` ne branche pas sur `soh` en dur
- `Worlds` lit les capacites du linkedworld
- pour `SoH`, `Worlds` doit pouvoir emettre:
  - metadata de seed
  - metadata de slot
  - contrat room-facing de dispatch
  - package sans patch ROM local

Non-objectifs du cycle 1:

- runtime tracker SoH
- bridge memoire local SoH
- packaging final public SoH

Etat valide le 2026-05-18:

- `sekailink-linkedworld-soh` expose `patch.mode=server_dispatch`.
- `Worlds` accepte ce linkedworld sans ouvrir les gates de generation native.
- `Worlds` emet un package sans patch ROM local, sans checks, sans items et sans placements.
- Le package contient `link_room_seed_contract.json` et `patch_contracts/slot-1.patch.contract.json`.
- Le snapshot de config `Jade-SoH` est transporte dans les manifests et le contrat Link Room.

Commande de generation locale:

```sh
/tmp/sekailink-worlds-cycle1-build/sekailink_generic_generation_package \
  /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-soh \
  /tmp/sekailink-soh-cycle1-package \
  seed-soh-cycle1 \
  jade-soh-cycle1 \
  1 \
  1001 \
  Jade \
  7001 \
  /home/thelovenityjade/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-soh/presets/jade-soh.config-snapshot.json
```

Verification locale:

```sh
/tmp/sekailink-worlds-cycle1-build/sekailink_generic_generation_smoke
/tmp/sekailink-worlds-cycle1-build/sekailink_generation_server_smoke
```

Note: `sekailink_generation_server_smoke` ouvre un socket TCP local. Dans le
sandbox Codex, il peut echouer avec `generation_server_start_failed`; hors
sandbox il a ete valide avec `generation_server_ok=1`.

Sortie package attendue:

- `seed_manifest.json` rapporte `patch_mode=server_dispatch`.
- `seed_manifest.json` rapporte `location_count=0`, `item_count=0`, `placement_count=0`.
- `patch_contracts/slot-1.patch.contract.json` a `artifact=null`.
- `patch_contracts/slot-1.patch.contract.json` a `server_dispatch.target=link_room`.
- `patch_contracts/slot-1.patch.contract.json` a `server_dispatch.transport=room_contract`.
- `patch_contracts/slot-1.patch.contract.json` a `server_dispatch.payload_ref=link_room_seed_contract.json`.

Handoff Link Room:

```sh
/tmp/sekailink-link-room-cycle1-build/sekailink_room_seed_package_dispatch \
  --package-dir /tmp/sekailink-soh-cycle1-package/seed-soh-cycle1 \
  --room-id room-linkedworld-soh-1 \
  --print
```

Cette commande lit `link_room_seed_contract.json` dans le package Worlds et
prepare l'envelope `apply_seed_contract` pour Link Room.
