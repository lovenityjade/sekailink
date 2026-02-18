# ELECTRON-TODO.md

Objectif: préparer un client Electron SekaiLink qui **reproduit** le comportement du launcher Archipelago/MultiworldGG en s'appuyant sur le code existant (pour ne pas réinventer).
Principe: **toutes les opérations serveur** (lobbies, room servers, génération, hosting) restent côté serveur; le client desktop **remplace le site web** côté joueur avec **la même UI** (version desktop + features OS).

## 1) Ce que fait le launcher actuel (à cloner côté Electron)
Référence: `Launcher.py`, `worlds/LauncherComponents.py`
- Sélection d'un patch via UI → détecte le bon client selon l'extension (`SuffixIdentifier` + `AutoPatchRegister`).
- Si un patch est fourni en argument: lance le client adapté avec le patch.
- Sinon: affiche une UI Kivy listant les composants (clients/outils/etc.).
- Supporte les URIs `archipelago://`/`mwgg://` (via `handle_uri()` + `supports_uri`).
- Démarre les clients en CLI (subprocess) avec flags (`--connect`, `--password`, etc.).

À **reproduire** dans Electron:
- Association de fichiers `.ap*` → ouvrir le client + patch automatique.
- Résolution d'un patch vers **quel client** et **quel serveur**.
- Launch args/flags standardisés (server, slot, password, patch_path).

## 2) Système de patch (format + pipeline)
Référence: `Patch.py`, `worlds/Files.py`
- `Patch.create_rom_file(patch_file)`:
  - Utilise `AutoPatchRegister.get_handler` pour identifier la classe patch via extension.
  - Extrait **meta**: server, player, player_name.
  - Applique le patch et génère une ROM locale (suffix `result_file_ending`).
- Formats: `.ap*` = containers zip **APContainer** avec `archipelago.json`.
- Procédure de patch:
  - `APProcedurePatch` (procédure + files) → applique `apply_bsdiff4`, `apply_tokens`, etc.
  - `APDeltaPatch` (delta.bsdiff4) → patch bsdiff4.
- Extension ↔ game mapping via classes `AutoPatchRegister` (dans `worlds/*`).

À **implémenter** côté Electron:
- Détecter le handler par extension (même mapping que `AutoPatchRegister`).
- Appliquer patch localement (besoin d'une implémentation JS/TS):
  - bsdiff4
  - token patch (WRITE/COPY/RLE/AND/OR/XOR)
- Stockage ROMs patchées + mapping vers base ROMs (par jeu).

## 3) Démarrage SNI / BizHawk / RetroArch
Références:
- `SNIClient.py` (SNES/SNI workflow)
- `worlds/_bizhawk/context.py` (BizHawk client flow)
- Docs jeu: `worlds/*/docs/setup_en.md` (instructions end-user)

SNI (SNES):
- SNI server par défaut: `localhost:23074`.
- Si patch fourni: `Patch.create_rom_file` → auto-connect au serveur issu du patch.
- Auto-start de l'emu (selon settings): `settings.get_settings().sni_options.snes_rom_start`.
- Cas particulier `.apsoe`: ouvre Evermizer client web.

BizHawk:
- `_patch_and_run_game()` → patch + auto-run EmuHawk avec `connector_bizhawk_generic.lua`.
- `BizHawkClientContext` gère la boucle réseau multiworld.

RetroArch:
- Docs indiquent usage de `SNI/lua/Connector.lua`.
- Nécessite que RetroArch version >= 1.10.3.

À **implémenter** côté Electron:
- Détection/installation SNI (bundlé ou externe).
- Lancement BizHawk avec Lua connector.
- Lancement RetroArch avec Lua connector.
- Gestion des settings user (chemins emu + auto-launch).

## 4) Client multiworld (protocole)
Références:
- `CommonClient.py` (Multiworld protocol)
- `worlds/*/Client.py` (logique jeu spécifique)

À **reproduire**:
- Le client récupère `server` du patch.
- Connexion WebSocket → handshake multiworld.
- Gestion état: auth, slot, resync, keepalive.

## 5) Gestion APWorld / assets
Référence: `worlds/LauncherComponents.py` (`install_apworld`)
- Install `*.apworld` dans `custom_worlds`.
- Validation: zip valid + `__init__.py`.

À **implémenter** côté Electron:
- Installer/mettre à jour APWorlds.
- Versioning & compatibilité (manifest `archipelago.json`).

## 6) Liste des flux à reproduire (UX)
1. L’utilisateur clique sur un patch `.ap*` → ROM auto patchée → client auto-lancé → connexion auto.
2. L’utilisateur choisit un client (SNI/BizHawk) → lance l’emu + connecte.
3. L’utilisateur ouvre une URL `archipelago://` → propose client compatible.

## 7) Points techniques à valider (à analyser plus en profondeur)
- `SNIClient.py` logique de reconnexion + validation ROM.
- `worlds/_bizhawk/context.py` usage exact du lua connector + auto launch.
- `worlds/Files.py` formats patch + manifeste `archipelago.json`.
- `LauncherComponents.py` logique d'association extension ↔ client.
- Dépendances nécessaires (bsdiff4, token patch, zip manifest).

## 8) TODO Electron (exécution)
- [ ] Créer service `PatchService` (bsdiff4 + token patch + zip manifest).
- [ ] Mapper toutes les extensions `.ap*` aux handlers (porter `AutoPatchRegister`).
- [ ] Implémenter `MultiworldClient` (WebSocket) en s'inspirant de `CommonClient.py`.
- [ ] Implémenter `SNI` launcher + connexion (binaire SNI + ports + logs).
- [ ] Implémenter `BizHawk` launcher + lua connector.
- [ ] Implémenter `RetroArch` launcher + lua connector.
- [ ] UI pour choix client + status (SNI attached, BizHawk connected).
- [ ] Gestion de ROMs de base (paths + vérifs MD5 par jeu).
- [ ] Installer APWorlds (zip + manifest).
- [ ] Associer les patchs au launcher (file association OS).

## 9) Schéma d'architecture (Electron)
```
┌────────────────────────────────────────────────────────────────┐
│                        SekaiLink Desktop (Electron)            │
├─────────────────────────────┬──────────────────────────────────┤
│ Renderer (UI)               │ Main (Node/Electron)             │
│ - Lobby/Rooms UI            │ - Auth + session                 │
│ - Patch flow UI             │ - File associations (.ap*)       │
│ - Emulator settings         │ - PatchService (bsdiff + tokens) │
│ - Status (SNI/BizHawk)      │ - MultiworldClient (WebSocket)   │
├─────────────────────────────┴──────────────────────────────────┤
│                IPC bridge (preload, safe APIs)                 │
├────────────────────────────────────────────────────────────────┤
│ Services                                                          │
│ - PatchService: read AP patch → apply → ROM                      │
│ - MultiworldClient: connect to room server                        │
│ - EmulatorManager: SNI / BizHawk / RetroArch                      │
│ - ROM Manager: base ROM paths + MD5 checks                        │
│ - APWorld Manager: install/update *.apworld                       │
└────────────────────────────────────────────────────────────────┘
            │                          │
            ▼                          ▼
   ┌─────────────────┐        ┌─────────────────────────┐
   │ SekaiLink API   │        │ Room Server (Multiworld)│
   │ (webhost REST)  │        │ WebSocket protocol      │
   └─────────────────┘        └─────────────────────────┘
            │                          │
            ▼                          ▼
   ┌────────────────────────────────────────────────────┐
   │ Emulators + Bridges                                 │
   │ - SNI (localhost:23074)                              │
   │ - BizHawk + connector_bizhawk_generic.lua           │
   │ - RetroArch + Connector.lua                         │
   └────────────────────────────────────────────────────┘
```

## 10) MVP client (v1)
Objectif: client joueur “tout‑en‑un” pour SekaiLink/Archipelago.
- OS: **Windows, macOS, Linux** (focus compat SteamOS / handhelds).
- UI: **React** (renderer) + Electron main.
- Emus v1: **BizHawk** (GB/GBA/PSX) + **SNI** (SNES). RetroArch ensuite.
- Features v1:
  - Login (Discord via webhost).
  - Liste des lobbies + room info.
  - Télécharger patch(s) + auto‑patch.
  - Démarrer emu + Lua connector (auto).
  - Connexion multiworld + logs client.
  - Lancer PopTracker avec pack + host:port + slot + password.
  - Wizard “setup par jeu” (ROM base + émulateur requis).

## 11) Flow exact (Patch → ROM → Emu → Connect)
1) Client récupère patch(s) via API webhost (ou lien room).\n
2) PatchService applique le patch → ROM locale + meta (server, player, game).\n
3) EmulatorManager lance l’emu avec le bon Lua connector.\n
4) MultiworldClient se connecte au room server (WebSocket).\n
5) PopTracker (si disponible) est lancé avec pack + host/port/slot/password.\n
6) Client log/monitor: connexion, items, erreurs, reconnect.\n

## 12) Modules principaux à implémenter (React + Electron)
- **PatchService**: .ap* → ROM (bsdiff4 + tokens + manifest).
- **MultiworldClient**: protocole Archipelago (port de CommonClient).
- **EmulatorManager**: SNI / BizHawk / RetroArch (auto‑launch + Lua).
- **ROM Manager**: base ROMs + MD5 checks + storage local.
- **TrackerManager**: PopTracker packs + auto‑launch.
- **APWorldManager**: install/update .apworld.
- **Game Setup Wizard**: guide par jeu (dépendances + scripts + ROM base).

## 13) Fichiers clés (à étudier)
- `Launcher.py`
- `worlds/LauncherComponents.py`
- `Patch.py`
- `worlds/Files.py`
- `SNIClient.py`
- `worlds/_bizhawk/context.py`
- `worlds/*/Client.py`
- `worlds/*/Rom.py`
- `worlds/*/docs/setup_en.md`

## 10) Notes / limites
- Certaines infos (SNI repo, BizHawk docs, RetroArch) sont externes: à valider une fois que l’environnement Electron est prêt.
- Le client Electron doit rester compatible avec les patchs MultiworldGG/Archipelago existants.
