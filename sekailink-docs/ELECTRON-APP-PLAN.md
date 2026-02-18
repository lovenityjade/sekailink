# ELECTRON-APP-PLAN.md

## SekaiLink Desktop Client - Plan d'Architecture

> Application Electron pour Windows/macOS/Linux permettant de se connecter aux rooms MultiworldGG/SekaiLink avec gestion simplifiée des émulateurs et ROMs.

---

## 1. Objectifs

### 1.1 Objectif Principal
Créer une application desktop accessible qui:
- Se connecte aux room servers MultiworldGG (protocole WebSocket)
- Gère automatiquement les émulateurs (SNI, BizHawk, RetroArch)
- Simplifie le setup pour les nouveaux joueurs
- S'intègre aux lobbies SekaiLink (chat, génération, patches)

### 1.2 Ce qui reste côté serveur (90%)
- Génération de seeds
- Hosting des rooms (MultiServer)
- Hosting des lobbies
- Stockage des YAMLs utilisateur
- Authentification Discord
- Trackers web

### 1.3 Ce qui passe côté client Electron
- Connexion aux room servers (remplace CommonClient.py)
- Communication avec émulateurs (SNI/BizHawk bridges)
- Gestion des ROMs et patches
- Configuration locale (paths, préférences)
- Interface utilisateur native

---

## 2. Stack Technologique

### 2.1 Electron (Main Process)
```
Node.js 20 LTS
├── electron 28+
├── electron-builder (packaging)
├── electron-updater (auto-update)
├── better-sqlite3 (config locale)
├── ws (WebSocket client)
└── node-fetch (HTTP client)
```

### 2.2 Renderer (UI)
```
React 18 + TypeScript
├── Vite (build tool)
├── Tailwind CSS (styling)
├── Zustand (state management)
├── Socket.IO Client (SekaiLink realtime)
└── React Query (cache API)
```

### 2.3 Pourquoi ce stack?
| Choix | Raison |
|-------|--------|
| Electron | Cross-platform, accès filesystem, IPC natif |
| React | Composants réutilisables, écosystème mature |
| TypeScript | Type safety pour le protocole réseau complexe |
| Tailwind | Réutilise le design system SekaiLink existant |
| Zustand | Léger, simple, pas de boilerplate Redux |

---

## 3. Architecture des Fichiers

```
sekailink-desktop/
├── electron/                    # Main Process (Node.js)
│   ├── main.ts                  # Point d'entrée Electron
│   ├── preload.ts               # Bridge IPC sécurisé
│   │
│   ├── ipc/                     # Handlers IPC
│   │   ├── config.ipc.ts        # Lecture/écriture config
│   │   ├── emulators.ipc.ts     # Lancement émulateurs
│   │   ├── roms.ipc.ts          # Scan et patch ROMs
│   │   └── multiworld.ipc.ts    # Contrôle client MW
│   │
│   ├── services/                # Services métier
│   │   ├── MultiworldClient.ts  # Client WebSocket AP/MWGG
│   │   ├── SNIBridge.ts         # Connexion SNI (SNES)
│   │   ├── BizHawkBridge.ts     # Connexion BizHawk (TCP)
│   │   ├── PatchService.ts      # Application patches .ap*
│   │   ├── RomScanner.ts        # Scan dossiers ROMs
│   │   └── ConfigStore.ts       # Persistance config
│   │
│   └── handlers/                # Handlers par jeu
│       ├── BaseGameHandler.ts   # Classe abstraite
│       ├── ALttPHandler.ts      # A Link to the Past
│       ├── SMHandler.ts         # Super Metroid
│       ├── OoTHandler.ts        # Ocarina of Time
│       ├── LTTPHandler.ts       # Links Awakening
│       └── index.ts             # Registry des handlers
│
├── src/                         # Renderer (React)
│   ├── App.tsx                  # Root component
│   ├── main.tsx                 # Entry point React
│   │
│   ├── components/
│   │   ├── ui/                  # Design system
│   │   │   ├── Glass.tsx
│   │   │   ├── Button.tsx
│   │   │   ├── Input.tsx
│   │   │   └── ...
│   │   │
│   │   ├── setup/               # Setup wizard
│   │   │   ├── SetupWizard.tsx
│   │   │   ├── WelcomeStep.tsx
│   │   │   ├── EmulatorStep.tsx
│   │   │   ├── RomPathsStep.tsx
│   │   │   └── LoginStep.tsx
│   │   │
│   │   ├── client/              # Client multiworld
│   │   │   ├── ClientView.tsx
│   │   │   ├── ConnectionPanel.tsx
│   │   │   ├── ItemLog.tsx
│   │   │   ├── ChatPanel.tsx
│   │   │   └── EmulatorStatus.tsx
│   │   │
│   │   └── lobby/               # Intégration lobby
│   │       ├── LobbyBrowser.tsx
│   │       ├── LobbyRoom.tsx
│   │       └── LobbyChat.tsx
│   │
│   ├── stores/                  # State management
│   │   ├── configStore.ts
│   │   ├── connectionStore.ts
│   │   ├── gameStore.ts
│   │   └── lobbyStore.ts
│   │
│   ├── hooks/                   # Custom hooks
│   │   ├── useMultiworld.ts
│   │   ├── useEmulator.ts
│   │   └── useSekaiLink.ts
│   │
│   └── styles/
│       ├── globals.css          # Tailwind + tokens
│       └── components.css       # Composants custom
│
├── assets/
│   ├── img/                     # Backgrounds SekaiLink
│   ├── sfx/                     # Sons UI
│   └── icons/                   # Icônes app
│
├── resources/                   # Bundled avec l'app
│   ├── sni/                     # SNI binaire (optionnel)
│   └── lua/                     # Scripts Lua BizHawk
│
├── package.json
├── electron-builder.yml
├── vite.config.ts
├── tailwind.config.js
└── tsconfig.json
```

---

## 4. Flux de Données

### 4.1 Architecture Globale

```
┌─────────────────────────────────────────────────────────────────────┐
│                         SEKAILINK SERVER                            │
│  (lobbies, génération, YAMLs, auth Discord, trackers)               │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ Socket.IO + REST API
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      ELECTRON RENDERER                              │
│  React UI (lobby browser, chat, setup wizard, préférences)          │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ IPC (contextBridge)
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       ELECTRON MAIN                                 │
│  MultiworldClient + SNIBridge + BizHawkBridge + GameHandlers        │
└──────┬──────────────────┬──────────────────┬───────────────────────┘
       │                  │                  │
       ▼                  ▼                  ▼
┌────────────┐    ┌────────────┐    ┌────────────────┐
│ Room Server│    │    SNI     │    │    BizHawk     │
│ (WebSocket)│    │ (WS:23074) │    │  (TCP:43884)   │
└────────────┘    └─────┬──────┘    └───────┬────────┘
                        │                   │
                        ▼                   ▼
                 ┌────────────┐      ┌────────────┐
                 │  Émulateur │      │  BizHawk   │
                 │   SNES     │      │  Emulator  │
                 └────────────┘      └────────────┘
```

### 4.2 Flux de Connexion à une Room

```
1. Utilisateur rejoint un lobby SekaiLink (via UI React)
2. Lobby génère une seed → room créée côté serveur
3. UI reçoit room_info (address:port, slot, patches)
4. UI demande au Main Process de se connecter:
   └─► IPC: multiworld.connect({ address, slot, password })
5. Main Process:
   a. MultiworldClient.connect() → WebSocket vers room server
   b. Reçoit RoomInfo → envoie Connect packet
   c. Reçoit Connected → authentifié
6. UI télécharge le patch pour le slot du joueur
7. Main Process:
   a. PatchService.apply(basePath, patchPath) → ROM patchée
   b. Détecte le jeu → instancie le bon GameHandler
   c. Lance l'émulateur avec la ROM patchée
   d. SNIBridge/BizHawkBridge se connecte à l'émulateur
8. GameHandler.gameWatcher() commence à poll l'émulateur
9. Boucle de jeu:
   - Locations checkées → envoyées au serveur
   - Items reçus → écrits en mémoire émulateur
```

### 4.3 Flux de Réception d'Item

```
Room Server                    MultiworldClient              GameHandler
     │                              │                            │
     │  ReceivedItems               │                            │
     │  {index: 5, items: [...]}    │                            │
     │─────────────────────────────►│                            │
     │                              │                            │
     │                              │  emit('itemReceived', item)│
     │                              │───────────────────────────►│
     │                              │                            │
     │                              │                            │ writeMemory()
     │                              │                            │────────────►
     │                              │                            │   Émulateur
     │                              │                            │
     │                              │  emit('itemProcessed')     │
     │                              │◄───────────────────────────│
     │                              │                            │
```

---

## 5. Composants Clés

### 5.1 MultiworldClient (Port de CommonClient.py)

```typescript
// electron/services/MultiworldClient.ts

interface ConnectionState {
  status: 'disconnected' | 'connecting' | 'connected' | 'authenticated';
  serverAddress: string;
  slotName: string;
  team: number;
  slot: number;
  game: string;
}

interface GameState {
  itemsReceived: NetworkItem[];
  locationsChecked: Set<number>;
  locationsMissing: Set<number>;
  hintPoints: number;
  slotData: Record<string, any>;
}

class MultiworldClient extends EventEmitter {
  private ws: WebSocket | null = null;
  private connectionState: ConnectionState;
  private gameState: GameState;

  // Connexion avec reconnexion automatique
  async connect(address: string, slotName: string, password?: string): Promise<void>;

  // Envoi de messages au serveur
  send(messages: ServerMessage[]): void;

  // Actions de jeu
  checkLocations(locationIds: number[]): void;
  scoutLocations(locationIds: number[]): void;
  updateStatus(status: ClientStatus): void;

  // Gestion des événements serveur
  private handleRoomInfo(msg: RoomInfo): void;
  private handleConnected(msg: Connected): void;
  private handleReceivedItems(msg: ReceivedItems): void;
  private handlePrintJSON(msg: PrintJSON): void;
}
```

### 5.2 SNIBridge (Connexion émulateurs SNES)

```typescript
// electron/services/SNIBridge.ts

type SNIState = 'disconnected' | 'connecting' | 'connected' | 'attached';

class SNIBridge extends EventEmitter {
  private ws: WebSocket | null = null;
  private state: SNIState = 'disconnected';
  private deviceName: string = '';

  // Connexion à SNI
  async connect(address?: string): Promise<void>;

  // Découverte des devices
  async listDevices(): Promise<string[]>;
  async attach(deviceName: string): Promise<void>;

  // Opérations mémoire
  async readMemory(address: number, size: number): Promise<Buffer>;
  async writeMemory(address: number, data: Buffer): Promise<void>;

  // Lecture ROM info
  async getRomHash(): Promise<string>;
}
```

### 5.3 BaseGameHandler (Abstraction par jeu)

```typescript
// electron/handlers/BaseGameHandler.ts

abstract class BaseGameHandler {
  // Métadonnées
  abstract readonly gameId: string;
  abstract readonly gameName: string;
  abstract readonly patchSuffix: string | string[];
  abstract readonly platform: 'snes' | 'n64' | 'gba' | 'gb' | 'nes' | 'psx';

  // Références aux services
  protected client: MultiworldClient;
  protected bridge: SNIBridge | BizHawkBridge;

  // Méthodes abstraites à implémenter par jeu
  abstract validateRom(): Promise<boolean>;
  abstract gameWatcher(): Promise<void>;
  abstract receiveItem(item: NetworkItem): Promise<void>;
  abstract killPlayer(): Promise<void>; // DeathLink

  // Hook optionnel
  onServerMessage(cmd: string, args: any): void {}
}
```

### 5.4 ConfigStore (Persistance locale)

```typescript
// electron/services/ConfigStore.ts

interface SekaiLinkConfig {
  // Authentification
  user: {
    discordId: string;
    username: string;
    accessToken: string;
    refreshToken: string;
  } | null;

  // Serveur SekaiLink
  server: {
    url: string;           // https://sekailink.xyz
    autoConnect: boolean;
  };

  // Émulateurs
  emulators: {
    sni: {
      enabled: boolean;
      autoLaunch: boolean;
      path: string | null;
      address: string;     // localhost:23074
    };
    bizhawk: {
      enabled: boolean;
      path: string | null;
      luaAutoLoad: boolean;
    };
    retroarch: {
      enabled: boolean;
      path: string | null;
      corePaths: Record<string, string>;
    };
  };

  // Chemins ROMs par système
  romPaths: {
    snes: string[];
    n64: string[];
    gba: string[];
    gb: string[];
    nes: string[];
    psx: string[];
  };

  // ROMs spécifiques par jeu (pour auto-patch)
  gameRoms: {
    [gameId: string]: {
      basePath: string;
      lastPatchHash: string | null;
    };
  };

  // Préférences UI
  ui: {
    theme: 'green' | 'purple';
    sfxEnabled: boolean;
    sfxVolume: number;
    language: string;
    showTutorials: boolean;
  };

  // Historique connexions
  recentServers: Array<{
    address: string;
    slotName: string;
    game: string;
    lastConnected: string;
  }>;
}
```

---

## 6. Protocole MultiworldGG

### 6.1 Messages Client → Serveur

| Commande | Description |
|----------|-------------|
| `Connect` | Authentification initiale |
| `Sync` | Demande resync items |
| `LocationChecks` | Signale locations checkées |
| `LocationScouts` | Demande info sur locations |
| `StatusUpdate` | Met à jour statut (ready, goal) |
| `Say` | Message chat |
| `Bounce` | Message broadcast (DeathLink) |
| `Get` | Lecture data storage |
| `Set` | Écriture data storage |

### 6.2 Messages Serveur → Client

| Commande | Description |
|----------|-------------|
| `RoomInfo` | Info serveur à la connexion |
| `Connected` | Confirmation authentification |
| `ConnectionRefused` | Erreur authentification |
| `ReceivedItems` | Items à recevoir |
| `RoomUpdate` | MAJ état room |
| `PrintJSON` | Message formaté (chat, events) |
| `Bounced` | Echo broadcast reçu |
| `Retrieved` | Réponse Get |
| `SetReply` | Réponse Set |

### 6.3 Format Connect Packet

```typescript
interface ConnectPacket {
  cmd: 'Connect';
  password: string;
  name: string;              // Slot name ou ROM hash base64
  version: {
    major: number;
    minor: number;
    build: number;
    class: 'Version';
  };
  tags: string[];            // ['AP', 'DeathLink', etc.]
  items_handling: number;    // Bitmask: 0b111 = all items
  uuid: string;              // UUID client unique
  game: string;
  slot_data: boolean;
}
```

---

## 7. Gestion des Jeux

### 7.1 Jeux Prioritaires (Phase 1)

| Jeu | Platform | Bridge | Complexité |
|-----|----------|--------|------------|
| A Link to the Past | SNES | SNI | Moyenne |
| Super Metroid | SNES | SNI | Moyenne |
| Link's Awakening DX | GB | BizHawk | Simple |

### 7.2 Jeux Phase 2

| Jeu | Platform | Bridge | Complexité |
|-----|----------|--------|------------|
| Ocarina of Time | N64 | BizHawk | Élevée |
| Pokémon Red/Blue | GB | BizHawk | Moyenne |
| Pokémon Emerald | GBA | BizHawk | Moyenne |

### 7.3 Structure Handler Exemple (ALttP)

```typescript
// electron/handlers/ALttPHandler.ts

class ALttPHandler extends BaseGameHandler {
  readonly gameId = 'A Link to the Past';
  readonly gameName = 'The Legend of Zelda: A Link to the Past';
  readonly patchSuffix = ['.aplttp', '.apz3'];
  readonly platform = 'snes';

  // Adresses mémoire SNES (WRAM)
  private readonly WRAM_START = 0x7E0000;
  private readonly RECV_PROGRESS = 0x7EF36F;
  private readonly RECV_ITEM = 0x7EF36E;
  private readonly GAME_MODE = 0x7E0010;
  private readonly LOCATION_FLAGS = 0x7EF000;

  async validateRom(): Promise<boolean> {
    // Lire header ROM et vérifier titre
    const header = await this.bridge.readMemory(0x00FFC0, 21);
    const title = header.toString('ascii').trim();
    return title.includes('ZELDA') || title.includes('ALTTP');
  }

  async gameWatcher(): Promise<void> {
    while (this.isRunning) {
      // Vérifier que le jeu est dans un état jouable
      const gameMode = await this.readByte(this.GAME_MODE);
      if (!this.isValidGameMode(gameMode)) {
        await this.sleep(500);
        continue;
      }

      // Scanner les locations checkées
      const locationData = await this.bridge.readMemory(
        this.LOCATION_FLAGS,
        0x500
      );
      const checked = this.parseLocationFlags(locationData);
      this.client.checkLocations(checked);

      // Traiter les items en attente
      await this.processItemQueue();

      await this.sleep(100);
    }
  }

  async receiveItem(item: NetworkItem): Promise<void> {
    // Attendre que le jeu soit prêt à recevoir
    while (await this.readByte(this.RECV_ITEM) !== 0) {
      await this.sleep(50);
    }

    // Écrire l'item
    const gameItemId = this.mapToGameItem(item.item);
    await this.writeByte(this.RECV_ITEM, gameItemId);
  }
}
```

---

## 8. Interface Utilisateur

### 8.1 Écrans Principaux

1. **Boot Screen** - Chargement initial, auto-update check
2. **Setup Wizard** - Configuration première utilisation
3. **Dashboard** - Vue principale après setup
4. **Lobby Browser** - Liste des lobbies SekaiLink
5. **Lobby Room** - Chat, membres, génération
6. **Client View** - Connexion active à une room
7. **Settings** - Configuration émulateurs/ROMs

### 8.2 Setup Wizard Flow

```
[Welcome] → [Login Discord] → [Émulateurs] → [ROMs] → [Terminé]
    │              │                │            │
    │              │                │            └─► Scan auto des ROMs
    │              │                │                dans les paths fournis
    │              │                │
    │              │                └─► Sélection des émulateurs
    │              │                    (SNI, BizHawk, RetroArch)
    │              │                    + paths executables
    │              │
    │              └─► OAuth Discord via SekaiLink
    │                  (ouvre navigateur externe)
    │
    └─► Explication de SekaiLink/Multiworld
        pour les nouveaux joueurs
```

### 8.3 Client View Layout

```
┌────────────────────────────────────────────────────────────┐
│ [🔌 Connecté] Server: sekailink.xyz:38281    [⚙️] [❌]    │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌─────────────────────────┐  ┌─────────────────────────┐  │
│  │      GAME STATUS        │  │      ITEM LOG           │  │
│  │                         │  │                         │  │
│  │  🎮 A Link to the Past  │  │  ✓ Bow from Player2     │  │
│  │  📍 Slot: Player1       │  │  ✓ Bombs from Player3   │  │
│  │  🏆 Progress: 45%       │  │  ✓ Moon Pearl from ...  │  │
│  │                         │  │  ► Hookshot pending...  │  │
│  │  [SNI: Attached]        │  │                         │  │
│  │  [ROM: Valid]           │  │                         │  │
│  │                         │  │                         │  │
│  └─────────────────────────┘  └─────────────────────────┘  │
│                                                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                      CHAT                            │  │
│  │  [Server] Room started                               │  │
│  │  [Player2] glhf!                                     │  │
│  │  [Player1] gg                                        │  │
│  │                                                      │  │
│  │  [____________________________________] [Send]       │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 9. Sécurité

### 9.1 IPC Sécurisé (Preload)

```typescript
// electron/preload.ts
import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('electron', {
  // Config
  getConfig: () => ipcRenderer.invoke('config:get'),
  setConfig: (key: string, value: any) =>
    ipcRenderer.invoke('config:set', key, value),

  // Multiworld
  connect: (opts: ConnectOptions) =>
    ipcRenderer.invoke('multiworld:connect', opts),
  disconnect: () => ipcRenderer.invoke('multiworld:disconnect'),
  sendChat: (message: string) =>
    ipcRenderer.invoke('multiworld:chat', message),

  // Émulateurs
  launchEmulator: (game: string, romPath: string) =>
    ipcRenderer.invoke('emulator:launch', game, romPath),
  getEmulatorStatus: () =>
    ipcRenderer.invoke('emulator:status'),

  // Fichiers (avec validation)
  selectFile: (opts: FileDialogOptions) =>
    ipcRenderer.invoke('dialog:selectFile', opts),
  selectFolder: (opts: FolderDialogOptions) =>
    ipcRenderer.invoke('dialog:selectFolder', opts),

  // Events (one-way)
  onItemReceived: (callback: (item: any) => void) =>
    ipcRenderer.on('game:itemReceived', (_, item) => callback(item)),
  onConnectionChange: (callback: (state: any) => void) =>
    ipcRenderer.on('connection:change', (_, state) => callback(state)),
});
```

### 9.2 Validation des Chemins

```typescript
// Ne jamais exposer fs directement au renderer
// Valider tous les chemins côté main process

function validatePath(userPath: string, allowedDirs: string[]): boolean {
  const resolved = path.resolve(userPath);
  return allowedDirs.some(dir => resolved.startsWith(dir));
}
```

---

## 10. Packaging et Distribution

### 10.1 Configuration electron-builder

```yaml
# electron-builder.yml
appId: xyz.sekailink.desktop
productName: SekaiLink
copyright: Copyright © 2024 SekaiLink

directories:
  output: release
  buildResources: build

files:
  - dist/**/*
  - assets/**/*
  - "!**/*.map"

extraResources:
  - from: resources/lua
    to: lua

win:
  target:
    - target: nsis
      arch: [x64]
    - target: portable
      arch: [x64]
  icon: assets/icons/icon.ico
  artifactName: ${productName}-Setup-${version}.${ext}

mac:
  target:
    - target: dmg
      arch: [x64, arm64]
    - target: zip
      arch: [x64, arm64]
  icon: assets/icons/icon.icns
  category: public.app-category.games
  hardenedRuntime: true
  gatekeeperAssess: false

linux:
  target:
    - target: AppImage
      arch: [x64]
    - target: deb
      arch: [x64]
  icon: assets/icons
  category: Game
  maintainer: sekailink@example.com

publish:
  provider: github
  owner: sekailink
  repo: sekailink-desktop
  releaseType: release
```

### 10.2 Auto-Update

```typescript
// electron/main.ts
import { autoUpdater } from 'electron-updater';

autoUpdater.autoDownload = false;
autoUpdater.autoInstallOnAppQuit = true;

autoUpdater.on('update-available', (info) => {
  // Notifier le renderer
  mainWindow?.webContents.send('update:available', info);
});

autoUpdater.on('download-progress', (progress) => {
  mainWindow?.webContents.send('update:progress', progress);
});

autoUpdater.on('update-downloaded', () => {
  mainWindow?.webContents.send('update:ready');
});

// Vérifier au démarrage
app.whenReady().then(() => {
  autoUpdater.checkForUpdates();
});
```

---

## 11. Phases de Développement

### Phase 1: Fondations (2-3 semaines)
- [ ] Setup projet Electron + React + TypeScript + Vite
- [ ] Structure fichiers et configuration build
- [ ] Design system (tokens CSS, composants de base)
- [ ] ConfigStore avec better-sqlite3
- [ ] MultiworldClient (connexion basique)
- [ ] UI: Boot screen, Settings basique

### Phase 2: SNI Integration (2 semaines)
- [ ] SNIBridge complet
- [ ] ALttPHandler (premier jeu)
- [ ] Test end-to-end avec vraie seed ALttP
- [ ] UI: Client View, Item Log, Chat

### Phase 3: Setup Wizard (1-2 semaines)
- [ ] Login Discord OAuth (via SekaiLink)
- [ ] Configuration émulateurs
- [ ] Scan et configuration ROMs
- [ ] Tutoriel intégré pour nouveaux joueurs

### Phase 4: Intégration SekaiLink (2 semaines)
- [ ] Socket.IO vers lobbies SekaiLink
- [ ] Lobby Browser
- [ ] Lobby Room (chat, membres, génération)
- [ ] Téléchargement et application patches automatique
- [ ] Lancement émulateur post-génération

### Phase 5: Jeux Additionnels (3-4 semaines)
- [ ] Super Metroid Handler
- [ ] BizHawkBridge
- [ ] Link's Awakening Handler
- [ ] Ocarina of Time Handler (si temps)

### Phase 6: Polish et Release (2 semaines)
- [ ] Auto-update
- [ ] Packaging multi-plateforme
- [ ] Tests sur Windows/macOS/Linux
- [ ] Documentation utilisateur
- [ ] Beta release

---

## 12. Ressources et Références

### Documentation MultiworldGG
- `/opt/multiworldgg/docs/network protocol.md` - Protocole WebSocket
- `/opt/multiworldgg/docs/world api.md` - API des jeux
- `/opt/multiworldgg/CommonClient.py` - Client Python référence
- `/opt/multiworldgg/SNIClient.py` - SNI Python référence

### SNI
- https://github.com/alttpo/sni - SNI repository
- Protocol: JSON-RPC over WebSocket (port 23074)

### BizHawk
- https://github.com/TASEmulators/BizHawk
- Connector: TCP socket avec JSON protocol

### Electron
- https://www.electronjs.org/docs/latest
- https://www.electron.build/ (electron-builder)

---

## 13. Questions Ouvertes

1. **Bundler SNI?** - Inclure SNI dans l'app ou demander installation séparée?
2. **RetroArch support?** - Priorité ou phase future?
3. **Offline mode?** - Supporter le jeu solo/local?
4. **Mobile companion?** - App mobile pour notifications?
5. **Tracker intégré?** - Embarquer le tracker ou lien vers web?

---

## Changelog

| Date | Modification |
|------|--------------|
| 2024-01-28 | Création initiale du document |
