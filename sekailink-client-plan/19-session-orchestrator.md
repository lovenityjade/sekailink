# Session Orchestrator (Play pipeline end-to-end)

But: decrire un workflow complet "one-button play" et extensible, de la generation cote serveur jusqu'au jeu lance et connecte, en minimisant les fenetres et les actions utilisateur.

Ce document sert de reference pour:
- evoluer de l'implementation actuelle (BizHawk-only) vers un orchestrator multi-familles
- standardiser les etapes, la supervision process, et le logging
- supporter Linux (X11 + Wayland, gamescope prefere) et Windows

## Workflow actuel (etat du code)

UI:
- `client/app/src/pages/Lobby.tsx` implemente `handleLaunch()`.

Electron main:
- `client/app/electron/main.cjs` expose les IPC:
- `patcher:apply` -> `client/runtime/patcher_wrapper.py`
- `bizhawk:launch`
- `commonclient:start`, `commonclient:send`
- `tracker:launch` (PopTracker)
- `session:autoLaunch` (orchestrator centralise)

Sequence actuelle (par patch URL):
1. Download du fichier associe au slot (patch ou slot file) dans `userData/runtime/downloads/`.
   - important: les endpoints webhost (`/dl_patch/...`, `/slot_file/...`) n'ont pas d'extension dans l'URL.
   - SekaiLink utilise `Content-Disposition: filename=...` pour deduire l'extension.
2. (Nouveau) Artifact handlers (slot files):
   - avant de declarer "manual", SekaiLink tente un handler par jeu (ex: Factorio mod zip) pour installer/launcher automatiquement.
   - si le handler installe mais ne peut pas launcher: mode manuel reduit (le joueur n'a plus qu'a lancer le jeu).
2. Resolve module runtime:
   - preferer mapping via `manifest.patch_extensions`
   - fallback possible via `room_status.players[].game` (match `manifest.display_name`) pour les fichiers non supportes auto-patch.
3. Apply patch si applicable (AP `.ap*` -> `Patch.create_rom_file`).
4. Launch BizHawk (`EmuHawkMono.sh --lua=... <rom>`), wrapper gamescope si configure.
5. Start CommonClient wrapper et connecter au serveur.
6. Launch PopTracker avec autoconnect AP (best-effort).
7. Apply layout best-effort (X11/XWayland via `wmctrl`) si configure.

Limitations explicites:
- slot files / fichiers non `.ap*`:
  - telechargement OK
  - connection CommonClient OK
  - tracker best-effort si module match
  - mais pas d'auto-install/mod/patch specifiques (a faire par world family)
- l'orchestrator n'est pas une machine d'etats; c'est une boucle imperative, fragile aux erreurs intermediaires.

## Objectif "extensif": une machine d'etats (state machine)

Pourquoi:
- les worlds ne se ressemblent pas (patch `.ap*`, slot files, mods, generateurs externes)
- la robustesse vient de transitions explicites, reprises, timeouts, et fallbacks

Concept: `Session`
- identite: `{lobby_id, room_id, slot_name, slot_id, game_id, module_id}`
- input server: `room_status.downloads[]` (patches ou slot files)
- output client: un jeu lance, connecte, tracker lance, logs accessibles

State machine proposee:
1. `IDLE`
2. `FETCH_DOWNLOADS`
3. `RESOLVE_ARTIFACTS`
4. `VALIDATE_PREREQS`
5. `DOWNLOAD_ARTIFACTS`
6. `APPLY_PATCH` (si applicable)
7. `PREPARE_RUNTIME` (profiles, copies, mods, fichiers)
8. `LAUNCH_GAME_RUNTIME`
9. `LAUNCH_CLIENT_BRIDGE` (CommonClient, BizHawkClient, SNIClient, client par world)
10. `CONNECT_TO_SERVER`
11. `LAUNCH_TRACKER` (PopTracker ou UT panel)
12. `RUNNING`
13. `STOPPING`
14. `ERROR` (avec code + diagnostics)

## Etapes standard (contract)

Les etapes doivent venir d'un contrat, pas du code UI.

Inputs:
- module manifest: `client/runtime/modules/<moduleId>/manifest.json`
- runtime contract schema: `client/schema/game_runtime.schema.json` (a etendre)
- server data: `GET /api/room_status/<room_id>` (voir `sekailink-client-plan/10-server-apis-and-logs.md`)

Regles:
- l'UI doit dire "quoi lancer" et "pour qui" (room + slot)
- l'orchestrator decide "comment" (drivers, patcher, steps)

## Supervision des processus (process control)

Etat actuel:
- Electron main conserve:
- `commonClientProc`
- `bizhawkProcs: Map<pid, proc>`
- `trackerProcs: Map<pid, proc>`

Objectif:
- une supervision unifiee par `session_id`
- un "kill tree" fiable (surtout Windows)
- detection "process died" et mise a jour UI

Pratiques recommandees:
- associer chaque process a un `session_id`
- stocker `{pid, start_time, command, cwd, env_hash, purpose}` en memoire
- sur crash: capture des derniers logs stdout/stderr (si capture active)
- cleanup: toujours arreter tracker et bridge avant le jeu si possible

## Fenetres, focus, et gamescope

Contexte:
- sous Wayland, on ne peut pas repositionner de maniere fiable les fenetres externes
- sur handheld, l'objectif est "console": une fenetre visible, focus stable

Strategie:
- Linux:
- preferer `gamescope` comme wrapper principal pour le jeu quand disponible
- fallback best-effort sans gamescope (XWayland, fullscreen)
- Desktop multi-ecran:
- presets (voir `sekailink-client-plan/15-layout-and-streaming.md`)
- Streamers:
- titres de fenetre stables, pas de secrets en argv, logs separables

Voir aussi:
- `sekailink-client-plan/07-emulators-windowing.md`
- `sekailink-client-plan/15-layout-and-streaming.md`

## Tracking: PopTracker vs Universal Tracker (UT)

Mode recommande pour limiter les fenetres:
- UT headless dans SekaiLink (renderer) quand le world le supporte
- PopTracker comme fallback (fenetre externe)

Probleme de securite:
- l'implementation actuelle passe `--ap-pass` en argv a PopTracker et CommonClient wrapper
- argv est observable localement (risque streamer)

Voir:
- `sekailink-client-plan/08-trackers.md`
- `sekailink-client-plan/24-security-privacy-and-logs.md`

## Gestion des erreurs (diagnostics)

Chaque etape doit produire:
- `error_code` stable (ex: `rom_missing`, `poptracker_missing`, `download_failed`, `patch_failed`, `emulator_missing`)
- `error_context` minimal (paths, host, moduleId) sans secrets
- `suggested_fix` (UI: bouton `Fix`)

Exemples fixes:
- `rom_missing`: ouvrir `Game Setup` et importer ROM (hash match)
- `poptracker_missing`: installer runtime tracker
- `no_pack`: installer pack tracker (source GitHub release)
- `slot_file_unsupported`: ouvrir doc du world et proposer mode manuel

## Extensibilite: families

On ne vise pas "un seul driver". On vise des families avec leurs propres steps.

Families typiques:
- `bizhawk_lua` (ROM + Lua connector + AP bridge)
- `sni` (SNIClient + emulator/hardware)
- `dolphin_memory` (Dolphin + client mem read)
- `mod_loader` (BepInEx, SMAPI, Fabric/Forge, r2modman)
- `external_web_generator` (voir `sekailink-client-plan/18-external-web-generation.md`)

Chaque family:
- definit ses prereqs
- definit ses steps
- definit ses outputs
