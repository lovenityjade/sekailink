# ALTTP Handoff

Ce fichier sert de point d'entree rapide pour un agent qui reprend `ALTTP` sans relire le monorepo.

Point d'architecture prioritaire:

- `ALTTP` est un module `LinkedWorld` canonique possede par ce repo
- `SKLMI` et `Link Room` en consomment des projections differentes

## Read Order

1. `README.md`
2. `manifest/manifest.json`
3. `metadata/module.json`
4. `metadata/provenance.json`
5. `metadata/room-metadata.complete.json`
6. `metadata/slot-data.complete.json`
7. `docs/EXTRACTION_STATUS.md`
8. `bridge/bridge.manifest.json`
9. `patch/patch.manifest.json`
10. `tracker/tracker.manifest.json`
11. `tracker/location-groups.complete.json`
12. `tracker/item-slots.complete.json`
13. `tracker/dungeon-progress.complete.json`
14. `docs/ALTTP_RUNTIME_CORE_INVENTORY.md`
15. `tracker/runtime-bindings.phase1.json`
16. `presets/presets.manifest.json`
17. `presets/alttp.ap-defaults.core.json`
18. `presets/alttp.runtime-test.side-by-side.json`
19. `presets/alttp.runtime-complete.side-by-side.json`
20. `docs/REALTIME_TEST_RUN.md`
21. `docs/TEST_COMPLETE_PREP.md`
22. `docs/TRACKER_RUN_COMPLETE_SURFACE.md`
23. `docs/LINKEDWORLD_CONTENT_OWNERSHIP.md`
24. `docs/EXTERNAL_TEST_CHECKLIST.md`
25. `CHANGELOG.md`

## What Is Already Concrete

- le module cible est `A Link to the Past`
- le systeme cible est `SNES`
- une baseline metadata existe dans `metadata/module.json`
- un contrat memoire ALTTP existe dans `bridge/sklmi.phase1.json`
- `SKLMI` consomme la projection memoire/checks/actions de ce contenu
- `Link Room` consomme la projection events/metadata/session du meme contenu
- un contrat patch `.aplttp` minimal est redeclare dans `patch/patch.manifest.json`
- un preset canonique minimal existe dans `presets/alttp.ap-defaults.core.json`
- un preset runtime de test existe dans `presets/alttp.runtime-test.side-by-side.json`
- un inventaire/priorisation runtime-core existe dans
  `docs/ALTTP_RUNTIME_CORE_INVENTORY.md`
- des bindings tracker/runtime aligns sur le bridge runtime-core existent dans
  `tracker/runtime-bindings.phase1.json`
- un bundle tracker clean-room existe en source dans `tracker/default.bundle/`
- une surface run-complete checks/items/settings existe dans `tracker/default.bundle/surface.complete.json`
- une surface donjons/rewards/objectifs existe dans `tracker/dungeon-progress.complete.json`
- un archive tracker canonique existe en `tracker/default.zip`
- un YAML canonique existe en `config/default.yaml`
- des snapshots de comparaison existent dans `source-snapshots/`
- une checklist testeur externe existe dans `docs/EXTERNAL_TEST_CHECKLIST.md`

## What Is Not Done Yet

- decision finale entre baseline placeholder clean-room et pack externe epingle
- couverture bridge live encore non exhaustive sur les familles dungeon/shop
- finalisation des `tracker_node_id` et de la preuve live au-dela du binding
  declaratif actuel
- versionnage clair des compatibilites externes

## Safe Working Rules

- modifier la forme canonique du repo, pas `source-snapshots/` pour y ecrire la verite finale
- ne pas inventer de comportements runtime non verifies
- preferer marquer `pending` plutot que remplir un faux detail
- conserver la distinction entre "transitional canonical" et "snapshot"
- ne pas deplacer l'ownership du contenu ALTTP vers un moteur generique

## Suggested Next Extraction Pass

1. verifier les details de patch depuis `sekaiemu/worlds/alttp/Rom.py`
2. finaliser les correspondances pack-facing a partir du bridge et des besoins
   runtime reels
3. confirmer si le couple `tracker/default.zip` + `config/default.yaml` materialise ici reste la baseline canonique
4. sinon epingler une revision externe-compatible du pack tracker retenu
5. confirmer la strategie tracker finale: externe referencee ou asset vendore
