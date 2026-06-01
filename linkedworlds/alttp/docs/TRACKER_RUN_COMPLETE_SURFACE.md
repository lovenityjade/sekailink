# ALTTP Tracker Run-Complete Surface

Ce document fige une premiere surface tracker repo-owned pour une run ALTTP
complete cote packaging/linkedworld, sans pretendre que la preuve live
`SKLMI` est deja complete.

Point d'architecture:

- cette surface tracker appartient au module jeu `ALTTP`
- `SKLMI` transporte la projection memoire qui l'alimente
- `Link Room` consomme la projection events/metadata/session du meme module

## Fichiers de reference

- `tracker/default.bundle/manifest.json`
- `tracker/default.bundle/surface.complete.json`
- `tracker/default.bundle/tracker-flow.v1.json`
- `tracker/dungeon-progress.complete.json`
- `tracker/default.zip`
- `config/default.yaml`
- `tracker/runtime-bindings.phase1.json`
- `bridge/sklmi.phase1.json`

## Ce que cette passe materialise

- un bundle tracker clean-room plus large que la simple passe phase 1
- une structure side-by-side exploitable avec onglets monde, donjons et items
- des panneaux metadata/settings prevus pour une vraie run seed/slot
- un contrat de flux tracker data-driven pour charger les items serveur,
  charger les metadata seed/settings, appliquer les settings et autotabber
- un inventaire normalise checks/items/settings lisible sans copier un pack tiers
- une separation nette entre contenu tracker ALTTP et moteurs consommateurs
- une surface explicite pour rewards, bosses, prizes, medallions et objectifs
  de fin de partie
- une projection groupe/location consommable:
  `286` checks actifs du bridge sont maintenant assignes une seule fois dans
  `tracker/location-groups.complete.json` et
  `tracker/default.bundle/location-groups.complete.json`
- une projection slot/item consommable:
  `145` actions room-controlled sont maintenant assignees une seule fois dans
  `tracker/item-slots.complete.json` et
  `tracker/default.bundle/item-slots.complete.json`
- des slots items elargis de `27` a `38` pour representer les ressources,
  consommables, fillers et traps sans les forcer dans les slots progression
- une archive `tracker/default.zip` realignee avec le bundle canonique:
  elle embarque maintenant les inventaires normalises references par
  `manifest.json`

## Ce que cette passe ne pretend pas encore

- que tous les checks ALTTP remontent deja en live
- que tous les items soient deja prouves injectables en session reelle
- que le zip soit un substitut byte-for-byte a un pack externe
- que dungeon prizes / crystals / pendants soient injectes comme actions
  `SKLMI`; ils restent portes par la surface metadata/progression tant qu'une
  projection declarative propre n'est pas fixee

## Lecture recommandee

1. `tracker/default.bundle/surface.complete.json`
2. `tracker/default.bundle/manifest.json`
3. `config/default.yaml`
4. `tracker/runtime-bindings.phase1.json`
5. `bridge/sklmi.phase1.json`

## Interpretation honnete

Le bundle et le YAML sont maintenant materialises par le repo.

La surface tracker visee pour une run complete est donc explicite.

Le vrai gap restant n'est plus "on ne sait pas quoi afficher", mais
"combien de cette surface est deja prouvee en run live au-dela du bridge et des
bindings maintenant alignes declarativement".
