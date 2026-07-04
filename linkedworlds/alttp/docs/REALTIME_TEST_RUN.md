# ALTTP Real-Time Test Run

Ce document decrit un run de verification complet sans inventer de pipeline hors des sources deja referencees.

## Objectif

Verifier ensemble:

- la validation ROM et la surface patch `ALTTP`
- le contrat memoire ALTTP consomme par `SKLMI`
- la surface runtime minimale utile a `Sekaiemu`, `SKLMI` et `Link Room`
- le rattachement tracker actuellement documente

## Artefacts utilises

- `patch/patch.manifest.json`
- `bridge/sklmi.phase1.json`
- `tracker/tracker.manifest.json`
- `tracker/runtime-bindings.phase1.json`
- `tracker/default.bundle/surface.complete.json`
- `presets/alttp.ap-defaults.core.json`
- `presets/alttp.runtime-test.side-by-side.json`
- `sekaiemu/client/linkedworlds/alttp/linkedworld.json`
- `sekaiemu/worlds/alttp/Rom.py`
- `sekaiemu/worlds/alttp/Options.py`
- `sekaiemu/worlds/alttp/__init__.py`
- `sklmi/tests/sklmi_alttp_golden_main.cpp`

## Pre-checks

1. Confirmer que la ROM source correspond bien a `Japan 1.0`.
   Exemple de verification:
   `md5sum "<rom>.sfc"`
   MD5 attendu: `03a63945398191337e896e5771f77173`

2. Confirmer la surface runtime declaree par `linkedworld.json`.
   Valeurs attendues:
   - `linkedworld_id`: `alttp`
   - `runner`: `snes`
   - `system`: `snes`
   - `module_id`: `alttp_bizhawk`
   - `default_yaml`: `config/default.yaml`
   - `default_tracker_pack`: `tracker/default.zip`

3. Confirmer que le bridge canonique de ce repo reste `bridge/sklmi.phase1.json`.
   Note:
   le nom de fichier reste heritage, mais le contenu n'est plus limite a un
   simple echantillon phase 1.
4. Confirmer que la cible de presentation runtime reste `side-by-side` via `presets/alttp.runtime-test.side-by-side.json`.

## Etape 1: Verification patch

Ce repo ne remplace pas le generateur amont; il documente le contrat extrait:

1. `get_base_rom_path()` resolve la ROM via `lttp_options.rom_file`.
2. `LocalRom.patch_base_rom()` applique `data/basepatch.bsdiff4` et peut reutiliser `user_path/basepatch.sfc`.
3. `patch_rom()` applique la generation ALTTP.
4. `patch_enemizer()` ne doit etre active que si `ALTTPWorld.use_enemizer` devient vrai.
5. `patch_race_rom()` ne doit etre active qu'en mode race.
6. `apply_rom_settings()` applique la couche cosmetique/runtime.
7. La sortie finale attendue est un delta `.aplttp`; le `.sfc` intermediaire est temporaire.

Critere de succes:

- la ROM de base passe la verification MD5
- la sortie finale est un patch `.aplttp`
- aucune etape supplementaire n'est introduite localement

## Etape 2: Verification bridge SKLMI

Le test de reference deja pointe par ce repo est
`sklmi/tests/sklmi_alttp_golden_main.cpp`.

Important:

- ce test valide un runtime capable de consommer le contrat memoire ALTTP
  redeclare dans ce repo
- il ne deplace pas l'ownership des checks/items/actions hors du linkedworld

Ce test couvre:

- deux checks `location_checked`
- une injection `item_received`
- la lecture/ecriture sur `system_ram`

Le contrat memoire canonique de ce repo couvre maintenant bien plus large:

- `286` checks declares
- `145` actions room-controlled declarees

Breakdown utile:

- `220` underworld
- `13` npc
- `12` overworld
- `4` misc
- `37` shops nommes

Pour l'inventaire/priorisation exacts, voir:

- [ALTTP_RUNTIME_CORE_INVENTORY.md](ALTTP_RUNTIME_CORE_INVENTORY.md)

Evenements et adresses attendus:

- `Sanctuary`: event key `0xEA79`, adresse `61476` (`0xF024`)
- `Link's House`: event key `0xE9BC`, adresse `61960` (`0xF208`)
- `Hookshot`: `item_id` Archipelago `10` (`0x0A`), ecrit sur `62672` (`0xF4D0`), `62674` (`0xF4D2`), `62675` (`0xF4D3`)

Exemples maintenant declares dans le manifest au-dela de `Hookshot`:

- `Bow`: `item_id` `11` (`0x0B`)
- `Hammer`: `item_id` `9` (`0x09`)
- `Magic Mirror`: `item_id` `26` (`0x1A`)
- `Moon Pearl`: `item_id` `31` (`0x1F`)
- `Progressive Sword`: `item_id` `94` (`0x5E`)
- `Progressive Bow`: `item_id` `100` (`0x64`)

Pour ces actions room-controlled, le profil memoire reste identique a
`Hookshot`:

- increment `0xF4D0`
- write `item_id` sur `0xF4D2`
- write player slot `0` sur `0xF4D3`

Critere de succes:

- le test retourne la chaine `sklmi_alttp_golden_ok`
- les trois evenements attendus sont observes

Note:

- ce document ne fige pas la commande de build du test, car elle depend du workspace `SKLMI`; seul le binaire logique de reference est canonise ici

## Etape 3: Verification tracker/runtime

Le repo materialise maintenant une baseline tracker/YAML clean-room-owned. La
surface canonique minimale est donc:

- UID pack externe: `https://github.com/StripesOO7/alttp-ap-poptracker-pack`
- variante par defaut: `Map Tracker - AP`
- archive canonique materialisee: `tracker/default.zip`
- YAML canonique materialise: `config/default.yaml`
- surface run-complete normalisee: `tracker/default.bundle/surface.complete.json`
- preset runtime: `presets/alttp.runtime-test.side-by-side.json`
- bindings semantiques: `tracker/runtime-bindings.phase1.json`

Le contenu jeu-specifique a considerer comme possede ici est:

- les checks ALTTP
- les items et actions ALTTP
- les metadata room/seed utiles au tracker
- la surface tracker side-by-side

Critere de succes:

- le runtime peut s'appuyer sur des bindings semantiques declares pour `286`
  checks et `145` actions room-controlled
- le runtime emet au minimum les checks `Sanctuary` et `Link's House`
- le runtime emet au minimum l'item `Hookshot`
- le runtime affiche une surface tracker separee avec sections `map`, `inventory`, `status`
- aucune hypothese sur des `tracker_node_id` internes n'est ajoutee tant qu'un
  pack externe-compatible n'est pas epingle

## Etape 4: Run temps reel minimal

Pour un run temps reel minimal, la pile attendue est:

1. `Sekaiemu` charge le linkedworld `alttp` sur surface `snes`.
2. La ROM de base validee sert a produire le patch `.aplttp`.
3. Le bridge `SKLMI` phase 1 surveille `system_ram` au rythme declare (`16 ms` dans le bridge snapshot actuel).
4. `Link Room` ou toute surface evenementielle equivalente consomme au minimum:
   - `location_checked` pour `Sanctuary`
   - `location_checked` pour `Link's House`
   - `item_received` pour `Hookshot`
   Et peut maintenant declarativement mapper une surface bien plus large sans
   changement de schema, y compris bosses de donjons, key drops, et shops
   nommes deja declares dans le LinkedWorld ALTTP.
5. Le tracker consomme au minimum les bindings semantiques declares, sans supposer les IDs internes du pack.

## Lecture metadata en run live

Ordre prefere:

1. `room.state`
2. `trace.jsonl`

Chemin prefere:

- `room.state` contient directement les `meta|...` utiles au tracker

Chemin de secours acceptable pour la run de test:

- si `room.state` reste presque vide, `Sekaiemu` peut encore recuperer une
  partie de l'identite room/seed/slot depuis:
  - `room_client_ready`
  - `room_metadata_ready`
  - `slot_connected`

Ce fallback ne remplace pas le contrat final, mais il suffit pour garder le
tracker lisible pendant la phase de test si la room moderne n'a pas encore
replique toutes les metadata dans `room.state`.

## Preuve operateur attendue

Pour considerer la run live exploitable, il faut pouvoir montrer au moins:

- une identite de session visible cote tracker:
  - `seed_id`
  - `seed_hash`
  - `slot_id` ou `slot_name`
- une identite tracker visible:
  - `tracker_pack`
  - `tracker_variant`
- un evenement gameplay visible:
  - `location_checked`
  - ou `item_received`

Pour le contrat live room-controlled detaille, voir aussi:

- [ROOM_CONTROLLED_LIVE_CONTRACT.md](ROOM_CONTROLLED_LIVE_CONTRACT.md)
- [EXTERNAL_TEST_CHECKLIST.md](EXTERNAL_TEST_CHECKLIST.md)

## Gaps Restants

- couverture bridge encore incomplete sur les families profondes de donjons,
  shops, boss, cristaux et pendentifs
- pack tracker externe de reference non epingle
- baseline `tracker/default.zip` et `config/default.yaml` presente, mais non
  revendiquee comme equivalente a un pack tiers
- preset canonique volontairement minimal
- aucune promesse locale sur un wrapper CLI unique tant qu'il n'existe pas deja en source
- la parite visuelle complete avec un vrai pack PopTracker reste hors de ce repo
- la dependance sur le fallback `trace.jsonl` doit disparaitre des que `room.state`
  porte naturellement toutes les metadata room/seed/slot

Note importante:

- aucun changement de schema `SKLMI` n'etait necessaire pour ce palier
  runtime-core
- le bridge a ete etendu en restant sur:
  - `mask_any` pour les checks
  - `current_plus` + `item_id` + `player` pour les items recus
- la prochaine preuve utile est maintenant:
  - soit une extension des smokes sur cette surface plus large
  - soit des runs live qui valident plusieurs familles d'items et de checks
