# ALTTP Test-Complete Prep

Ce document cadre ce qui doit etre vrai pour considerer `ALTTP` pret a un test
runtime side-by-side propre avec une surface tracker plus proche d'une run
complete.

## Cible

Le runtime doit pouvoir charger:

- un patch `.aplttp`
- une ROM source `Japan 1.0`
- un bundle tracker materialise en `tracker/default.zip`
- un YAML par defaut materialise en `config/default.yaml`
- un bridge `SKLMI` actif

Et afficher:

- le jeu a gauche
- le tracker a droite
- une variante tracker `Map Tracker - AP`
- une surface visible map plus items plus metadata seed/settings

## Contrats canoniques relies

- `manifest/manifest.json`
- `patch/patch.manifest.json`
- `tracker/tracker.manifest.json`
- `tracker/runtime-bindings.phase1.json`
- `tracker/default.bundle/surface.complete.json`
- `tracker/dungeon-progress.complete.json`
- `presets/alttp.runtime-test.side-by-side.json`
- `bridge/sklmi.phase1.json`

## Ce que le runtime ne doit pas deviner

- quel pack tracker utiliser
- quelle variante tracker prendre par defaut
- quel YAML charger
- quelles metadata de room/seed exposer au tracker
- quelle presentation side-by-side viser
- quels groupes checks/items/settings doivent etre visibles pour une run complete

Toutes ces informations doivent venir du module canonique.

## Materiel minimum maintenant present

Le repo porte maintenant une baseline minimale et honnete pour:

- `tracker/default.zip`
- `config/default.yaml`
- `tracker/default.bundle/surface.complete.json`

Cela retire le blocage de packaging le plus evident.

## Metadata room/seed a fournir

Les metadata suivantes doivent etre disponibles pour le runtime:

- `seed_id`
- `seed_hash`
- `slot_name`
- `player_alias`
- `tracker_pack`
- `tracker_variant`
- `slot_data`

Metadata utiles mais optionnelles a ce stade:

- `goal`
- `mode`
- `entrance_shuffle`
- `item_pool`
- `item_functionality`
- `dark_room_logic`
- `open_pyramid`
- `big_key_shuffle`
- `small_key_shuffle`

## Surface tracker attendue

Le contrat vise une presentation side-by-side:

- panneau gauche: jeu
- panneau droit: tracker

Le tracker doit pouvoir exprimer au minimum:

- une vue `items`
- des vues map monde et donjons principales
- une surface inventaire/items normalisee
- une surface statut/seed
- une surface settings seed utile a une run complete
- une surface donjons/rewards/objectifs pour bosses, prizes et crystals

La cible de surface complete est decrite dans:

- `tracker/default.bundle/surface.complete.json`

## Etat honnete

Ce repo est maintenant plus pres d'un vrai test runtime:

1. `tracker/default.zip` existe
2. `config/default.yaml` existe
3. la surface checks/items/settings cible est explicite
4. la surface donjons/rewards/objectifs est explicite

Le vrai gap restant n'est plus l'absence de packaging, mais la difference entre:

- la surface tracker complete preparee par le repo
- la part de cette surface reellement prouvee en run live, au-dela du bridge et
  des bindings runtime-core maintenant alignes

## Blocages restants les plus concrets

1. couverture `SKLMI` encore partielle par rapport a la surface complete cible,
   surtout cote donjons, shops, boss et preuves live exhaustives
2. compatibilite finale du bundle placeholder avec un host externe non
   determinee
3. decision encore ouverte entre baseline clean-room et pack externe epingle
4. la preuve finale testeur externe depend encore du jugement humain sur:
   - lisibilite du tracker
   - lisibilite des logs
   - suffisance de la surface items et settings
