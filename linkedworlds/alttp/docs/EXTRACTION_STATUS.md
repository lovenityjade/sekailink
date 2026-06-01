# Extraction Status

## Current State

Ce chantier est maintenant reprenable sans relire tout le monorepo:

- manifest canonique initialise
- metadata de base initialisees
- contrat memoire ALTTP place dans `bridge/` sur forme compatible `SKLMI`
- snapshots des sources actives preserves
- contrat patch minimal redeclare depuis `worlds/alttp`
- preset par defaut minimal redeclare dans `presets/`
- bindings tracker/runtime redeclares et relies au contrat memoire ALTTP dans
  `tracker/`
- bundle tracker clean-room materialise dans `tracker/default.bundle/`
- surface run-complete tracker materialisee dans `tracker/default.bundle/surface.complete.json`
- groupes tracker run-complete materialises avec `286` locations assignees
  depuis le bridge actif
- slots items tracker run-complete materialises avec `145` actions assignees
  depuis le bridge actif
- `tracker/default.zip` et `config/default.yaml` materialises
- inventaire ALTTP ajoute pour sortir du simple echantillon phase 1
- doc de run temps reel ajoutee
- checklist testeur externe ajoutee

Lecture d'architecture:

- le contenu jeu-specifique reste ici
- `SKLMI` et `Link Room` sont des consommateurs de ce contenu, pas ses
  proprietaires

## Canonical vs Snapshot

Canonique dans ce repo:

- `manifest/manifest.json`
- `metadata/*.json`
- `bridge/*.json`
- `tracker/*.json`
- `patch/*.json`
- `presets/*.json`
- `docs/*.md`

Snapshot uniquement:

- `source-snapshots/*`

Regle:

- un champ conserve depuis la source n'est canonique qu'une fois redeclare dans les dossiers de travail ci-dessus
- un snapshot peut justifier une extraction, mais ne remplace pas un contrat de repo
- un fichier de forme `sklmi` reste ici un contrat linkedworld ALTTP si ce repo
  en porte la verite canonique

## Source Roots

- `sekaiemu/client/linkedworlds/alttp/`
- `sekaiemu/worlds/alttp/`
- `sklmi/manifests/alttp.phase1.json`

## Missing For First Clean Repo Cut

- decision finale entre baseline clean-room et pack externe-compatible epingle
- presets utilisateur plus complets
- preuve live exhaustive au niveau de la surface run-complete declaree
- compatibilites versionnees avec `Contracts`, `Sekaiemu`, `Link Room`, `Worlds`
- inventaire explicite des champs encore herites du monorepo
- suppression future du fallback `trace.jsonl` une fois `room.state` complet en live

## Resume Path

1. prouver en live davantage de checks/items/actions ALTTP deja declares
2. confirmer si la surface run-complete repo-owned suffit ou si un pack
   externe-compatible doit la remplacer
3. sortir davantage d'options utiles en presets normalises
4. versionner les compatibilites externes
5. remplacer ensuite le bridge phase 1 transitoire par une forme plus complete

## Immediate Next Pass

1. lire `docs/HANDOFF.md`
2. lire `docs/TRACKER_RUN_COMPLETE_SURFACE.md`
3. lire `docs/LINKEDWORLD_CONTENT_OWNERSHIP.md`
4. finaliser les `tracker_node_id` et la preuve live sur
   `tracker/runtime-bindings.phase1.json`
5. epingler si necessaire une revision concrete du pack tracker retenu
6. completer d'autres presets a partir des options ALTTP utiles cote room/test
