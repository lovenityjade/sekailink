# Repo map (where things live)

But: donner des pointeurs concrets pour ne pas relire tout le code a chaque fois.

## Cote client desktop (Electron)
- UI renderer (React): `client/app/src/`
- Lobby (launch + terminal): `client/app/src/pages/Lobby.tsx`
- Game setup (ROM import + packs): `client/app/src/pages/GameManager.tsx`
- Runtime IPC client: `client/app/src/services/runtime.ts`
- API wrapper (token, base URL): `client/app/src/services/api.ts`
- Electron main process: `client/app/electron/main.cjs`
- Electron main helpers: `downloadToFile()`, `runPatcher()`, `launchBizHawk()`, `installTrackerPack()`, `startCommonClient()`
- Electron preload: `client/app/electron/preload.cjs` (expose `window.sekailink.*` au renderer)

## Runtime desktop (Python)
- CommonClient wrapper: `client/runtime/commonclient_wrapper.py`
- Patcher wrapper: `client/runtime/patcher_wrapper.py`

## Modules runtime (par jeu)
- dossier: `client/runtime/modules/<moduleId>/`
- manifest: `client/runtime/modules/<moduleId>/manifest.json`
- lua connectors (BizHawk): `client/runtime/modules/<moduleId>/lua/*.lua`

## Clients Archipelago (Python)
- Common client: `CommonClient.py`
- SNI client: `SNIClient.py`
- BizHawk client: `BizHawkClient.py` (wrapper) + `worlds/_bizhawk/context.py`
- Manual client: `ManualClient.py`

## Universal Tracker (UT)
- world: `worlds/tracker/`
- docs: `worlds/tracker/docs/`

## Webhost (serveur SekaiLink)
- WebHost lib: `WebHostLib/`
- Room status API (downloads): `WebHostLib/api/room.py`
- YAML endpoints: `WebHostLib/options.py`
- Lobby API: `WebHostLib/lobbies.py`
- realtime (Socket.IO): `WebHostLib/realtime.py`

## Data inventory pour autolaunch
- Extrait des guides MWGG: `CLIENT_AUTOLAUNCH_RAW.json`
- Docs SekaiLink autolaunch: `sekailink-docs/CLIENT_AUTOLAUNCH_PLAN.md`
- Notes emulator research: `sekailink-docs/EMULATOR-RESEARCH.md`
- Liste BizHawk connectors: `sekailink-docs/BIZHAWK-CONNECTORS.md`
