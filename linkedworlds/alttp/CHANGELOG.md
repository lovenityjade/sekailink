# Changelog

## 2026-05-10

- conversion partielle des surfaces contractuelles prioritaires
  `dark_world_route_or_portal_state_missing` et
  `npc_or_follower_state_missing` en contrats plus consommables:
  ajout de `contract_roles`, `required_fact_families`, `fact_templates`,
  `route_state_examples`, et `location_refinement_requirements` dans
  `generation/logic.open-no-glitches.v1.json`, avec miroir non autorisant dans
  `generation/placement.open-no-glitches.v1.json`
- conversion partielle des surfaces `dungeon_policy_missing` et
  `reward_or_medallion_contract_missing`: ajout de `fact_templates`,
  `key_rule_templates`, `reward_gate_templates`,
  `medallion_requirement_templates`, et bindings machine-readable vers les
  edges/locations GT, Ice, Mire, PoD, Skull, Swamp, Thieves et Turtle; ces
  contrats restent `declared-*-not-consumed`
- miroir item-pool de `key_rule_templates` sous `dungeon_key_policy` pour small
  keys original-dungeon, big keys en `separate_key_pool`, et anti self-lock; le
  main pool normal reste a `153` et aucune dungeon key n'y est ajoutee
- aucun edge bloque n'a ete promu: les facts deja declares restent seulement
  partiels, les templates ajoutes ne sont pas des facts declares, les `13`
  placeholders restent requis, `authorizes`/`authorizes_native_placement`
  restent false, et `generation-ir.json` reste intouche
- quatrieme passe contractuelle sur les `13` edges
  `blocked-unresolved-traversal`: aucun placeholder n'a pu etre remplace
  honnetement avec les seuls facts deja declares, donc les `13` placeholders
  restent; ajout de `4` surfaces versionnees
  `declared-contract-not-consumed` sous
  `region_graph.edge_audit.missing_generation_contract_surfaces`
- formalisation machine-readable des blockers
  `dark_world_route_or_portal_state_missing`, `dungeon_policy_missing`,
  `reward_or_medallion_contract_missing`, et
  `npc_or_follower_state_missing`: chaque surface liste son schema, ses edges,
  ses roles de facts generiques a declarer, son statut non consomme, et garde
  `authorizes=false`
- miroir placement via `generation_contract_constraints` et ajout de
  `docs/generation-contracts.md`; aucun gate/capability n'est ouvert,
  aucun `requires` n'est force, et `generation-ir.json` reste intouche
- troisieme passe sur les `13` edges `blocked-unresolved-traversal` du
  `region_graph`: `0` nouvel edge audite, `13` edges restent bloques, et les
  `13` placeholders `predicate.region.traversal_declared_not_consumed:*`
  restent necessaires parce qu'aucun requires complet ne peut etre formule avec
  les seuls facts deja declares
- ajout de `region_graph.edge_audit.edge_blocker_requirements`: chaque edge
  bloque liste ses facts partiels declares, les contrats LinkedWorld manquants,
  `placeholder_required=true`, et `satisfiable_with_declared_facts=false`; les
  blockers sont aussi regroupes par familles route/portal dark-world, groupes
  provisoires trop grossiers, policy donjon, rewards/medallions, et NPC/follower
- miroir des compteurs de troisieme passe dans
  `generation/placement.open-no-glitches.v1.json`; aucun gate n'est flippe,
  `authorizes` reste false, et `generation-ir.json` n'est pas modifie
- evolution de la surface `dungeon_key_policy` en contrat generique
  `sekailink-generic-dungeon-key-policy-v1`: declaration des roles
  `main_pool`/`separate_key_pool`, de la policy anti self-lock bloquee, des
  facts requis, des bindings locations-vers-keys, et des preconditions
  manquantes pour une consommation Worlds generique; la surface reste
  `declared-not-consumed`, les keys restent hors main pool, aucun gate n'est
  flippe, et `generation-ir.json` reste intouche
- deuxieme passe sur les `13` edges `blocked-unresolved-traversal` du
  `region_graph`: `0` nouvel edge audite, `13` edges restent bloques, et les
  placeholders `predicate.region.traversal_declared_not_consumed:*` restants
  sont conserves parce qu'aucun requires complet ne peut etre formule avec les
  seuls facts deja declares dans `item_effects`, `locations`, ou
  `starting_state`
- precision des blockers par edge bloque: les facts partiels honnetes
  (`item.moon_pearl`, `item.flippers`, `item.magic_mirror`, `item.fire_rod`,
  bomb/bonk/hookshot deja declares ailleurs) restent notes comme insuffisants;
  aucun gate n'est flippe, `authorizes` reste false, et `generation-ir.json`
  n'est pas modifie
- declaration d'une surface `dungeon_key_policy` LinkedWorld-owned dans
  `generation/item-pool.normal.v1.json`: les big keys Desert Palace, Swamp
  Palace, et Skull Woods vivent dans un `separate_key_pool` non consomme,
  ne contribuent pas au main pool normal `153`, et n'ouvrent aucun gate natif
- liaison explicite des 3 rules bloquees `Desert Palace - Big Chest`,
  `Swamp Palace - Big Chest`, et `Skull Woods - Big Chest` vers cette policy
  via `generation/logic.open-no-glitches.v1.json`, avec
  `native_logic_solve=false` et `native_placement=false` conserves
- miroir de la policy dans `generation/placement.open-no-glitches.v1.json`
  comme contrainte non consommee; aucune modification de `generation-ir.json`
  et aucune ouverture de placement Worlds
- audit conservateur des `16` edges du `region_graph`: `3` edges
  (`light_world_surface`, `hyrule_castle_and_escape`, `misc_checks`) remplacent
  le placeholder `predicate.region.traversal_declared_not_consumed:*` par
  `predicate.state.mode_open` + `predicate.state.no_glitches`; `13` edges
  restent bloques avec `status=blocked-unresolved-traversal`, blockers
  explicites, et aucun flip de gate natif
- ajout d'un miroir d'audit dans
  `generation/placement.open-no-glitches.v1.json`: `16` total, `3` audites,
  `13` bloques, `118/118` bindings conserves, `authorizes_native_placement=false`
- materialisation prudente du `region_traversal_binding_plan` en
  `region_graph` declaratif pour Worlds: `17` nodes dont le start, `16` edges
  non consommees, et `118/118` `location_region_bindings` uniques
- miroir des compteurs graph dans
  `generation/placement.open-no-glitches.v1.json`; maintien de
  `declared-not-consumed`, `native_logic_solve=false`,
  `authorizes_native_placement=false`, et aucun changement a
  `generation-ir.json`
- blockers inchanges avant toute activation native: remplacer les segments
  provisoires par un graphe audite, declarer de vraies requires de traversal,
  ajouter les raffinements par location, et prouver la consommation C++ Worlds
  sans branche ALTTP specifique
- enrichissement declaratif de `item_effects` a `27` effets miroires dans
  `candidate_item_effects`: bombes du pool normal vers `cap.can_use_bombs` et
  `cap.can_bomb_or_bonk`, rupees comme faits additifs non depenses,
  `magic_upgrade_half` comme capacite simple, plus faits `mushroom`, `flute`,
  et `shovel`
- promotion prudente des `9` locations bomb-only deja candidates vers un
  statut `declared-consumable-resource-capability`; les locations rupee/priced,
  followers, fetch exchanges, dungeon keys, regions et objectifs restent non
  autorises pour le solve natif
- maintien explicite de `authorizes.native_logic_solve=false`,
  `can_solve_logic=false`, et absence de changement a `generation-ir.json` ou
  aux gates de placement natif
- enrichissement declaratif de `item_effects` pour les progressifs supportes
  par Worlds: `progressive_sword`, `progressive_glove`, `progressive_bow`,
  `progressive_shield`, `progressive_mail`, plus un count generique pour
  `item.bottle`; seuls `fact_derivation`, `progressive_stage_derivation`, et
  `generic_count_derivation` sont autorises, avec `native_logic_solve=false`
- documentation des familles toujours non autorisees pour la logique native:
  ressources bombes/fleches depensables, dungeon keys, prizes/objectifs,
  medallion requirements, et profils hors normal comme swordless/retro bow
- audit des `candidate_item_effects` et `candidate_location_rules` dans
  `generation/logic.open-no-glitches.v1.json`, puis miroir vers des blocs
  `item_effects` et `locations.rules` plus proches d'une surface consommable
  par schema
- promotion prudente de `11` effets monotoniques simples et de `19` formes de
  regles de locations, avec `authorizes.native_logic_solve=false` et sans
  modifier `generation-ir.json`, `can_solve_logic`, ni `can_place_items`
- annotation des limites restantes: bombs/resources sans semantique consommee,
  big keys absentes du pool normal, absence de region graph, de traversal
  donjon complet, de regles d'objectif, et de preuve de consommation C++ Worlds
- materialisation dans `generation/placement.open-no-glitches.v1.json` des
  contraintes declaratives issues de `risk_audit` avec
  `authorizes_native_placement=false`; elles restent non consommees et ne
  changent pas `can_place_items=false`
- enrichissement de `generation/logic.open-no-glitches.v1.json` comme surface
  indexable/non-autorisante: refs catalogues, capacites partielles, predicates
  de base, et ruleset vide explicite sans activer `can_solve_logic`
- ajout des surfaces de generation declaratives:
  `generation/item-pool.normal.v1.json`,
  `generation/logic.open-no-glitches.v1.json`, et
  `generation/placement.open-no-glitches.v1.json`
- ajout des refs `item_pool_ref`, `logic_rules_ref`, et `placement_rules_ref`
  dans `generation/generation-ir.json` sans ouvrir les gates natifs
- materialisation de `generation/item-pool.normal.v1.json` comme pool ALTTP
  normal compact: `43` entrees comptees pour `153` items expanses par Worlds
- ouverture de `can_build_item_pool=true` maintenant que le pool de generation
  est non vide et distinct des items tracker
- maintien de `can_solve_logic=false` et `can_place_items=false`: les surfaces
  logic/placement existent, mais la logique ALTTP complete n'est pas encore
  declaree comme consommable pour une seed native
- ajout de `generation/fillable-locations.open-no-glitches-normal.v1.json`
  pour separer explicitement les `153` emplacements attendus du pool normal
  des `286` checks tracker/runtime
- materialisation de `sets.main_pool_fillable` avec `153` entrees explicites,
  uniques et recoupees avec `tracker/location-groups.complete.json`; la
  selection reste `proposed-selection-pending-audit`
- ajout de `risk_audit.location_tags_pending` pour isoler `19` checks a risque
  avant activation du placement: pedestal, tablets, priced/fetch NPC, hobo,
  purple chest et big chests
- ajout de `candidate_item_effects` et `candidate_location_rules` dans
  `generation/logic.open-no-glitches.v1.json` pour une premiere tranche de
  `19` regles simples, non consommees et non autorisantes
- mise a jour de `generation/placement.open-no-glitches.v1.json` pour pointer
  vers la surface fillable dediee au lieu de laisser Worlds deduire les
  emplacements depuis le tracker complet
- ajout de `generation/generation-ir.json` comme surface Worlds generique pour
  ALTTP, avec references catalogues, patch, runtime, options et metadata room
- declaration explicite des gates manquants `can_solve_logic` et
  `can_place_items` pour empecher Worlds de pretendre generer une seed native
  ALTTP avant le port C++ du solver/placeur
- materialisation de la surface tracker par groupes:
  `tracker/location-groups.complete.json` et sa copie bundle assignent
  maintenant les `286` checks actifs du bridge a exactement un groupe tracker
- materialisation de la surface tracker par slots items:
  `tracker/item-slots.complete.json` et sa copie bundle assignent maintenant
  les `145` actions room-controlled a exactement un slot tracker
- extension des slots item normalises de `27` a `38` pour couvrir aussi les
  ressources, consommables, fillers et traps deja presents dans le bridge
- ajout d'un resume `coverage` dans les groupes de locations pour exposer le
  lien direct avec `bridge/sklmi.phase1.json`
- realignement de `tracker/default.zip` avec `tracker/default.bundle/`:
  l'archive embarque maintenant aussi `location-groups`, `item-slots`,
  `dungeon-progress`, `room-metadata`, et `slot-data`
- clarification de `tracker/default.bundle/surface.complete.json`:
  `covered_location_group_assignments` vaut maintenant `286`
- aucun changement de schema `SKLMI`, de contrat global, ni de code moteur

## 2026-05-05

- extension declarative majeure de `bridge/sklmi.phase1.json` et
  `tracker/runtime-bindings.phase1.json` vers la surface ALTTP utile quasi
  complete lue par le client:
  `286` checks et `145` actions room-controlled sans changement de schema
- couverture checks maintenant alignee sur toutes les familles `Client.py`
  bornables proprement:
  underworld, npc, overworld, misc, et `37` slots shops nommes
- couverture actions etendue a tous les items ALTTP a `item_code` entier dans
  `Items.py`, y compris progression, dungeon items, consumables, fillers, et
  traps
- realignement des manifests et snapshots sur ces nouveaux compteurs, avec
  limites restantes recentrees sur prizes / crystals / pendants, slot shop
  reserve non nomme, et preuve live exhaustive
- ajout de `tracker/dungeon-progress.complete.json` pour normaliser la surface
  donjons, rewards, prizes, medallions, et objectifs de run complete
- ajout du preset `presets/alttp.runtime-complete.side-by-side.json` pour
  porter une vraie cible de run complete cote linkedworld
- materialisation d'une baseline clean-room-owned pour `tracker/default.zip` et
  `config/default.yaml`, avec source canonique dans `tracker/default.bundle/`
- ajout de `tracker/default.bundle/surface.complete.json` pour normaliser une
  premiere surface run-complete checks/items/settings cote tracker ALTTP
- extension du bundle clean-room avec onglets monde/donjons et maps placeholder
  supplementaires pour une run side-by-side plus complete
- ajout de `docs/TRACKER_RUN_COMPLETE_SURFACE.md` pour separer la surface cible
  repo-owned de la couverture live encore limitee au bridge `phase1`
- extension declarative large de `bridge/sklmi.phase1.json` vers un baseline
  runtime-core ALTTP:
  `54` checks et `35` actions room-controlled sans changement de schema
- realignement de `tracker/runtime-bindings.phase1.json` sur cette surface
  runtime-core, avec statuts et compteurs harmonises dans les manifests
- extension bornee de `bridge/sklmi.phase1.json` et
  `tracker/runtime-bindings.phase1.json` avec cinq items standards
  supplementaires a delivery `room_controlled`:
  `Hammer`, `Lamp`, `Magic Mirror`, `Moon Pearl`, `Pegasus Boots`
- ajout de `docs/PHASE1_NEXT_ITEMS.md` pour documenter la preuve locale, le
  profil memoire partage avec `Hookshot`, et les familles explicitement exclues
  de cette passe
- enrichissement de `patch/patch.manifest.json` avec le contrat `.aplttp`, la validation ROM et les dependances runtime optionnelles
- enrichissement de `tracker/tracker.manifest.json` avec la surface pack externe, le bridge phase 1 et la couverture minimale connue
- ajout de `tracker/runtime-bindings.phase1.json` pour les bindings semantiques `Sanctuary`, `Link's House` et `Hookshot`
- declaration du preset canonique minimal `presets/alttp.ap-defaults.core.json`
- mise a jour de `presets/presets.manifest.json` pour declarer un preset par defaut repo-owned
- ajout de `docs/REALTIME_TEST_RUN.md` pour documenter un run de verification temps reel complet
- mise a jour des docs de reprise et du manifest canonique du repo
- ajout de `docs/TEST_COMPLETE_PREP.md` pour cadrer la preparation au test externe et la separation runtime/tracker/map
- clarification dans les manifests de ce que `Sekaiemu` consomme reellement, de ce que le tracker doit resoudre, et de ce qui reste presentationnel
- consolidation des README et manifests `bridge/`, `tracker/`, `patch/`, `presets/` pour un usage testeur externe plus direct
- remplacement des champs `pending` du manifest par un contrat launch/distribution plus concret pour `.aplttp`, ROM source, bundle tracker et metadata room
- ajout du preset `presets/alttp.runtime-test.side-by-side.json` pour decrire le test runtime cible sans forcer le host a deviner la presentation
- ajout de `docs/TEST_COMPLETE_PREP.md` pour separer clairement le contrat complet de test et les dependances encore a materialiser
- correction du `item_id` phase 1 pour `Hookshot` afin de rester coherent avec la reference Archipelago ALTTP (`10` / `0x0A`)
- ajout de `docs/ROOM_CONTROLLED_LIVE_CONTRACT.md` pour cadrer le contrat live
  room-controlled ALTTP sans elargir le schema
- ajout de la sequence de traces operateur attendue pour une delivery
  room-controlled supportee cote `SKLMI`
- ajout de `docs/EXTERNAL_TEST_CHECKLIST.md` pour cadrer la lecture d'une session
  par un testeur externe et fixer les criteres `passable pour test externe`
- durcissement de `docs/REALTIME_TEST_RUN.md` sur l'ordre de lecture metadata:
  `room.state` d'abord, `trace.jsonl` ensuite seulement comme fallback de phase test
- clarification des blocages restants dans `docs/TEST_COMPLETE_PREP.md`
