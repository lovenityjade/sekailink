# SekaiLink

<p align="center">
  <img src="assets/img/sekailink-logo-image.png" alt="SekaiLink Logo" width="160" />
</p>
<p align="center">
  <img src="assets/img/sekailink-logo-text.png" alt="SekaiLink" width="320" />
</p>

<p align="center">
  <a href="https://discord.gg/jTaefxAEDW"><img alt="Discord" src="https://img.shields.io/badge/Discord-Join%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-22c55e?style=for-the-badge"></a>
  <a href="https://sekailink.com/downloads"><img alt="Downloads" src="https://img.shields.io/badge/Downloads-Live-06b6d4?style=for-the-badge"></a>
</p>

SekaiLink is a desktop client + server stack for streamlined Archipelago multiplayer sessions:
- lobby creation/join flow
- YAML and player options workflow
- one-click patch/launch pipeline
- emulator + tracker orchestration
- moderation/social features in one UI

## Downloads
- Linux AppImage: `https://sekailink.com/static/downloads/sekailink-beta-install.appimage`
- Windows Installer: `https://sekailink.com/static/downloads/sekailink-beta-install.exe`
- Downloads page: `https://sekailink.com/downloads`

## Current Features
- Neon-themed production UI (home, lobby, game manager, settings, profile modal)
- Auto-update flow with startup check and patch pipeline
- Global chat + lobby chat + toast notifications
- Friends system (presence, requests, context actions)
- Solo mode flow
- Room moderation actions and room management
- Runtime orchestration for BizHawk / PopTracker / SNI bridge
- SteamGridDB boxart integration (client + server)
- Multi-language foundation (EN, FR, ES, JA, ZH-CN, ZH-TW)

## Supported Games (Current Integration)
- A Link to the Past
- A Link Between Worlds
- Donkey Kong Country 1/2/3
- EarthBound
- Final Fantasy IV Free Enterprise
- Final Fantasy Tactics Advance
- Kirby's Dream Land 3
- Lufia II Ancient Cave
- Mega Man 2/3
- Metroid Fusion / Metroid Zero Mission
- Ocarina of Time / Ship of Harkinian
- Pokemon Crystal / Emerald / FireRed-LeafGreen / Red-Blue
- SMZ3
- Super Mario 64 / Super Mario Land 2 / Super Mario World
- Super Metroid
- The Legend of Zelda / Oracle of Seasons / The Minish Cap / Zelda II
- Wario Land / Wario Land 4
- Yoshi's Island

## Repository Layout
- `client/app/` : desktop app (Electron + React + TypeScript)
- `client/admin-app/` : admin desktop panel
- `WebHostLib/` : web UI + APIs + lobby/room endpoints
- `worlds/` : world integrations
- `services/` : auxiliary services (social bots, etc.)
- `sekailink-client-plan/` : project docs, runbooks, implementation notes

## Build (Desktop Client)
```bash
cd client/app
npm install
npm run build
npm run electron:pack
npm run electron:pack:win
```

## Documentation
- Main docs index: `sekailink-client-plan/README.md`
- Architecture and workflows: `sekailink-client-plan/03-target-architecture.md`, `sekailink-client-plan/28-integration-workflow.md`
- Release and updater notes: `sekailink-client-plan/34-updater-sprint-2026-02-11.md`

## Credits
SekaiLink builds on and integrates upstream projects:
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago)
- [PopTracker](https://github.com/black-sliver/PopTracker)
- [BizHawk](https://github.com/TASEmulators/BizHawk)

