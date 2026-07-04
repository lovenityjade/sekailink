# Extraction Status

## BETA-3 Direction

Source de verite immediate:

- `../../../docs/BETA3_ARCHITECTURE_CANON.md`
- `../../../docs/repo-contracts/sklmi.md`

## Current State

Ce repo clean-room est autonome pour le coeur `sklmi`:

- `include/` contient l'API publique du bridge memoire
- `src/` contient le runtime et les implementations generiques
- `tests/` couvre le smoke, le legacy manifest, et les preuves runtime
- `docs/` contient le contrat, les flows runtime, et les handoffs historiques
- `CMakeLists.txt` expose la lib et les executables de validation

## Remaining Cleanup

- sortir les contenus manifest permanents specifiques a un jeu vers les repos
  `LinkedWorld`
- sortir la gouvernance de schema et les politiques inter-repos vers
  `Contracts`
- garder dans `sklmi` uniquement des exemples/tests minimaux qui prouvent le
  contrat runtime
- continuer a preferer le format `LinkedWorld` + bloc `sklmi` au manifest legacy

## Hard Rule

`SKLMI` reste le bridge memoire runtime des jeux emules, mais pour `BETA-3` il
parle le langage/comportement Archipelago et remplace les couches Lua/SNI
fragiles. Il ne devient pas un moteur gameplay global ni un repo de logique
jeu canonique.

## Present Boundary

Appartient a `sklmi`:

- interfaces memoire
- decode/validation du contrat manifest
- sessions runtime
- glue room sync
- persistence locale de bridge
- tests du comportement runtime generique

N'appartient pas a `sklmi`:

- logique de gameplay par jeu
- authoring permanent des manifests de jeu
- autorite room distante
- schema governance partagee entre repos

## Notes

- Le repo ne contient pas actuellement de dossier `manifests/` actif.
- `fixtures/` et `source-snapshots/` sont reserves a du support de test ou de
  migration, pas a du code runtime.
