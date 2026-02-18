# SekaiLink - Game Setup Central

Ce document resume les exigences d'installation et d'execution par jeu, et comment on va les integrer dans le client Electron SekaiLink. Les docs sources parlent de MultiworldGG/Archipelago; ici on normalise pour SekaiLink (serveur "SekaiLink", client Electron).

Classement par categorie (emulateur/mod/natif/archipelago-specific/other). Chaque entree inclut: ROM client/serveur, Lua requis, connecteur/flow, et un resume d'implantation SekaiLink.

## BizHawk

### Kirby 64 - The Crystal Shards (k64)
- Installation: BizHawk 2.7+ (2.10 recommande) + ROM US/EN `.z64`. MultiworldGG Launcher pour patcher le `.apk64cs` et produire un `.z64` patche.
- Execution: ouvrir le ROM patche dans `EmuHawk.exe`.
- ROM client: Oui (ROM vanilla pour patcher, ROM patche pour jouer).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua` dans l'install MultiworldGG.
- Connexion serveur: via BizHawk Client (MultiworldGG) qui se connecte au serveur, et Lua relie EmuHawk au client.
- SekaiLink: fournir un "BizHawk Client" integre dans Electron, capable de:
  - lancer BizHawk,
  - ouvrir le ROM patche,
  - ouvrir le Lua Console et charger `connector_bizhawk_generic.lua`,
  - se connecter au serveur SekaiLink (adresse:port).

### Wario Land 4 (wl4)
- Installation: BizHawk 2.3.1+ (2.9.1 recommande) + ROM GBA. MultiworldGG pour patcher le `.apwl4` et produire un ROM.
- Execution: ouvrir le ROM patche dans `EmuHawk.exe`.
- ROM client: Oui (ROM vanilla pour patcher, ROM patche pour jouer).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua` (charge via Lua Console si auto-connexion ne demarre pas).
- Connexion serveur: via client MultiworldGG (adresse:port, slot), Lua relie EmuHawk au client.
- SekaiLink: reutiliser le connecteur BizHawk generique. Ajouter un assistant "Wario Land 4" qui verifie BizHawk + ROM, lance EmuHawk et charge le Lua.

### Banjo-Tooie (banjo_tooie)
- Installation: BizHawk 2.10+ + ROM US. Option Everdrive (USB) supportee via connecteur Everdrive.
- Execution (BizHawk): client Banjo-Tooie via MultiworldGG, patch ROM auto, lancer BizHawk.
- ROM client: Oui (ROM vanilla pour patcher, ROM patche pour jouer).
- ROM serveur: Non.
- Lua: `data/lua/connector_banjo_tooie_bizhawk.lua` (charge via autostart ou drag/drop).
- Connexion serveur: via client Banjo-Tooie (adresse:port, slot).
- SekaiLink: fournir un mode BizHawk (auto-lua + autostart) et un mode Everdrive (lance `banjo_tooie_everdrive_connector`).

### Diddy Kong Racing (diddy_kong_racing)
- Installation: BizHawk 2.10 uniquement + ROM US v1.0. Client DKR via MultiworldGG.
- Execution: client DKR patch le ROM, puis ouvrir le ROM patche dans BizHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Non.
- Lua: `data/lua/connector_diddy_kong_racing.lua`.
- Connexion serveur: via client DKR (adresse:port), puis Lua relie BizHawk au client.
- SekaiLink: ajouter un preset BizHawk N64 + auto-run du Lua specifique.

### Final Fantasy Tactics Advance (ffta)
- Installation: BizHawk 2.7+ + ROM GBA. Patch `.apffta` via MultiworldGG.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: reutiliser BizHawk generic + flux "Open Patch".

### Golden Sun: The Lost Age (gstla)
- Installation: BizHawk 2.7+ + ROM GBA EN. Patch `.apgstla`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: idem FFTA (BizHawk generic).

### Yu-Gi-Oh! Forbidden Memories (fm)
- Installation: BizHawk 2.7+ + ISO/BIN NTSC. Pas de patch requis. `fm.apworld` si non bundle.
- Execution: ouvrir l'ISO dans EmuHawk, faire un "New Game" et choisir un starter deck avant de se connecter.
- ROM client: Oui (ISO).
- ROM serveur: Non.
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: fournir un rappel "starter deck avant connect" et flux BizHawk generic.

### Pokemon Red/Blue (pokemon_rb)
- Installation: BizHawk 2.3.1+ (2.9.1 recommande) + ROM Red/Blue. Patch `.apred`/`.apblue`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: reutiliser BizHawk generic; rappels sur `game_version`.

### Castlevania: Symphony of the Night (sotn)
- Installation: BizHawk 2.7/2.8/2.9.1 + ROM PS1 (BIN/CUE). Patch `.apsotn` local (apworld non supporte).
- Execution: Open Patch dans MultiworldGG (demande Track 1/Track 2), puis lancer le `.cue` patche.
- ROM client: Oui (BIN/CUE + Track 2).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux "Open Patch" + selection Track1/Track2 + Lua generic.

### Mega Man 2 (mm2)
- Installation: BizHawk 2.7+ + ROM EN (ou Mega Man Legacy Collection via `Proteus.exe`). Patch `.apmm2`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM/`Proteus.exe` + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic + option "Proteus.exe" si Legacy Collection.

### Mega Man 3 (mm3)
- Installation: BizHawk 2.7+ + ROM EN (ou Mega Man Legacy Collection via `Proteus.exe`). Patch `.apmm3`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM/`Proteus.exe` + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic + option "Proteus.exe" si Legacy Collection.

### The Minish Cap (tmc)
- Installation: BizHawk 2.7+ + ROM EU. Patch `.aptmc`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Pokemon Crystal (pokemon_crystal)
- Installation: BizHawk 2.7+ ou mGBA 0.10.3+ + ROM Crystal v1.0/1.1. Patch `.apcrystal`. mGBA requiert `connector_bizhawkclient_mgba.lua`.
- Execution: ouvrir le ROM patche dans BizHawk ou mGBA.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: BizHawk `connector_bizhawk_generic.lua`; mGBA `connector_bizhawkclient_mgba.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: proposer choix BizHawk/mGBA et charger le bon script.

### Super Mario Land 2: 6 Golden Coins (marioland2)
- Installation: BizHawk 2.9.1+ + ROM GB v1.0. Patch `.apsml2`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Pokemon Emerald (pokemon_emerald)
- Installation: BizHawk 2.7+ + ROM GBA EN. Patch `.apemerald`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- PopTracker pack: `https://github.com/seto10987/Archipelago-Emerald-AP-Tracker`.
- SekaiLink: flux BizHawk generic + telechargement/installation auto du pack PopTracker.

### Metroid Zero Mission (mzm)
- Installation: BizHawk 2.3.1+ + ROM US. Patch `.apmzm`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Castlevania 64 (cv64)
- Installation: BizHawk 2.7+ + ROM US 1.0. Patch `.apcv64`.
- Execution: ouvrir le ROM patche dans EmuHawk; activer Controller Pak (Memory Card).
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic + toggle Controller Pak.

### Pokemon FireRed/LeafGreen (pokemon_frlg)
- Installation: BizHawk + ROM FR/LG (hashs supportes) + apworld. Patch `.apfirered`/`.apleafgreen`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- PopTracker pack: `https://github.com/vyneras/pokemon-frlg-tracker`.
- SekaiLink: flux BizHawk generic + telechargement/installation auto du pack PopTracker.

### Pokemon Mystery Dungeon: Explorers of Sky (pmd_eos)
- Installation: BizHawk 2.10 + ROM NDS EU. Patch `.apeos`.
- Execution: ouvrir le patch pour generer ROM + lancer BizHawk, puis charger `connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic + rappel "Save RAM Flush".

### Castlevania: Dawn of Sorrow (cv_dos)
- Installation: BizHawk 2.10+ + ROM US NDS. Patch `.apcvdos`.
- Execution: ouvrir le patch pour generer ROM + lancer BizHawk, charger `connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Castlevania: Circle of the Moon (cvcotm)
- Installation: BizHawk 2.7+ + ROM GBA US. Patch `.apcvcotm`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Ape Escape (apeescape)
- Installation: BizHawk 2.7-2.9.1 + ISO/CUE PS1 US. NymaShock core requis. Patch via MultiworldGG.
- Execution: ouvrir ISO/CUE dans EmuHawk, charger `connector_bizhawk_generic.lua`.
- ROM client: Oui (ISO/CUE).
- ROM serveur: Non.
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic + preset NymaShock + Dual Analog.

### Mario Kart 64 (mk64)
- Installation: BizHawk + patch `.apmk64`.
- Execution: ouvrir le patch via MultiworldGG, puis connecter le BizHawk Client.
- ROM client: Oui (ROM via patch).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Mega Man Battle Network 3 (mmbn3)
- Installation: BizHawk + ROM GBA US (Legacy Collection possible). Patch `.apbn3`.
- Execution: ouvrir patch via MultiworldGG, lancer BizHawk, charger `connector_mmbn3.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_mmbn3.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk + lua specifique mmbn3.

### Ocarina of Time (oot)
- Installation: BizHawk 2.3.1+ + ROM v1.0. Patch `.apz5`.
- Execution: ouvrir patch, lancer BizHawk, charger `connector_oot.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_oot.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk + lua specifique oot; cote serveur, exiger ROM v1.0 pour generer.

### Adventure (adventure)
- Installation: BizHawk 2.3.1+ + ROM NTSC. Patch `.apadvn`.
- Execution: lancer AdventureClient, patch ROM, puis EmuHawk avec `connector_adventure.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_adventure.lua`.
- Connexion serveur: via AdventureClient (adresse:port).
- SekaiLink: flux BizHawk avec script specifique + option `rom_args` dans `host.yaml`.

### Mario & Luigi: Superstar Saga (mlss)
- Installation: BizHawk 2.9.1+ + ROM US. Patch `.apmlss`.
- Execution: ouvrir le patch pour generer ROM + lancer BizHawk, charger `connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Kingdom Hearts II (kh2)
- Installation: OpenKH Mod Manager + Panacea + Lua Backend + mods APCompanion + KH2-ArchipelagoEnablers + seed zip.
- Execution: lancer le jeu via OpenKH, entrer GoA, puis lancer le client KH2 depuis MultiworldGG.
- ROM client: Non (PC port).
- ROM serveur: Non.
- Lua: Non (Lua backend OpenKH).
- Connexion serveur: via KH2 Client (adresse:port).
- SekaiLink: wrapper OpenKH + verifier ordre des mods + lancer client KH2.

### Paper Mario (papermario)
- Installation: BizHawk 2.7-2.9.1 + ROM US + apworld. Patch `.appm64`.
- Execution: ouvrir `.appm64` via MultiworldGG, generer `.z64`, lancer BizHawk + Lua generic.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Faxanadu (faxanadu)
- Installation: Daxanadu (standalone) + ROM US.
- Execution: lancer `Daxanadu.exe`, menu ARCHIPELAGO, entrer host/slot/pw.
- ROM client: Oui (ROM).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-app.
- SekaiLink: launcher simple + presets host/port/slot.

### Adventure (adventure)
- Installation: BizHawk 2.3.1+ + ROM NTSC. Patch `.apadvn`.
- Execution: lancer AdventureClient, patch ROM, puis EmuHawk avec `connector_adventure.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_adventure.lua`.
- Connexion serveur: via AdventureClient (adresse:port).
- SekaiLink: flux BizHawk avec script specifique + option `rom_args` dans `host.yaml`.

### Mario & Luigi: Superstar Saga (mlss)
- Installation: BizHawk 2.9.1+ + ROM US. Patch `.apmlss`.
- Execution: ouvrir le patch pour generer ROM + lancer BizHawk, charger `connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Zelda II: The Adventure of Link (zelda2)
- Installation: BizHawk 2.3.1+ + ROM US. Patch `.apz2`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Wario Land (wl)
- Installation: BizHawk 2.9.x + ROM GB (World). Patch `.apwl`.
- Execution: ouvrir le ROM patche dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Star Fox 64 (star_fox_64)
- Installation: BizHawk 2.10+ ou Project64 4.0 ou EverDrive64 + ROM Star Fox 64 v1.1 (MD5 specifique). `star_fox_64.apworld` + `connector_sf64_bizhawk.lua` (si BizHawk).
- Execution: ouvrir `Star Fox 64 Client`, fournir le ROM v1.1 pour patch, charger le ROM patche dans l'emu, puis connecter le client au serveur.
- ROM client: Oui (ROM v1.1 + ROM patche).
- ROM serveur: Non.
- Lua: `data/lua/connector_sf64_bizhawk.lua` (BizHawk) ou script PJ64/ED64 selon plateforme.
- Connexion serveur: via client Star Fox 64 (adresse:port).
- SekaiLink: flux BizHawk/PJ64/EverDrive avec auto-start; integrer le client SF64.

### Yu-Gi-Oh! 2006 (yugioh06)
- Installation: BizHawk 2.7+ + client BizHawk MWGG + ROM GBA US/EU.
- Execution: ouvrir `.apygo06` pour patcher, lancer EmuHawk, charger `data/lua/connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port).
- SekaiLink: flux BizHawk generic + note config Lua Core `Lua+LuaInterface`.

### Yu-Gi-Oh! Dungeon Dice Monsters (yugiohddm)
- Installation: BizHawk 2.7+ + ROM GBA + MWGG 0.7.50+ (ou apworld). Nouvelle save requise par seed.
- Execution: ouvrir patch (Open Patch), lancer EmuHawk, charger `data/lua/connector_bizhawk_generic.lua`, puis connecter.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port).
- SekaiLink: flux BizHawk generic + note "creer nouvelle save avant connexion".

### Final Fantasy (ff1)
- Installation: BizHawk + MultiworldGG (client BizHawk) + ROM US `Final Fantasy (USA).nes`.
- Execution: generer ROM + YAML via finalfantasyrandomizer.com (option Archipelago activee), lancer `MultiworldGGBizhawkClient.exe`, puis ouvrir le ROM dans EmuHawk et charger `data/lua/connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Non.
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port/slot) puis Lua vers EmuHawk.
- SekaiLink: assistant BizHawk standard + rappel "generer ROM+YAML sur le site FF1 Randomizer".

## Mods

### Hatsune Miku Project DIVA Mega Mix+ (megamix)
- Installation: DivaModLoader + Archipelago Mod (nom de dossier `mods/ArchipelagoMod`). Jeu Steam.
- Execution: lancer le "Mega Mix Client" (launcher MultiworldGG), selectionner `DivaMegaMix.exe` si demande.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Mega Mix (MultiworldGG) qui parle au serveur.
- SekaiLink: integrer un client "Mega Mix" qui:
  - verifie la presence de `DivaMegaMix.exe`, `dinput8.dll`, `mods/ArchipelagoMod`,
  - lance le jeu avec mod loader,
  - expose une UI simple de connexion (adresse:port, slot).

### Trackmania (trackmania)
- Installation: Trackmania (2020) Club Access ou Trackmania 2 (Canyon/Stadium/Valley/Lagoon) + Openplanet + plugin Archipelago.
- Execution: lancer le client Trackmania (MultiworldGG) et connecter, puis dans Openplanet `Scripts > Archipelago` et Connect.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Trackmania + plugin Openplanet.
- SekaiLink: wrapper client Trackmania + check Openplanet + preset host/port/slot.

### Wargroove (wargroove)
- Installation: Wargroove + DLC Double Trouble (Steam Windows/Linux). MultiworldGG + client Wargroove.
- Execution: lancer `Wargroove Client`, connecter, puis dans Wargroove `Story > Campaign > Custom > Archipelago`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Wargroove.
- SekaiLink: wrapper client + note "backup playerProgress" + config `host.yaml` root_directory.

### Wargroove 2 (wargroove2)
- Installation: Wargroove 2 (Steam Windows/Linux) + client Wargroove 2 (MWGG).
- Execution: lancer `Wargroove 2 Client`, connecter, puis `Story > Campaign > Custom > Archipelago 2`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Wargroove 2.
- SekaiLink: wrapper client + note "supprimer progression campagne avant maj".

### Balatro (balatro)
- Installation: Steamodded (Alpha) + Lovely injector + `lua-apclientpp.dll` (depuis `lua51.7z`) dans le dossier du jeu. Mod `BalatroAP` dans `%AppData%/Roaming/Balatro/Mods/BalatroAP`.
- Execution: lancer Balatro normalement via Steam.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non (DLL apclient).
- Connexion serveur: selection du profil "Archipelago" dans le jeu.
- SekaiLink: fournir un verificateur d'installation (presence de `version.dll`/Lovely, Steamodded Alpha, `lua-apclientpp.dll`, mod `BalatroAP`), puis laisser l'utilisateur lancer le jeu. Option: bouton "Ouvrir le profil Archipelago".

### The Messenger (messenger)
- Installation: Courier Mod Loader + TheMessengerRandomizerModAP dans `TheMessenger/Mods/`. Option auto via MultiworldGG Launcher.
- Execution: lancer via launcher (auto) ou manuel.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: auto depuis la page room (URI), ou manuel via menu `Options > Archipelago Options`.
- SekaiLink: proposer un mode "Auto-connect" qui ouvre le jeu avec arguments d'URI; sinon UI pour remplir host/port/slot et ecrire `APConfig.toml` si besoin.

### CrossCode (crosscode)
- Installation: CCLoader2 + mod "Multiworld randomizer" installe depuis le menu mods dans le jeu. Option PopTracker pack.
- Execution: lancer CrossCode, activer le mod, redemarrer.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via "Archipelago Start" ou `Pause > Archipelago Options` (in-game).
- SekaiLink: integration "in-game config" avec rappels UI; pas de connecteur externe requis.

### HITMAN World of Assassination (hitman_woa)
- Installation: Peacock (serveur local) + `archipelago.plugin.js` dans `Peacock/plugins`. APworld installe dans MultiworldGG. Jeu Steam/Epic.
- Execution: lancer Peacock (`Start Server.cmd`) + `PeacockPatcher.exe`, puis client MultiworldGG "HITMAN WOA Client", puis le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client MultiworldGG (adresse:port, slot). Le jeu lit la progression via Peacock.
- SekaiLink: fournir un "client Hitman" qui:
  - valide l'etat Peacock + plugin,
  - lance Peacock et Patcher,
  - lance le client SekaiLink (equivalent MultiworldGG) pour se connecter au serveur.

### Factorio: Space Age Without Space (factorio_saws)
- Installation: Factorio + mod AP_*.zip (genere par seed) + mod SpaceAgeWithoutSpace. Serveur/Client partagent l'install Factorio standard. SAWS utilise un dossier `MultiworldGG/factorio_saws/mods` pour le serveur.
- Execution (host): lancer client SAWS (MultiworldGG) puis lancer Factorio et se connecter a `localhost`.
- Execution (client): installer mod AP_*.zip dans `%AppData%/Roaming/Factorio/mods` et se connecter a l'host via "Multiplayer > Connect to address".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: le client SAWS se connecte au serveur SekaiLink via `/connect host:port`.
- SekaiLink: exposer un mode "Host" qui:
  - place le mod AP_*.zip dans `factorio_saws/mods`,
  - telecharge/verifie SpaceAgeWithoutSpace,
  - lance le client SAWS et envoie `/connect`.

### Terraria (terraria)
- Installation: Terraria + tModLoader + mod Archipelago via Steam Workshop.
- Execution: lancer tModLoader, activer le mod.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: config in-game du mod (Name/Address/Port), puis ouvrir un monde.
- SekaiLink: fournir un assistant qui:
  - ouvre tModLoader,
  - guide la config du mod (name/host/port),
  - conserve les presets par slot.

### Into the Breach (into_the_breach)
- Installation: ITB ModLoader + dossier `randomizer` (ITB randomizer for AP) dans les mods. Apworld + dependances a copier dans l'install MultiworldGG si on veut l'option `randomize_squads`.
- Execution: lancer le jeu, activer les mods (randomizer + modApiExt + memedit), puis console `makeitso` sur un profil dedie.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client MultiworldGG (ITB Client) et mod in-game.
- SekaiLink: ajouter un check d'integrite ITB (mods actifs, profil AP, commande `makeitso` effectuee) et un client ITB integre pour la connexion.

### DORONKO WANKO (doronko_wanko)
- Installation: mod Archipelago (GitHub) selon README.
- Execution: editer `AP.json` dans le dossier du jeu, puis lancer le jeu et `New Game`/`Continue`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via `AP.json` (Url, SlotName, Password).
- SekaiLink: ecrire/mettre a jour `AP.json` depuis Electron, puis lancer le jeu.

### Satisfactory (satisfactory)
- Installation: Satisfactory + Satisfactory Mod Manager + mod "Archipelago Randomizer".
- Execution: lancer le jeu (mods charges), creer une partie et ouvrir "Mod Savegame Settings".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via "Mod Savegame Settings" (Server URI, User Name, Password).
- SekaiLink: fournir un assistant qui:
  - verifie le mod installe,
  - pre-remplit Server URI/slot/pw,
  - gere les changements de port (room sleep).

### Brotato (brotato)
- Installation: mod Archipelago via Steam Workshop (ou zip pour EGS). Version Xbox/Game Pass non supportee.
- Execution: lancer le jeu; le mod est le client AP.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via le mod (pas de client externe).
- SekaiLink: detecter la presence du mod et exposer un ecran de connexion simple; pas de client separé.

### Bomb Rush Cyberfunk (bomb_rush_cyberfunk)
- Installation: BepInEx 5.4.22 + ModLocalizer + mod Archipelago (BRC_Archipelago) dans `BepInEx/plugins`.
- Execution: lancer le jeu; connecter depuis le menu des sauvegardes (bouton MultiworldGG).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/port/slot/pw).
- SekaiLink: proposer un verificateur BepInEx + plugins, puis lancer le jeu; UI simple de connexion.

### ULTRAKILL (ultrakill)
- Installation: mod Archipelago via r2modman ou manuel + PluginConfigurator. `ultrakill.apworld` si non bundle.
- Execution: lancer le jeu, ouvrir Plugin Config, entrer host/port/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via Plugin Config (ou console F8).
- SekaiLink: fournir un guide de configuration PluginConfigurator et un assistant de connexion.

### Choo-Choo Charles (cccharles)
- Installation: copier dossier `Obscure/` dans le dossier du jeu (mod). `cccharles.apworld` pour host.
- Execution: lancer le jeu; ouvrir console (F10 ou `) et faire `/connect host:port slot`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: offrir un ecran "Connect" qui copie la commande `/connect` ou l'injecte si possible.

### Super Mario Odyssey (smo)
- Installation: pack `SMO_Archipelago` (Switch ou Emu). Mods pour Ryujinx/Suyu. Tres early/beta.
- Execution: lancer le jeu modde. Pour le ROM modde, copier `romfs` dans le dossier `SMOAP`.
- ROM client: Oui (ROM du jeu pour emu, ou cartouche/sd sur Switch).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via un "Connector" externe; le jeu se connecte au connector sur IP locale, port 1027 par defaut.
- SekaiLink: integrer le connector dans Electron (ecoute locale), fournir l'IP locale et pousser l'info host/port/slot vers le connector.

### Ori and the Will of the Wisps (ori_wotw)
- Installation: WotW randomizer standalone (version > 4.31.0 / 4.38.0+ pour AP World).
- Execution: ouvrir le launcher randomizer, `Play Archipelago`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: depuis le launcher (secure connection pour multiworld.gg).
- SekaiLink: integrer un wrapper qui lance le client randomizer et pre-remplit host/port/slot; afficher note "secure connection".

### shapez (shapez)
- Installation: mod `shapezipelago@X.X.X.js` via mod.io (copie dans dossier mods).
- Execution: lancer le jeu, connecter dans le menu principal.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (slot/host/port/pw).
- SekaiLink: afficher un ecran de connexion rapide et valider presence du mod.

### The Sims 4 (sims4)
- Installation: `sims4.apworld` + mod Sims 4 Archipelago + Sims4CommunityLibrary (fichiers `.ts4script` et `.package`) dans `Documents/Electronic Arts/The Sims 4/Mods`.
- Execution: lancer client Sims 4 (MultiworldGG) et se connecter; lancer le jeu (nouvelle sauvegarde).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Sims 4 (adresse:port, slot).
- SekaiLink: fournir un client Sims 4 integre + verif "Script Mods Allowed"; rappeler de desactiver les packs.

### Cuphead (cuphead)
- Installation: BepInEx 5.x + CupheadArchipelagoMod (dans `BepInEx/plugins`).
- Execution: lancer le jeu (mods charges).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via le mod (details sur le repo).
- SekaiLink: verif BepInEx + plugin, puis lancer le jeu; UI de connexion si exposee.

### Inscryption (inscryption_beta)
- Installation: r2modman/Thunderstore (recommande) ou BepInEx + ArchipelagoMod manuellement.
- Execution: lancer "Start Modded", puis `New Game` et entrer host/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via menu ArchipelagoMod.
- SekaiLink: proposer un flow r2modman + detection "mods charges" (console visible).

### Risk of Rain 2 (ror2)
- Installation: r2modman + mod Archipelago.
- Execution: lancer via r2modman "Start modded".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (fields AP) ou commandes `archipelago_connect`.
- SekaiLink: fournir un profil r2modman et un ecran de connexion in-game.

### The Binding of Isaac: Repentance (tboir)
- Installation: Mod Config Menu Pure + AP Mod (dans `mods/`). Ajouter `--luadebug` aux options de lancement.
- Execution: lancer le jeu, ouvrir Mod Config Menu (L), configurer AP Integration.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via Mod Config Menu.
- SekaiLink: helper pour ajouter `--luadebug` + verif des mods.

### TUNIC (tunic)
- Installation: BepInEx 6.0.0-pre.1 (IL2CPP x64) + Tunic Randomizer Mod.
- Execution: lancer le jeu, choisir `Randomizer Mode > Archipelago`, configurer AP.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via "Edit AP Config".
- SekaiLink: verif BepInEx + plugin + preset host/port/slot.

### Hollow Knight (hk)
- Installation: Lumafly Mod Manager + mod Archipelago (pas de BepInEx).
- Execution: lancer le jeu, nouvelle sauvegarde, mode Archipelago.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via mode Archipelago.
- SekaiLink: integration avec Lumafly (installation mod) + presets host/port/slot.

### Ori and the Blind Forest (oribf)
- Installation: BepInEx x86 + OriBFArchipelago (dans `BepInEx/plugins`). Modifier `BepInEx.cfg` Type=Camera.
- Execution: lancer le jeu, remplir les champs AP en haut a gauche.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/port/slot/pw).
- SekaiLink: verif BepInEx x86 + mod + patch config `Type=Camera`.

### Getting Over It (getting_over_it)
- Installation: BepInEx 5.4.23.2 + Checking Over It (copier dans game root).
- Execution: edit `BepInEx/config/BlastSlimey.CheckingOverIt.cfg`, puis lancer le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via fichier cfg.
- SekaiLink: ecrire le cfg avant lancement, option console BepInEx.

### Resident Evil 3 Remake (residentevil3remake)
- Installation: REFramework + RE3R_AP_Client (copie dans dossier du jeu).
- Execution: lancer le jeu, utiliser fenetre "Archipelago client for REFramework".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/slot/pw).
- SekaiLink: check d'installation + UI de connexion.

### HuniePop (huniepop)
- Installation: plugin Archipelago + VC++ Redistributable x86. Copier le zip plugin dans le dossier du jeu.
- Execution: lancer le jeu (connecte via plugin).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via le plugin.
- SekaiLink: verif deps (VC++ x86) + plugin.

### HuniePop 2 (huniepop2)
- Installation: BepInEx v5 64-bit + plugin AP HuniePop2.
- Execution: lancer le jeu et connecter via plugin.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via plugin.
- SekaiLink: verif BepInEx + plugin.

### Hylics 2 (hylics2)
- Installation: BepInEx 5 (32-bit) + mod ArchipelagoHylics2 dans `BepInEx/plugins`.
- Execution: lancer le jeu, console in-game `/connect host:port name [password]`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: helper pour executer la commande /connect.

### Jak and Daxter: The Precursor Legacy (jakanddaxter)
- Installation: OpenGOAL Launcher + mod ArchipelaGOAL + compile (OpenGOAL).
- Execution: lancer le client Jak and Daxter via MultiworldGG; compiler + REPL ouverts; connecter via Text Client.
- ROM client: Non (OpenGOAL).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via Text Client (adresse:port) une fois le titre affiche.
- SekaiLink: wrapper pour lancer client + verifier compilation; garder fenetres REPL/Compiler ouvertes.

### Timespinner (timespinner)
- Installation: TsRandomizer (zip) dans le dossier du jeu.
- Execution: lancer TsRandomizer, choisir seed `<< Archipelago >>` et se connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-app.
- SekaiLink: lancer TsRandomizer et pre-remplir host/slot/pw.

### Dark Souls II (dark_souls_2)
- Installation: dinput8.dll + apworld selon version. Sous Linux: `WINEDLLOVERRIDES="dinput8.dll=n,b"`.
- Execution: lancer le jeu, console s'ouvre; `/connect server:port slot [password]`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: helper pour lancer + injecter commande /connect.

### Sonic Heroes (sonic_heroes)
- Installation: Reloaded-II Mod Loader + mod MultiworldGG.
- Execution: configurer host/port/slot dans Reloaded, lancer via Reloaded.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via mod (log).
- SekaiLink: integrer le profil Reloaded-II et la config du mod.

### Noita (noita)
- Installation: mod Archipelago (zip) dans `mods/archipelago`, activer "Unsafe mods".
- Execution: configurer server/port/slot/pw dans Mod Settings, puis new run.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via mod.
- SekaiLink: verif mod + activer Unsafe mods + preset host/port/slot.

### Flipwitch (flipwitch)
- Installation: client Flipwitch (zip) dans le dossier du jeu. Sous Linux: `WINEDLLOVERRIDES="winhttp.dll=n,b"`.
- Execution: lancer le jeu (client integre).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Flipwitch.
- SekaiLink: verif install + injecter overrides Linux si besoin.

### Saving Princess (saving_princess)
- Installation: via MultiworldGG Launcher (auto) ou manuel (patch bsdiff + dll). 
- Execution: lancer "Saving Princess Client" (auto-connect) ou config in-game via menu Archipelago.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: auto depuis room ou manuel in-game.
- SekaiLink: preferer installation auto, garder preset connection persistant.

### Lingo (lingo)
- Installation: mod Archipelago via Steam Workshop (ou manuel).
- Execution: Settings > Level > Archipelago, puis New Game et connect.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/slot/pw).
- SekaiLink: ecran de connexion + rappel "restart pour revenir vanilla".

### DLC Quest (dlcquest)
- Installation: DLCQuestipelago (installer) + BepInEx inclus. Config via `ArchipelagoConnectionInfo.json`.
- Execution: lancer `BepInEx.NET.Framework.Launcher.exe`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via JSON.
- SekaiLink: ecrire le JSON avant lancement.

### Momodora: Moonlit Farewell (momodoramoonlitfarewell)
- Installation: MelonLoader 0.5.7 + mod MomodoraMFRandomizer (dans `Mods`).
- Execution: editer `config.json`, puis lancer le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via config.json.
- SekaiLink: ecrire config + rappel backup saves.

### Star Wars Episode I Racer (swr)
- Installation: Ultimate ASI Loader (dinput/dsound) + scripts folder du client.
- Execution: lancer le jeu, fenetre pop-up de connexion AP.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via pop-up (host/slot/pw).
- SekaiLink: verif ASI + scripts folder, puis lancer le jeu.

### Chained Echoes (chainedechoes)
- Installation: BepInEx 6.0.0-pre.2+ (IL2CPP) + fichiers du mod (txt + dll).
- Execution: editer `RandomizerOptions.txt` (host/slot/pw), puis lancer le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via `RandomizerOptions.txt`.
- SekaiLink: ecrire le fichier d'options avant lancement.

### Crystal Project (crystal_project)
- Installation: version Steam "archipelago branch" + .NET 8 Desktop Runtime + installer AP World.
- Execution: lancer le jeu, ecran AP in-game pour host/port/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via ecran AP.
- SekaiLink: guider le changement de branche Steam + lancer l'installer AP.

### Tetris Attack (tetrisattack)
- Installation: MultiworldGG + ROM SNES + emu SNI (snes9x-rr/BizHawk/RetroArch). Patch `.aptatk`.
- Execution: ouvrir patch pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### A Short Hike (shorthike)
- Installation: A Short Hike Randomizer + dependances (via readme).
- Execution: lancer le jeu; popup de connexion a l'ouverture/continue.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via popup.
- SekaiLink: simple ecran de connexion (host/port/slot/pw) et lancement du jeu.

### Rabi-Ribi (rabi_ribi)
- Installation: MultiworldGG + mettre `game_installation_path` dans `host.yaml`.
- Execution: lancer le client Rabi-Ribi puis le jeu; choisir la map Archipelago (F5).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Rabi-Ribi (adresse:port, slot).
- SekaiLink: ecrire `host.yaml` et lancer client + jeu.

### PowerWash Simulator (powerwashsimulator)
- Installation: BepInEx 6.0.0-pre1 + mod `SW_CreeperKing.ArchipelagoMod` dans `BepInEx/plugins`.
- Execution: lancer le jeu; champs de connexion sur l'ecran titre.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via champs AP.
- SekaiLink: verif BepInEx + mod; preset host/port/slot.

### A Hat in Time (ahit)
- Installation: Steam beta `tcplink` + mod Workshop Archipelago. Activer la console dev.
- Execution: lancer client AHIT via MultiworldGG, puis creer une sauvegarde dans le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client AHIT (adresse:port).
- SekaiLink: wrapper pour basculer vers beta `tcplink`, puis lancer client + jeu.

### Don't Starve Together (dontstarvetogether)
- Installation: mod Workshop Archipelago + MultiworldGG.
- Execution: lancer client DST, puis `Host Game` et activer le mod (Server Mods).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client DST (adresse:port).
- SekaiLink: assistant "Host World" avec rappel des settings (Local Save, seasons, caves).

### Resident Evil 2 Remake (residentevil2remake)
- Installation: REFramework + RE2R_AP_Client (fichiers a copier dans le dossier du jeu).
- Execution: lancer le jeu, utiliser la fenetre "Archipelago client for REFramework" in-game.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/slot/pw).
- SekaiLink: check d'installation REFramework + UI de connexion.

### Against the Storm (against_the_storm)
- Installation: Thunderstore Mod Manager + mod Archipelago ATS.
- Execution: lancer modde, ouvrir console ` et `ap.connect host:port "slot" [password]`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: wrapper Thunderstore + helper pour commande /connect.

### Blasphemous (blasphemous)
- Installation: Mod Installer + mod Multiworld (plus mods optionnels).
- Execution: choisir une save et entrer host/slot/pw dans le menu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game.
- SekaiLink: wrapper Mod Installer + preset host/port/slot.

### Monster Sanctuary (monster_sanctuary)
- Installation: Archipelago Mod (BepInEx) dans le dossier du jeu.
- Execution: lancer le jeu; UI AP en haut a gauche.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game.
- SekaiLink: verif mod BepInEx + preset host/port/slot.

### PEAK (peak)
- Installation: BepInEx 5.x + Peakpelago mod.
- Execution: lancer le jeu; UI de connexion en haut a gauche.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game.
- SekaiLink: verif BepInEx + mod + preset host/port/slot (note: host seul en multi).

### Phoenotopia: Awakening (phoa)
- Installation: BepInEx x86 + PhoA AP Mod (dlls dans `BepInEx/plugins`).
- Execution: editer `PhoA_AP_client.cfg`, puis lancer le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via cfg.
- SekaiLink: ecrire le cfg avant lancement.

### Pseudoregalia (pseudoregalia)
- Installation: mod pseudoregalia-archipelago (copie dossier mod).
- Execution: lancer le jeu, F10 console, `/connect ip:port slot password`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: helper /connect + launcher.

### Raft (raft)
- Installation: Raft Mod Loader + ModUtils + Raftipelago.
- Execution: lancer via RML, F10 console `/connect server slot [password]`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: console in-game.
- SekaiLink: wrapper RML + helper /connect.

### RimWorld (rimworld)
- Installation: Harmony + ArchipelagoRimworld (mods) + apworld.
- Execution: menu principal > Connect to Archipelago.
- ROM client: Non.
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: in-game via menu.
- SekaiLink: helper d'installation mod + preset host/port/slot.

### Rift of the Necrodancer (rotn)
- Installation: RiftArchipelago (zip) + BepInEx (si besoin).
- Execution: lancer le jeu; fenetre de connexion sur l'ecran titre.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via UI.
- SekaiLink: verif mod + preset host/port/slot.

### Lunacid (lunacid)
- Installation: mod Lunacid AP + apworld (drop dans `custom_worlds`). Sous Linux: `WINEDLLOVERRIDES="winhttp.dll=n,b"`.
- Execution: creer une nouvelle sauvegarde, connexion en debut de creation.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/port/slot).
- SekaiLink: verif mod + apworld, preset host/port/slot.

### Placid Plastic Duck Simulator (placidplasticducksim)
- Installation: BepInEx 6 IL2CPP (build be.735) + mod Archipelago (SW_CreeperKing).
- Execution: lancer le jeu; champs de connexion sur l'ecran titre.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via champs AP.
- SekaiLink: verif BepInEx + mod, preset host/port/slot.

### DREDGE (dredge)
- Installation: Dredge Mod Manager + mods Winch + Archipelago Dredge.
- Execution: lancer via Mod Manager, puis se connecter (terminal, menu mods ou pop-up F7).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (ap connect).
- SekaiLink: wrapper pour Mod Manager + preset host/port/slot.

### Overcooked! 2 (overcooked2)
- Installation: OC2-Modding (installeur) + BepInEx auto.
- Execution: lancer le jeu; login AP avant d'acceder au menu principal.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: ecran de login in-game.
- SekaiLink: wrapper oc2-modding + preset host/port/slot.

### Muse Dash (musedash)
- Installation: MelonLoader 0.7.0 + mod Archipelago. .NET Framework 4.8 et .NET Desktop Runtime 6.0 si besoin.
- Execution: lancer le jeu, bouton AP en bas a droite pour se connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via pop-up.
- SekaiLink: verif MelonLoader + mod + deps .NET.

### OSRS (osrs)
- Installation: RuneLite + plugins Archipelago + Region Locker (Plugin Hub).
- Execution: se connecter via panel Archipelago dans RuneLite.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via plugin RuneLite.
- SekaiLink: assistant d'installation plugin + preset host/port/slot.

### Dark Souls III (dark_souls_3)
- Installation: DS3 AP Client + DS3 1.15.2. Generer data via `DS3Randomizer.exe`.
- Execution: lancer `launchmod_darksouls3.bat` (DS3 en mode offline).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client/console du mod.
- SekaiLink: wrapper client + checks version jeu + flow "Load" data.

### Lethal Company (lethal_company)
- Installation: mod APLC via Thunderstore/r2modman + yaml + apworld.
- Execution: lancer modde, puis `/connect multiworld.gg:port` dans le chat (hote).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: chat in-game via /connect.
- SekaiLink: wrapper r2modman + helper /connect + gestion YAML.

### Cat Quest (cat_quest)
- Installation: BepInEx 5 x86 + Cat Quest Randomizer (zip) dans le dossier du jeu.
- Execution: editer `ArchipelagoRandomizer/SaveData/RoomInfo.json`, puis lancer le jeu et creer une nouvelle partie AP.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via `RoomInfo.json`.
- SekaiLink: ecrire JSON avant lancement.

### Peaks of Yore (peaks_of_yore)
- Installation: r2modman + mod PeaksOfArchipelago (downgrade requis).
- Execution: lancer modde, menu Mods > config AP (hostname/port/slot).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via config mod.
- SekaiLink: wrapper r2modman + note "downgrade requis".

### Risk of Rain (ror1)
- Installation: Rainfusion Mod Loader + RoR_AP_Mod.
- Execution: ouvrir RoRML Launcher, configurer host/slot/pw dans les options du mod.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via options du mod.
- SekaiLink: wrapper RoRML + preset host/port/slot.

### Slay the Spire (spire)
- Installation: Steam Workshop (ModTheSpire + BaseMod + Archipelago Multiworld Randomizer). GOG/GamePass: SteamCMD + copier `.jar` + `start.bat`.
- Execution: lancer le jeu avec mods, activer BaseMod + Archipelago (Downfall/StSLib optionnels), puis menu Archipelago in-game pour host:port + slot.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via menu Archipelago.
- SekaiLink: assistant d'installation mods (Steam/SteamCMD) + preset host/port/slot.

### Celeste (celeste)
- Installation: Celeste + Olympus/Everest + mod `CelesteArchipelago.zip` dans `Mods` (APWorld si non bundle). MultiworldGG optionnel pour Text Client.
- Execution: lancer Celeste via Everest, bouton "Archipelago" au menu, entrer name/server/port/password et connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via menu Archipelago.
- SekaiLink: verifier Everest + mod; fournir UI de connexion. Option: bouton pour ouvrir `MultiworldGGTextClient.exe` si besoin de commandes.

### Celeste (Open World) (celeste_open_world)
- Installation: Celeste v1.4 + Olympus/Everest + mod `Archipelago_Open_World.zip` dans `mods`.
- Execution: lancer Everest, bouton `Connect` au menu, entrer host/port/slot/pw. (Optionnel) activer `Debug Mode` pour Text Client (`~`).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via menu Connect.
- SekaiLink: verifier Everest + mod; fournir UI de connexion et option "Debug Mode".

### Frogmonster (frogmonster)
- Installation: jeu Steam + activer la branche beta "randomizer" (version titre contient `RANDOMIZER`).
- Execution: lancer, ouvrir un save, puis dans la console utiliser `/connect host:port slot` (password si requis).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via commande console `/connect`.
- SekaiLink: generer la commande `/connect` et verifier que la branche randomizer est activee.

### Sonic Adventure 2: Battle (sa2b)
- Installation: SA Mod Manager + mod SA2B_Archipelago + .NET Desktop Runtime 8.0.
- Execution: configurer AP dans SAModManager, `Save & Play` (ou Steam direct sur Linux).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via mod (config SAModManager).
- SekaiLink: assistant SAModManager + preset host/slot/pw.

### Sonic Adventure DX (sadx)
- Installation: SA Mod Manager + mod SADX_Archipelago + apworld.
- Execution: configurer AP dans SAModManager, `Save & Play`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via mod (config SAModManager).
- SekaiLink: assistant SAModManager + preset host/slot/pw.

### The Simpsons: Hit & Run (simpsonshitnrun)
- Installation: Lucas' Mod Manager + SHARRandomizer + apworld + .NET 8 x86.
- Execution: lancer le client SHAR, charger `.apshar`, puis lancer le jeu via LMM.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client SHAR.
- SekaiLink: wrapper LMM + client SHAR + verif .NET x86.

### Stardew Valley (stardew_valley)
- Installation: SMAPI + mod StardewArchipelago (7.x.x).
- Execution: lancer via SMAPI, entrer host/port/slot lors de la creation de la ferme.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via champs AP.
- SekaiLink: helper SMAPI + preset host/port/slot.

### Subnautica (subnautica)
- Installation: Archipelago mod (BepInEx) dans le dossier du jeu. Linux: `WINEDLLOVERRIDES="winhttp=n,b"`.
- Execution: connecter via formulaire sur l'ecran titre.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via formulaire.
- SekaiLink: verif mod + preset host/port/slot.

### Hades (hades)
- Installation: Hades (Steam) + mods ModImporter, ModUtils, StyxScribe (sans REPL), Polycosmos. `hades.apworld` dans `custom_worlds` (MultiworldGG).
- Execution: lancer `MultiworldGGLauncher.exe`, ouvrir `HadesClient`, selectionner le dossier Hades, puis se connecter avant de choisir la save.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via HadesClient (adresse:port, slot).
- SekaiLink: assistant d'installation des mods + wrapper client Hades + rappel "connecter avant la save".

### Here Comes Niko! (hcniko)
- Installation: BepInEx 5.4.23.3 (Unity Mono x64) + mod NikoArchipelago (`BepInEx/plugins/NikoArchipelago.dll`). Linux: `WINEDLLOVERRIDES="winhttp=n,b"`.
- Execution: menu in-game, cliquer logo Archipelago, entrer host/port/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via menu Archipelago.
- SekaiLink: verifier BepInEx + mod + preset host/port/slot.

### Nine Sols (nine_sols)
- Installation: r2modman + mod "Archipelago Randomizer" (via r2modman). MultiworldGG requis (ou `nine_sols.apworld` + `Nine.Sols.yaml`).
- Execution: lancer via r2modman, `Start Game` puis entrer host/port/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game au lancement d'une nouvelle partie.
- SekaiLink: wrapper r2modman + preset host/port/slot; option d'importer une version de mod specifique.

### Outer Wilds (outer_wilds)
- Installation: Outer Wilds Mod Manager + mod "Archipelago Randomizer".
- Execution: lancer via le Mod Manager, `New Random Expedition`, entrer host/port/slot/pw.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game au lancement d'une nouvelle expedition.
- SekaiLink: wrapper Mod Manager + preset host/port/slot.

### Rogue Legacy (rogue_legacy)
- Installation: Rogue Legacy Randomizer (release GitHub) selon README.
- Execution: in-game, ecran AP (Start), entrer host/port/slot/pw puis Connect.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game via ecran AP.
- SekaiLink: launcher simple + preset host/port/slot; pointer vers README d'install.

### Kindergarten 2 (kindergarten_2)
- Installation: Archipelagarten (BepInEx) dans le dossier du jeu (idealement copie).
- Execution: editer `ArchipelagoConnectionInfo.json`, puis lancer `Kindergarten2.exe`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via JSON.
- SekaiLink: ecrire JSON avant lancement.

## Natif

### ChecksFinder (checksfinder)
- Installation: telecharger le jeu (GitHub releases ou itch.io). Version web possible.
- Execution: lancer le jeu.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: `Play Online` puis entrer host/port/slot/pw dans la console.
- SekaiLink: fournir un ecran de connexion qui pre-remplit host/port/slot et ouvre le jeu; option "copier/paster" ou injection via args si supporte.

### Sea of Thieves (seaofthieves)
- Installation: Sea of Thieves + acces Captain status + client SoT (MultiworldGG). Chaque joueur recoit un `.sotci`.
- Execution: se connecter au site Sea of Thieves, recuperer le header `Cookie` (pas `Set-Cookie`), lancer le client SoT, choisir `.sotci`, puis `/setcookie` et `/setmode` (pirate ou ship), enfin connecter `user@ip:port`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client SoT (auto-tracker), et connexion manuelle `user@ip:port`.
- SekaiLink: flow guide cookie + stockage `cookie.txt`, import `.sotci`, boutons `/setcookie`/`/setmode`, puis connexion.

### The Witness (witness)
- Installation: The Witness (Windows 64-bit) + The Witness Archipelago Randomizer.
- Execution: lancer le jeu et une nouvelle save, puis lancer le randomizer, entrer host/slot/pw et Connect.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via randomizer client.
- SekaiLink: wrapper client + option Text Client MWGG / tracker PopTracker.

### Celeste 64 (celeste64)
- Installation: Archipelago build de Celeste 64 (zip). Linux/Steam Deck: ajouter `Celeste64.exe` a Steam et activer Proton.
- Execution: editer `AP.json` (Url/SlotName/Password), puis lancer `Celeste64.exe`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via `AP.json`.
- SekaiLink: ecrire `AP.json` avant lancement; option pack PopTracker Celeste 64.

### Undertale (undertale)
- Installation: MultiworldGG + client Undertale. Premiere fois: `/auto_patch <dossier_undertale>` pour creer une copie moddee dans l'install MultiworldGG.
- Execution: lancer Undertale depuis le dossier MultiworldGG + lancer le client Undertale.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: client Undertale (adresse:port, slot). Option `/savepath` pour Linux/Proton.
- SekaiLink: fournir un bouton "Auto patch" (chemin Steam) et lancer client+jeu ensemble; gestion `savepath` pour Linux si supporte.

### VVVVVV (v6)
- Installation: V6AP dans le dossier du jeu (Steam/GOG).
- Execution: lancer le jeu avec options de lancement `-v6ap_name` et `-v6ap_ip` (+ `-v6ap_passwd` si besoin).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via options de lancement (host:port).
- SekaiLink: generer et injecter les launch args automatiquement.

### Kingdom Hearts (kh1)
- Installation: KH 1.5+2.5 (Steam/EGS) + OpenKH (Panacea + Lua backend KH1) + KH1FM Randomizer. MultiworldGG pour le client KH1.
- Execution: generer un seed zip, lancer `mod_generator.exe` pour produire `mod_*.zip`, installer via `OpenKh.Tools.ModsManager.exe`, `Build and Run`, puis lancer `KH1 Client` via MultiworldGG et connecter (adresse:port, slot).
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non (Lua backend OpenKH).
- Connexion serveur: via KH1 Client (adresse:port, slot).
- SekaiLink: wrapper OpenKH + pipeline de seed zip + launcher KH1 Client.

### Tyrian (tyrian)
- Installation: APTyrian + data files Tyrian 2.1/2000 (ou GOG). Extraire et mettre les data dans `APTyrian/data/`.
- Execution: lancer APTyrian, `Start Game` > `Online via Archipelago Server`, entrer host/slot et connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (menu APTyrian).
- SekaiLink: launcher simple + preset host/slot; verifier presence des data files.

### Zillion (zillion)
- Installation: RetroArch 1.10.3+ + ROM `Zillion (UE) [!].sms` + MultiworldGG.
- Execution: ouvrir patch `.apzl` pour generer ROM, lancer RetroArch (core SMS Plus GX ou Genesis Plus GX) avec Network Commands ON.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: via client Zillion (adresse:port) + RetroArch network commands.
- SekaiLink: flux RetroArch + note core requis et Network Commands.

### Zork Grand Inquisitor (zork_grand_inquisitor)
- Installation: Windows only + GOG version + ScummVM 2.7.1 x64 + MultiworldGG 0.7.120+.
- Execution: lancer le jeu via ScummVM, puis `Zork Grand Inquisitor Client`, connecter, puis `/zork` pour attacher.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client ZGI (adresse:port, slot), puis `/zork`.
- SekaiLink: wrapper client ZGI + verif ScummVM 2.7.1 + note GOG only.

### UFO 50 (ufo50)
- Installation: UFO 50 PC + APWorld (bundle MWGG). Installation via client UFO 50 (copy vers dossier modde) ou patch manuel (`ufo_50_basepatch.bsdiff4`, `gm-apclientpp.dll`).
- Execution: lancer via `UFO 50 Client` (MWGG) ou `ufo50.exe`, entrer server:port/slot/pw a l'ecran de profil.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game sur l'ecran profil.
- SekaiLink: fournir flow "install modded copy" + checks de version; note possible downpatch.

### Metroid Fusion (metroidfusion)
- Installation: BizHawk + client BizHawk MultiworldGG + ROM US. Patch `.apmetfus`.
- Execution: ouvrir `.apmetfus` pour patcher, lancer EmuHawk, charger `data/lua/connector_bizhawk_generic.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port) + Lua.
- SekaiLink: flux BizHawk generic + patch `.apmetfus`.

### Majora's Mask Recompiled (mm_recomp)
- Installation: Zelda64Recomp + MMRecompRando (mods) + VC++ Redist. ROM NTSC-U. (Mods dans `%LOCALAPPDATA%/Zelda64Recompiled/mods` ou `~/.config/Zelda64Recompiled/mods`).
- Execution: lancer Zelda64Recompiled, selectionner ROM, Start Game. (Mods rando via dossier mods).
- ROM client: Oui (ROM NTSC-U).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via le mod rando (selon UI du build).
- SekaiLink: detecter installation Zelda64Recomp + dossier mods; option tracker PopTracker.

### Minecraft (minecraft)
- Installation: Minecraft Java + MultiworldGG (client Minecraft). `.apmc` fourni par le host.
- Execution: ouvrir `.apmc` pour lancer serveur Forge local, puis Minecraft `Multiplayer > Direct Connection` vers `localhost`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: auto depuis room web ou `/connect <host> (port) (password)` in-game (sans `:` entre host et port).
- SekaiLink: bouton "Open .apmc" + auto-lancement Minecraft + aide pour `/connect`; note Java requis sur Linux.

### ANIMAL WELL (animal_well)
- Installation: MultiworldGG + `animal_well.apworld` (si non bundle). Client ANIMAL WELL via launcher.
- Execution: lancer le client ANIMAL WELL, connecter au serveur; jeu affiche la version sur l'ecran titre.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client ANIMAL WELL.
- SekaiLink: integrer client ANIMAL WELL et pre-remplir host/port/slot.

### Aquaria (aquaria)
- Installation: Aquaria Randomizer (copie dans dossier du jeu). Lancement via `aquaria_randomizer` ou args.
- Execution: lancer avec `--name` et `--server`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via args CLI.
- SekaiLink: generer les args et lancer l'exe.

### Path of Exile (poe)
- Installation: PoE + Python 3.13 + MultiworldGG + apworld PoE. Client PoE MWGG.
- Execution: lancer client PoE, configurer OAuth `/poe_auth`, `client.txt`, filtre, puis `/start`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client PoE (host:port, slot).
- SekaiLink: wizard pour config `client.txt` + filtre + auth, et lancement du client.

### StarCraft II (sc2)
- Installation: StarCraft 2 + MultiworldGG. Client SC2 dans le launcher.
- Execution: lancer SC2 Client, `/download_data`, puis `/connect` et lancer missions via l'onglet SC2.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client SC2 (adresse:port, slot).
- SekaiLink: wrapper client SC2 + helper /download_data.

### Toontown (toontown)
- Installation: Toontown AP (zip). Lancer `start_servers.bat` puis `start_client.bat`.
- Execution: configurer username + server IP; en jeu utiliser `!slot` puis `!connect`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: commandes in-game.
- SekaiLink: integrer un launcher qui demarre les serveurs et injecte `!slot/!connect` (ou copie rapide).

### Wordipelago (wordipelago)
- Installation: aucune, utiliser le site `apworlds.com/wordipelago`.
- Execution: ouvrir le site, selectionner Archipelago, entrer server/slot/pw, puis `Connect`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI web.
- SekaiLink: ouvrir dans une WebView + pre-remplir host/slot/pw.

### Yacht Dice (yachtdice)
- Installation: aucune (web) ou zip `Website.zip` (offline) depuis releases.
- Execution: ouvrir le site `yacht-dice-ap.netlify.app`, cliquer Archipelago, se connecter; client integre dans la page.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI web.
- SekaiLink: WebView + pre-remplir host/slot/pw; fallback zip offline.

### Ender Lilies (enderlilies)
- Installation: LiveSplit + composant Ender Lilies Randomizer (extrait dans `LiveSplit/Components`).
- Execution: lancer LiveSplit, ajouter le composant, configurer l'onglet Archipelago, puis "Launch Ender Lilies".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via composant LiveSplit (server/port/slot).
- SekaiLink: integrer un assistant LiveSplit (verif composant + preset layout).

### osu! (osu)
- Installation: client osu! + MultiworldGG (osu! APWorld inclus).
- Execution: lancer le client osu! via MultiworldGG.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: client osu! utilise OAuth. Necessite Client ID/Secret + Player ID (`/set_client_id`, `/set_api_key`, `/set_player_id`).
- SekaiLink: fournir un wizard OAuth (callback `http://localhost:3914`) et stocker les cles; bouton "Auto Track".

### Bumper Stickers (bumpstik)
- Installation: telecharger et extraire le jeu (GitHub/itch). Ne pas mettre dans Program Files.
- Execution: lancer `BumpStikAP.exe`, choisir "Archipelago Mode".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-game (host/port/slot). Auto-detect WS/WSS possible.
- SekaiLink: ecran de connexion simple + note WS/WSS.

### Meritous Gaiden (meritous)
- Installation: telecharger et extraire le jeu. Ne pas mettre dans Program Files.
- Execution: editer `meritous-ap.json`, puis lancer `meritous.exe`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via `meritous-ap.json` (server/port/slot/password).
- SekaiLink: generer/mettre a jour le JSON avant lancement.

### An Untitled Story (aus)
- Installation: AUS Randomizer (zip).
- Execution: editer `ArchipelagoConnectionInfo.ini`, puis lancer `AnUntitledStory_Randomizer_*.exe`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via INI (host/port/slot).
- SekaiLink: ecrire l'INI avant lancement.

### Refunct (refunct)
- Installation: `practice-windows.zip` ou `practice-linux.zip` du client refunct-tas.
- Execution: lancer Refunct, puis `refunct-tas.exe`, ouvrir le menu (m) et se connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via menu in-game.
- SekaiLink: lancer client refunct-tas et pre-remplir host/port/slot.

### Manual Randomizer (_manual)
- Installation: aucune (doc placeholder).
- Execution: n/a.
- ROM client: n/a.
- ROM serveur: n/a.
- Lua: n/a.
- Connexion serveur: n/a.
- SekaiLink: a completer quand les docs seront remplis.

### Archipelago Tracker (tracker)
- Installation: installer le client tracker (Kivy) et suivre `client-integration.md`/`apworld-integration.md`.
- Execution: lancer le Tracker, connecter via host/slot/pw selon l'integration.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client tracker.
- SekaiLink: documenter comme outil (pas un jeu) + fournir presets de connexion.

## Archipelago-specific

### APQuest (apquest)
- Installation: MultiworldGG + APQuest apworld (si non bundle).
- Execution: lancer le client APQuest via MultiworldGG.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: auto depuis la page room (webhost) ou manuel via client APQuest (host/slot/pw).
- SekaiLink: integrer un client APQuest natif dans Electron; permettre auto-join depuis un lien de room SekaiLink.

### ArchipIDLE (archipidle)
- Installation: client web (idle.multiworld.link) ou client telechargeable (GitHub releases).
- Execution: ouvrir le client, entrer `Server Address`, slot name, puis `Begin!`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI du client.
- SekaiLink: integrer une WebView et pre-remplir host/slot; option "ouvrir client local".

### Archipela-Go! (apgo)
- Installation: app mobile Android (.apk). MultiworldGG pour la generation/host.
- Execution: installer l'app et se connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: depuis l'app (host/port/slot).
- SekaiLink: fournir un flow "mobile client" avec QR ou copie d'URL host:port/slot.

## Other

### (Reserve)
- Cette section est gardee pour les jeux qui ne rentrent pas proprement dans les categories ci-dessus.

### Super Mario 64 Hacks (sm64hacks)
- Installation: Luna's Project64 + PJ64 Connector Script. JSON de hack (data/sm64hacks/), template YAML.
- Execution: generer JSON via site (ou presets), mettre `json_file` dans le YAML, lancer la ROM dans Project64, charger `connector_pj64_generic.js` (Debugger > Scripts).
- ROM client: Oui (ROM hack).
- ROM serveur: Non.
- Lua: Non (script PJ64 `.js`).
- Connexion serveur: via client StarDisplay (Archipelago) ou PJ64 connector (selon setup).
- SekaiLink: fournir un flow "Project64" + loader du script `.js`, et guide pour JSON hack.

### Generic Setup Guide (generic)
- Installation: n/a (guide general).
- Execution: n/a.
- ROM client: n/a.
- ROM serveur: n/a.
- Lua: n/a.
- Connexion serveur: n/a.
- SekaiLink: document de reference general, pas un jeu.

### Candy Box 2 (candybox2)
- Installation: aucune, utiliser le client web `candybox2-ap.vicr123.com`.
- Execution: ouvrir le site, entrer server url:port, slot, mot de passe si requis, puis `Connect to Archipelago`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI web.
- SekaiLink: ouvrir dans une WebView + pre-remplir host/slot/pw.

### Clique (clique)
- Installation: aucune, utiliser le site `clique.pharware.com`.
- Execution: ouvrir le site, entrer server/slot/password, puis `Connect`.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI web.
- SekaiLink: ouvrir dans une WebView + pre-remplir host/slot/pw.

### Paint (paint)
- Installation: aucune, utiliser le site `mariomantaw.github.io/jspaint/`.
- Execution: ouvrir le site, entrer server/slot/pw, puis `Connect`. Option: File > Open Goal Image.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI web.
- SekaiLink: ouvrir dans une WebView + pre-remplir host/slot/pw; conserver l'URL avec hash pour l'async.

### OpenRCT2 (openrct2)
- Installation: RCT2/RCT Classic + OpenRCT2 + plugin "rollercoaster-tycoon-randomizer".
- Execution: lancer OpenRCT2, choisir le scenario, activer le randomizer si besoin.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client MultiworldGG + bouton Archipelago dans le menu scenario.
- SekaiLink: integrer un client "OpenRCT2" et guider l'installation du plugin; fournir un bouton "Connect" qui lance le client et attend l'ack dans OpenRCT2.

### Super Mario 64 EX (sm64ex)
- Installation: SM64AP-Launcher (compile) + ROM US/JP nommee `baserom.us.z64`/`baserom.jp.z64` ou build manuel. 
- Execution: lancer le binaire avec `--sm64ap_name` et `--sm64ap_ip` (ou `--sm64ap_file` offline).
- ROM client: Oui (ROM vanilla pour build; pas de ROM patchee separee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via arguments de lancement.
- SekaiLink: fournir un "launcher" qui stocke build + injecte args; possible bouton "offline" avec fichier `.apsm64ex`.

### DOOM 1993 (doom_1993)
- Installation: APDOOM (Crispy) + copier `DOOM.WAD` dans le dossier APDOOM.
- Execution: lancer `apdoom-launcher.exe` ou `crispy-apdoom` avec args `-apserver`/`-applayer`.
- ROM client: Oui (WAD).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via launcher ou args CLI.
- SekaiLink: wrapper pour lancer APDOOM avec args + champ host/port/slot.

### DOOM II (doom_ii)
- Installation: APDOOM (Crispy) + copier `DOOM2.WAD` dans le dossier APDOOM.
- Execution: lancer `apdoom-launcher.exe` ou `crispy-apdoom -game doom2 ...`.
- ROM client: Oui (WAD).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via launcher ou args CLI.
- SekaiLink: wrapper identique a DOOM 1993 avec selection "doom2".

### Heretic (heretic)
- Installation: APDOOM (Crispy) + copier `HERETIC.WAD` dans le dossier APDOOM.
- Execution: lancer `apdoom-launcher.exe` (mode Heretic) ou `crispy-apheretic` en CLI.
- ROM client: Oui (WAD).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via launcher ou args CLI.
- SekaiLink: wrapper APDOOM avec selection Heretic.

### gzArchipelago (gzdoom)
- Installation: gzDoom + `gzArchipelago.pk3` + apworld correspondant. Ajouter le pk3 genere par seed en fin de load order.
- Execution: lancer gzDoom avec l'ordre de mods; client GZDoom requis pour multiworld.
- ROM client: Oui (WAD/PK3).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via GZDoom Client (MultiworldGG).
- SekaiLink: helper pour gerer load order + lancer client GZDoom.

### Quake (quake)
- Installation: ironwail_ap + fichiers q1 + `id1/pak2.pak`.
- Execution: editer `ap_connect_info.json`, puis lancer `ironwail_ap.exe`.
- ROM client: Oui (pak).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via JSON.
- SekaiLink: ecrire JSON avant lancement.

### Jigsaw Puzzles (jigsaw)
- Installation: aucune (web).
- Execution: ouvrir le site jigsaw-ap.netlify.app.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: in-app web (selon le site).
- SekaiLink: ouvrir le site dans un WebView + fournir les infos host/slot.

### APSudoku (apsudoku)
- Installation: client APSudoku (desktop ou web).
- Execution: ouvrir le client, configurer host/port/slot, connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via UI APSudoku.
- SekaiLink: launcher simple + presets de connexion.

### Chatipelago (chatipelago)
- Installation: config cote serveur Chatipelago.
- Execution: entrer les infos de seed dans le serveur Chatipelago.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: n/a (serveur).
- SekaiLink: documenter comme "other/outil" (pas un jeu).

### Dark Souls Remastered (dsr)
- Installation: DSAP Client. (Note doc mention PCSX2 1.6.0 si utilise emu).
- Execution: lancer le jeu, puis le client DSAP et connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via DSAP Client.
- SekaiLink: wrapper client DSAP.

### Donkey Kong 64 (dk64)
- Installation: suivre le guide DK64 Randomizer (doc externe).
- Execution: via les outils du DK64 Randomizer + client AP.
- ROM client: Oui (ROM N64).
- ROM serveur: Non.
- Lua: depend du setup DK64 (probable BizHawk/Lua).
- Connexion serveur: selon client DK64.
- SekaiLink: lien direct vers la doc DK64 et integration a definir.

### Factorio (factorio)
- Installation: mod AP_*.zip dans mods client + serveur. MultiworldGGFactorioClient pour host.
- Execution (host): lancer client Factorio MWGG, `/connect server`, puis connecter Factorio client a `localhost`.
- Execution (client): connecter via "Multiplayer > Connect to address".
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Factorio MWGG.
- SekaiLink: fournir un mode "host" et "join" (mods + client).

### Shivers (shivers)
- Installation: ScummVM 2.7+ + Shivers Randomizer Client.
- Execution: lancer Shivers dans ScummVM, puis lancer le client, "Attach" et se connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Shivers (host/slot/pw).
- SekaiLink: wrapper pour lancer ScummVM + client, preset host/port/slot.

### Landstalker (landstalker)
- Installation: client Landstalker (randstalker_archipelago) + ROM US + emu Genesis Plus GX (RetroArch/BizHawk).
- Execution: lancer le client, connecter au serveur, generer ROM, puis connecter a l'emulateur.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Landstalker.
- SekaiLink: wrapper client + detection emu (Genesis Plus GX).

## Snes9x / BSNES-plus / SNI

### Civilization VI (civ_6)
- Installation: Windows seulement. Civ VI + DLC Rise & Fall + Gathering Storm + mod Archipelago. Fichier `.apcivvi` a copier dans le dossier du mod.
- Execution: activer le mod + activer "Tuner" en options, puis lancer client Civ6 et connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Civ6 (adresse:port, slot).
- SekaiLink: assistant d'installation (mod + apcivvi), verifier Tuner actif, lancer client Civ6.

### A Link to the Past (alttp)
- Installation: MultiworldGG + SNI + emu SNES compatible SNI (snes9x-rr/nwa, BSNES-plus, BizHawk BSNES, RetroArch bsnes-mercury) ou hardware (SD2SNES/FXPak). ROM JP v1.0 `Zelda no Densetsu - Kamigami no Triforce (Japan).sfc`.
- Execution: ouvrir patch `.aplttp` pour generer ROM + client SNI, lancer l'emu, charger `SNI/lua/Connector.lua` si besoin.
- ROM client: Oui (ROM JP v1.0 + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- PopTracker pack: `https://github.com/StripesOO7/alttp-ap-poptracker-pack`.
- SekaiLink: flux SNI standard + support RetroArch network commands si choisi; telechargement/installation auto du pack PopTracker; cote serveur, exiger ROM JP v1.0 pour generer.

### Chrono Trigger: Jets of Time (ctjot)
- Installation: MultiworldGG + SNI + emu SNES compatible SNI ou hardware. ROM US Chrono Trigger.
- Execution: generer un couple ROM+YAML via le site CTJoT, fournir le YAML au host, puis lancer le ROM patche.
- ROM client: Oui (ROM US + ROM patche).
- ROM serveur: Non.
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port) (doc incomplet apres "Running the Client").
- SekaiLink: flux SNI standard + lien vers le generateur CTJoT pour ROM/YAML.

### Yoshi's Island (yoshisisland)
- Installation: MultiworldGG + ROM SNES EN + emu SNES compatible SNI (snes9x-rr/nwa, BizHawk BSNES).
- Execution: ouvrir patch `.apyi` pour generer ROM + client SNI, puis lancer emu.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: integrer flux SNI + auto-lua selon emulateur.

### Lufia II Ancient Cave (lufia2ac)
- Installation: MultiworldGG + ROM US + emu SNES compatible SNI (snes9x-rr/BizHawk/RetroArch) ou hardware (SD2SNES/FXPak).
- Execution: ouvrir patch `.apl2ac` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI + support RetroArch network commands si choisi.

### Donkey Kong Country (dkc)
- Installation: MultiworldGG + ROM SNES US v1.0 + emu SNES compatible SNI (snes9x-nwa/rr, BSNES-plus).
- Execution: ouvrir patch `.apdkc` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard (idem dkc2).

### Super Metroid (sm)
- Installation: MultiworldGG + SNI + ROM `Super Metroid (Japan, USA).sfc` + emu SNES compatible SNI (snes9x-rr/nwa, BSNES-plus, BizHawk, RetroArch).
- Execution: ouvrir patch `.apsm` pour generer ROM + client SNI, lancer emu, charger `SNI/lua/Connector.lua` si besoin.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua`.
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + support RetroArch network commands.

### Super Metroid Map Rando (sm_map_rando)
- Installation: MultiworldGG + SNI + ROM Super Metroid + emu SNES compatible SNI.
- Execution: ouvrir patch `.apsmmr` pour generer ROM + client SNI, lancer emu, charger `SNI/lua/x64/Connector.lua` ou `SNI/lua/x86/Connector.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Non.
- Lua: `SNI/lua/x64/Connector.lua` ou `SNI/lua/x86/Connector.lua`.
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + choix x86/x64 pour le Lua.

### Super Mario World (smw)
- Installation: Archipelago/MultiworldGG + ROM `Super Mario World (USA).sfc` + emu SNES compatible SNI.
- Execution: ouvrir patch `.apsmw` pour generer ROM + client SNI, lancer emu, charger `SNI/lua/Connector.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua`.
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + option tracker PopTracker.

### SMZ3 (smz3)
- Installation: MultiworldGG + SNI + ROM Super Metroid + ROM Zelda3 JP v1.0.
- Execution: ouvrir patch `.apsmz3` pour generer ROM + client SNI, lancer emu, charger `SNI/lua/Connector.lua`.
- ROM client: Oui (ROMs vanilla + ROM patche).
- ROM serveur: Non.
- Lua: `SNI/lua/Connector.lua`.
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + rappel des deux ROMs base.

### Secret of Evermore (soe)
- Installation: SNI + ROM US + client web ap-soeclient (evermizer.com). Patch `.apsoe` via apbpatch (evermizer.com).
- Execution: generer ROM via apbpatch, lancer emu + SNI, ouvrir le client web, puis `/connect host:port`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (ou x86/x64 selon emu).
- Connexion serveur: via client web ap-soeclient.
- SekaiLink: ouvrir apbpatch + client web en WebView, et guider SNI/emu.

### Final Fantasy IV: Free Enterprise (ff4fe)
- Installation: MultiworldGG + ROM SNES US 1.1 + emu SNES compatible SNI (snes9x-rr/BizHawk/RetroArch).
- Execution: ouvrir patch `.apff4fe` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Kirby's Dream Land 3 (kdl3)
- Installation: MultiworldGG + ROM SNES + emu SNI (snes9x-rr/nwa, BizHawk, bsnes-plus-nwa). RetroArch incompatible.
- Execution: ouvrir patch `.apkdl3` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + note "pas de RetroArch".

### Link's Awakening DX Beta (ladx_beta)
- Installation: MultiworldGG + ROM GBC + RetroArch (SameBoy) ou BizHawk 2.8+.
- Execution: ouvrir patch `.apladx` pour generer ROM + client. RetroArch via network commands; BizHawk via `connector_ladx_bizhawk.lua`.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: BizHawk via `data/lua/connector_ladx_bizhawk.lua`.
- Connexion serveur: via client SNI/AP.
- SekaiLink: flux GBC (RetroArch/BizHawk) + selection du connector specifique.

### The Legend of Zelda (tloz)
- Installation: BizHawk 2.10+ + ROM US v1.0 PRG0 `Legend of Zelda, The (U) (PRG0) [!].nes` + MultiworldGG.
- Execution: ouvrir patch `.aptloz` pour generer ROM, lancer BizHawk Client, puis charger `data/lua/connector_bizhawk.lua` dans EmuHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk.lua`.
- Connexion serveur: via client BizHawk (adresse:port).
- SekaiLink: flux BizHawk + option tracker PopTracker/Map Tracker.

### The Legend of Zelda: Oracle of Ages (tloz_ooa)
- Installation: BizHawk 2.9.1 x64 + ROM US GBC + `ooa.apworld` (si non bundle).
- Execution: ouvrir patch `.apooa` pour generer ROM, lancer BizHawk + client, charger Lua generique.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port).
- SekaiLink: flux BizHawk generic + note "x64 requis".

### The Legend of Zelda: Phantom Hourglass (tloz_ph)
- Installation: BizHawk 2.10+ + ROM EU (anglais) + `tloz_ph.apworld` (si non bundle).
- Execution: lancer client BizHawk, lancer ROM dans EmuHawk, charger `data/lua/connector_bizhawk_generic.lua`, puis demarrer une nouvelle save (pont repare si OK).
- ROM client: Oui (ROM EU).
- ROM serveur: Non.
- Lua: `data/lua/connector_bizhawk_generic.lua`.
- Connexion serveur: via client BizHawk (adresse:port).
- SekaiLink: flux BizHawk generic + note ROM EU only.

### The Legend of Zelda: Oracle of Seasons (tloz_oos)
- Installation: BizHawk 2.10 + ROM US. Patch `.apoos`.
- Execution: ouvrir le patch via MultiworldGG, puis ROM patche dans BizHawk.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `data/lua/connector_bizhawk_generic.lua` (BizHawk Client).
- Connexion serveur: via BizHawk Client (adresse:port).
- SekaiLink: flux BizHawk generic.

### Madou Monogatari (madou)
- Installation: MultiworldGG + ROM SNES + patch de traduction EN applique avant generation. Emu SNI compatible.
- Execution: ouvrir patch pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard + rappel "appliquer traduction avant".

### EarthBound (earthbound)
- Installation: MultiworldGG + ROM SNES EN + emu SNI compatible (snes9x-rr/nwa, BizHawk, RetroArch).
- Execution: ouvrir patch `.apeb` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Mega Man X3 (mmx3)
- Installation: MultiworldGG + ROM SNES US + emu SNI compatible. Patch `.apmmx3`.
- Execution: ouvrir patch pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Final Fantasy Mystic Quest (ffmq)
- Installation: MultiworldGG + ROM SNES NA + emu SNI compatible.
- Execution: generer ROM via site FFMQR (avec .apmq), puis lancer SNI client.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Non.
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI + rappel "patch via site FFMQR".

### Donkey Kong Country 3 (dkc3)
- Installation: Archipelago/MultiworldGG + ROM SNES + emu SNI compatible.
- Execution: ouvrir patch `.apdkc3` pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Spicy Mycena Waffles (waffles)
- Installation: MultiworldGG + ROM SMW US + emu SNI compatible. Patch `.apwaffle`.
- Execution: ouvrir patch pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Final Fantasy V Career Day (ffvcd)
- Installation: MultiworldGG + ROM FFV Japan 1.0 (MD5 specifique) + emu SNI compatible. Patch `.apffvcd`.
- Execution: ouvrir patch pour generer ROM + client SNI.
- ROM client: Oui (ROM vanilla + ROM patche).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: `SNI/lua/Connector.lua` (snes9x-rr/BizHawk).
- Connexion serveur: via client SNI (adresse:port).
- SekaiLink: flux SNI standard.

### Donkey Kong Country 2 (dkc2)
- Installation: MultiworldGG + SNI (inclus) + emu SNES (snes9x-nwa, snes9x-rr, BSNES-plus). ROM US v1.1. Patch `.apdkc2`.
- Execution: double-cliquer le patch pour generer le ROM et lancer le client; lancer l'emu et charger le ROM.
- ROM client: Oui (ROM vanilla pour patcher, ROM patche pour jouer).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: oui pour snes9x-rr (`SNI/lua/connector_*.lua`), non pour snes9x-nwa/BSNES-plus (SNI direct).
- Connexion serveur: via client MultiworldGG (adresse:port, slot/password). SNI connecte emu <-> client.
- SekaiLink: integrer un client SNI+AP, avec detection d'emu et chargement Lua automatique si snes9x-rr.

## DuckStation

### Digimon World (dw1)
- Installation: DuckStation + ROM US. Client DWAP (C#).
- Execution: lancer DuckStation et charger le ROM, puis lancer DWAP Client.
- ROM client: Oui (ROM US).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via DWAP Client (host:port, slot, pw).
- SekaiLink: integrer le DWAP Client et ordre de lancement (emu d'abord, client ensuite).

### Spyro 3: Year of the Dragon (spyro3)
- Installation: DuckStation + ROM NTSC-U + client Spyro 3 (S3AP). MultiworldGG 0.7.100+.
- Execution: lancer le jeu dans DuckStation, puis `S3AP.exe`, entrer host/slot/pw et Connect (Windows only).
- ROM client: Oui (ROM NTSC-U).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client S3AP.
- SekaiLink: integrer client S3AP + verifier DuckStation; note "Windows only".

## Dolphin

### Metroid Prime (metroidprime)
- Installation: Dolphin + ISO GC + `metroidprime.apworld` (si non bundle). Patch `.apmp1` via MultiworldGG.
- Execution: ouvrir le patch `.apmp1` pour generer `AP_XXXX.iso`, puis lancer dans Dolphin.
- ROM client: Oui (ISO vanilla + ISO patchee).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: via client Metroid Prime (adresse:port).
- SekaiLink: offrir un flux "Open Patch" + lancement Dolphin; option `rom_start` dans `host.yaml` pour auto-launch.

### Mario Kart: Double Dash!! (mario_kart_double_dash)
- Installation: Dolphin + ROM NTSC-U. APWorld si non bundle. Pas de patch: injection runtime.
- Execution: lancer client MKDD, puis lancer le jeu dans Dolphin (ne pas depasser ecran titre avant connexion).
- ROM client: Oui (ISO/rvz).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client MKDD (adresse:port, slot).
- SekaiLink: integrer client MKDD et sequence "client d'abord, ensuite Dolphin".

### Paper Mario: The Thousand-Year Door (ttyd)
- Installation: Dolphin + ROM US `.iso` (pas ciso/nkit) + APWorld TTYD (si non bundle).
- Execution: generer patch via MultiworldGG, utiliser `Open Patch` pour produire l'ISO, lancer le client TTYD et connecter, puis lancer l'ISO dans Dolphin.
- ROM client: Oui (ISO US + ISO patchee).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: via client TTYD (adresse:port).
- SekaiLink: flux "Open Patch + Launch Dolphin" + appliquer settings Dolphin (no dual core, OpenGL/Vulkan, no memory/clock override).

### The Wind Waker (tww)
- Installation: Dolphin + ISO NTSC-U + TWW AP Randomizer Build.
- Execution: ouvrir le build, charger le fichier `.aptww`, randomize pour generer ISO, lancer dans Dolphin.
- ROM client: Oui (ISO vanilla + ISO randomisee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Wind Waker (MultiworldGG).
- SekaiLink: workflow "Randomize + Launch Dolphin" + client WW integre.

### Battle for Bikini Bottom (bfbb)
- Installation: Dolphin + apworld bfbb + libs additionnelles + ISO US. Patch `.apbfbb` -> `.gcm`.
- Execution: lancer BfBB Client, patcher, puis ouvrir la GCM dans Dolphin.
- ROM client: Oui (ISO US + GCM randomisee).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: via client BfBB (adresse:port).
- SekaiLink: flux "Open Patch + Launch Dolphin" + verifier deps lib/.NET.

### Lego Star Wars: The Complete Saga (lego_star_wars_tcs)
- Installation: apworld + client TCS, jeu PC.
- Execution: lancer le jeu, puis client TCS et connecter.
- ROM client: Non.
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client TCS (adresse:port, slot).
- SekaiLink: wrapper client + ordre de lancement.

### Luigi's Mansion (luigismansion)
- Installation: Dolphin + ISO NTSC-U + client Luigi's Mansion (MultiworldGG).
- Execution: ouvrir patch `.aplm`, lancer ISO patchee dans Dolphin, connecter via client.
- ROM client: Oui (ISO vanilla + ISO patchee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client LM (adresse:port).
- SekaiLink: flux "Open Patch + Launch Dolphin" + rappeler MMU off.

### The Legend of Zelda: Skyward Sword (ss)
- Installation: Dolphin dev (ou Wii/Wii U) + ISO US 1.00 + SS APWorld + SS patcher. APSSR fourni par l'hote.
- Execution: generer ISO randomisee via patcher + APSSR, ouvrir dans Dolphin, lancer `Skyward Sword Client` (MultiworldGG) et `/connect host:port`.
- ROM client: Oui (ISO US + ISO patchee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client SS (slot encode dans l'ISO). Mode console via `/console <ip>` si Wii.
- SekaiLink: workflow patcher (APSSR -> ISO), client SS integre, note "fichier 1 obligatoire".

### The Legend of Zelda: Twilight Princess (tp)
- Installation: Dolphin + ISO US + REL loader + `RandomizerAP.US.gci` + seed `aptest.gci` (tprandomizer.com) + TP APWorld.
- Execution: lancer Dolphin, charger save REL loader, selectionner seed APTest, demarrer une nouvelle save, lancer `Twilight Princess Client`, `/name` puis connecter au serveur.
- ROM client: Oui (ISO US).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client TP (adresse:port, slot via `/name`).
- SekaiLink: verifier fichiers save (REL loader + gci), integrer client TP, note "generation locale seulement".

### Super Mario Sunshine (sms)
- Installation: Dolphin + ISO US + `archipelago-sms` apworld si non bundle. Patch via MultiworldGG -> ISO randomisee.
- Execution: ouvrir ISO randomisee dans Dolphin, puis connecter via SMSClient (apres ecran File Select).
- ROM client: Oui (ISO vanilla + ISO randomisee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via SMSClient (adresse:port).
- SekaiLink: flux "Open Patch + Launch Dolphin" + rappel "connect au File Select".

## PCSX2

### Sly Cooper and the Thievius Raccoonus (sly1)
- Installation: PCSX2 avec PINE + ISO NTSC. Client Sly1 via Archipelago Launcher.
- Execution: lancer PCSX2 et charger l'ISO, puis lancer client Sly1.
- ROM client: Oui (ISO).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Sly1 (adresse:port).
- SekaiLink: sequence "PCSX2 d'abord, client ensuite" + verif PINE slot 28011.

### Ratchet & Clank 3: Up Your Arsenal (rac3)
- Installation: PCSX2 1.7+ (PINE) + ISO US + apworld rac3.
- Execution: lancer PCSX2, demarrer le jeu, puis lancer client RaC3 (PCSX2 doit etre deja ouvert).
- ROM client: Oui (ISO).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client RaC3 (adresse:port).
- SekaiLink: sequence stricte "PCSX2 d'abord, client ensuite" + verification PINE (slot 28011).

### Ratchet & Clank 2 (rac2)
- Installation: PCSX2 1.7+ (PINE) + ISO US SCUS-97268 + apworld rac2.
- Execution: ouvrir `.aprac2` via MultiworldGG, generer ISO randomisee, lancer dans PCSX2, puis connecter client.
- ROM client: Oui (ISO vanilla + ISO randomisee).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client RaC2 (adresse:port).
- SekaiLink: flux "Open Patch + Launch PCSX2" + verif PINE slot 28011.

## Cemu

### Xenoblade Chronicles X (xenobladex)
- Installation: Cemu + jeu + DLC/updates + apworld xenobladex.
- Execution: lancer client Xenoblade X, choisir Cemu exe si demande, puis lancer le jeu dans Cemu.
- ROM client: Oui (base game).
- ROM serveur: Non.
- Lua: Non.
- Connexion serveur: via client Xenoblade X (adresse:port).
- SekaiLink: wrapper client + path Cemu.

## Azahar / 3DS

### A Link Between Worlds (albw)
- Installation: ROM 3DS decryptee + apworld + Azahar (emu) ou 3DS avec Luma + plugin. Patch `.apalbw`.
- Execution: ouvrir patch -> generer mods `00040000000EC300`, placer dans `load/mods` (emu) ou `/luma/titles` (3DS).
- ROM client: Oui (ROM 3DS).
- ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement).
- Lua: Non.
- Connexion serveur: via client ALBW (adresse:port).
- SekaiLink: fournir flux patch + copie du dossier mod + support emu/3DS.
