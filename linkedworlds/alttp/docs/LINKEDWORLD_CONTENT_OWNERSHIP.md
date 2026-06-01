# ALTTP LinkedWorld Ownership

Ce repo doit etre lu comme un package de contenu `LinkedWorld` pour `ALTTP`.

## Principe

Le contenu jeu-specifique vit ici:

- checks et familles de checks
- items, actions et mappings de reception
- metadata room/seed attendues
- surface tracker, bundle et mappings lisibles
- patch contract et validation ROM
- presets runtime et defaults utiles au jeu

## Ce que `SKLMI` est ici

`SKLMI` est une famille de runtime/transport memoire.

Elle peut consommer:

- `bridge/sklmi.phase1.json`

`Link Room` consomme autre chose:

- les events gameplay emis a partir de ces checks/actions
- les metadata room/seed utiles au suivi de session
- l'etat tracker/room expose par le module

Mais ce fichier ne transforme pas `ALTTP` en specialisation `SKLMI`.

Le contrat reste:

- possede par ce linkedworld
- versionne dans ce repo
- relie au tracker, a la metadata et au patch contract du jeu

En pratique:

- `SKLMI` consomme surtout la projection memoire/checks/actions
- `Link Room` consomme surtout la projection events/metadata/session
- `Sekaiemu` consomme surtout la projection packaging/tracker/patch/preset

## Regle pratique

Quand une question porte sur:

- un check ALTTP
- un item ALTTP
- une action ALTTP
- un champ metadata tracker
- un bundle tracker
- un preset runtime jeu

la reponse canonique doit vivre dans ce repo, meme si la forme technique du
contrat est compatible avec un runtime `SKLMI`.

## Pool generation ALTTP normal

`generation/item-pool.normal.v1.json` materialise maintenant un pool compact
ALTTP `open-no-glitches-normal` de `153` items via des entrees comptees.

Provenance et limites:

- les comptes sont une traduction de donnees du pool ALTTP local, pas une copie
  de code executable Archipelago;
- les identites `item_id`, `event_key` et `tracker_slot_id` sont recoupees avec
  `tracker/item-slots.complete.json`;
- les variantes de bouteilles sont compactees sur `Bottle` pour garder une
  surface deterministe;
- dungeon items, shops, universal keys, triforce hunt, timers, beemizer et
  pools custom restent hors scope de ce pool compact;
- le pool non vide autorise seulement la surface `can_build_item_pool`; solve et
  placement natifs restent gates tant que Worlds C++ ne consomme pas les autres
  surfaces.

## Locations fillables

`generation/fillable-locations.open-no-glitches-normal.v1.json` est la surface
qui doit lier le pool normal de `153` items a `153` emplacements fillables
explicites.

Regle importante:

- `tracker/location-groups.complete.json` expose `286` checks pour le tracker et
  le runtime;
- ces `286` checks ne sont pas automatiquement des emplacements de placement;
- Worlds doit lire la surface `fillable-locations`, pas deduire le placement
  depuis le tracker complet;
- tant que `sets.main_pool_fillable` n'a pas `153` entrees auditees,
  `can_place_items` doit rester `false`.
- l'etat courant est mecaniquement aligne a `153` entrees explicites, mais
  reste une proposition en attente d'audit semantique ALTTP; le gate placement
  doit donc rester ferme.
