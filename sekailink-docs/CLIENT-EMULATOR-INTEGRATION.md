# Client Emulator Integration Plan (Linux First)

Mot d'ordre: **simplifier au maximum** l'experience Archipelago/MultiworldGG pour que l'utilisateur ait **0 manipulation** (sauf contraintes in-game comme creation de save). Priorite Linux (Steam Deck / ROG), Windows ensuite, macOS plus tard.

## Decisions cle
- **CommonClient** integre a l'UI/UX Electron (mais tourne en **processus Python** géré par l'app).
- **Modules par jeu** presents dans le repo, chargés dynamiquement (pas besoin de MAJ client pour modifier un jeu).
- **Aucune commande texte pour l'utilisateur**: tout doit etre **cliquable** (controller-friendly).
- **BizHawk** devient le socle pour les jeux emu (incluant ALttP via Lua BizHawk).
- **PopTracker** demarre automatiquement et reste visible (option configurable plus tard).

## Architecture cible (main process Electron)
- `RuntimeOrchestrator`
- `PatchService`
- `MultiworldClientService` (Python wrapper)
- `EmulatorService` (BizHawk runner)
- `TrackerService` (PopTracker runner)
- `ProcessSupervisor` (health, crash, relaunch)

## Modules par jeu (dans le repo)
Emplacement propose:
```
client/runtime/modules/
  alttp_bizhawk/
    manifest.json
    runner.js
    lua/connector_alttp.lua
    tracker/pack_uid.txt
```
`manifest.json` contient:
- `game_id`, `display_name`
- `emu` (bizhawk)
- `lua_connector`
- `tracker_pack_uid`
- `patcher`
- `rom_requirements` (hash, extension)
- `features` (autotracking, save_required, etc.)

`runner.js` expose:
- `prepare()` -> telecharge patch, applique, genere ROM finale
- `launchEmu()` -> lance BizHawk + charge Lua
- `launchTracker()` -> lance PopTracker + auto-connect AP
- `cleanup()` -> ferme les processus

## Config globale
Fichier: `~/.sekailink/config.json`
- `roms`: mapping game_id -> path
- `emulators`: overrides (bizhawk path)
- `client`: autoConnect, trackerVisible
- `runtime`: cache dirs

Exemple:
```json
{
  "roms": {
    "alttp": "/home/user/roms/alttp.sfc"
  },
  "emulators": {
    "bizhawk": "/home/user/.sekailink/emulators/bizhawk/EmuHawk"
  },
  "client": {
    "autoConnect": true,
    "trackerVisible": true
  }
}
```

## UI/UX (sans commandes texte)
- Panel "Client":
  - Statuts: **Server / Game / Tracker**
  - Boutons: **Connect**, **Disconnect**, **Ready**, **Reconnect**, **Open Logs**
- Bouton principal: **Play** (one-button flow)

## Pipeline one-button (auto)
1. Lobby pret -> recupere patch + room info
2. PatchService applique patch
3. MultiworldClientService se connecte
4. BizHawk lance ROM + Lua
5. PopTracker lance pack + auto-connect
6. UI met a jour les statuts

## BizHawk (Linux)
- Lance binaire BizHawk depuis bundle ou cache `~/.sekailink/`
- Auto-load Lua connector depuis module
- Lua se connecte au MultiworldClient local

## PopTracker
- Lancement avec:
  - `--load-pack <uid>`
  - `--ap-host host:port`
  - `--ap-slot <slot>`
  - `--ap-pass <pass>`
  - `--ap-autoconnect`
- Toujours visible (configurable plus tard)

## Milestones proposes
1. **MVP ALttP BizHawk** (Linux only)
2. Generaliser modules + manifests
3. Ajout Twilight Princess (Dolphin)
4. Support Windows
5. UI options (paths ROMs, display, tracker toggle)

## Notes
- Tout doit etre controller-friendly et sans commandes texte utilisateur.
- Les modules par jeu peuvent etre modifies sans MAJ du client principal.

## CommonClient IPC (JSON)
Le wrapper Python `client/runtime/commonclient_wrapper.py` expose un protocole JSON via stdin/stdout.

### Commandes (stdin)
```json
{"cmd":"connect","address":"host:port","slot":"Name","password":""}
{"cmd":"disconnect"}
{"cmd":"ready","value":true}
{"cmd":"say","text":"Hello"}
{"cmd":"set_password","value":"..."}
{"cmd":"set_slot","value":"..."}
{"cmd":"shutdown"}
```

### Evenements (stdout)
```json
{"event":"log","level":"info","logger":"Client","message":"..."}
{"event":"status","server":"connected"}
{"event":"status","ready":true}
{"event":"request","type":"slot","message":"Slot name required"}
{"event":"room_info","seed_name":"...","password":false,"players":[]}
{"event":"print_json","data":[{"text":"..."}]}
{"event":"error","message":"..."}
{"event":"exit","code":0,"signal":null}
```

## Electron IPC
- `commonclient:start` -> démarre le wrapper Python
- `commonclient:send` -> envoie une commande JSON
- `commonclient:stop` -> stoppe le wrapper
- `commonclient:event` -> stream des événements JSON vers renderer

### Patcher IPC
- `patcher:apply` -> applique un patch via `client/runtime/patcher_wrapper.py`
  - input: `{ "patchPath": "...", "patchUrl": "...", "configPath": "...", "outDir": "..." }`
  - output: `{ "ok": true, "meta": {...}, "output": "..." }`

### BizHawk IPC
- `bizhawk:launch` -> lance BizHawk **v2.10 intégré au client** (Linux: `EmuHawkMono.sh`) avec `--lua=...` + ROM
  - input: `{ "emuPath": "...", "romPath": "...", "luaPath": "..." }`
  - output: `{ "ok": true, "pid": 12345 }`
- `bizhawk:stop` -> stoppe un process BizHawk

### PopTracker IPC
- `tracker:launch` -> lance PopTracker (CLI AP) avec pack + auto-connect
  - input: `{ "moduleId": "...", "packUid": "...", "packPath": "...", "packVariant": "...", "apHost": "...", "apSlot": "...", "apPass": "..." }`
  - output: `{ "ok": true, "pid": 12345 }`
- `tracker:stop` -> stoppe un process PopTracker
- `tracker:installPack` -> telecharge + installe un pack dans `userData/poptracker/packs/<gameId>`
  - input: `{ "gameId": "...", "packUrl": "...", "packRepo": "owner/repo", "assetRegex": "\\\\.zip$" }`
  - output: `{ "ok": true, "path": "..." }`
- `tracker:validatePack` -> valide qu'un pack installe a un `manifest.json`
  - input: `gameId`
  - output: `{ "ok": true, "valid": true }`
- `tracker:status` -> verifie la presence du binaire PopTracker
  - output: `{ "ok": true, "exists": true, "path": "..." }`

## BizHawk Notes
- Auto-config écrit `config.ini` dans le bundle BizHawk (v2.10):
  - `RunInBackground=true`
  - `AutosaveSaveRAM=true`
  - `FlushSaveRamFrames=300` (≈ 5s)
  - `BackupSaveram=true`
- Vérifie `mono` avant lancement (retourne `mono_missing` si absent).

## PopTracker Build Notes
- Nécessite les submodules GitHub (`apclientpp`, etc.) + deps SDL2/SSL.
- Build: `make native CONF=RELEASE` dans `third_party/PopTracker`.
- Copier le binaire + `assets/` + `api/` + `schema/` + `key/` dans `client/runtime/poptracker/`.
