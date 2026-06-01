# ALTTP Phase 1 Next Items

Status: bounded phase-1 expansion note
Date: 2026-05-05

This note is now a historical stepping stone.
For the broader runtime-core inventory that supersedes it, see:

- [ALTTP_RUNTIME_CORE_INVENTORY.md](ALTTP_RUNTIME_CORE_INVENTORY.md)

## Goal

Identifier un petit sous-ensemble d'items ALTTP standards qui peuvent etre
declares des maintenant dans `actions[]` sans changer le schema `SKLMI` ni
inventer une logique runtime supplementaire.

Cette note borne volontairement la prochaine marche:

- seulement des items standard a un seul `item_code`
- seulement des deliveries `room_controlled`
- seulement le meme profil d'ecriture memoire que `Hookshot`

## Local Proof Used

Les preuves locales consultees sont:

- `reference-repos/Archipelago/worlds/alttp/Client.py`
  - la file de reception ecrit genericement:
    - `RECV_PROGRESS_ADDR` (`0xF4D0`) avec l'index recu
    - `RECV_ITEM_ADDR` (`0xF4D2`) avec `item.item`
    - `RECV_ITEM_PLAYER_ADDR` (`0xF4D3`) avec l'auteur
- `reference-repos/Archipelago/worlds/alttp/Items.py`
  - fournit les `item_code` canoniques des items ALTTP
- `reference-repos/alttp-ap-poptracker-pack/items/items.json`
  - confirme que certains items ont une surface tracker simple et distincte

Inference repo-owned a partir de ces sources:

- si un item standard ALTTP est represente par un `item_code` simple dans
  `Items.py`, alors il peut suivre le meme profil de delivery memoire que
  `Hookshot`
- les items dont la surface tracker locale est un simple toggle sont de bons
  candidats pour la prochaine extension phase 1

## Recommended Next Set

Le sous-ensemble retenu ici est:

1. `Hammer`
2. `Lamp`
3. `Magic Mirror`
4. `Moon Pearl`
5. `Pegasus Boots`

Raisons:

- tous ont un `item_code` direct dans `Items.py`
- tous ont une presence claire dans le pack tracker local
- aucun ne depend ici d'une semantique progressive, composite, bouteille, ou
  multi-etat particuliere
- tous peuvent etre modeles comme une delivery `room_controlled` bornee avec
  trois writes, exactement comme `Hookshot`

## Canonical Item Ids And Event Keys

Le contrat repo-owned retenu pour cette passe est:

| item_name | Archipelago item_id | event_key | mapped_value |
| --- | ---: | --- | --- |
| `Hammer` | `9` | `item.hammer` | `Hammer` |
| `Lamp` | `18` | `item.lamp` | `Lamp` |
| `Magic Mirror` | `26` | `item.magic_mirror` | `Magic Mirror` |
| `Moon Pearl` | `31` | `item.moon_pearl` | `Moon Pearl` |
| `Pegasus Boots` | `75` | `item.pegasus_boots` | `Pegasus Boots` |

## Shared Write Plan

Pour chacun de ces items, le plan borne reste:

1. incrementer `system_ram + 0xF4D0` avec `current_plus`
2. ecrire `item_id` sur `system_ram + 0xF4D2`
3. ecrire `0` sur `system_ram + 0xF4D3` pour un item local-slot

Cela reste coherent avec le contrat `SKLMI` actuel:

- ordre preserve
- pas de promesse d'atomicite materielle
- pas de nouvelle source d'ecriture

## Why These Items And Not Others

Cette passe exclut volontairement:

- `Bow`, `Progressive Bow`, `Progressive Sword`, `Progressive Glove`
  - semantics progressives ou multi-etapes
- `Blue Boomerang`, `Red Boomerang`
  - surface tracker composite locale
- `Shovel`, `Flute`, `Mushroom`, `Magic Powder`
  - surface tracker locale a etats multiples ou evenementielle
- `Bottle*`
  - famille a variants multiples
- `Bombos`, `Ether`, `Quake`
  - toggles cote item, mais deja lies a d'autres lectures medallion cote tracker

Ils ne sont pas interdits par le schema. Ils sont seulement hors de ce sous-
ensemble "faible risque / preuve locale simple".

## What Is Still Not Proven

Cette extension reste partiellement non prouvee sur trois points:

- aucun smoke `SKLMI` local n'exerce encore chacun de ces cinq items un par un
- aucune run live reelle n'a encore montre ces deliveries cote room
- les `tracker_node_id` concrets restent volontairement `pending-pack-import`

Autrement dit:

- le schema est inchange
- le profil memoire est localement justifie
- la preuve de bout en bout live reste a faire item par item
