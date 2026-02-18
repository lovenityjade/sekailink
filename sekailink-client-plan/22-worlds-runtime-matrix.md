# Worlds to runtime matrix (catalogue extensible)

But: definir une matrice "world -> pipeline runtime" utilisable par:
- l'UI (afficher prerequis, boutons Fix)
- l'orchestrator (choisir steps)
- les agents IA (se reperer rapidement)

Ce doc ne liste pas tous les worlds du repo (trop grand). Il definit:
- un schema de matrice
- une methode pour la construire (automatisable)
- un noyau initial (modules deja presents + cas speciaux)

## Colonne standard (recommandee)

Chaque entree doit contenir:
- `game_id`
- `world_id` (souvent egal a game_id, mais pas toujours)
- `module_id` (SekaiLink runtime module)
- `download_kind`: `ap_patch` ou `slot_file`
- `patch_exts[]` (si `ap_patch`)
- `generation_type`: voir `sekailink-client-plan/18-external-web-generation.md`
- `runtime_family`: `bizhawk_lua`, `sni`, `dolphin_memory`, `mod_loader`, `engine_port`, etc.
- `emulator_candidates[]` (avec contraintes de version)
- `connector`: type + fichier(s)
- `tracker`: PopTracker pack uid/repo + variant default, UT supported oui/non
- `required_user_assets[]`: rom, bios, keys, firmware, game install
- `installers[]`: quoi telecharger automatiquement
- `notes`: risques, caveats, legal

## Sources de donnees (dans ce repo)

Sources deja presentes:
- manifests runtime: `client/runtime/modules/*/manifest.json`
- schema runtime: `client/schema/game_runtime.schema.json` (minimal, a etendre)
- heuristiques guides: `CLIENT_AUTOLAUNCH_RAW.json`
- patchers externes: `CLIENT_EXTERNAL_PATCH_URLS.json`
- docs worlds: `worlds/*/docs/setup*.md` et `worlds/*/docs/*setup*.md`
- logic worlds: `worlds/*/__init__.py` (souvent contient des hints)

## Methode (workflow) pour construire la matrice

Etape 1:
- inventorier `room_status.downloads` pour differents rooms
- confirmer par world si on recoit `download_patch` ou `download_slot_file`

Etape 2:
- parser les guides:
- detecter familles (BizHawk + Lua, SNI, Dolphin, mod loader, external web generator)
- extraire liens (releases, sites, patchers)

Etape 3:
- creer ou mettre a jour un module runtime SekaiLink:
- declarer `rom_requirements` (md5/sha1) quand applicable
- declarer `lua_connector` si BizHawk
- declarer `tracker_pack_uid` et variants
- declarer contraintes runtime (version ranges)

Etape 4:
- valider:
- schema JSON
- tests de resolution patch extension -> moduleId (pas de hardcode)
- prereqs check (bouton Play desactive tant que pas OK)

## Noyau initial (modules runtime existants)

Modules presents dans `client/runtime/modules/`:
- `alttp_bizhawk`
- `dkc_bizhawk`
- `dkc2_bizhawk`
- `dkc3_bizhawk`
- `earthbound_bizhawk`
- `ff4fe_bizhawk`
- `ffta_bizhawk`
- `kdl3_bizhawk`
- `lufia2ac_bizhawk`
- `marioland2_bizhawk`
- `metroidfusion_bizhawk`
- `mm2_bizhawk`
- `mm3_bizhawk`
- `mzm_bizhawk`
- `oot_bizhawk`
- `pokemon_red_bizhawk`
- `pokemon_blue_bizhawk`
- `pokemon_crystal_bizhawk`
- `pokemon_firered_bizhawk`
- `pokemon_leafgreen_bizhawk`
- `pokemon_emerald_bizhawk`
- `bizhawk_base` (base/shared)
- `sm_bizhawk`
- `smw_bizhawk`
- `tloz_bizhawk`
- `tloz_oos_bizhawk`
- `tmc_bizhawk`
- `wl_bizhawk`
- `wl4_bizhawk`
- `zelda2_bizhawk`

Pipelines confirms:
- family: `bizhawk_lua`
- patch kind: `ap_patch` (extensions `.ap*`)
- patch apply: `client/runtime/patcher_wrapper.py` (injection ROM via `settings.get_settings()`)
- emulator: BizHawk vendore Linux (MVP)
- tracker: PopTracker (auto-install possible via repo GitHub releases)

## Cas speciaux a integrer dans la matrice des maintenant

Worlds externes ou local-only:
- voir `sekailink-client-plan/18-external-web-generation.md`

Contraintes BizHawk:
- certains worlds exigent BizHawk < 2.10 alors que d'autres exigent BizHawk >= 2.10
- consequence: matrice doit porter `emulator_version_min/max` (voir `sekailink-client-plan/17-third-party-emulators.md`)

## Slot files (non encore supporte cote SekaiLink)

Constat:
- le serveur peut renvoyer `download_slot_file` (pas un `.ap*`)

Action dans la matrice:
- chaque world qui renvoie un slot file doit avoir:
- `download_kind = slot_file`
- `slot_file_exts[]` (si connu)
- `slot_file_pipeline` (extract, copy, install mod, launch)

Note (etat actuel):
- certaines slot files ont deja des handlers "automation" (best-effort) cote client Electron main:
  - Factorio `.zip` (mod) + launch
  - gzDoom `.pk3` + launch (requires IWAD + gzArchipelago.pk3)
  - Dark Souls III `.json` (staging + installer package)
  - Super Mario 64 EX `.apsm64ex` (launch via SM64EX build path; compilation non automatisee MVP)

## Output attendu (fichier matrice)

Format recommande:
- un JSON versionne et machine-readable (pour le client)
- un markdown ou csv lisible (pour les humains)

Nom propose:
- `client/registry/worlds_runtime.generated.json` (genere)
- `sekailink-client-plan/22-worlds-runtime-matrix.md` (ce doc, reference)
