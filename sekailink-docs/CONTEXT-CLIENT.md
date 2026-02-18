# CONTEXT-CLIENT.md

Résumé condensé (client Electron) pour éviter de relire tous les docs.

## 1) Contexte global (serveur vs client)
- Le serveur (webhost) reste la source de vérité: lobbies, rooms, génération, YAMLs, auth Discord, trackers.
- Le client Electron remplace le site web côté joueur avec **la même UI**, plus accès OS (files, emus, auto‑patch).

## 2) État actuel (jan 29, 2026)
- Un squelette Electron + React (Vite) existe sous `client/app/`.
- Pages React portées (structure + logique) : Room List, Lobby (Socket.IO + polling), Game Manager, Account, Help.
- Composants & services ajoutés: AppShell/Sidebar/About/Contribute/SocialDrawer, `api.ts`, `useSfx`, `useInterval`.
- Assets copiés: backgrounds, SFX, game registry `games.generated.json`.
- Electron main/preload déjà créés: `client/app/electron/main.cjs`, `preload.cjs`.
- HashRouter utilisé pour compatibilité `file://`.
- AppImage alpha construit et ajouté au webhost (download page).

## 3) Plan Electron (ELECTRON-APP-PLAN.md)
- Stack prévue: Electron main + React renderer + TS.
- Architecture: main process (MultiworldClient, SNI/BizHawk bridges, PatchService) + renderer (UI lobbies/rooms).
- Flux cible: lobby -> room_info -> download patch -> patch ROM -> launch emu -> connect multiworld.
- Phases: fondations → SNI → setup wizard → intégration lobbies → jeux additionnels → release.

## 4) TODO Electron (ELECTRON-TODO.md)
- **À implémenter côté Electron**:
  - PatchService `.ap*` (APContainer, bsdiff4, token patch)
  - Mapping extension `.ap*` ↔ handler (AutoPatchRegister)
  - MultiworldClient (port de CommonClient.py)
  - Launchers SNI / BizHawk / RetroArch + Lua connectors
  - Gestion ROMs de base + MD5
  - APWorld install/update
  - File associations `.ap*`
- Flux UX à reproduire: ouverture patch → auto‑patch → auto‑launch → auto‑connect.

## 5) Client plan (CLIENT.md)
- Vision: un seul client pour automatiser emus/mods/patch/connect.
- Contrat “Game Runtime” (schema par jeu) + drivers par “family” d’émulateur.
- Config manager, asset manager, patch pipeline, auto‑connect layer.
- Registry généré depuis guides/metadata.

## 6) Autolaunch (CLIENT_AUTOLAUNCH_*.md)
- **CLIENT_AUTOLAUNCH_FAMILIES.md**: stratégie par famille (BizHawk, RetroArch, Dolphin, PCSX2, etc.).
- **CLIENT_AUTOLAUNCH_PLAN.md**: draft par jeu issu des guides (énorme liste), indique patch types, emu family, hints de connexion.

## 7) External patchers (CLIENT_EXTERNAL_PATCHERS.md)
- Twilight Princess: pipeline externe obligatoire (REL loader + seed + SaveFileHacker + TP Randomizer tools).

## 8) Backlog UI/UX (TODO.md)
- QA UI: landing/room list/friends/profile, SFX mapping, backgrounds, tracker embeds.
- QA Game Manager (tabs/duplicate/custom YAML) + rule “custom YAMLs”.
- Backlog features: admin timeout settings, Patreon UI + migrations, DM chatbot, localisation.

## 9) Points clés à retenir pour la V1 Electron
- Priorité: patch pipeline + multiworld client + autolaunch BizHawk/SNI.
- UI = miroir web (app shell déjà ported) + file system & auto‑patch.
- Registry de jeux + mapping extension→handler restent le cœur de l’automatisation.
