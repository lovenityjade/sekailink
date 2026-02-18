# Snapshot (beta-0.02-topaz) - Ce qui a ete fait / ce qui reste / prochaine etape

Date: 2026-02-09
Tag: `beta-0.02-topaz` (package.json: `0.0.2-beta.0.2-topaz`)

Objectif de la soiree:
- Solidifier un MVP "Play" pour la famille BizHawk/Lua (priorite GBA/SNES/NES/GB/GBC).
- Avoir un control minimal de commandes (Release/Slow release/Hint) via UI.
- Ajouter logs/crash handlers + diagnostics pour debogage.
- Sortir un build AppImage versionne.

## Etat actuel (ce qui est fonctionnel cote client)

### 1) AutoLaunch (Play button)
- `Play/Launch` telecharge l'artefact du serveur (patch ou slot file) et tente:
  - (si patch AP) patch -> launch emulateur -> connect client -> launch tracker -> layout
  - (si slot file connu) install/launch best-effort (Factorio/gzDoom/DS3), sinon mode manuel guide
- Orchestrator: `client/app/electron/main.cjs` (`session:autoLaunch`)

### 2) BizHawk: emulateur + connecteur Lua
- BizHawk lance via `client/app/electron/main.cjs` (`bizhawk:launch`)
- Connecteur Lua auto-charge depuis `client/runtime/modules/<module>/lua/*.lua`
- Layout best-effort:
  - Gamescope prefer/require (config)
  - X11 `wmctrl` repositionne game+tracker (fallback)

### 3) Client Archipelago game-specific (BizHawkClient)
- Nouveau wrapper headless: `client/runtime/bizhawkclient_wrapper.py`
  - IPC JSON stdin/stdout
  - import `worlds` pour enregistrer tous les handlers BizHawk (AutoBizHawkClientRegister)
  - forward logs + `PrintJSON` rendu en texte
  - emet `emu_status` periodique (server/bizhawk/handler)
- Electron IPC:
  - `bizhawkclient:start|send|stop`
  - event channel `bizhawkclient:event`

### 4) Tracker (PopTracker)
- PopTracker bundle dans `client/runtime/poptracker/poptracker`
- Packs:
  - telechargement auto via GitHub releases ou archive repo
  - variantes (layout) supportees (`--pack-variant`)
  - password AP via env + `--ap-pass-env` (pas dans argv)

### 5) UI: commandes MVP
- Release / Slow release:
  - menu `⋯` sur la liste des joueurs
  - visible pour host (tous) et pour self (self uniquement)
  - implementation: `POST /api/lobbies/<id>/release` (webhost)
- Hints:
  - bouton `Hints` (Lobby Controls + menu `⋯` self)
  - modal:
    - `!hint` (refresh + liste)
    - `!hint <item>`
    - `!hint_location <location>`
  - liste alimentee par `PrintJSON` type `Hint` venant du BizHawkClient wrapper
- Status panel:
  - "Local Game Client" montre server/bizhawk/handler/hint points/cost/last error + logs + copy debug

### 6) Debuggabilite
- Electron main:
  - logs fichier + console: `userData/logs/sekailink-<timestamp>.log`
  - crash handlers (uncaughtException/unhandledRejection/render-process-gone/child-process-gone)
  - capture console renderer
  - renderer errors forwarded via IPC `log:renderer`
- UI:
  - bouton `Copy Debug` dans status panel

### 7) SFX
- Passe d'integration SFX sur actions importantes (confirm/success/error) avec anti-spam sur toasts.

## Jeux/modules BizHawk integres (priorites retro)

Voir la liste exacte dans `sekailink-client-plan/22-worlds-runtime-matrix.md` et les manifests sous `client/runtime/modules/`.
En bref:
- GBA: MZM, Metroid Fusion, Wario Land 4, FFTA, Minish Cap
- SNES: SM, SMW, DKC1/2/3, EarthBound, FF4FE, KDL3, Lufia2AC, ALttP, SMZ3
- NES: TLoZ, Zelda2, MM2, MM3
- GB/GBC: Wario Land, SML2, Oracle of Seasons
- N64: OoT
- Pokemon (GB/GBC/GBA): Red/Blue/Crystal/Emerald/FR/LG

## Build
- AppImage genere:
  - `client/app/release/SekaiLink-client-0.0.2-beta.0.2-topaz.AppImage`
- electron-builder config:
  - `client/app/electron-builder.yml` utilise `artifactName: SekaiLink-client-${version}.${ext}`

## Ce qui reste a faire (prochaines grosses pieces)

### A) Family SNI (SNES via SNIClient)
- wrapper headless `SNIClient.py` (IPC JSON)
- UI actions equivalentes (connect, ready, hints si applicable, logs)
- install/launch SNI (ou detect) + gestion reconnect

### B) Family Dolphin (GC/Wii)
- ajout launch Dolphin + modules runtime `emu: dolphin`
- integration des clients par jeu (memory bridge)
- patches types `.ap*` pour Dolphin worlds (TP/TTYD/etc.)

### C) Universal Tracker (UT)
- engine headless + panel integre (ideal pour limiter fenetres)
- mapping UT supported worlds + fallback PopTracker

### D) Patching Python deps (robustesse packaging)
- s'assurer que la build shippe les deps necessaires a `Patch.create_rom_file` et aux worlds.
- eventuel: venv/vendoring ou extraction de patchers dans un runtime separable.

### E) PC priorities
- Celeste, Hollow Knight (plus tard, apres emu families stabilisees)

## Ou on est dans le workflow
- Phase: fin Phase 0 / debut Phase 1 (BizHawk family)
  - PatchResolver/manifest registry: en place (index par `patch_extensions`)
  - Orchestrator: en place (`session:autoLaunch`) avec mode manuel guide + artifact handlers
  - Securite: password non passe en argv pour PopTracker + clients
  - UI: actions MVP (Release/Slow/Hints) + status panel
  - Logging: main + renderer + client wrappers

## Prochaine etape logique (apres cette release)
1. Valider end-to-end sur machine cible (Bazzite/Steam Deck):
   - Launch BizHawk -> BizHawkClient connect -> hints -> tracker -> layout
2. Ajouter l'affichage des logs serveur + logs client dans un "Log Hub" unifie (facile a exporter)
3. Commencer la famille suivante: `SNIClient` wrapper + UI minimale.

