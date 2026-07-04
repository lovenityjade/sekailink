# ALTTP Runtime Core Inventory

Status: runtime surface expansion note
Date: 2026-05-05

## Goal

Pousser le bridge declaratif ALTTP bien au-dela du simple echantillon phase 1,
sans changer le schema `SKLMI` et sans deplacer de logique jeu-specifique vers
le moteur generic.

Tout ce qui suit reste du contenu `LinkedWorld` ALTTP:

- checks et familles de checks
- actions room-controlled et mappings item
- compteurs de couverture et limites encore ouvertes

## Local Sources Used

References lues seulement:

- `sekaiemu/worlds/alttp/Client.py`
  - tables de lecture `location_table_uw`, `location_table_npc`,
    `location_table_ow`, `location_table_misc`
  - buffer shops `SHOP_ADDR`, `SHOP_LEN`
  - voie de reception item `RECV_PROGRESS_ADDR`, `RECV_ITEM_ADDR`,
    `RECV_ITEM_PLAYER_ADDR`
- `sekaiemu/worlds/alttp/Regions.py`
  - `location_table`
  - `key_drop_data`
  - `lookup_name_to_id`
- `sekaiemu/worlds/alttp/Shops.py`
  - `shop_table_by_location_id`
  - ordonnancement des slots shops et extras
- `sekaiemu/worlds/alttp/Items.py`
  - `item_code` ALTTP a valeur entiere injectables avec le contrat courant

## Declared Now

Le bridge actif `bridge/sklmi.phase1.json` declare maintenant:

- `286` checks
- `145` actions room-controlled

Les bindings `tracker/runtime-bindings.phase1.json` sont realignes sur les
memes compteurs:

- `286` `location_bindings`
- `145` `item_bindings`

Les groupes tracker run-complete sont maintenant aussi materialises:

- `286` checks actifs assignes dans `tracker/location-groups.complete.json`
- `286` checks actifs assignes dans
  `tracker/default.bundle/location-groups.complete.json`
- chaque check du bridge actif apparait exactement une fois dans cette surface
  de presentation

Les slots items tracker sont egalement materialises:

- `145` actions room-controlled assignees dans
  `tracker/item-slots.complete.json`
- `145` actions room-controlled assignees dans
  `tracker/default.bundle/item-slots.complete.json`
- les slots normalises passent de `27` a `38` pour couvrir les ressources,
  consommables, fillers et traps sans modifier le contrat bridge

## Check Coverage

Repartition exacte des `286` checks declares:

- `220` checks underworld
- `13` checks npc
- `12` checks overworld
- `4` checks misc
- `37` checks shops nommes

Ce palier couvre declarativement toute la surface ALTTP lue par
`Client.py`:

- donjons, y compris pot keys, key drops, boss checks, GT, Castle Tower,
  Eastern, Desert, Hera, Pod, Swamp, Skull, Thieves, Ice, Mire, Turtle
- checks overworld et montagne
- checks npc et misc
- buffer shops nommes, y compris `Old Man Sword Cave` et `Take-Any #1..#4`

Ce que cela change concretement:

- les familles donjons et boss ne sont plus des trous majeurs du bridge
- les shops ne sont plus une note theorique: les slots nommes sont declares
- la couverture declarative est maintenant proche de la surface live standard
  lue par le client ALTTP de reference

## Item Coverage

Les `145` actions room-controlled couvrent tous les items ALTTP dont
`Items.py` expose un `item_code` entier simple.

Cela inclut notamment:

- progression standard et progressive
- utility items et medallions
- bottles et variantes de bottles
- small keys, big keys, maps, compasses
- consumables, fillers, traps, clocks, bees, potions

Le profil memoire reste strictement le meme pour toutes ces actions:

- increment de `0xF4D0`
- write `item_id` sur `0xF4D2`
- write player byte sur `0xF4D3`

Autrement dit, l'extension de surface se fait entierement dans les artefacts
ALTTP de ce repo, sans specialiser `SKLMI`.

## Still Not Converted

Le reste non prouve ou non declare proprement sous le schema courant est
maintenant bien borne:

- dungeon prizes / crystals / pendants
  Ces familles n'apparaissent pas comme simples `item_code` entiers dans
  `Items.py`, donc elles ne rentrent pas proprement dans le contrat actuel
  `actions[]` sans nouvelle projection declarative cote LinkedWorld.
- etats evenements non lus par `Client.py`
  Exemples: `Agahnim 1`, `Agahnim 2`, `Ganon`, et autres etats `None` ou
  non exposes comme checks memoire standards.
- un slot buffer shop reserve non nomme
  Le buffer shops ALTTP a un trou implicite; il reste volontairement non
  declare tant qu'aucun nom clean-room utile n'est justifie.
- preuve live exhaustive
  Cette passe prouve la coherence declarative et les compteurs, pas encore une
  run live complete item-par-item et check-par-check.

## Why This Stays Generic

Cette passe n'ajoute aucune semantique ALTTP dans `SKLMI`.

Le moteur generic reste:

- lecteur de contrat memoire
- emetteur d'evenements
- executeur d'actions room-controlled

Le contenu ALTTP reste ici:

- noms de checks
- ids
- adresses
- masks
- familles shops / donjons / items

La hausse de couverture renforce donc la frontiere voulue:

- `SKLMI` = moteur generic plug-and-play
- `sekailink-linkedworld-alttp` = bridge declaratif ALTTP
