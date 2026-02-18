# Etat actuel (ce qui existe deja)

## Client Electron (UI)
- Code: `client/app/`
- Stack: React + Vite + Electron
- IPC CommonClient wrapper: `commonclient:start`, `commonclient:send`, `commonclient:stop`
- IPC patcher wrapper: `patcher:apply`
- IPC BizHawk: `bizhawk:launch`, `bizhawk:stop`
- IPC PopTracker: `tracker:launch`, `tracker:stop`, `tracker:installPack`, `tracker:status`, `tracker:validatePack`
- IPC config ROMs: `config:get`, `config:setRom`, `roms:scan`, `roms:import`

Implementations:
- Electron main: `client/app/electron/main.cjs`
- Runtime client API: `client/app/src/services/runtime.ts`

## Flux "Launch" actuel (preuve de concept)
- UI Lobby: `client/app/src/pages/Lobby.tsx`
- Un bouton "Launch" pour soi meme, base sur `room_status.downloads`.

Support autolaunch partiel (hardcode dans `Lobby.tsx`):
1. Resolution par extension de patch URL: `.apred`, `.apblue`, `.apcrystal`, `.apfirered`, `.apleafgreen`, `.apemerald`
2. Patch via `runtime.patcherApply({ patchUrl })`
3. Lance BizHawk via `runtime.bizhawkLaunch({ moduleId, romPath })`
4. Connecte CommonClient via `runtime.commonClientStart` + `runtime.commonClientSend({cmd:'connect', ...})`
5. Lance PopTracker via `runtime.trackerLaunch({ moduleId, apHost, apSlot, apPass })`

Limits actuelles:
- la table de mapping patch extension -> module est hardcodee dans `Lobby.tsx`
- la validation setup est specifique aux jeux Pokemon (ROM + packs)
- pas d'integration de SNIClient/BizHawkClient/ManualClient (a part CommonClient wrapper)

## Runtime Python wrappers
- `client/runtime/commonclient_wrapper.py`: wrapper headless JSON stdin/stdout sur `CommonClient.server_loop`, forward logs + room_info
- `client/runtime/patcher_wrapper.py`: applique un `.ap*` via `Patch.create_rom_file`, injecte des chemins ROM dans `settings.py` via `~/.sekailink/config.json`

## Modules runtime (manifest) existants
- Dossiers: `client/runtime/modules/*`
- Contenu minimal par module: `manifest.json`, `runner.js`, `lua/` (si BizHawk)

Exemples:
- `client/runtime/modules/pokemon_firered_bizhawk/manifest.json`
- `client/runtime/modules/alttp_bizhawk/manifest.json`

## PopTracker (binaire) + packs
- Binaire compile et bundle: `client/runtime/poptracker/poptracker`
- Packs telecharges dans: `userData/poptracker/packs/<gameId>`
- Support variant: `--pack-variant` (patch SekaiLink dans `third_party/PopTracker/`)

## Universal Tracker (UT)
- World: `worlds/tracker/`
- UT est un client Kivy (UI) et un moteur de logique.
- UT peut etre integre dans un client CommonClient (doc): `worlds/tracker/docs/client-integration.md`
- Limites UT: desync possible si logique non-deterministe; UT depend d'une generation interne basee sur les yamls

## Cote serveur (webhost) pertinent pour autolaunch
- API room status: `WebHostLib/api/room.py`
- Endpoint: `/api/room_status/<room_id>`
- Champ `downloads`: liste `{slot, download}`
- Downloads possibles: `download_slot_file` (pas apdeltapatch) ou `download_patch` (apdeltapatch)
- Lobby UI stream deja des logs serveur via Socket.IO: `terminal_output`, `terminal_ack` (client: `Lobby.tsx`)

