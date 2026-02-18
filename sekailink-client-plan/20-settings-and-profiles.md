# Settings, profils, et stockage (client SekaiLink)

But: definir ou les settings vivent, comment on les versionne, et comment ils s'appliquent aux runtimes (emulateurs, modloaders, trackers) sans casser l'objectif "0 interaction" pour les nouveaux joueurs.

## Stockage actuel (etat du code)

Config user:
- chemin: `~/.sekailink/config.json` (voir `client/app/electron/main.cjs:getConfigPath()`)
- lecture/ecriture: `config:get`, `config:setRom`, `tracker:installPack` ecrivent dedans

UserData (Electron):
- `app.getPath("userData")`
- ROMs importees: `userData/roms/` (copie par hash match)
- runtime output:
- patches: `userData/runtime/patches/`
- downloads: `userData/runtime/downloads/` (patches/slot files telecharges, nommes via Content-Disposition)
- ROMs patchees: `userData/runtime/roms/`
- packs PopTracker: `userData/poptracker/packs/<gameId>/...`

Implication:
- `~/.sekailink/config.json` ne doit contenir que des references et des preferences, pas des artefacts lourds
- les artefacts lourds sont dans `userData/`

## Schema propose pour `~/.sekailink/config.json`

Le schema exact n'est pas encore formalise dans le repo. Ce doc propose un schema de reference (a stabiliser).

Champs recommandes:
- `version`: int (migration)
- `roms`: map `game_id -> path` (ROM vanilla)
- `bios`: map `system_id -> path(s)` (PS1, PS2, DS, etc.)
- `keys`: map `system_id -> path(s)` (Switch prod.keys, etc.)
- `firmware`: map `system_id -> path(s)`
- `trackerPacks`: map `game_id -> {path, source, installed_at}`
- `trackerVariants`: map `game_id -> variant`
- `games`: map `game_key -> config` (paths install/mods, exe, etc.)
- `layout`: prefs d'affichage (preset, displays, streamer)
- `windowing.gamescope`: `{enabled, mode, fullscreen, width, height, args}`
- `emulators`: selection et overrides (par family et par version)
- `modules`: overrides par module (rare)

Regles:
- ne jamais stocker de mot de passe AP dans ce fichier
- ne jamais stocker de token d'auth serveur dans ce fichier si on peut l'eviter
- eviter de stocker des paths sensibles pour streamer mode (options de masquage)

Exemples `config.games`:
- `games.factorio`: `{ mods_dir?, exe_path? }`
- `games.gzdoom`: `{ exe_path?, iwad_path?, gzap_pk3_path?, args?: string[] }`
- (futur) `games.ds3`: `{ tool_root?, steam_mode?, wine_path? }` (si on veut controler l'install/launch)

## Profils (portable mode) par runtime

Objectif:
- isoler la config de chaque runtime pour eviter les interactions entre jeux
- rendre les installs reproductibles sur handheld

Approche:
- preferer les runtimes qui supportent un user dir override ou un mode portable:
- Dolphin: `--user <path>` ou `portable.txt`
- DuckStation: `portable.txt` (attention licence et ecriture a cote exe)
- PCSX2: `portable.txt` / `-portable`
- mGBA: `portable.ini`
- ScummVM: `scummvm.ini` (portable mode)
- DOSBox Staging: `--conf`, `--set`, `--working-dir`

Convention SekaiLink proposee:
- profiles dans `userData/runtime/profiles/<runtime_id>/<profile_id>/`
- `profile_id` par defaut:
- `global` (machine)
- `game:<game_id>`
- `session:<room_id>:<slot_id>` (quand necessaire)

## Menu Options (conception)

Niveaux (recommande):
1. Bibliotheque
- ROM folders a scanner
- import ROM par fichier
- validation par hash (md5/sha1) basee sur `rom_requirements` des manifests

2. Systemes (BIOS, keys, firmware)
- import par systeme, avec validation presence
- pas de hash obligatoire au debut, mais extension et taille minimales

3. Emulateurs et runtimes
- choix par family (PS1, PS2, Switch, SNES, etc.)
- install/update/uninstall
- selection de version (important pour BizHawk)

4. Layout et streaming
- presets multi-ecrans
- gamescope on/off
- streamer mode on/off
- layout best-effort (X11/XWayland via `wmctrl`)

## BizHawk multi-versions (necessite)

Constat:
- certains worlds exigent des ranges BizHawk differents (voir `sekailink-client-plan/17-third-party-emulators.md`)

Exigence:
- pouvoir installer plusieurs versions BizHawk en parallele
- selectionner la version par moduleId

Config proposee:
- `emulators.bizhawk.default_version`
- `emulators.bizhawk.installs[version] = path`
- `modules[<module_id>].emulator_version = version`

## Chemins et compat Linux/Windows

Linux:
- viser XDG pour les caches SekaiLink si possible a moyen terme
- mais l'existant utilise `~/.sekailink` + `userData`
- Wayland/X11: ne pas depend d'un window manager particulier

Windows:
- `userData` est typiquement sous `%APPDATA%` ou `%LOCALAPPDATA%`
- attention aux paths longs et aux caracteres interdits

## Migration et compat

Migrations:
- `config.version` + fonctions de migration dans Electron main au demarrage
- migrations idempotentes
- conserver une sauvegarde `config.json.bak` en cas de corruption

Compat:
- ne pas casser un config ancien si un champ manque
- valider au runtime (et afficher des fixes) plutot que refuser de demarrer
