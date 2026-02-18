# Recent changes (notes)

Purpose: garder une trace de ce qui a ete modifie/decouvert pendant la discussion pour ne pas se perdre.

Date: 2026-02-09

## Clarification requirements (PopTracker)
- "multi-source" dans le contexte PopTracker voulait dire: un pack peut offrir plusieurs layouts (variants)
- exemple concret observe: le pack FR/LG a des variants `standard` (horizontal) et `standard_vertical` (vertical)
- objectif produit confirme: SekaiLink doit installer et garder a jour les packs (pas a l'utilisateur de gerer les zips)

## Findings (PopTracker packs)
- les variants sont declares dans `manifest.json` sous la cle `variants` (objet)
- certains packs ont un `manifest.json` avec UTF-8 BOM (ex: `crystal-ap-tracker-0.10.3.zip`), ce qui peut casser des parsers JSON stricts

## UX constraints added during discussion
- Linux: supporter Wayland et X11 (X11 peut rester primaire), ne pas casser Wayland
- Linux: `gamescope` comme chemin principal (fiable), avec fallback best-effort si indisponible
- Desktop: presets multi-ecrans (side-by-side, dual monitor), streamer-first (OBS capture, fenetres stables, eviter secrets)
- Limiter les fenetres: preferer tracker integre (UT headless) quand possible, PopTracker en fallback

## Code changes made (to document, not a final decision)
File: `client/app/electron/main.cjs`
- `validatePackDir()`:
  - strip UTF-8 BOM (`^\ufeff`) avant `JSON.parse`
  - accepte un manifest valide si `name` ou `package_uid` existe
- `launchPopTracker()`:
  - si un module runtime declare `tracker_pack_uid` et qu'on n'a pas de `packPath` local, tentative d'auto-install via `installTrackerPack()`
  - nouveau champ possible dans manifest module: `tracker_asset_regex` (default `\.zip$`) pour choisir l'asset de release GitHub
  - password AP ne passe plus dans argv (`--ap-pass`), mais via env + `--ap-pass-env SEKAILINK_AP_PASS`
- CommonClient:
  - le wrapper python n'est plus demarre avec `--password` en argv (password via stdin JSON seulement)
- Patcher wrapper:
  - injection settings pour `alttp` (`lttp_options.rom_file`) et `oot` (`oot_options.rom_file`)
- Runtime module setup UI:
  - `GameManager` expose un setup generique base sur `client/runtime/modules/*/manifest.json` (ROMs + tracker packs)
  - support choix de layout PopTracker via variants (stocke dans `~/.sekailink/config.json` -> `trackerVariants`)

## New work (slot files + multi-display layout)
Auto-launch robustness:
- le serveur expose parfois des "slot files" via `/slot_file/...` au lieu d'un patch `.ap*` (`WebHostLib/downloads.py`).
- les endpoints `/dl_patch/...` et `/slot_file/...` n'ont pas d'extension dans l'URL; l'extension est dans `Content-Disposition: filename=...`.
- SekaiLink telecharge maintenant d'abord dans `userData/runtime/downloads/` et deduit l'extension depuis le `filename` HTTP.
- si l'extension n'est pas routee par `manifest.patch_extensions`:
  - fallback: match du jeu via `room_status.players[].game` contre `manifest.display_name`
  - mode "manual": telecharge + connect CommonClient + tracker best-effort, sans patch/emulateur.

Layout:
- ajout d'un layout manager best-effort (X11/XWayland via `wmctrl`) dans `client/app/electron/main.cjs`.
- `session:autoLaunch` applique la config layout apres lancement emulateur + tracker (warning si `wmctrl` manquant).
- Settings UI expose maintenant `layout.*` (preset/mode/displays/split) dans `client/app/src/pages/Settings.tsx`.

Manual mode UX:
- ajout d'un event `session:event` de type `manual` (payload: `downloadedPath`, `ext`, `moduleId?`, `apGameName?`).
- Lobby affiche un panneau "Manual Download" avec:
  - bouton `Open Folder` (Electron `shell.showItemInFolder`)
  - bouton `Copy Path`

Artifact automation (first handler):
- ajout d'un mini-registry `tryHandleDownloadedArtifact()` dans `client/app/electron/main.cjs`.
- premier handler implemente: Factorio slot file `.zip`:
  - installe le zip dans le dossier mods (defaut `~/.factorio/mods` ou `%APPDATA%/Factorio/mods`, override via `config.games.factorio.mods_dir`)
  - tentative de launch Factorio via:
    - `config.games.factorio.exe_path` ou `factorio` dans PATH
    - fallback `steam -applaunch 427520`
  - si install OK mais launch impossible: mode manuel avec `installedPath` + note.

Artifact automation (second handler):
- handler implemente: `gzDoom` slot file `.pk3`:
  - tentative de launch `gzdoom -iwad <iwad> -file <gzArchipelago.pk3> <seed.pk3>` (best-effort, ordre important)
  - prereq: `config.games.gzdoom.iwad_path` doit etre configure
  - prereq: `config.games.gzdoom.gzap_pk3_path` doit etre configure
  - override exe via `config.games.gzdoom.exe_path`, args additionnels via `config.games.gzdoom.args`

Artifact automation (third handler):
- handler implemente: `Dark Souls III` slot file `.json`:
  - telecharge/installe le package externe `nex3/Dark-Souls-III-Archipelago-client` dans `userData/runtime/tools/ds3/`
  - stage le slot file dans `<toolRoot>/slot_files/`
  - affiche une note courte avec les executables usuels (`DS3Randomizer.exe`, `launchmod_darksouls3.bat`) si detectes

Artifact automation (fourth handler):
- handler implemente: `Super Mario 64 EX` slot file `.apsm64ex`:
  - SekaiLink tente de lancer un binaire SM64EX existant avec `--sm64ap_file <downloadedPath>`
  - path configurable via `Settings -> Games (Automation) -> Super Mario 64 EX (sm64ex)` (`config.games.sm64ex.exe_path`)
  - fallback de detection via `root_dir` (scan best-effort pour `sm64.us.f3dex2e` / `sm64.jp.f3dex2e`)
  - note: le build/compile n'est pas automatise dans le MVP (contraintes legales + toolchain). Plan: `sekailink-client-plan/31-sm64ex-builder.md`

Config UI (to support automation):
- ajout IPC `config:setGame` (shallow merge) pour stocker des configs par jeu sous `config.games`.
- Settings expose `Factorio`:
  - `mods_dir`
  - `exe_path`

## Platform prioritization work (GBA/SNES/NES/GB/GBC)
- ajout d'un generateur de modules BizHawk:
  - `client/runtime/scripts/generate_bizhawk_modules.py`
  - scan statique des worlds pour `patch_file_ending`, `copy_to`, `md5s`
  - ecrit `client/runtime/modules/<world>_bizhawk/manifest.json` + copie `lua/connector.lua`
- patcher wrapper: support generique ROMs:
  - `client/runtime/patcher_wrapper.py` mappe `config.roms[<world_folder>]` -> `get_settings()[<world_folder>_options].rom_file` si present
- nouveaux modules runtime BizHawk ajoutes (MVP):
  - GBA: `mzm_bizhawk`, `wl4_bizhawk`, `metroidfusion_bizhawk`, `ffta_bizhawk`, `tmc_bizhawk`
  - SNES: `sm_bizhawk`, `smw_bizhawk`, `dkc_bizhawk`, `dkc2_bizhawk`, `dkc3_bizhawk`, `earthbound_bizhawk`, `ff4fe_bizhawk`, `kdl3_bizhawk`, `lufia2ac_bizhawk`
  - NES: `tloz_bizhawk`, `zelda2_bizhawk`, `mm2_bizhawk`, `mm3_bizhawk`
  - GB/GBC: `wl_bizhawk`, `marioland2_bizhawk`, `tloz_oos_bizhawk`

Raison:
- permettre a SekaiLink d'installer les packs automatiquement (repo/source connue), et de ne pas obliger l'utilisateur a garder ses packs a jour
- tolerer des packs existants qui ont BOM
 - streamer-first: eviter de leak des secrets via process list (argv)
 - remplacer les hardcodes Pokemon par un setup base sur manifests

Note:
- ce changement a ete fait avant que tu demandes "documenter seulement"; il est ici pour qu'on puisse decider si on le garde ou si on le refactor.

## MVP progress: BizHawk game-specific client integration
Problem:
- Jusqu'ici SekaiLink patchait + lancait BizHawk + connectait `CommonClient` uniquement.
- Pour les jeux BizHawk/Lua, `CommonClient` ne suffit pas: il faut un client qui parle aussi au connecteur Lua pour faire l'autotracking et les checks (BizHawkClient + handlers par world).

Changements:
- ajout d'un wrapper headless BizHawkClient: `client/runtime/bizhawkclient_wrapper.py`
  - IPC stdin/stdout JSON (meme pattern que `commonclient_wrapper.py`)
  - importe `worlds` pour enregistrer les handlers BizHawk (via `AutoBizHawkClientRegister`)
  - evite les secrets dans argv (password uniquement via stdin JSON)
- Electron:
  - ajoute IPC `bizhawkclient:start|send|stop` + event channel `bizhawkclient:event` dans `client/app/electron/main.cjs` et `client/app/electron/preload.cjs`
  - `session:autoLaunch` utilise maintenant BizHawkClient (au lieu de CommonClient) quand `manifest.emu === "bizhawk"`
- UI:
  - le flow patch->launch dans `client/app/src/pages/Lobby.tsx` envoie maintenant `connect` au BizHawkClient wrapper.

Impact:
- Les jeux retro BizHawk (GBA/SNES/NES/GB/GBC/N64) ont maintenant un chemin vers un MVP "fonctionnel" (emulateur + handler world + connexion serveur).
Limitations:
- importer `worlds` dans le process client peut etre lourd et depend des deps python. Pour un MVP c'est le plus fiable; optimisation plus tard (import selectif par world/module).

## Debugging: Crash handlers + file logging (Linux)
Changements:
- Electron main ajoute un logger fichier + console:
  - logs ecrits dans `app.getPath("userData")/logs/sekailink-<timestamp>.log`
  - les events `session/commonclient/bizhawkclient` sont aussi dumpees en JSONL
- handlers:
  - `process.on("uncaughtException")`
  - `process.on("unhandledRejection")`
  - `app.on("render-process-gone")`, `app.on("child-process-gone")`
  - capture `webContents console-message` + `render-process-gone`
- renderer:
  - `window.error` + `window.unhandledrejection` forwardes vers main via IPC `log:renderer`

## UI: Lobby actions for MVP (Release/Slow Release/Hint)
- `Release/Slow Release`:
  - deja presents dans le menu `⋯` des joueurs dans `client/app/src/pages/Lobby.tsx`
  - permissions UI ajustees: visible pour le host (tous) et pour le joueur lui-meme (self uniquement)
  - implementation: appel webhost `POST /api/lobbies/<id>/release` (le webhost push ensuite les commandes au room server)
- `Hints`:
  - nouveau bouton `Hints` (self) dans le menu `⋯`
  - ouvre un modal:
    - `!hint` (refresh + liste)
    - `!hint <item>` et `!hint_location <location>` (requests)
  - liste les hints recus via `PrintJSON` type `Hint` (texte rendu cote wrapper python).

## UI: Local Client Status Panel
- ajoute dans `Lobby Controls` un panneau "Local Game Client" qui affiche:
  - etat connexion serveur (connecte/deconnecte)
  - etat connexion BizHawk (connected/tentative/not_connected)
  - handler world courant (si detecte)
  - hint points + hint cost
  - dernier message d'erreur + derniers logs (ring buffer)
  - bouton `Copy Debug` (copie un JSON avec etat + derniers logs)
- supporte par un nouvel event `emu_status` emis periodiquement par `client/runtime/bizhawkclient_wrapper.py`.

## UI: SFX integration pass (coherent UX)
- objectif: feedback audio sur actions importantes (handheld/streamer-friendly), sans spam.
- `Lobby`:
  - `showToast()` joue `success` / `error` (avec cooldown local pour eviter le spam)
  - click actions cles jouent `confirm` (ouvrir modals, tracker, generate confirm, launch, release/slow release, hints, terminal send, copy debug)
- `RoomList`:
  - `showToast()` joue `success` / `error`
  - `Create Room`, `Open lobby`, `Copy link`, `Room info` jouent `confirm`
- `Sidebar`:
  - navigation + toggles jouent `confirm`
  - mute toggle joue `confirm` uniquement quand on reactive les sons
- `SocialDrawer`:
  - ouvrir/fermer drawer + actions (open conversation, send message, accept/decline/remove) jouent `confirm`/`success`

## Documentation added/updated
- nouveau dossier: `sekailink-client-plan/`
- nouveau doc layout+streaming: `sekailink-client-plan/15-layout-and-streaming.md`
- nouveau doc emulateurs: `sekailink-client-plan/17-third-party-emulators.md`
- nouveau doc: worlds a generation externe / local-only: `sekailink-client-plan/18-external-web-generation.md`
- nouveaux docs "workflow complet":
- `sekailink-client-plan/19-session-orchestrator.md`
- `sekailink-client-plan/20-settings-and-profiles.md`
- `sekailink-client-plan/21-runtime-installers.md`
- `sekailink-client-plan/22-worlds-runtime-matrix.md`
- `sekailink-client-plan/23-modloaders-and-native-clients.md`
- `sekailink-client-plan/24-security-privacy-and-logs.md`
- `sekailink-client-plan/25-contract-checks-and-testing.md`
- `sekailink-client-plan/26-sekailink-docs-map.md`
- `sekailink-client-plan/27-archipelago-docs-map.md`

## UI prototype (2026-02-14)
- Notes: `sekailink-client-plan/42-ui-prototype-2026-02-14.md`

## Integration workflow
- `sekailink-client-plan/28-integration-workflow.md`

## Debugging (AppImage runtime + Python deps)
Contexte:
- Autorun tombait en "Manual mode" et/ou crashait dans le patcher Python en AppImage.

Root causes:
- assets runtime/emulateur pas emballes (runtime et BizHawk etaient absents du build)
- chemins hardcodes via `__dirname` (app.asar) au lieu de `process.resourcesPath`
- AP python sources pas disponibles via `PYTHONPATH`
- deps pip manquantes (`yaml`, `pathspec`, `pkg_resources`, `orjson`, etc.)
- subset `worlds/` incomplet (ex: `worlds.generic` manquant)
- bootstrap AP `user_path()` casse si `Players/` manque dans un install read-only (AppImage)

Etat apres fixes:
- bug "manual mode cause runtime/py missing" resolu par packaging + venv auto deps.
- la suite des problemes est maintenant dans la phase `launch` (orchestration, emulateur, connector, focus/layout).

Details: `sekailink-client-plan/32-appimage-runtime-debug.md`
