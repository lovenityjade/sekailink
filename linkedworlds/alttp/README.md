# Target Repo: sekailink-linkedworld-alttp

Ce repo clean-room est la zone de reprise dediee au linkedworld `ALTTP`.

But:

- sortir les contrats utiles hors du monorepo source
- rendre explicite ce qui est canonique ici
- conserver les captures source sans les confondre avec la cible finale
- porter ici le contenu jeu-specifique du linkedworld `ALTTP`

Statut:

- chantier prioritaire
- base active detectee
- reprise preparee dans ce dossier
- statut vise ici: `test complete prep`
- extraction encore partielle, mais les surfaces de test externe sont maintenant cadrees

Lecture d'architecture:

- `docs/LINKEDWORLD_CONTENT_OWNERSHIP.md`

Ce repo est le proprietaire du contenu linkedworld `ALTTP`.

Ce qui vit ici:

- `metadata/`: identite jeu et metadata tracker/room
- `patch/`: contrat patch et validation ROM
- `bridge/`: checks, items, actions, et contrat memoire ALTTP
- `tracker/`: surface tracker, mappings, bundle source, zip materialise
- `presets/`: defaults et presets runtime ALTTP
- `docs/`: runbooks, handoff, checklist, clarification d'ownership

Canonique dans ce repo:

- `manifest/manifest.json`
- `metadata/module.json`
- `metadata/provenance.json`
- `metadata/room-metadata.complete.json`
- `metadata/slot-data.complete.json`
- `bridge/sklmi.phase1.json`
- `bridge/bridge.manifest.json`
- `tracker/tracker.manifest.json`
- `tracker/runtime-bindings.phase1.json`
- `tracker/location-groups.complete.json`
- `tracker/item-slots.complete.json`
- `tracker/dungeon-progress.complete.json`
- `tracker/default.bundle/manifest.json`
- `tracker/default.bundle/surface.complete.json`
- `tracker/default.zip`
- `generation/generation-ir.json`
- `generation/item-pool.normal.v1.json`
- `generation/fillable-locations.open-no-glitches-normal.v1.json`
- `generation/logic.open-no-glitches.v1.json`
- `generation/placement.open-no-glitches.v1.json`
- `patch/patch.manifest.json`
- `presets/presets.manifest.json`
- `presets/alttp.ap-defaults.core.json`
- `presets/alttp.runtime-test.side-by-side.json`
- `presets/alttp.runtime-complete.side-by-side.json`
- `config/default.yaml`
- `docs/EXTRACTION_STATUS.md`
- `docs/HANDOFF.md`
- `docs/REALTIME_TEST_RUN.md`
- `docs/TEST_COMPLETE_PREP.md`
- `docs/EXTERNAL_TEST_CHECKLIST.md`
- `docs/TRACKER_RUN_COMPLETE_SURFACE.md`
- `docs/LINKEDWORLD_CONTENT_OWNERSHIP.md`
- `docs/generation-contracts.md`
- `CHANGELOG.md`

Snapshots de migration seulement:

- `source-snapshots/linkedworld.current.json`
- `source-snapshots/sklmi.phase1.current.json`
- `source-snapshots/*.md`

Reference:

- `../../../docs/repo-contracts/sekailink-linkedworld-game.md`

Contrat de lecture rapide:

- `Sekaiemu` consomme la surface linkedworld/runtime et patch: identite du jeu, `runner`, `system`, `module_id`, chemins par defaut tracker/yaml, preset de base, contrat patch, reference bridge
- le preset `alttp.runtime-test.side-by-side` porte la cible de presentation et de metadata room pour un test runtime coherent
- `bridge/sklmi.phase1.json` est un contrat memoire ALTTP possede par ce repo,
  au format qu'un runtime `SKLMI` peut consommer
- le tracker consomme des bindings semantiques, pas des suppositions sur les `tracker_node_id`
- la variante `Map Tracker - AP` reste une couche de presentation du pack externe, distincte du contrat runtime
- `tracker/default.zip` et `config/default.yaml` existent maintenant sous forme clean-room-owned, avec une baseline honnete mais non equivalente a un pack tiers epingle
- `tracker/default.bundle/surface.complete.json` porte maintenant l'inventaire repo-owned checks/items/settings cible pour une run complete, distinct de la couverture live `phase1`
- `generation/placement.open-no-glitches.v1.json` materialise les contraintes declaratives issues de `risk_audit`, mais elles ne sont pas encore consommees par Worlds et n'autorisent pas la generation native
- `generation/logic.open-no-glitches.v1.json` porte maintenant une passe auditee
  de `item_effects` et `locations.rules` partiellement consommables par schema,
  mais non autorisantes: `can_solve_logic` reste `false`, la reachability
  native reste fermee, et les big keys/bombs/regions/objectifs ALTTP restent
  incomplets
- `item_effects` declare aussi des grants progressifs/counts consommables pour
  `progressive_sword`, `progressive_glove`, `progressive_bow`,
  `progressive_shield`, `progressive_mail`, et les bottles generiques; cela
  autorise seulement la derivation de faits/stages/counts, pas le solve natif
  ALTTP ni les familles encore bloquees: ressources depensables, dungeon keys,
  prizes/objectifs, et profils non-normal comme swordless/retro bow
- `item_effects` expose maintenant aussi des faits LinkedWorld-owned pour les
  bombes du pool normal (`cap.can_use_bombs`, `cap.can_bomb_or_bonk`), les
  rupees comme total additif non depense, `magic_upgrade_half`, `mushroom`,
  `flute`, et `shovel`; cela ne declare pas les paiements, la depense de
  ressources, les followers, les exchanges differes, ni le solve natif
- `generation/item-pool.normal.v1.json` declare une policy dungeon-key separee
  et non consommee pour les big keys requises par Desert Palace, Swamp Palace,
  et Skull Woods big chests; ces keys ne contribuent pas au main pool normal
  `153`, et `placement`/`logic` les gardent non autorisantes
- cette policy est maintenant formulee comme contrat generique
  `sekailink-generic-dungeon-key-policy-v1`: roles `main_pool`/
  `separate_key_pool`, policy anti self-lock bloquee, facts requis, bindings
  locations-vers-keys, et checklist explicite de ce que Worlds doit consommer
  avant tout usage natif; elle reste `declared-not-consumed`
- `generation/logic.open-no-glitches.v1.json` materialise maintenant le
  `region_traversal_binding_plan` en `region_graph` declaratif:
  `17` nodes, `16` edges, et `118/118` `location_region_bindings` uniques;
  l'audit des edges est maintenant partiel: `3/16` edges remplacent le
  placeholder par des requires declares (`mode_open` + `no_glitches`), `13/16`
  restent bloques avec blockers explicites; la surface reste non consommee et
  non autorisante (`native_logic_solve=false`) jusqu'a preuve de consommation
  C++ generique
- deuxieme passe conservative sur les `13` edges
  `blocked-unresolved-traversal`: aucun placeholder supplementaire n'a pu etre
  remplace uniquement avec les facts declares dans `item_effects`, `locations`,
  ou `starting_state`; les `13` restent bloques avec blockers plus precis,
  `authorizes=false`, et `generation-ir.json` reste intouche
- troisieme passe conservative sur ces `13` edges: `0` edge nouvellement
  audite, `13` restent bloques, et `13` placeholders restent en place; la
  surface `region_graph.edge_audit.edge_blocker_requirements` declare les
  contrats LinkedWorld manquants par edge et regroupe les blockers sans ouvrir
  de gate ni modifier `generation-ir.json`
- quatrieme passe contractuelle sur ces memes `13` edges: aucun placeholder
  n'est retire, mais
  `region_graph.edge_audit.missing_generation_contract_surfaces` declare `4`
  surfaces versionnees `declared-contract-not-consumed` pour les blockers
  `dark_world_route_or_portal_state_missing`, `dungeon_policy_missing`,
  `reward_or_medallion_contract_missing`, et
  `npc_or_follower_state_missing`; `placement` les reference comme contraintes
  non consommees et `generation-ir.json` reste intouche
- raffinement consommable des deux surfaces de traversee generiques prioritaires:
  `dark_world_route_or_portal_state_missing` et
  `npc_or_follower_state_missing` exposent maintenant `contract_roles`,
  `required_fact_families`, `fact_templates`, `route_state_examples`, et
  `location_refinement_requirements`; ces templates restent non declares comme
  facts, donc les `13` placeholders restent en place et aucun gate n'est ouvert
- raffinement cible des surfaces donjons/rewards: `dungeon_policy_missing`
  expose maintenant `fact_templates`, `key_rule_templates`, et `edge_bindings`
  pour small keys, big keys, intra-dungeon traversal et les `8` edges donjons;
  `reward_or_medallion_contract_missing` expose `reward_gate_templates`,
  `medallion_requirement_templates`, et bindings crystal-count/medallion pour
  GT/Mire/TR. Ces contrats restent non consommes, les keys restent hors main
  pool normal, les `13` placeholders restent en place, et aucun gate n'est
  ouvert

Regle de reprise:

1. mettre a jour la forme canonique dans `manifest/`, `metadata/`, `bridge/`, `tracker/`, `patch/`, `presets/`
2. garder `source-snapshots/` comme reference historique
3. ne pas promouvoir un snapshot en canon sans l'avoir redeclare proprement ici
