# Autolaunch et patch pipeline

## Source de patch cote serveur
- endpoint: `/api/room_status/<room_id>` (code: `WebHostLib/api/room.py`)
- champ: `downloads: [{slot, download}]`

Le serveur choisit entre:
1. `download_patch` si le world supporte `apdeltapatch`
2. `download_slot_file` sinon

Implication:
- le client doit traiter des `.ap*` patches (via `Patch.create_rom_file`)
- le client doit traiter des slot files (souvent zip, ou bundle specifique) et declencher un pipeline externe

## Patcher actuel (Archipelago Patch.create_rom_file)
- wrapper: `client/runtime/patcher_wrapper.py`
- applique un patch en se basant sur les settings ROM (ex: pokemon_frlg_settings, pokemon_emerald_settings)

Gaps:
- l'injection settings n'est pas generique, elle ne couvre pas tous les worlds

## Proposition: PatchService + game adapters

### Option A (pragmatique): adapter mapping par jeu
- maintenir un mapping `game_id -> injection settings` dans `patcher_wrapper.py`
- ajouter au fur et a mesure les worlds a ROM/ISO

Pros:
- rapide
- stable

Cons:
- maintenance continue

### Option B (plus propre): introspection settings.py
- introspecter `settings.get_settings()` et appliquer des chemins via un contract standard
- ex: dans manifest, un champ `archipelago_settings_paths` avec des strings type `settings.pokemon_frlg_settings.firered_rom_file` ou `settings.alttp_options.rom_file`

Pros:
- centralise

Cons:
- plus fragile si settings changent

## External patchers (ex: Twilight Princess)
- deja documente: `sekailink-docs/CLIENT_EXTERNAL_PATCHERS.md`

Pipeline typique:
1. download des assets randomizer (seed file, rel loader, etc)
2. patch/injection dans save Dolphin ou dossier mods
3. launch Dolphin
4. connect client

Le runtime contract doit pouvoir exprimer:
- inputs multiples (pas juste 1 `.ap*`)
- sorties multiples (fichiers a copier dans plusieurs emplacements)
- versions/outils vendores dans `third_party/`

## Multi-patch par joueur
- certains jeux peuvent produire plusieurs downloads par slot
- UI actuelle: `Lobby.tsx` permet "Launch All" si plusieurs patches
- contract: `features.multi_patch_per_player = true`

## Cache et stockage
- telechargements: `userData/runtime/patches/`
- outputs (ROM patch): `userData/runtime/roms/`
- garder `~/.sekailink/config.json` uniquement pour les inputs utilisateur (paths ROM, overrides)

