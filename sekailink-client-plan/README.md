# SekaiLink Client Plan (SekaiLink Desktop)

But: avoir une vue d'ensemble actionnable du systeme (Archipelago + SekaiLink webhost + client Electron) et un plan realiste pour atteindre le "0 interaction" (ou quasi) pour:
- consoles PC portables (Steam Deck / ROG Ally / Legion Go)
- nouveaux joueurs (jamais utilise Archipelago)
- power users (robuste, debuggable)

Ce dossier est volontairement "IA-friendly":
- chemins et fichiers concrets
- contrats d'interface (schema) et flux
- decisions et tradeoffs explicites

## Index
- `sekailink-client-plan/00-repo-map.md`
- `sekailink-client-plan/01-scope-and-goals.md`
- `sekailink-client-plan/02-current-state.md`
- `sekailink-client-plan/03-target-architecture.md`
- `sekailink-client-plan/04-runtime-contract.md`
- `sekailink-client-plan/05-autolaunch-patching.md`
- `sekailink-client-plan/06-archipelago-clients.md`
- `sekailink-client-plan/07-emulators-windowing.md`
- `sekailink-client-plan/08-trackers.md`
- `sekailink-client-plan/09-lua-connectors.md`
- `sekailink-client-plan/10-server-apis-and-logs.md`
- `sekailink-client-plan/11-implementation-plan.md`
- `sekailink-client-plan/12-open-questions.md`
- `sekailink-client-plan/13-autolaunch-source-data.md`
- `sekailink-client-plan/14-distribution.md`
- `sekailink-client-plan/15-layout-and-streaming.md`
- `sekailink-client-plan/16-recent-changes.md`
- `sekailink-client-plan/17-third-party-emulators.md`
- `sekailink-client-plan/18-external-web-generation.md`
- `sekailink-client-plan/19-session-orchestrator.md`
- `sekailink-client-plan/20-settings-and-profiles.md`
- `sekailink-client-plan/21-runtime-installers.md`
- `sekailink-client-plan/22-worlds-runtime-matrix.md`
- `sekailink-client-plan/23-modloaders-and-native-clients.md`
- `sekailink-client-plan/24-security-privacy-and-logs.md`
- `sekailink-client-plan/25-contract-checks-and-testing.md`
- `sekailink-client-plan/26-sekailink-docs-map.md`
- `sekailink-client-plan/27-archipelago-docs-map.md`
- `sekailink-client-plan/28-integration-workflow.md`
- `sekailink-client-plan/29-status-workflow-snapshot.md`
- `sekailink-client-plan/30-repo-layout.md`
- `sekailink-client-plan/31-sm64ex-builder.md`
- `sekailink-client-plan/32-appimage-runtime-debug.md`
- `sekailink-client-plan/33-handoff-2026-02-11.md`
- `sekailink-client-plan/34-updater-sprint-2026-02-11.md`
- `sekailink-client-plan/35-server-reboot-runbook.md`
- `sekailink-client-plan/36-build-upload-2026-02-11.md`
- `sekailink-client-plan/37-admin-control-plane-spec.md`
- `sekailink-client-plan/38-admin-app-sprint-2026-02-11.md`
- `sekailink-client-plan/39-windows-python-hotfix-2026-02-12.md`
- `sekailink-client-plan/40-updater-install-security-hotfix-2026-02-12.md`
- `sekailink-client-plan/41-auto-self-update-no-installer-2026-02-12.md`

## Repos et docs sources
- Webhost (Flask): `WebHostLib/`
- Client desktop (Electron + React): `client/app/`
- Runtime desktop (Python wrappers + modules): `client/runtime/`
- Modules par jeu (proof-of-concept): `client/runtime/modules/`
- Inventaire de setup/autolaunch (extrait des guides): `CLIENT_AUTOLAUNCH_RAW.json`
- Docs SekaiLink existantes: `sekailink-docs/`
- Docs Archipelago/MultiworldGG (local): `docs/`

## Glossaire (termes)
- "AP" = Archipelago (serveur MultiServer + clients)
- "MWGG" = MultiworldGG (ecosysteme et setup guides)
- "Slot" = nom du joueur dans la seed
- "Patch" = fichier `.ap*` ou download equivalant fourni par le webhost
- "Module runtime" = dossier `client/runtime/modules/<moduleId>` qui decrit comment lancer un jeu
- "Driver" = code Electron main qui sait lancer un emulateur / modloader / outil
