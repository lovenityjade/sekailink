# CLAUDE.md - SekaiLink Project Knowledge Base

This document captures the complete architecture and implementation details of the SekaiLink desktop client for AI assistant continuity.

---

## Project Overview

**SekaiLink** is an Electron + React desktop client that provides a "one-button Play" experience for Archipelago/MultiworldGG multiworld randomizer games. The goal is minimal user interaction ("0 interaction" ideal), targeting:

- Handheld consoles (Steam Deck, ROG Ally, Legion Go)
- New players unfamiliar with Archipelago
- Power users needing robust, debuggable tooling

**Current version**: beta-0.02-topaz
**Primary platform**: Linux (X11 + Wayland via XWayland)
**Windows**: Planned for Phase 6

---

## Repository Structure

```
sekailink/
├── client/                     # Desktop client (Electron + React)
│   ├── app/                    # Electron application
│   │   ├── electron/
│   │   │   ├── main.cjs        # Main process (~2600 lines)
│   │   │   └── preload.cjs     # Context bridge
│   │   ├── src/                # React frontend
│   │   │   ├── components/     # Reusable components
│   │   │   ├── pages/          # Route pages
│   │   │   ├── services/       # api.ts, runtime.ts
│   │   │   ├── hooks/          # useInterval, useSfx
│   │   │   └── styles/         # CSS
│   │   ├── electron-builder.yml
│   │   └── package.json
│   ├── runtime/
│   │   ├── commonclient_wrapper.py
│   │   ├── bizhawkclient_wrapper.py
│   │   ├── patcher_wrapper.py
│   │   ├── modules/            # Per-game runtime configs
│   │   │   └── <moduleId>/manifest.json
│   │   └── poptracker/         # PopTracker schemas
│   ├── drivers/                # Stub driver classes (not yet integrated)
│   ├── registry/               # Generated game registry
│   └── schema/                 # JSON schemas
├── WebHostLib/                 # Flask server (DO NOT MODIFY - server-side code)
├── worlds/                     # Archipelago world implementations
├── third_party/
│   └── emulators/
│       └── BizHawk-2.10-linux-x64/
├── sekailink-client-plan/      # Design documentation
└── sekailink-docs/             # User documentation
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Electron Main Process                        │
│                           (main.cjs)                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │   Session   │  │   Python    │  │  Emulator   │  │   Tracker   │ │
│  │Orchestrator │  │  Wrappers   │  │  Launcher   │  │  Launcher   │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘ │
└─────────┼────────────────┼────────────────┼────────────────┼────────┘
          │                │                │                │
          │ IPC            │ spawn          │ spawn          │ spawn
          ▼                ▼                ▼                ▼
┌─────────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐
│  React UI       │ │ bizhawk-    │ │  BizHawk    │ │   PopTracker    │
│  (Renderer)     │ │ client_     │ │  EmuHawk    │ │                 │
│                 │ │ wrapper.py  │ │  + Lua      │ │                 │
└─────────────────┘ └──────┬──────┘ └──────┬──────┘ └─────────────────┘
                           │               │
                           │ Lua connector │
                           └───────────────┘
```

---

## Electron Main Process (main.cjs)

### Key Functions

| Function | Purpose |
|----------|---------|
| `autoLaunchFromPatchUrl()` | Main orchestration: download → patch → emu → client → tracker |
| `ensurePythonRuntime()` | Bootstrap venv, install deps, auto-heal missing modules |
| `runPatcher()` | Spawn patcher_wrapper.py to apply AP patches |
| `launchBizHawk()` | Launch EmuHawk with ROM + Lua connector |
| `startBizHawkClient()` | Spawn bizhawkclient_wrapper.py |
| `startCommonClient()` | Spawn commonclient_wrapper.py (non-BizHawk games) |
| `launchPopTracker()` | Launch tracker with pack + AP connection |
| `applyLayoutBestEffort()` | wmctrl-based window positioning |
| `spawnMaybeGamescope()` | Optional gamescope wrapper for handheld mode |

### Key Paths

```javascript
getConfigPath()         // ~/.sekailink/config.json
getRomsDir()            // ~/.config/sekailink-client/roms/
getRuntimeDownloadsDir() // ~/.config/sekailink-client/runtime/downloads/
getRuntimeToolsDir()    // ~/.config/sekailink-client/runtime/tools/
getPythonVenvDir()      // ~/.config/sekailink-client/runtime/tools/python/venv/
getPopTrackerPacksDir() // ~/.config/sekailink-client/poptracker/packs/
getLogsDir()            // ~/.config/sekailink-client/logs/
getBundledRuntimeDir()  // client/runtime (dev) or resources/runtime (packaged)
getBundledThirdPartyDir() // third_party (dev) or resources/third_party (packaged)
getBundledApRootDir()   // repo root (dev) or resources/ap (packaged)
```

### IPC Handlers

All handlers registered via `ipcMain.handle()`:

**App/System**:
- `app:openExternal`, `app:showItemInFolder`, `app:openDashboard`
- `app:pickFile`, `app:pickFolder`

**Config**:
- `config:get`, `config:setRom`, `config:setGame`
- `config:setWindowing`, `config:setLayout`

**ROMs**:
- `roms:scan`, `roms:import`

**CommonClient** (non-BizHawk):
- `commonclient:start`, `commonclient:send`, `commonclient:stop`

**BizHawkClient**:
- `bizhawkclient:start`, `bizhawkclient:send`, `bizhawkclient:stop`

**Patcher/Emulator**:
- `patcher:apply`
- `bizhawk:launch`, `bizhawk:stop`

**Tracker**:
- `tracker:launch`, `tracker:stop`, `tracker:installPack`
- `tracker:status`, `tracker:validatePack`
- `tracker:listPackVariants`, `tracker:setVariant`

**Runtime**:
- `runtime:resolveModuleForDownload`, `runtime:getModuleManifest`, `runtime:listModules`
- `runtime:gamescopeStatus`, `runtime:wmctrlStatus`, `runtime:getDisplays`

**Session**:
- `session:autoLaunch`

**Logging**:
- `log:renderer`

---

## Python Wrappers

All wrappers use JSON-over-stdin/stdout for IPC with Electron.

### bizhawkclient_wrapper.py

- Uses `BizHawkClientContext` from `worlds._bizhawk.context`
- Imports `worlds` to register game handlers via `AutoBizHawkClientRegister`
- Runs `_game_watcher()` loop to connect to BizHawk Lua connector
- Emits events: `log`, `status`, `emu_status`, `room_info`, `print_json`, `error`
- Accepts commands: `connect`, `disconnect`, `ready`, `say`, `toggle_text`, `set_password`, `set_slot`, `shutdown`

### commonclient_wrapper.py

- Uses `CommonContext` from `CommonClient`
- For non-BizHawk games (Factorio, native clients, etc.)
- Same event/command protocol as bizhawkclient

### patcher_wrapper.py

- Imports world module by patch extension (e.g., `.apemerald` → `worlds.pokemon_emerald`)
- Applies ROM settings from config.json
- Calls `Patch.create_rom_file()` to generate patched ROM
- Outputs JSON: `{ ok: true, meta: {...}, output: "/path/to/rom" }`

---

## Runtime Module Manifest

Located at `client/runtime/modules/<moduleId>/manifest.json`:

```json
{
  "game_id": "pokemon_emerald",
  "display_name": "Pokemon Emerald",
  "emu": "bizhawk",
  "lua_connector": "lua/connector.lua",
  "tracker_pack_uid": "https://github.com/user/repo",
  "tracker_asset_regex": "\\.zip$",
  "patcher": "archipelago",
  "patch_extensions": [".apemerald"],
  "rom_requirements": {
    "extensions": [".gba"],
    "md5": ["605b89b67018abcea91e693a4dd25be3"],
    "sha1": []
  },
  "features": {
    "autotracking": true,
    "save_required": true
  }
}
```

### Key Fields

| Field | Purpose |
|-------|---------|
| `game_id` | Unique identifier, used in config.json for ROM paths |
| `emu` | Emulator family: `bizhawk`, `dolphin`, `sni`, etc. |
| `lua_connector` | Relative path to Lua connector script |
| `patch_extensions` | File extensions that trigger this module (e.g., `.apemerald`) |
| `rom_requirements.md5` | Valid ROM hashes for auto-detection |
| `tracker_pack_uid` | GitHub repo URL for PopTracker pack |

---

## React Frontend

### Pages (`client/app/src/pages/`)

| Page | Size | Purpose |
|------|------|---------|
| `Lobby.tsx` | ~93KB | Main game session: Socket.IO chat, YAML selection, generation, TrackerPanel, AP client status |
| `RoomList.tsx` | ~18KB | Browse/create lobbies |
| `Settings.tsx` | ~30KB | ROMs, gamescope, layout, game-specific configs |
| `GameManager.tsx` | ~28KB | YAML dashboard |
| `PlayerOptions.tsx` | ~36KB | Per-game options editor |
| `Account.tsx` | ~10KB | User account/profile |
| `Help.tsx` | ~0.5KB | Help page |

### Key Components (`client/app/src/components/`)

| Component | Purpose |
|-----------|---------|
| `AppShell.tsx` | Main layout wrapper |
| `AuthGate.tsx` | OAuth authentication flow |
| `Sidebar.tsx` | Navigation sidebar |
| `TrackerPanel.tsx` | In-app tracker view (player/multi/sphere modes) |
| `SocialDrawer.tsx` | Friends/social features |
| `BootScreen.tsx` | Initial loading screen |
| `ErrorBoundary.tsx` | React error boundary |

### Services (`client/app/src/services/`)

**api.ts**:
- `API_BASE_URL`, `getDesktopToken()`, `setDesktopToken()`
- `apiUrl()`, `apiFetch()`, `apiFetchWithTimeout()`, `apiJson()`

**runtime.ts**:
- TypeScript wrappers for all `window.sekailink.*` IPC methods
- Types: `CommonClientCommand`, `PatcherOptions`, `BizHawkLaunchOptions`, `TrackerLaunchOptions`, `SessionAutoLaunchOptions`

### Type Definitions (`vite-env.d.ts`)

Declares the `window.sekailink` interface with all IPC methods.

---

## Session AutoLaunch Flow

```
1. UI: runtime.sessionAutoLaunch({ downloadUrl, serverAddress, slot, password, apGameName })
   │
2. main.cjs: autoLaunchFromPatchUrl()
   ├── emitSessionEvent({ event: "status", status: "Downloading..." })
   ├── downloadToDir(downloadUrl, getRuntimeDownloadsDir())
   │
3. Artifact handling (tryHandleDownloadedArtifact)
   ├── .apsm64ex → tryLaunchSm64Ex()
   ├── .zip (Factorio) → install mod → tryLaunchFactorio()
   ├── .pk3 (gzDoom) → tryLaunchGzDoom()
   └── .json (DS3) → ensureGithubReleaseZipInstalled()
   │
4. If AP patch file (.ap*):
   ├── resolveModuleForDownload() → find moduleId by extension
   ├── validateSetupForModule() → check ROM present
   ├── runPatcher() → spawn patcher_wrapper.py
   │
5. launchBizHawk({ romPath, moduleId })
   ├── ensureBizHawkConfig()
   ├── spawnMaybeGamescope(emuPath, args)
   │
6. startBizHawkClient() → spawn bizhawkclient_wrapper.py
   ├── sendBizHawkClientCommand({ cmd: "connect", address, slot, password })
   │
7. launchPopTracker({ moduleId, apHost, apSlot, apPass })
   ├── installTrackerPack() if needed
   ├── Auto-detect pack variant
   │
8. applyLayoutBestEffort({ gamePid, trackerPid })
   ├── wmctrlFindWindowIdByPid()
   ├── wmctrlMoveResize()
   │
9. emitSessionEvent({ event: "ready", ... })
```

---

## Config File Structure

Located at `~/.sekailink/config.json`:

```json
{
  "roms": {
    "pokemon_emerald": "/path/to/pokemon_emerald.gba",
    "oot": "/path/to/oot.z64"
  },
  "games": {
    "factorio": {
      "exe_path": "/path/to/factorio",
      "mods_dir": "~/.factorio/mods"
    },
    "sm64ex": {
      "exe_path": "/path/to/sm64.us.f3dex2e"
    },
    "gzdoom": {
      "exe_path": "/path/to/gzdoom",
      "iwad_path": "/path/to/doom2.wad",
      "gzap_pk3_path": "/path/to/gzArchipelago.pk3"
    }
  },
  "windowing": {
    "gamescope": {
      "enabled": false,
      "mode": "prefer",
      "fullscreen": true,
      "width": 1920,
      "height": 1080,
      "args": []
    }
  },
  "layout": {
    "preset": "handheld",
    "mode": "side_by_side",
    "game_display": 0,
    "tracker_display": 1,
    "split": 0.7
  },
  "trackerPacks": {
    "pokemon_emerald": {
      "path": "/path/to/pack",
      "source": "https://...",
      "installed_at": "2026-02-09T..."
    }
  },
  "trackerVariants": {
    "pokemon_emerald": "variant_id"
  }
}
```

---

## Driver System (Stub)

Located at `client/drivers/`. Currently **not integrated** - actual logic is in main.cjs.

```typescript
// BaseDriver.ts
export abstract class BaseDriver {
  abstract family: string;
  async verify(ctx: DriverContext): Promise<void>;
  async prepare(ctx: DriverContext): Promise<void>;
  async launch(ctx: DriverContext): Promise<void>;
  async connect(ctx: DriverContext): Promise<void>;
}

// DriverContext (client/runtime/types.ts)
export interface DriverContext {
  game: GameRuntimeContract;
  assetsRoot: string;
  outputRoot: string;
  server?: string;
  slot?: string;
  password?: string;
}
```

Available drivers (stubs): BizHawk, RetroArch, Dolphin, PCSX2, DuckStation, PPSSPP, Cemu, Switch, ModLoader

---

## Design Decisions

1. **Streamer-first security**: Passwords never in argv, use stdin/env instead
2. **Extension-based routing**: `.apemerald` → pokemon_emerald module (no hardcoding)
3. **JSON-over-stdio IPC**: Python wrappers communicate via JSON lines
4. **Best-effort automation**: Tracker/layout failures don't block session
5. **Gamescope for handhelds**: Optional wrapper for Steam Deck-like experience
6. **Legal compliance**: Never distribute ROMs/BIOS/keys

---

## Implementation Phases

| Phase | Focus | Status |
|-------|-------|--------|
| 0 | Stabilization (PatchResolver, logs, Wayland) | Done |
| 1 | BizHawk family complete | Current |
| 2 | Slot files, non-AP-patch pipelines | Planned |
| 3 | SNIClient + SNES family | Planned |
| 4 | Dolphin family (TP, TTYD) | Planned |
| 5 | Universal Tracker integration | Planned |
| 6 | Windows parity | Planned |

---

## Supported Games (BizHawk modules)

Located in `client/runtime/modules/`:

- **GBA**: Pokemon Emerald/FireRed/LeafGreen, MZM, Wario Land 4, Metroid Fusion, FFTA, TMC
- **GB/GBC**: Pokemon Red/Blue/Crystal, Mario Land 2, Wario Land, Zelda Oracle of Seasons
- **SNES**: ALttP, SM, SMW, SMZ3, DKC/2/3, Earthbound, KDL3, Lufia 2 AC, FF4 FE
- **NES**: TLoZ, Zelda 2, Mega Man 2/3
- **N64**: OoT

---

## Development Notes

### Running in Dev Mode
```bash
cd client/app
npm install
npm run dev
```

### Building AppImage
```bash
cd client/app
npm run build
npm run package
# Output: client/app/release/*.AppImage
```

### Key Environment Variables
- `SEKAILINK_PYTHON`: Override Python executable
- `SEKAILINK_BIZHAWK`: Override BizHawk path
- `SEKAILINK_GAMESCOPE`: Override gamescope path
- `SEKAILINK_AP_ROOT`: AP python sources root (set by main.cjs)
- `SKIP_REQUIREMENTS_UPDATE`: Disable pip/update flows in wrappers

### Logs
- Main logs: `~/.config/sekailink-client/logs/sekailink-*.log`
- Format: `[timestamp] [level] scope: message` or JSON lines

---

## Important Reminders

1. **Never modify WebHostLib/** - it's server code managed separately
2. **Module manifests are the source of truth** for game support
3. **Python wrappers must remain headless** - no GUI, no interactive prompts
4. **IPC is JSON lines** - one JSON object per line on stdin/stdout
5. **Test on Linux first** - Windows support is Phase 6

---

## Known Issues and Recent Fixes

### setuptools / pkg_resources (Fixed 2026-02-09)

**Issue**: On Python 3.12+, setuptools >= 70 no longer bundles `pkg_resources`. The auto-heal loop would endlessly try to install it.

**Fix**: Pin `setuptools<70` in three places in `main.cjs`:
- `pipModuleToPipSpec()` mapper: `pkg_resources: "setuptools<70"`
- Bootstrap pip install: `pip install --upgrade pip "setuptools<70" wheel`
- `deps` set: `"setuptools<70"`

### Launch Progress Modal (Added 2026-02-09)

**Issue**: No visual feedback during autolaunch - just a simple status div.

**Fix**: Added `launchModalOpen` state and a proper modal with:
- Spinner animation during progress
- Error display with close button
- Status text updates from session events

CSS added to `lobby.css` (`.skl-launch-modal-*` classes).

### Autolaunch Logging (Added 2026-02-09)

Added `writeLogLine()` calls at key points in `autoLaunchFromPatchUrl()`:
- Patch start/result
- Emulator launch start/result
- Client start result
- Tracker launch result
- Layout application

Logs are written to `~/.config/sekailink-client/logs/sekailink-*.log`.

---

*Last updated: 2026-02-09*
