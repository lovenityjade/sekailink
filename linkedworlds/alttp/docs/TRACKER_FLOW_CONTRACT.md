# ALTTP Tracker Flow Contract

Status: canonical tracker data-flow contract
Date: 2026-05-10

## Objectif

Ce contrat decrit le flux tracker complet pour une run ALTTP sans ajouter de
code runtime dans `SKLMI`, Link Room ou Sekaiemu.

La source machine-readable est:

- `tracker/default.bundle/tracker-flow.v1.json`

Le contrat est data-driven. Sekaiemu peut le lire comme instructions de tracker:
quels fichiers charger, comment appliquer les metadata seed/settings, comment
projeter les items serveur, et quand changer automatiquement d'onglet/map.

## Frontieres

Ce contrat appartient au LinkedWorld ALTTP:

- il consomme les donnees room/session/serveur deja disponibles
- il projette ces donnees vers l'etat tracker
- il reference les surfaces normalisees du bundle ALTTP

Ce contrat ne fait pas:

- d'ecriture memoire `SKLMI`
- de changement de protocole Link Room
- de copie de code PopTracker
- de rendu UI impose a Sekaiemu

Les packs PopTracker historiques peuvent rester des references de compatibilite,
mais les instructions tracker livrees ici sont des donnees SekaiLink.

## Sequence de chargement

Sekaiemu doit appliquer le flux dans cet ordre:

1. Charger `tracker/default.bundle/manifest.json`.
2. Charger les refs du bundle: surface, groupes de locations, slots items,
   progression donjon, metadata room, slot_data, puis `tracker-flow.v1.json`.
3. Charger les metadata seed/slot depuis `room.state` ou le payload session.
4. Appliquer les settings depuis `slot_data.complete.json`.
5. S'abonner au feed item serveur et aux evenements location/session.
6. Maintenir `active_tab_id` et `active_map_id` via la logique autotab.

## Feed Items Serveur

Le feed attendu est append-only et idempotent.

Payload minimal:

```json
{
  "delivery_id": 7,
  "event_key": "item.hookshot"
}
```

Payload recommande:

```json
{
  "delivery_id": 7,
  "item_id": 10,
  "item_name": "Hookshot",
  "event_key": "item.hookshot",
  "tracker_semantic_id": "item.hookshot",
  "mapped_value": "Hookshot",
  "source_player": "Player 2",
  "recipient_slot": "Player 1",
  "location_id": 59836,
  "location_name": "Link's House",
  "server_index": 42
}
```

Resolution d'identite, dans l'ordre:

1. `item_id`
2. `event_key`
3. `tracker_semantic_id`
4. `item_name`
5. `mapped_value`

Projection:

- matcher l'item dans `tracker/default.bundle/item-slots.complete.json`
- appliquer le comportement du slot (`progressive`, `toggle`, compteur, etc.)
- dedupliquer par `delivery_id`
- incrementer `received_count` seulement a la premiere application
- conserver les items non mappes dans un journal tracker, sans bloquer la run

L'event optionnel `tracker_item_observed` est seulement un accusé de lecture
tracker. Il ne remplace pas l'ack gameplay de Link Room ou `SKLMI`.

## Metadata Seed Et Settings

Les metadata requises restent:

- `seed_id`
- `seed_hash`
- `slot_name`
- `player_alias`
- `tracker_pack`
- `tracker_variant`
- `slot_data`

Priorite de chargement:

1. `room.state.seed_metadata`
2. `room.state.slot_data`
3. `session.connect_payload`
4. trace `room_metadata_ready`, uniquement comme fallback de test
5. defaults de preset

Si une metadata obligatoire manque, le tracker affiche `UNKNOWN` et marque
`metadata_status` comme incomplet. Si `slot_data` arrive plus tard, Sekaiemu
recalcule les panels settings sans reset inventory/checks.

Les settings sont affiches via:

- `settings_core`
- `settings_logic`
- `settings_dungeons`
- `settings_world`
- `goal_progress`

Ils ne doivent modifier que l'etat tracker et les panels visibles. Ils ne
doivent pas modifier la memoire jeu, le feed item serveur, l'etat des checks ou
les actions `SKLMI`.

## Autotabbing

L'autotab choisit un onglet et une map a partir de contextes declaratifs.

Priorite:

1. `force_autotab_event`
2. check/location actif ou valide
3. preview de check actif
4. contexte donjon
5. contexte zone
6. focus session explicite
7. feed item serveur
8. onglet par defaut

Le verrou utilisateur `tracker_state.manual_tab_lock` suspend les changements
automatiques jusqu'a son retrait ou un `force_autotab_event`.

Sources de mapping:

- locations: `location-groups.complete.json`, champ `preferred_tab`
- donjons: `dungeon-progress.complete.json`, champ `preferred_tab`
- zones directes: `tracker-flow.v1.json`, section `autotab_zone_rules`
- maps: `manifest.json`, onglets et maps declares

Exemples:

- `location_group_id: "light_world_surface"` -> tab `light-world`, map
  `light_world`
- `dungeon_id: "palace_of_darkness"` -> tab `pod`, map `dark_dungeons`
- `zone_id: "ganons_tower"` -> tab `gt`, map `turtle_gt`
- item recu sans contexte plus fort -> tab `items`, pas de map imposee

## Validation

Validation attendue pour cette passe:

- tous les JSON tracker/docs/presets parses
- `config/default.yaml` parse
- `tracker/default.bundle/tracker-flow.v1.json` reference uniquement des tabs et
  maps declares par `manifest.json`
- `tracker/default.zip` contient le contrat de flux et reste aligne avec
  `tracker/default.bundle/`

## Fichiers De Reference

- `tracker/default.bundle/tracker-flow.v1.json`
- `tracker/default.bundle/manifest.json`
- `tracker/default.bundle/surface.complete.json`
- `tracker/default.bundle/location-groups.complete.json`
- `tracker/default.bundle/item-slots.complete.json`
- `tracker/default.bundle/dungeon-progress.complete.json`
- `tracker/default.bundle/room-metadata.complete.json`
- `tracker/default.bundle/slot-data.complete.json`
- `config/default.yaml`
- `presets/alttp.runtime-complete.side-by-side.json`
