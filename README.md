# SekaiLink

SekaiLink is a desktop-first platform for Archipelago-style multiworld play with a low-friction UX:
- create/join lobbies
- manage YAML/player options
- patch and launch automatically
- connect emulator + tracker + client runtime
- coordinate rooms, moderation, hints, and social features in one UI

## Current Scope
- Client: Electron + React + TypeScript (`client/app`)
- Runtime orchestration: Python wrappers + AP clients + emulator/tracker bridges
- Server: WebHostLib + room/lobby/social APIs + generation pipeline
- Admin: `client/admin-app`

## Key Features (current)
- Unified Game Manager (My Games / Add a Game / YAML Editor)
- Lobby system with generation flow, moderation actions, room info, timer, spoiler log access
- Solo mode workflow
- Global and lobby chat with persistent toast notifications
- Friend system (requests, accept/decline, presence, DMs)
- Self-update pipeline (incremental sync + staged updates)
- SteamGridDB boxart integration (client + server endpoints)
- Multi-language UI foundation (EN, FR, ES, JA, ZH-CN, ZH-TW)

## Supported Game Set (currently integrated)
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
- Ocarina of Time
- Pokemon Crystal / Emerald / FireRed-LeafGreen / Red-Blue
- Ship of Harkinian
- SMZ3
- Super Mario 64 / Super Mario Land 2 / Super Mario World
- Super Metroid
- The Legend of Zelda / Oracle of Seasons / The Minish Cap / Zelda II
- Wario Land / Wario Land 4
- Yoshi's Island

## Repository Layout
- `client/app/` — desktop client UI + Electron shell
- `client/admin-app/` — admin control panel
- `WebHostLib/` — server web/API layer
- `worlds/` — AP world integrations
- `third_party/` — emulators, patched tools, external runtimes
- `sekailink-client-plan/` — execution logs / implementation notes / roadmap docs
- `sekailink-docs/` — developer and architecture docs

## Production VPS (non-secret operational summary)
- Domain: `sekailink.com`
- Admin domain: `admin.sekailink.com`
- Main path: `/opt/multiworldgg`
- Web stack: Apache2 reverse proxy + Gunicorn WebHost
- Active services:
  - `multiworldgg-webhost.service`
  - `multiworldgg-workers.service`
  - `sekailink-social-bots.service`
  - `sekailink-llama.service`
  - `webmin.service`

Do **not** commit credentials/tokens. Use local private files (`*.local`) or a proper secret manager.

## Build (Client)
```bash
cd client/app
npm install
npm run build
npm run electron:pack:ui-prototype
```

AppImage output:
- `client/app/release/sekailink-UI-prototype-<version>.AppImage`

## Documentation
- Sprint logs and integration progress: `sekailink-client-plan/`
- Dev setup and release process: `sekailink-docs/DEV_SETUP.md`, `sekailink-docs/RELEASE_PROCESS.md`
- Runtime integration notes: `sekailink-docs/CLIENT_AUTOLAUNCH_PLAN.md`, `sekailink-docs/POPTRACKER.md`, `sekailink-docs/BIZHAWK-CONNECTORS.md`

## Credits
SekaiLink builds on upstream ecosystems and tooling:
- **Archipelago** — protocol, clients, world architecture
- **PopTracker** — tracker runtime and pack ecosystem
- **BizHawk** — emulator core integration
- plus other world/emulator/tool maintainers referenced in `THIRD_PARTY_NOTICES.md`
