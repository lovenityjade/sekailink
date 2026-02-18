# Client Autolaunch Draft (from setup guides)

This is an initial automation map derived from setup guides. Use per-game sections to plan client flow and bundling.

## A_Hat_in_Time

- Setup files: A_Hat_in_Time/setup_en.md
- Connection hints:
  - 5. You should now be good to go. See below for more details on how to use the mod and connect to a MultiworldGG game.
  - To connect to the multiworld server, simply run the **MultiworldGG AHIT Client** from the Launcher
  - The game will connect to the client automatically when you create a new save file.
- Install/patch hints:
  - # Setup Guide for A Hat in Time in MultiworldGG
  - 5. You should now be good to go. See below for more details on how to use the mod and connect to a MultiworldGG game.
  - ## Connecting to the MultiworldGG server
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## A_Link_Between_Worlds

- Setup files: A_Link_Between_Worlds/setup_en.md
- Patch extensions mentioned: apalbw, apworld
- Connection hints:
  - 5. Run A Link Between Worlds in the emulator. The client should automatically connect to the emulator.
  - 6. Enter the server URL into the client and press Connect.
  - 8. Enter the server URL into the client and press Connect.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - A decrypted, North American A Link Between Worlds `.cci` ROM (if you have a `.3ds` ROM, just rename it). Instructions for dumping your ROM can be found [here](https://wiki.hacks.guide/wiki/3DS:Dump_titles_and_game_cartridges). **Make sure to select "decrypt" when dumping.**
  - 1. Install the latest version of MultiworldGG.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## A_Short_Hike

- Setup files: A_Short_Hike/setup_en.md
- Connection hints:
  - Enter in the Server Address and Port, Name, and Password (optional) in the popup menu that appears and hit connect.
  - Connect to Archipelago via the AP button in the top left.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Adventure

- Setup files: Adventure/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apadvn
- Connection hints:
  - the credits room.
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - # Setup Guide for Adventure: MultiworldGG
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - An Adventure NTSC ROM file. The MultiworldGG community cannot provide these.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Against_the_Storm

- Setup files: Against_the_Storm/setup_en.md
- Mod loaders/tools: Fabric, Thunderstore, thunderstore
- Patch extensions mentioned: apworld
- Connection hints:
  - 1. Don't worry too much about the `name` if you're just trying this out on your own. The slot name would be more relevant if you are playing a MultiworldGG Multiworld.
  - 2. From the world map, open the dev console (default \` (backtick, to the left of 1)) and type `ap.connect <url>:<port> "<slotName>" [password]`.
  - * If you uploaded `AP_#######.zip` to MultiworldGG, then the room you generated should have the url: `multiworld.gg:#####`
- Install/patch hints:
  - # Against the Storm for MultiworldGG Setup and Usage Guide
  - * Latest release of [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases). Currently tested/working on version 0.7.195.
  - * For any non-MWGG client: The `against_the_storm.apworld` from the latest [Against The Storm for Archipelago](https://github.com/RyanCirincione/ArchipelagoATS/releases) release.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## An_Untitled_Story

- Setup files: An_Untitled_Story/setup_en.md
- Connection hints:
  - ### Connect to the MultiServer
  - * Edit ArchipelagoConnectionInfo.ini with the host / port / slot information from the mwgg multiworld room
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## ANIMAL_WELL

- Setup files: ANIMAL_WELL/setup_en.md
- Patch extensions mentioned: apworld
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - - Place the `animal_well.apworld` in your `MultiworldGG/custom_worlds` folder. (not needed with MWGG)
  - - Open up the MultiworldGG Launcher.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Ape_Escape

- Setup files: Ape_Escape/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `archipelago.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases). Please use version 0.7.70 or later for integrated
  - - Ape Escape (USA) ISO or BIN/CUE. Either an original black label version or the Greatest Hits version should work.
  - - (Only if you are not using the MWGG client:) The latest `apeescape.apworld` file. You can find this on the [Releases page](https://github.com/Thedragon005/Archipelago-Ape-Escape/releases/latest). Put this in your `MultiworldGG/custom_worlds` folder.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## APQuest

- Setup files: APQuest/setup_en.md
- Connection hints:
  - First, you need a room to connect to. For this, you or someone you know has to generate a game.
  - From here, connecting to your APQuest slot is easy. There are two scenarios.
  - ### Webhost Room
- Install/patch hints:
  - - [MultiworldGG](github.com/MultiworldGG/MultiworldGG/releases/latest)
  - if not bundled with your version of MultiworldGG / AP
  - but you can check the [MultiworldGG Setup Guide](/tutorial/Archipelago/setup_en#generating-a-game).
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Aquaria

- Setup files: Aquaria/setup_en.md
- Patch extensions mentioned: AppImage
- Connection hints:
  - or, if the room has a password:
  - aquaria_randomizer.exe  --name YourName --server theServer:thePort --password thePassword
  - You can also use command line arguments to set the server and slot of your game:
- Install/patch hints:
  - - For sending [commands](/tutorial/MultiworldGG/commands/en) like `!hint`: the TextClient from [the most recent MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - There is multiple way to start the game. The easiest one is using the launcher. To do that, just run
  - To launch the randomizer using the integrated launcher, just execute the AppImage file.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Archipela-Go

- Setup files: Archipela-Go/setup_en.md
- Install/patch hints:
  - * [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Archipelago

- Setup files: Archipelago/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - - Connect to the multiworld after hosting has begun
  - native coop system or using MultiworldGG's coop support. Each world will hold one slot in the multiworld and will have a
  - slot name and, if the relevant game requires it, files to associate it with that multiworld.
- Install/patch hints:
  - # MultiworldGG Setup Guide
  - - Install, set up, and run the MultiworldGG Multiworld software
  - ## Installing the MultiworldGG software
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Balatro

- Setup files: Balatro/setup_en.md
- Install/patch hints:
  - This is a Mod for [Balatro](https://store.steampowered.com/app/2379780/Balatro/) that provides Integration for [MultiworldGG](https://multiworld.gg)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Banjo-Tooie

- Setup files: Banjo-Tooie/setup_en.md
- Emulators: Bizhawk, BizHawk
- Connection hints:
  - -   Under N64 enable "Use Expansion Slot". (The N64 menu only appears after loading a ROM.)
  - - To connect the client to the multiserver simply put  `<address>:<port>`  on the textfield on top and press `connect` (if the server uses password, then it will prompt after connection).
  - - To connect the client to the multiserver, simply put  `<address>:<port>`  on the textfield on top and press `connect` (if the server uses password, then it will prompt after connection).
- Install/patch hints:
  - # Setup Guide for Banjo-Tooie MultiworldGG
  - -   A Banjo-Tooie ROM (USA ONLY).
  - -   Under N64 enable "Use Expansion Slot". (The N64 menu only appears after loading a ROM.)
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.

## Battle_for_Bikini_Bottom

- Setup files: Battle_for_Bikini_Bottom/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: apbfbb, apworld, apworlds
- Connection hints:
  - ### Connect to the Client
  - The Client will automatically try to connect to Dolphin every 5 seconds and will do so if BfBB is running. If this
  - ### Connect to the MultiworldGG Server
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases) v0.7.150 or higher. Make sure to install the
  - - [This AP world](https://github.com/Cyb3RGER/bfbb_ap_world/releases) (ships with MultiworldGG)
  - named ``Nickelodeon SpongeBob SquarePants - Battle for Bikini Bottom (USA).iso``.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Blasphemous

- Setup files: Blasphemous/setup_en.md
- Connection hints:
  - To connect to a multiworld: Choose a save file and enter the address, your name, and the password (if the server has one) into the menu.
- Install/patch hints:
  - You will need the [Multiworld](https://github.com/BrandenEK/Blasphemous.Randomizer.Multiworld) mod to play a MultiworldGG randomizer.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Bomb_Rush_Cyberfunk

- Setup files: Bomb_Rush_Cyberfunk/setup_en.md
- Mod loaders/tools: BepInEx, r2modman, Thunderstore, thunderstore
- Connection hints:
  - To connect to a MultiworldGG server, click one of the MultiworldGG buttons next to the save files. If the save file is
  - blank or already has randomizer save data, it will open a menu where you can enter the server address and port, your
  - name, and a password if necessary. Then click the check mark to connect to the server.
- Install/patch hints:
  - After installing MultiworldGG, there are some additional mods that can also be installed for a better experience:
  - To connect to a MultiworldGG server, click one of the MultiworldGG buttons next to the save files. If the save file is
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Brotato

- Setup files: Brotato/setup_en.md
- Mod loaders/tools: ModLoader
- Install/patch hints:
  - ## MultiworldGG Text Client
  - The Brotato mod acts as a client to MultiworldGG, so you do not need to launch a separate
  - client. However, you may find it useful to keep the MultiworldGG Text Client (included in
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Bumper_Stickers

- Setup files: Bumper_Stickers/setup_en.md
- Connection hints:
  - 3. Enter your server details in the fields provided, and click "Start".
  - While playing the multiworld, you can interact with the server using various commands listed in the [commands guide](/tutorial/MultiworldGG/commands/en). As this game does not have an in-game text client at the moment, you can optionally connect to the multiworld using the text client, which can be found in the [MultiworldGG installation](https://github.com/MultiworldGG/MultiworldGG/releases) as MultiworldGG Text Client to enter these commands.
- Install/patch hints:
  - - Launch the Bumper onto the field by clicking on a Launcher. It will first move in the direction launched, then in the direction printed on the Bumper once it hits something.
  - - Turners allow you to change the direction of any bumper. You won't get them from scoring, but you can get them as MultiworldGG items, and they'll refresh every time you start a new board.
  - - Task Advances allow you to skip one step in any task. They can only be obtained as MultiworldGG items. Make sure you keep them for when you need them most; if you use one, it won't come back!
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Castlevania_-_Circle_of_the_Moon

- Setup files: Castlevania_-_Circle_of_the_Moon/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apcvcotm
- Connection hints:
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - 5. Enter the Archipelago server address (the one you connected your client to), slot name, and password.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases/latest).
  - - A Castlevania: Circle of the Moon ROM of the US version specifically. The Archipelago community cannot provide this.
  - The Castlevania Advance Collection ROM can be used, but it has no audio. The Wii U Virtual Console ROM does not work.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Castlevania_64

- Setup files: Castlevania_64/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apcv64
- Connection hints:
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A Castlevania 64 ROM of the US 1.0 version specifically. The Archipelago community cannot provide this.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Castlevania_Dawn_of_Sorrow

- Setup files: Castlevania_Dawn_of_Sorrow/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apcvdos
- Connection hints:
  - the room will provide you with either a link to download your patch file, or with a zip file containing everyone's patch
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A legally obtained USA ROM of Castlevania Dawn of Sorrow
  - 1. Download and install MultiworldGG from the link above, making sure to install the most recent version.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Cat_Quest

- Setup files: Cat_Quest/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - You connect by opening `...\ArchipelagoRandomizer\SaveData\RoomInfo.json` from your Cat Quest root folder, and adding your player and room information.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Chained_Echoes

- Setup files: Chained_Echoes/setup_en.md
- Mod loaders/tools: BepInEx, BepInEX
- Connection hints:
  - 4. Edit RandomizerOptions.txt to add the world room, username and Password (nothing by default).
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Chatipelago

- Setup files: Chatipelago/setup_en.md
- Connection hints:
  - After rolling the seed, enter the config details into the Chatipelago server.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## ChecksFinder

- Setup files: ChecksFinder/setup_en.md
- Connection hints:
  - - The name of the slot you wish to connect to
  - - The room password (optional)
- Install/patch hints:
  - See the guide on setting up a basic YAML at the MultiworldGG setup
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Choo-Choo_Charles

- Setup files: Choo-Choo_Charles/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - * Type ``/connect <IP> <PlayerName>`` with \<IP\> and \<PlayerName\> found on the hosting MultiworldGG web page in the form ``multiworld.gg:XXXXX`` and ``CCCharles``
  - 6. Send the generated room page to each player
- Install/patch hints:
  - * [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - * The game console must be opened to type MultiworldGG commands, press "F10" key or "`" (or "~") key in querty ("²" key in azerty)
  - * Type ``/connect <IP> <PlayerName>`` with \<IP\> and \<PlayerName\> found on the hosting MultiworldGG web page in the form ``multiworld.gg:XXXXX`` and ``CCCharles``
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Civilization_VI

- Setup files: Civilization_VI/setup_en.md
- Patch extensions mentioned: apcivvi
- Connection hints:
  - 5. Your mod path should look something like `C:\Users\YOUR_USER\Documents\My Games\Sid Meier's Civilization VI\Mods\civilization_archipelago_mod`. If everything was done correctly you can now connect to the game.
  - 4. To connect to the room open the MultiworldGG Launcher, from within the launcher open the Civ6 client and connect to the room. Once connected to the room enter your slot name and if everything went right you should now be connected.
  - - If boostsanity is enabled and those items are not being sent out but regular techs are, make sure you placed the files from your new room in the mod folder.
- Install/patch hints:
  - # Setup Guide for Civilization VI MultiworldGG
  - This guide is meant to help you get up and running with Civilization VI in MultiworldGG. Note that this requires you to have both Rise & Fall and Gathering Storm installed. This will not work unless both of those DLCs are enabled.
  - - Installed [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## CrossCode

- Setup files: CrossCode/setup_en.md
- Connection hints:
  - 3. Click the "Create New Room" link.
- Install/patch hints:
  - # Setup Guide for the CrossCode Randomizer for MultiworldGG
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Crystal_Project

- Setup files: Crystal_Project/setup_en.md
- Connection hints:
  - ## Connect to the MultiServer
  - 1. Fill out the hostname and port (multiworld.gg: #####), slot name, and password (if applicable). You can use your keyboard to type, or you can hit the Paste button on controller.
  - After you've successfully connected once, your save file will automatically reconnect to the multiworld the next time you open the game.
- Install/patch hints:
  - ## Switching Between Different Versions of MultiworldGG
  - See [MultiworldGG Multiworld Setup Guide](/tutorial/Archipelago/setup/en#generating-a-game) for a more in-depth explanation!
  - 1. Start a new game in Crystal Project. An AP/MultiworldGG connection screen will appear.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Cuphead

- Setup files: Cuphead/setup_en.md
- Mod loaders/tools: BepInEx
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Dark_Souls_II

- Setup files: Dark_Souls_II/setup_en.md
- Connection hints:
  - - In that console type `/connect server_address:port slot_name password`, replacing the correct values. The password is optional and the slot name is the name you placed in the yaml file.
  - - For example, if you host in MultiworldGG's website it would look something like `/connect multiworldgg:12345 JohnSouls`.
  - ### **I get `Access is denied` when trying to connect to archipelago.**
- Install/patch hints:
  - - For example, if you host in MultiworldGG's website it would look something like `/connect multiworldgg:12345 JohnSouls`.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Dark_Souls_III

- Setup files: Dark_Souls_III/setup_en.md
- Connection hints:
  - Before you first connect to a multiworld, you need to generate the local data files for your world's
  - 1. Before you first connect to a multiworld, run `randomizer\DS3Randomizer.exe`.
  - 2. Put in your MultiworldGG room address (usually something like `multiworld.gg:12345`), your player
- Install/patch hints:
  - - [Dark Souls III AP Client]
  - [Dark Souls III AP Client]: https://github.com/nex3/Dark-Souls-III-Archipelago-client/releases/latest
  - First, download the client from the link above (`DS3.Archipelago.*.zip`). It doesn't need to go
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Dark_Souls_Remastered

- Setup files: Dark_Souls_Remastered/setup_en.md
- Emulators: PCSX2
- Connection hints:
  - ## Join a Multiworld Room
  - * Fill in your host, slot and password (if required) and press Connect.
- Install/patch hints:
  - * [Latest DSAP Client](https://github.com/ArsonAssassin/DSAP/releases/)
  - * Open the client folder you unzipped earlier and run the included .exe.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Diddy_Kong_Racing

- Setup files: Diddy_Kong_Racing/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - - Under N64 enable "Use Expansion Slot". (The N64 menu only appears after loading a ROM.)
  - ## Connect To The Multiserver
  - the server uses password, then it will prompt after connection).
- Install/patch hints:
  - - A Diddy Kong Racing v1.0 ROM (USA ONLY).
  - - Under N64 enable "Use Expansion Slot". (The N64 menu only appears after loading a ROM.)
  - It is strongly recommended to associate N64 rom extensions (*.n64, *.z64) to the EmuHawk we've just installed. To do so,
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Digimon_World

- Setup files: Digimon_World/setup_en.md
- Emulators: Duckstation, duckstation
- Connection hints:
  - 4. Enter your host (including port), slot name and password (if set)
- Install/patch hints:
  - - [DWAP Client](https://github.com/ArsonAssassin/DWAP/releases)
  - - Digimon World US ROM. The Archipelago community cannot provide this.
  - The DWAP Client is a C# client which reads memory addresses from ePSXe and communicates with MultiworldGG. Location Checks are sent when specific memory addresses update, and items are given by editing the memory addresses.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## DLCQuest

- Setup files: DLCQuest/setup_en.md
- Mod loaders/tools: BepinEx, BepInEx, modloader
- Connection hints:
  - ### Connect to the MultiServer
  - - Locate the file "ArchipelagoConnectionInfo.json", at the root of your modded installation. You can edit this file with any text editor, and you need to enter the server ip address, port and your slotname into the relevant fields.
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - It will offer the choice of creating a desktop shortcut for the modded launcher
  - - Run BepInEx.NET.Framework.Launcher.exe. If you opted for a desktop shortcut, you will find it with an icon and a more recognizable name.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Donkey_Kong_64

- Setup files: Donkey_Kong_64/setup_en.md
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Donkey_Kong_Country

- Setup files: Donkey_Kong_Country/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, snes9x
- Patch extensions mentioned: apdkc
- Connection hints:
  - ### Connect to the multiworld
  - server has a password, then write `/connect <address>:<port> [password]` in the bottom text box)
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - [SNI](https://github.com/alttpo/sni/releases). This is automatically included with your MultiworldGG installation above.
  - - Software capable of loading and playing SNES ROM files:
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Donkey_Kong_Country_2

- Setup files: Donkey_Kong_Country_2/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, snes9x
- Patch extensions mentioned: apdkc2
- Connection hints:
  - ### Connect to the multiworld
  - server has a password, then write `/connect <address>:<port> [password]` in the bottom text box)
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - [SNI](https://github.com/alttpo/sni/releases). This is automatically included with your MultiworldGG installation above.
  - - Software capable of loading and playing SNES ROM files:
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Donkey_Kong_Country_3

- Setup files: Donkey_Kong_Country_3/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apdkc3
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the Archipelago Server
- Install/patch hints:
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your legally obtained Donkey Kong Country 3 ROM file, probably named `Donkey Kong Country 3 - Dixie Kong's Double Trouble! (USA) (En,Fr).sfc`
  - 2. The first time you do local generation or patch your game, you will be asked to locate your base ROM file.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Dont_Starve_Together

- Setup files: Dont_Starve_Together/setup_en.md
- Connection hints:
  - - Open the MultiworldGG launcher and run the Don't Starve Together client. Connect to the MultiworldGG server.
  - - Once you load in and select your character, the client should automatically connect to DST and you can start playing!
  - you connect to MultiworldGG again. However, offline progress is lost if you regenerate your world!
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - - Install MultiworldGG.
  - - Follow MultiworldGG's basic tutorial on how to generate a game. [Basic Multiworld Setup Guide](/tutorial/Archipelago/setup/en)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## DOOM_1993

- Setup files: DOOM_1993/setup_en.md
- Connection hints:
  - 3. Enter the Archipelago server address, slot name, and password (if you have one)
  - 2. Run `crispy-apdoom -game doom -apserver <server> -applayer <slot name>`, where:
  - - `<slot name>` is your slot name; if it contains spaces, surround it with double quotes
- Install/patch hints:
  - - [MultiworldGGTextClient](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ## Joining a MultiWorld Game (via Launcher)
  - 1. Launch apdoom-launcher.exe
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## DOOM_II

- Setup files: DOOM_II/setup_en.md
- Connection hints:
  - 3. Enter the Archipelago server address, slot name, and password (if you have one)
  - 2. Run `crispy-apdoom -game doom2 -apserver <server> -applayer <slot name>`, where:
  - - `<slot name>` is your slot name; if it contains spaces, surround it with double quotes
- Install/patch hints:
  - - [MultiworldGGTextClient](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ## Joining a MultiWorld Game (via Launcher)
  - 1. Launch apdoom-launcher.exe
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## DORONKO_WANKO

- Setup files: DORONKO_WANKO/setup_en.md
- Connection hints:
  - 2. For the `Url` field, enter the address of the server, such as `multiworld.gg:38281`. Your server host should be able to tell you this.
  - 4. For the `Password` field, enter the server password if one exists; otherwise leave this field blank.
  - 5. Save the file, and run DORONKO WANKO. You should connect to the room upon selecting `New Game` or `Continue`
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## DREDGE

- Setup files: DREDGE/setup_en.md
- Connection hints:
  - Once the terminal is open, type: `ap connect <hostname> <port> <slot name> [-p <password>]`
  - - You can include spaces in your slot name (e.g. `ap connect multiworld.gg 38281 boat guy`).
  - - The `-p` (or `password=<value>`) part is **optional** — only needed if the server requires a password.
- Install/patch hints:
  - You can see the [basic multiworld setup guide](/tutorial/Archipelago/setup/en) here on the MultiworldGG website to learn
  - about why MultiworldGG uses YAML files and what they're for.
  - You can use the [game options page](/games/Dredge/player-options) here on the MultiworldGG
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## EarthBound

- Setup files: EarthBound/setup_en.md
- Emulators: BizHawk, BSNES, RetroArch, snes9x
- Patch extensions mentioned: apeb
- Connection hints:
  - ### Connect to the client
  - ### Connect to the MultiworldGG Server
  - The client will attempt to reconnect to the new server address, and should momentarily show "Server Status: Connected".
- Install/patch hints:
  - # EarthBound MultiworldGG Randomizer Setup Guide
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Ender_Lilies

- Setup files: Ender_Lilies/setup_en.md
- Connection hints:
  - Remember the name you type in the `Player Name` box; that's the "slot name" the client will ask you for when you attempt to connect!
  - 4. In the "Archipelago" sub-tab, fill the Server/Port, Password (if any) and Slot name.
- Install/patch hints:
  - This guide contains instructions on how to install and troubleshoot the Ender Lilies client, as well as where to obtain a config file for Ender Lilies.
  - - [The most recent MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases)
  - Remember the name you type in the `Player Name` box; that's the "slot name" the client will ask you for when you attempt to connect!
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Factorio

- Setup files: Factorio/setup_en.md
- Connection hints:
  - 6. Click on "Connect to address"
  - Clients may connect to this server.
  - 4. Obtain the MultiworldGG Server address from the website's host room, or from the server host.
- Install/patch hints:
  - - MultiworldGG: [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - Connecting to someone else's game is the simplest way to play Factorio with MultiworldGG. It allows multiple people to
  - In MultiworldGG, multiple Factorio worlds may be played simultaneously. Each of these worlds must be hosted by a Factorio
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Factorio_-_Space_Age_Without_Space

- Setup files: Factorio_-_Space_Age_Without_Space/setup_en.md
- Connection hints:
  - 6. Click on "Connect to address"
  - Clients may connect to this server.
  - 6. Obtain the MultiworldGG Server address from the website's host room, or from the server host.
- Install/patch hints:
  - - **Factorio Client** - The Factorio instance which will be used to play the game.
  - - **Archipelago Client** - The middleware software used to connect the Factorio Server to the Archipelago Server.
  - - One running instance of `ArchipelagoFactorioClient.exe` (the Archipelago Client) per Factorio world
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Faxanadu

- Setup files: Faxanadu/setup_en.md
- Connection hints:
  - 2. From the Main menu, go to the `ARCHIPELAGO` menu. Enter the server's address, slot name, and password. Then select `PLAY`.
- Install/patch hints:
  - - Faxanadu ROM, English version
  - - [MultiworldGGTextClient](https://github.com/MultiworldGG/MultiworldGG/releases)
  - 2. Copy your rom `Faxanadu (U).nes` into the newly extracted folder.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Final_Fantasy_IV_Free_Enterprise

- Setup files: Final_Fantasy_IV_Free_Enterprise/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apff4fe
- Connection hints:
  - ### Connect to the client
  - ### Connect to the Archipelago Server
  - The client will attempt to reconnect to the new server address, and should momentarily show "Server Status: Connected".
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Software capable of loading and playing SNES ROM files
  - - Your legally obtained Final Fantasy IV 1.1 ROM file, probably named `
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Final_Fantasy_Mystic_Quest

- Setup files: Final_Fantasy_Mystic_Quest/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apmq, AppImage
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the Archipelago Server
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your legally obtained Final Fantasy Mystic Quest NA 1.0 or 1.1 ROM file, probably named `Final Fantasy - Mystic Quest (U) (V1.0).sfc` or `Final Fantasy - Mystic Quest (U) (V1.1).sfc`
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Final_Fantasy_Tactics_Advance

- Setup files: Final_Fantasy_Tactics_Advance/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apffta
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - 2. Follow the general MultiworldGG instructions for [generating a game](../../Archipelago/setup/en#generating-a-game).
  - This will generate an output file for you. Your patch file will have the `.apffta` file extension.
  - 4. Select "Open Patch" on the left side and select your patch file.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Final_Fantasy_V_Career_Day

- Setup files: Final_Fantasy_V_Career_Day/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apffvcd
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the MultiworldGG Server
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases). Make sure to check the box for `SNI Client`
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your legally obtained Final Fantasy V Career Day ROM file, probably named `Final Fantasy V (Japan).sfc`
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Flipwitch_Forbidden_Sex_Hex

- Setup files: Flipwitch_Forbidden_Sex_Hex/setup_en.md
- Install/patch hints:
  - - [MultiworldGG Client](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - The [Latest Flipwitch AP Client](https://github.com/Witchybun/FlipwitchAPClient/releases).
  - * Download the latest Flipwitch Client (FlipwitchXXX.zip)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Getting_Over_It

- Setup files: Getting_Over_It/setup_en.md
- Mod loaders/tools: BepInEx
- Install/patch hints:
  - # Setup Guide for Getting Over It in MultiworldGG
  - - [Checking Over It](https://github.com/BlastSlimey/CheckingOverIt/releases) (client mod)
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Golden_Sun_The_Lost_Age

- Setup files: Golden_Sun_The_Lost_Age/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apgstla
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English Golden Sun The Lost Age ROM. The Archipelago community cannot provide this.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## gzDoom

- Setup files: gzDoom/setup_en.md
- Emulators: GZDoom, gzDoom
- Install/patch hints:
  - - A launcher like [DoomRunner](https://github.com/Youda008/DoomRunner) is optional but highly recommended
  - - MultiworldGG will emit another pk3; add it to your load order **at the end**
  - - **Multiplayer**: start `GZDoom Client` from the Archipelago launcher; it will
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Hatsune_Miku_Project_Diva_Mega_Mix

- Setup files: Hatsune_Miku_Project_Diva_Mega_Mix/setup_en.md
- Connection hints:
  - Make sure the **Mega Mix Client** is open and connected to a room.
- Install/patch hints:
  - - The game can be played in MultiworldGG without the Extra Song Pack DLC.
  - 3. Start the **Mega Mix Client** from the MultiworldGG Launcher
  - ├ DivaMegaMix.exe   <─ game, select if prompted by Client/JSON generator
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Heretic

- Setup files: Heretic/setup_en.md
- Connection hints:
  - 3. Enter the Archipelago server address, slot name, and password (if you have one)
  - 2. Run `crispy-apheretic -apserver <server> -applayer <slot name>`, where:
  - - `<slot name>` is your slot name; if it contains spaces, surround it with double quotes
- Install/patch hints:
  - - [MultiworldGGTextClient](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ## Joining a MultiWorld Game (via Launcher)
  - 1. Launch apdoom-launcher.exe
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## HITMAN_World_of_Assassination

- Setup files: HITMAN_World_of_Assassination/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - If your Peacock Server is not running locally on your machine, you must change the address that the Client uses to connect to Peacock in the `host.yaml` of your MultiworldGG installation.
  - 3. Enter room's address and port (e.g. `multiworld.gg:12345`) into the top text field of the client and click Connect.
  - 4. Enter your slot name.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - If you already have installed Peacock, it is still recommended to install a seperate instance for MultiworldGG, as the obtaining of items and unlocks will mess with your Peacock progression.
  - To install the APworld, simply double click it or drag and drop it into the running Archipelago Launcher. Alternatively, you can choose the option `Install APWorld` in the MultiworldGG Launcher or drop the `.apworld` file into the `custom_world`-folder in your Archipelago install. This is not needed if you use MultiworldGG.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Hollow_Knight

- Setup files: Hollow_Knight/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - 4. Enter the correct settings for your Archipelago server.
- Install/patch hints:
  - [commands guide](/tutorial/MultiworldGG/commands/en). You can use the MultiworldGG Text Client to do this,
  - which is included in the latest release of the [Archipelago software](https://github.com/MultiworldGG/MultiworldGG/releases/latest).
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Hunie_Pop

- Setup files: Hunie_Pop/setup_en.md
- Mod loaders/tools: bepinex
- Connection hints:
  - ## How to join a Multiworld Room
  - * Play and connect to a Archipelago server
- Install/patch hints:
  - * [HuniePop AP Plugin](https://github.com/DotsofdarknessArchipelago/HuniePop-Archiepelago-Client/releases)
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Hunie_Pop_2

- Setup files: Hunie_Pop_2/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - ## How to join a Multiworld Room
  - * Play and connect to a Archipelago server
- Install/patch hints:
  - * [HuniePop 2 AP Plugin](https://github.com/DotsofdarknessArchipelago/HunniePop2-Archipelago-Client/releases)
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Hylics_2

- Setup files: Hylics_2/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - To connect to a MultiworldGG server, open the in-game console (default key: `/`) and use the command `/connect [address:port] [name] [password]`. The port and password are both optional - if no port is provided then the default port of 38281 is used.
- Install/patch hints:
  - To connect to a MultiworldGG server, open the in-game console (default key: `/`) and use the command `/connect [address:port] [name] [password]`. The port and password are both optional - if no port is provided then the default port of 38281 is used.
  - - `/disconnect` - Disconnect from an MultiworldGG server.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Inscryption_Beta

- Setup files: Inscryption_Beta/setup_en.md
- Mod loaders/tools: BepInEx, r2modman, thunderstore, Thunderstore
- Connection hints:
  - 3. On the next screen, enter the information needed to connect to the MultiWorld server, then press the `Connect` button.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Into_the_Breach

- Setup files: Into_the_Breach/setup_en.md
- Mod loaders/tools: ModLoader
- Patch extensions mentioned: apworld
- Install/patch hints:
  - - MultiworldGG
  - To do the full setup, extract everything found in the itb_apworld_and_dependancies archive that can be downloaded in the releases of [ITB randomizer for AP](https://github.com/Ishigh1/ITB-randomizer-for-AP/releases) your local MultiworldGG install, merging folders if asked.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Jak_and_Daxter_The_Precursor_Legacy

- Setup files: Jak_and_Daxter_The_Precursor_Legacy/setup_en.md
- Emulators: OPENGOAL, OpenGOAL, opengoal
- Connection hints:
  - - Once you see `CONNECT TO ARCHIPELAGO NOW` on the title screen, use the text client to connect to the MultiworldGG server. This will communicate your current settings and slot info to the game.
  - - Once you reach the title screen, connect to the MultiworldGG server **BEFORE** you load your save file.
  - - Instead of choosing `New Game` in the title menu, choose `Load Game`, then choose the save file **THAT HAS YOUR CURRENT SLOT NAME.**
- Install/patch hints:
  - - [The OpenGOAL Launcher](https://opengoal.dev/)
  - ## Installation via OpenGOAL Launcher
  - - Follow the installation process for the official OpenGOAL Launcher. See [here](https://opengoal.dev/docs/usage/installation).
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Jigsaw

- Setup files: Jigsaw/setup_en.md
- Install/patch hints:
  - # Jigsaw Puzzles for MultiworldGG Setup Guide
  - - Create your MultiworldGG game.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Kindergarten_2

- Setup files: Kindergarten_2/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - ### Connect to the MultiServer
  - - Edit the `ArchipelagoConnectionInfo.json` and enter the ip, port, slot name, etc.
- Install/patch hints:
  - # Kindergarten 2 MultiworldGG Setup Guide
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - Your MultiworldGG save will take up all 3 save slots, and your original saves will be unaffacted, but inaccessible as long as you remain modded (This is why making a copy of the original, to have a vanilla game available, was recommended).
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Kingdom_Hearts_2

- Setup files: Kingdom_Hearts_2/setup_en.md
- Mod loaders/tools: ModLoader
- Connection hints:
  - 2. This mod overwrites slot 99 with an autosave. Make sure to copy your save data to another slot before installing.
  - When you generate a game you will see a download link for a KH2 .zip seed on the room page. Download the seed then open OpenKH Mod Manager and click the green plus and "Select and install Mod Archive".<br>
  - After Installing the seed click "Mod Loader -> Build/Build and Run". Every slot is a unique mod to install and will be needed be repatched for different slots/rooms.
- Install/patch hints:
  - # Kingdom Hearts 2 MultiworldGG Setup Guide
  - 3. Install the mod `KH2FM-Mods-Num/GoA-ROM-Edition` using OpenKH Mod Manager
  - - Needed for MultiworldGG
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Kirby_64_-_The_Crystal_Shards

- Setup files: Kirby_64_-_The_Crystal_Shards/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apk64cs
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English Kirby 64 - The Crystal Shards ROM.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Kirbys_Dream_Land_3

- Setup files: Kirbys_Dream_Land_3/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, snes9x
- Patch extensions mentioned: apkdl3
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - These should automatically connect to SNI. If this is the first time launching, you may be prompted to allow it to
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
  - - An emulator capable of connecting to SNI with ROM access. Any one of the following will work:
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Landstalker_-_The_Treasures_of_King_Nole

- Setup files: Landstalker_-_The_Treasures_of_King_Nole/landstalker_setup_en.md
- Emulators: BizHawk, Bizhawk, RetroArch, Retroarch, retroarch
- Connection hints:
  - Once the game has been created, you need to connect to the server using the Landstalker MultiworldGG client.
  - - **Slot name**: Put the player name you specified in your YAML config file in this field.
  - - **Password**: If the server has a password, put it there.
- Install/patch hints:
  - - [Landstalker MultiworldGG client](https://github.com/Dinopony/randstalker-archipelago/releases) (only available on Windows)
  - - A Landstalker US ROM file dumped from the original cartridge
  - - Unzip the Landstalker MultiworldGG client archive into its own folder
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.

## Lego_Star_Wars_The_Complete_Saga

- Setup files: Lego_Star_Wars_The_Complete_Saga/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - multiworld seed and slot name they were connected to. To resume playing a multiworld at a later time, the same save slot
  - To connect to the multiworld server, run the **Lego Star Wars: The Complete Saga Client** from the **MultiworldGG Launcher**
  - The first time a save file connects to a MultiworldGG server, the slot name needs to be entered. After that, the slot
- Install/patch hints:
  - # Setup Guide for Lego Star Wars: The Complete Saga in MultiworldGG
  - - MultiworldGG
  - - [Universal Tracker](https://discord.com/channels/731205301247803413/1367270230635839539) for MultiworldGG (links to the
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Lethal_Company

- Setup files: Lethal_Company/setup_en.md
- Mod loaders/tools: Thunderstore, thunderstore, UnityExplorer
- Patch extensions mentioned: apworld
- Connection hints:
  - - name: Enter your desired slot name here, could be almost anything. {player} or {PLAYER} are replaced with the slot number, while {number} or {NUMBER} will increment once for each duplicate YAML name
  - the Spoiler log. Click on 'Create New Room', and you're done! The MultiworldGG server is now
  - and boot up a save. Once you are ready, you can type /connect multiworld.gg:port in the
- Install/patch hints:
  - You also need to install the latest version of the [MultiworldGG Multiworld Randomizer](https://github.com/MultiworldGG/MultiworldGG/releases/latest) client.
  - Once your YAML is configured, navigate to your MultiworldGG installation folder (will vary
  - C:\Program Files\MultiworldGG). In the 'Players' folder, paste your YAML file as well as the
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Lingo

- Setup files: Lingo/setup_en.md
- Connection hints:
  - 4. Enter the Archipelago address, slot name, and password into the fields.
- Install/patch hints:
  - - [MultiworldGG Text Client](https://github.com/MultiworldGG/MultiworldGG/releases)
  - download the client, as well as update it whenever an update is available.
  - while the client is installed.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Links_Awakening_DX_Beta

- Setup files: Links_Awakening_DX_Beta/setup_en.md
- Emulators: BizHawk, Retroarch, retroarch, RetroArch
- Patch extensions mentioned: apladx
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the MultiworldGG Server
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Software capable of loading and playing GBC ROM files
  - - Your American 1.0 ROM file, probably named `Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).gbc`
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Lufia_II_Ancient_Cave

- Setup files: Lufia_II_Ancient_Cave/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apl2ac
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the Archipelago Server
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your American ROM file, probably named `Lufia II - Rise of the Sinistrals (USA).sfc`
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Luigis_Mansion

- Setup files: Luigis_Mansion/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: aplm
- Connection hints:
  - 5. Click the "Create New Room" link. You are now able to download your patch file from here.
  - - This action will automatically run the Luigi's Mansion Client (and connect to the webhost if the patch was downloaded from there).
  - 10. To rejoin the room later, you need to open the webpage, open the LM Client through the MultiworldGG Launcher, and open the patched ISO with Dolphin.
- Install/patch hints:
  - - [MultiworldGG Multiworld Suite](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Your American ISO file, probably named `Luigi's Mansion (NTSC-U).iso`. Support for the PAL version is planned in the distant future
  - 1. Download and install the MultiworldGG Multiworld Suite from the link above, making sure to install the most recent version.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Lunacid

- Setup files: Lunacid/setup_en.md
- Patch extensions mentioned: apworld
- Install/patch hints:
  - - [MultiworldGG Client](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - Install MultiworldGG Client.  Documentation is [here](/tutorial/Archipelago/setup/en).
  - - Once installed, go to where your client is installed, go to custom_worlds, drop the attached .apworld here.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Madou_Monogatari_Hanamaru_Daiyouchienji

- Setup files: Madou_Monogatari_Hanamaru_Daiyouchienji/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, snes9x
- Patch extensions mentioned: apkdl3
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - These should automatically connect to SNI. If this is the first time launching, you may be prompted to allow it to
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
  - - An emulator capable of connecting to SNI with ROM access. Any one of the following will work:
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Mario__Luigi_Superstar_Saga

- Setup files: Mario__Luigi_Superstar_Saga/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apmlss
- Connection hints:
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in Bizhawk client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ### Obtain your GBA patch file
  - Double-click on your `.apmlss` file to start your client and start the ROM patch process. Once the process is finished, the client and the emulator will be started automatically (if you associated the extension
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Mario_Kart_64

- Setup files: Mario_Kart_64/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apmk64
- Connection hints:
  - ## Joining a Multiworld Room
  - * Download the .apmk64 file offered in the MWGG room.
  - * The MWGG BizHawk client opens automatically - connect to the server by entering the server IP for the multiworld room.
- Install/patch hints:
  - * Click Open Patch in the MWGG Launcher and point it to .apmk64 player file (or drag/drop it onto the the launcher exe)
  - * Click Open Patch in the MWGG Launcher and point it to .apmk64 player file (or drag/drop it onto the the launcher exe)
  - * The MWGG BizHawk client opens automatically - connect to the server by entering the server IP for the multiworld room.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Mario_Kart_Double_Dash

- Setup files: Mario_Kart_Double_Dash/setup_en.md
- Emulators: dolphin, Dolphin
- Connection hints:
  - All players must provide the room host with a YAML file containing the settings for their world. Modify the template yaml from the [releases](https://github.com/aXu-AP/archipelago-double-dash/releases) page to your liking.
  - Once you're happy with your settings, provide the room host with your YAML file and proceed to the step "Connecting to a Room". If you want to play by yourself, you need to host the game yourself, see the next section.
  - ## Connecting to a Room
- Install/patch hints:
  - * [MultiworldGG](https://multiworld.gg/tutorial/Archipelago/setup/en) 0.7.100 or newer
  - * A rom of Mario Kart: Double Dash!! (NTSC-U / USA version)
  - * Format (`.iso`, `.rvz`) doesn't matter.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Mega_Man_2

- Setup files: Mega_Man_2/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apmm2
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English Mega Man 2 ROM. Alternatively, the [Mega Man Legacy Collection](https://store.steampowered.com/app/363440/Mega_Man_Legacy_Collection/) on Steam.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Mega_Man_3

- Setup files: Mega_Man_3/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apmm3
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English Mega Man 3 ROM. Alternatively, the [Mega Man Legacy Collection](https://store.steampowered.com/app/363440/Mega_Man_Legacy_Collection/) on Steam.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Mega_Man_X3

- Setup files: Mega_Man_X3/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, snes9x
- Patch extensions mentioned: apmmx3
- Connection hints:
  - ### Connect to the multiworld
  - server has a password, then write `/connect <address>:<port> [password]` in the bottom text box)
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - [SNI](https://github.com/alttpo/sni/releases). This is automatically included with your MultiworldGG installation above.
  - - Software capable of loading and playing SNES ROM files:
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## MegaMan_Battle_Network_3

- Setup files: MegaMan_Battle_Network_3/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apbn3
- Connection hints:
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - A US MegaMan Battle Network 3 Blue Rom. If you have the [MegaMan Battle Network Legacy Collection Vol. 1](https://store.steampowered.com/app/1798010/Mega_Man_Battle_Network_Legacy_Collection_Vol_1/)
  - on Steam, you can obtain a copy of this ROM from the game's files, see instructions below.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Meritous

- Setup files: Meritous/setup_en.md
- Connection hints:
  - 3. Start a new game. If it is able to successfully connect to the AP server, "Connected" will show up in the bottom left of the game screen for a few seconds.
  - "password": null,
  - - `ap-enable`: Enables the game to connect to the Archipelago server. If this is `false` or missing, it will generate a local item randomizer.
- Install/patch hints:
  - [commands guide](/tutorial/MultiworldGG/commands/en). As this game does not have an in-game text client at the moment,
  - You can optionally connect to the multiworld using the text client, which can be found in the
  - [MultiworldGG installation](https://github.com/MultiworldGG/MultiworldGG/releases) as MultiworldGG Text Client to
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Metroid_Prime

- Setup files: Metroid_Prime/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: apmp1, apworld
- Connection hints:
  - All players playing _Metroid Prime_ must provide the room host with a YAML file containing the player options for their world.
  - ## Hosting a Room
  - Once you have the zip file corresponding to your multiworld, follow [MultiworldGG Setup Guide: Hosting a MultiworldGG Server](https://multiworld.gg/tutorial/Archipelago/setup/en#hosting-an-archipelago-server) to host a room.
- Install/patch hints:
  - # Setup Guide for Metroid Prime MultiworldGG
  - This guide is meant to help you get up and running with Metroid Prime APWorld with MultiworldGG.
  - The following are required in order to play _Metroid Prime_ in MultiworldGG:
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Metroid_Zero_Mission

- Setup files: Metroid_Zero_Mission/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apmzm
- Connection hints:
  - ### Connect to the Multiserver
  - press enter (if the server uses a password, type in the bottom text field
  - `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A Metroid - Zero Mission ROM. Only US is supported. You do not need one to generate a seed, only to play it.
  - It is strongly recommended to associate the GBA ROM extension (\*.gba) to the BizHawk we've just
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Momodora_Moonlit_Farewell

- Setup files: Momodora_Moonlit_Farewell/setup_en.md
- Connection hints:
  - You can copy the Save0.sav (or any corresponding slot) into a backup folder (Save0 is slot 1, save4 is slot 5).
  - If you already have a skill received, but haven't checked its location yet, entering the room where you get the location will automatically send the location check, with a few exceptions:
  - - Entering the Harpy fight room will remove your Dash until you defeat her and pick up the Sacred Anemone. If you don't get it back, go back to the Title Screen and reload your save.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Monster_Sanctuary

- Setup files: Monster_Sanctuary/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - * Once you start the game with the client mod, you can connect to an MWGG server by selecting either "New Game" or "Continue".
  - * If you are starting a new file, you will be prompted to enter the connection information for the MWGG server. This information is then saved along with the game's file slot, and will allow you to quickly reconnect when continuing the save file.
  - * When you want to Continue a save file, if that save file has connection information associated with it, you will be prompted to connect to that MWGG server before loading the game.
- Install/patch hints:
  - * Once you start the game with the client mod, you can connect to an MWGG server by selecting either "New Game" or "Continue".
  - NOTE: The client mod prevents you from loading an MWGG save file while not connected to MWGG, and will not let you load a vanilla save file while connected to MWGG.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Muse_Dash

- Setup files: Muse_Dash/setup_en.md
- Connection hints:
  - 2. Enter in the details for the MultiworldGG game, such as the server address with port (e.g. multiworld.gg:38381), username and password.
- Install/patch hints:
  - If you've successfully installed everything, a button will appear in the bottom right which will allow you to log into a MultiworldGG server.
  - - (For instructions on how to generate a MultiworldGG game, refer to the [MultiworldGG Web Guide](/tutorial/Archipelago/setup/en))
  - 2. Enter in the details for the MultiworldGG game, such as the server address with port (e.g. multiworld.gg:38381), username and password.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Noita

- Setup files: Noita/setup_en.md
- Connection hints:
  - *Port*, *Slot*, and *Password* where you can fill in the relevant information.
  - Please note that Noita only allows you to type certain characters for your slot name.
  - Click on the "AP" symbol at the top, then enter the desired address, slot name, and password.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Ocarina_of_Time

- Setup files: Ocarina_of_Time/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apz5
- Connection hints:
  - - Under N64 enable "Use Expansion Slot". This is required for savestates to work.
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - An Ocarina of Time v1.0 ROM.
  - (The N64 menu only appears after loading a ROM.)
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Old_School_Runescape

- Setup files: Old_School_Runescape/setup_en.md
- Connection hints:
  - ### Connect to the Multiserver
  - In the Archipelago Plugin, enter your server information. The `Auto Reconnect on Login For` field should remain blank;
  - it will be populated by the character name you first connect with, and it will reconnect to the AP server whenever that
- Install/patch hints:
  - - If the account being used has been migrated to a Jagex Account, the [Jagex Launcher](https://www.jagex.com/en-GB/launcher)
  - one account through the Jagex Launcher. Note that there is currently no way to _remove_ characters
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## OpenRCT2

- Setup files: OpenRCT2/setup_en.md
- Emulators: OpenRCT2, openrct2
- Connection hints:
  - You'll be using OpenRCT2 to run the Archipelago plugin, which will connect to the built-in OpenRCT2 client in \
  - should automatically connect, and you can connect to the server. Type your server address and port in the \
  - "Connect" box at the top of the client, put in the name for your slot, and you'll be able to select the \
- Install/patch hints:
  - You'll be using OpenRCT2 to run the Archipelago plugin, which will connect to the built-in OpenRCT2 client in \
  - the game to the client, and the client to the server.
  - or any of the expansion packs for either game, you may select the scenario for use in your game of MultiworldGG.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Ori_and_the_Blind_Forest

- Setup files: Ori_and_the_Blind_Forest/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - 5. Fill out the server name, port, slot name, and (optional) password in the upper left text boxes
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Ori_and_the_Will_of_the_Wisps

- Setup files: Ori_and_the_Will_of_the_Wisps/setup_en.md
- Connection hints:
  - Remark: when connection to a game hosted on `multiworld.gg`, make sure to enable secure connection. Disable it if you connect to `localhost` or an IP address.
- Install/patch hints:
  - # Setup Guide for Ori and the Will of the Wisps for MultiworldGG
  - [Client code](https://github.com/ori-community/wotw-rando-client/tree/archipelago)
  - ### Client installation
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## osu

- Setup files: osu/setup_en.md
- Install/patch hints:
  - # osu! Setup Guide for MultiworldGG
  - * Install the MultiworldGG Launcher - the osu! APWorld is included
  - * Name it something informative, e.g., **(YourName) osu!AP Client**.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Overcooked_2

- Setup files: Overcooked_2/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - 3. Sign-in with server address, username and password of the corresponding room you would like to join.
  - - To play online multiplayer, the guest *must* also have the same version of OC2-Modding installed. In order for the game to work, the guest must sign in using the same information the host used to connect to the Archipelago session. Once both host and client are both connected, they may join one another in-game and proceed as normal. It does not matter who hosts the game, and the game's hosts may be changed at any point. You may notice some things are different when playing this way:
  - - When the host loads the campaign, any connected guests are forced to select "Don't Save" when prompted to pick which save slot to use. This is because randomizer uses the Archipelago service as a pseudo "cloud save", so progress will always be synchronized between all participants of that randomized *Overcooked! 2* instance.
- Install/patch hints:
  - - [OC2-Modding Client](https://github.com/toasterparty/oc2-modding/releases) (instructions below)
  - *OC2-Modding* is a general purpose modding framework which doubles as a MultiworldGG Multiworld Client. It works by using Harmony to inject custom code into the game at runtime, so none of the original game files need to be modified in any way.
  - When connecting to a MultiworldGG session using the in-game login screen, a mod file containing all relevant game modifications is automatically downloaded and applied.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Paper_Mario

- Setup files: Paper_Mario/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: appm64, apworld
- Connection hints:
  - 3. Once the patch file has been created, BizHawk should start up automatically with the patched ROM. The Generic BizHawk Client for MultiworldGG will also open, as well as a Lua Console window. At this point all you need to do to connect is enter your room's address and port (e.g. multiworld.gg:38281) into the top text field of the client and click Connect.
- Install/patch hints:
  - - A legally obtained US 1.0 Paper Mario ROM.
  - Follow [the general MultiworldGG instructions](https://multiworld.gg/tutorial/Archipelago/setup/en#generating-a-game) for generating a game, specifically on your local installation. You cannot generate games using the Paper Mario AP World on the website.
  - Follow [the general MultiworldGG instructions](https://multiworld.gg/tutorial/Archipelago/setup/en#hosting-an-archipelago-server) for hosting a MultiworldGG server. You _can_ host games that use the Paper Mario AP World on the website, or you can host it locally.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Paper_Mario_The_Thousand-Year_Door

- Setup files: Paper_Mario_The_Thousand-Year_Door/setup_en.md
- Emulators: dolphin, Dolphin
- Connection hints:
  - Any `.zip` file you generate can be uploaded [here](https://multiworld.gg/uploads) to create a room, which you can join in your client.
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - # Setup Guide for Paper Mario: The Thousand-Year Door MultiworldGG
  - - MultiworldGG: [Latest releases](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A US copy of Paper Mario: The Thousand-Year Door in .iso format. (EU and JP versions are not supported at this time)
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Path_of_Exile

- Setup files: Path_of_Exile/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - - Flask slot upgrades (up to 5 total)
  - - Get your slot name from the host, and the server address (IP or domain) and port.
  - 7. Enter your slot name and server address from step 4. (Example: `127.0.0.1:38281`, slot `Player1`, no password)
- Install/patch hints:
  - # MultiworldGG Randomizer for Path of Exile (PC)
  - - Reads your PoE client logs.
  - 1. **Enter a zone** → the client validates your current gear and progression.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## PEAK

- Setup files: PEAK/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - 3. **Connect to Archipelago**:
  - - If you play in multiplayer mode, only the host should connect to the MultiworldGG server. Note that only the world connected by the host will send and receive items, so consider to only add one world to the seed if you want to play together.
  - ### Cannot Connect to Server
- Install/patch hints:
  - # Guide for PEAK in MultiworldGG
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - If you play in multiplayer mode, only the host should connect to the MultiworldGG server. Note that only the world connected by the host will send and receive items, so consider to only add one world to the seed if you want to play together.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Peaks_of_Yore

- Setup files: Peaks_of_Yore/setup_en.md
- Mod loaders/tools: r2modman, thunderstore, Thunderstore
- Connection hints:
  - and go to the Peaks Of Archipelago config, this is where you connect to
  - the server, enter your slot name (the name entered when preparing your yaml file), the hostname and port so for
  - example: `multiworld.gg:60324` becomes `hostname=multiworld.gg` and `port=60324`, add a password if necessary.
- Install/patch hints:
  - Click on the `Start modded` button in the top left in `r2modman` to start the game with the MultiworldGG mod installed.
  - You can see the [basic multiworld setup guide](/tutorial/Archipelago/setup/en) here on the MultiworldGG website to learn
  - about why MultiworldGG uses YAML files and what they're for.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Phoenotopia_Awakening

- Setup files: Phoenotopia_Awakening/setup_en.md
- Mod loaders/tools: bepinex, BepInEx
- Install/patch hints:
  - * PhoA Archipelago Mod: [Github](https://github.com/Amphyros/Phoenotopia-Awakening-AP-client)
  - 2. Download the latest release of the [Phoa Archipelago Mod](https://github.com/Amphyros/Phoenotopia-Awakening-AP-client/releases). The release has a zipfile containing a bunch of .dll files. Extract these files to the `BepInEx\plugins` folder in your game folder.
  - 1. File an issue at the repository on [Github](https://github.com/Amphyros/Phoenotopia-Awakening-AP-client/issues)
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Placid_Plastic_Duck_Simulator

- Setup files: Placid_Plastic_Duck_Simulator/setup_en.md
- Mod loaders/tools: bepinex, BepInEx
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Pokemon_Crystal

- Setup files: Pokemon_Crystal/setup_en.md
- Emulators: Bizhawk, BizHawk, mGBA, mgba
- Patch extensions mentioned: apworld
- Connection hints:
  - 5. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 5. Enter the MultiworldGG server address (the one you connected your client to), slot name, and password. If you did not set a password for your room, leave that field empty.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English (UE) Pokémon Crystal v1.0 or v1.1 ROM. The Archipelago community cannot provide this.
  - - A valid v1.1 ROM can be extracted from the 3DS eShop release of the game.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Pokemon_Emerald

- Setup files: Pokemon_Emerald/setup_en.md
- Emulators: BizHawk
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - 5. Enter the Archipelago server address (the one you connected your client to), slot name, and password.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An English Pokémon Emerald ROM. The Archipelago community cannot provide this.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.

## Pokemon_FireRed_and_LeafGreen

- Setup files: Pokemon_FireRed_and_LeafGreen/setup_en.md
- Emulators: bizhawk, Bizhawk, BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - 5. Enter the Archipelago server address (the one you connected your client to), slot name, and password.
- Install/patch hints:
  - * [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - * An English FireRed or LeafGreen ROM
  - Place the `pokemon_frlg.apworld` file in your MultiworldGG installation's `custom_worlds` folder (Default location for Windows: `%programfiles%/Archipelago`).
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Pokemon_Mystery_Dungeon_Explorers_of_Sky

- Setup files: Pokemon_Mystery_Dungeon_Explorers_of_Sky/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apeos, apworld
- Connection hints:
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in BizHawk Client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - Install the download .apworld file with the Archipelago Launcher. In the launcher, click "Generate Template Options" to generate a template YAML for EoS. You can make a copy of this template, and edit it to configure your settings.
  - ### Obtain your NDS patch file
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Pokemon_Red_and_Blue

- Setup files: Pokemon_Red_and_Blue/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apblue, apred
- Connection hints:
  - ### Connect to the Multiserver
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - Pokémon Red and/or Blue ROM files. The Archipelago community cannot provide these.
  - - Under Config > Customize, check the "Run in background" box. This will prevent disconnecting from the client while
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Powerwash_Simulator

- Setup files: Powerwash_Simulator/setup_en.md
- Mod loaders/tools: BepInEx
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Pseudoregalia

- Setup files: Pseudoregalia/setup_en.md
- Connection hints:
  - /connect ip:port slotname password
- Install/patch hints:
  - ## Joining a MultiworldGG Session
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Quake_1

- Setup files: Quake_1/setup_en.md
- Install/patch hints:
  - # Setup Guide for Quake 1 for MultiworldGG
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Rabi-Ribi

- Setup files: Rabi-Ribi/setup_en.md
- Connection hints:
  - 1. Start the Rabi-Ribi Client and connect to the server. Enter your username from your
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - 1. Look for your MultiworldGG install files. By default, the installer puts them in `C:\Program Files\MultiworldGG`.
  - 4. Start the Rabi-Ribi client.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Raft

- Setup files: Raft/setup_en.md
- Connection hints:
  - 3. Type */connect {serverAddress} {username} {password}* into the console and hit Enter.
  - - Example: */connect multiworld.gg:12345 SunnyBat*
  - - If there is no password, the password argument may be omitted (as is the case in the above example).
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Ratchet__Clank_2

- Setup files: Ratchet__Clank_2/setup_en.md
- Emulators: PCSX2, pcsx2
- Patch extensions mentioned: apworld
- Connection hints:
  - **Check** Enable and ensure Slot is set to 28011
  - All players playing Ratchet & Clank 2 must provide the room host with a YAML file containing the settings for their world.
  - Once complete, provide the room host with your YAML file.
- Install/patch hints:
  - # Setup Guide for Ratchet & Clank 2 MultiworldGG
  - This guide is meant to help you get up and running with Ratchet & Clank 2 in your MultiworldGG run.
  - The following are required in order to play Ratchet & Clank 2 in MultiworldGG
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Ratchet__Clank_3

- Setup files: Ratchet__Clank_3/setup_en.md
- Emulators: PCSX2, pcsx2
- Patch extensions mentioned: apworld
- Connection hints:
  - **Check** Enable and ensure Slot is set to 28011
  - ### Connect to the MultiServer
- Install/patch hints:
  - This guide is meant to help you get up and running with Ratchet and Clank 3 in your MultiworldGG run.
  - The following are required in order to play Ratchet and Clank 3 in MultiworldGG
  - - Installed [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases) v0.5.0 or higher.\
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Refunct

- Setup files: Refunct/setup_en.md
- Connection hints:
  - - After a short lag, you'll see "Press 'm' to open the menu" top-left. Navigate to Archipelago, paste your host, port and slot name and press enter to login.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Resident_Evil_2_Remake

- Setup files: Resident_Evil_2_Remake/setup_en.md
- Connection hints:
  - ## Joining a Multiworld Room
  - * "Slot" will be the player name that you configured in the "2. YAML Options" step.
  - * "Password" can be left empty, unless a password was configured for your generated multiworld. (This is usually not the case.)
- Install/patch hints:
  - * Extract the contents of the AP Client zip file into your RE2R game install's folder. There should now be a"RE2R_AP_Client-x.x.x" folder.
  - * In the "Archipelago client for REFramework" window in your game, enter the connection details for your multiworld.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Resident_Evil_3_Remake

- Setup files: Resident_Evil_3_Remake/setup_en.md
- Connection hints:
  - ## Joining a Multiworld Room
  - * "Slot" will be the player name that you configured in the "2. YAML Options" step.
  - * "Password" can be left empty, unless a password was configured for your generated multiworld. (This is usually not the case.)
- Install/patch hints:
  - * Extract the contents of the AP Client zip file into your RE3R game install's folder. (It should ask you to overwrite the "reframework" folder, choose Yes.)
  - * In the "Archipelago client for REFramework" window in your game, enter the connection details for your multiworld.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Rift_of_the_Necrodancer

- Setup files: Rift_of_the_Necrodancer/setup_en.md
- Mod loaders/tools: BepInEx
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Rimworld

- Setup files: Rimworld/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - 2. You may notice the main menu removes the new game and load game buttons - this is normal! Click the "Connect to Archipelago" button
  - 3. Fill in the host name (likely `multiworld.gg:#####`), slot name, and password (if you didn't set a password, leave this empty.)
  - 2. Open RimWorld, and DO NOT connect to a MultiworldGG server
- Install/patch hints:
  - * Latest release of [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - * The `ArchipelagoRimworld.zip` mod file from the [Rimworld Archipelago Client](https://github.com/PhantomOfAres/RimworldArchipelagoClient)
  - * If you are unfamiliar with Archipelago, I recommend reading through the [MultiworldGG Setup Guide](https://multiworld.gg/tutorial/Archipelago/setup/en) to gain an understanding of how MultiworldGG works and to better understand the steps below.
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Risk_of_Rain

- Setup files: Risk_of_Rain/setup_en.md
- Mod loaders/tools: modloader, ModLoader, Modloader
- Connection hints:
  - In RoRML Launcher, click on the `Archipelago Ranomizer` mod.  Once selected, on the right side of the screen will be the mod options.  Input your server address, slot name, and password into the corresponding boxes.
- Install/patch hints:
  - Run `RoRML Launcher.exe` and run through the installer prompt.  It will ask you where your Risk of Rain (2013) install is located.  To find this, On Steam find **Risk of Rain (2013)** in your library.  Right click, select `Manage > Browse local files`.
  - Run `RoRML Launcher` once again and you should now have a new window displaying currently installed mods.  If you followed the prior step, you should see `Archipelago Randomizer` listed in the `Disabled` box.  Click on it and click the "Enable" button.
  - ## Connecting to MultiworldGG Game
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Risk_of_Rain_2

- Setup files: Risk_of_Rain_2/setup_en.md
- Mod loaders/tools: r2modman, thunderstore, Thunderstore
- Connection hints:
  - - Slot Name: your name in the multiworld. This is the name you entered in the YAML.
  - - Password: optional password, leave blank if no password was set.
  - Once everything is entered click the Connect to AP button to connect to the server, and you should be connected!
- Install/patch hints:
  - ## Joining a MultiworldGG Session
  - also optionally connect to the multiworld using the text client, which can be found in the
  - [MultiworldGG installation](https://github.com/MultiworldGG/MultiworldGG/releases).
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Satisfactory

- Setup files: Satisfactory/setup_en.md
- Connection hints:
  - configuring a MultiworldGG slot for Satisfactory,
  - *Any number of Satisfactory Clients may connect to this server.*
  - - **Satisfactory Client** - The Satisfactory instance (game client) with which additional players can use to connect to the same Satisfactory world.
- Install/patch hints:
  - configuring a MultiworldGG slot for Satisfactory,
  - and playing the game with a Satisfactory client.
  - In MultiworldGG, multiple Satisfactory worlds may be played simultaneously.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Saving_Princess

- Setup files: Saving_Princess/setup_en.md
- Connection hints:
  - 1. Go to the room page of the MultiWorld you are going to join.
  - 2. Click on your slot name on the left side.
  - - **server:port** (e.g. `multiworld.gg:38281`)
- Install/patch hints:
  - Once everything is set up, it is recommended to continue launching the game through this method, as it will check for any updates to the mod and automatically apply them.
  - 2. Download and install the latest [MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - 3. Launch `MultiworldGGLauncher` and click on "Saving Princess Client"
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Sea_of_Thieves

- Setup files: Sea_of_Thieves/setup_en.md
- Connection hints:
  - 8. If you would like to play a manual, connect to the host by entering user@ip:port in the connect bar and connecting. Otherwise if you intend to use the auto-tracker, do not connect and continue.
  - 8. Connect to the host with user@ip:port in the connect bar
- Install/patch hints:
  - - Sea of Thieves must be installed (agnostic of launcher, console works too)
  - for each player. Each player needs to save their file and load their appropriate file when starting the client.
  - 6. Each player that is connecting to the hosted world must run the client, the Sea of Thieves client can be found in the
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## shapez

- Setup files: shapez/setup_en.md
- Connection hints:
  - 2. In the main menu, type the slot name, address, port, and password (optional) into the input box.
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Shivers

- Setup files: Shivers/setup_en.md
- Emulators: scummvm, ScummVM
- Connection hints:
  - 6. Enter the Archipelago server address, slot name, and password
- Install/patch hints:
  - - [Shivers Randomizer Client](https://github.com/Shivers-Randomizer/Shivers-Randomizer/releases/latest) Latest release version
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Skyward_Sword

- Setup files: Skyward_Sword/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: apssr, apworld
- Connection hints:
  - - [Dolphin Emulator](https://dolphin-emu.org/download/) (use the dev version!) or a homebrewed Wii or Wii U console that can connect to the Internet.
  - - Your filename in game does not need to match your AP slot name
  - - Connect to the room in your client by running `/connect {address}`. The link to the room should be given to you by the multiworld host. The address will be in the form of `multiworld.gg:XXXXX`.
- Install/patch hints:
  - - The latest release of [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - - A `The Legend of Zelda: Skyward Sword` unrandomzied US 1.00 iso
  - - This includes the [APWorld file and the YAML options file](https://github.com/Battlecats59/SS_APWorld/releases/latest) (not needed for MultiworldGG)
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Sly_Cooper_and_the_Thievius_Raccoonus

- Setup files: Sly_Cooper_and_the_Thievius_Raccoonus/setup_en.md
- Emulators: PCSX2, pcsx2
- Connection hints:
  - * In PCSX2, System -> Settings -> Advanced -> PINE Settings, check Enable and ensure Slot is set to 28011.
  - ### Connect to the MultiServer
- Install/patch hints:
  - - A legally obtained NTSC ISO of Sly Cooper and the Thievius Raccoonus
  - - The built-in Archipelago client, which can be installed [here](https://github.com/ArchipelagoMW/Archipelago/releases). 0.6.1 or higher required.
  - 4. Set Up the Client
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## SM64_Romhack

- Setup files: SM64_Romhack/setup_en.md
- Emulators: bizhawk, BizHawk, Retroarch
- Install/patch hints:
  - A Video guide can be found [here](https://youtu.be/ugKJhTIC1OE), which describes the setup process in detail. Note that in the MultiworldGG client, the json files go in data/sm64hacks/
  - Export the .json file, and put it data/sm64hacks/ in your MultiworldGG folder
  - Open the rom in [Luna's Project64](https://github.com/Luna-Project64), and open the generic bizhawk client (DO NOT use BizHawk, despite the name. It might work on BizHawk, but I haven't tested it and I am not providing any support to BizHawk users.) Go to Debugger -> Scripts (enable debugger if it isn't enabled), download the two .js files from the releases page, put them in the scripts folder (the scripts folder is in the folder that opens when you hit the ... button in the bottom left), run the 'connector_pj64_generic.js', and you should be ready to go!
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.

## SMW_Spicy_Mycena_Waffles

- Setup files: SMW_Spicy_Mycena_Waffles/setup_en.md
- Emulators: BSNES, snes9x
- Patch extensions mentioned: apwaffle
- Connection hints:
  - ### Connect to the client
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - Software or hardware capable of loading and playing SNES ROM files
  - - Your Super Mario World (US) ROM file from the original cartridge. We cannot provide this file.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Sonic_Adventure_2_Battle

- Setup files: Sonic_Adventure_2_Battle/setup_en.md
- Connection hints:
  - 2. For the `Server IP` field under `AP Settings`, enter the address of the server, such as archipelago.gg:38281, your server host should be able to tell you this.
  - 4. For the `Password` field under `AP Settings`, enter the server password if one exists, otherwise leave blank.
  - 6. Create a new save to connect to the MultiWorld game. A "Connected to Archipelago" message will appear if you sucessfully connect. If you close the game during play, you can reconnect to the MultiWorld game by selecting the same save file slot.
- Install/patch hints:
  - 9. Right click the .NET Desktop Runtime exe that was downloaded in step 6, and assuming protontricks was installed correctly, the option to "Open with Protontricks Launcher" should be available. Click that, and in the popup window that opens, select SAModManager.exe. Follow the prompts after this to install the .NET Desktop Runtime for SAModManager. Once it is done, you should be able to successfully launch SAModManager to steam.
  - 1. Run the Launcher.exe which should be in the same folder as the your Sonic Adventure 2: Battle install.
  - 3. Click the `Save settings and launch SONIC ADVENTURE 2` button. (Any mod manager settings will apply even if the game is launched this way rather than through the mod manager)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Sonic_Adventure_DX

- Setup files: Sonic_Adventure_DX/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - 2. For the `Server IP` field under `AP Settings`, enter the address of the server, such as multiworld.gg:54321. Your
  - 4. For the `Password` field under `AP Settings`, enter the server password if one exists, otherwise leave blank.
  - 3. Restart the MultiworldGG Launcher and open the Universal Tracker then connect with your server IP/port and slot name.
- Install/patch hints:
  - launcher. (Not needed with MultiworldGG)
  - 2. On the AP Launcher click on `Generate Template Options`.
  - latest [Universal Tracker](https://discord.com/channels/731205301247803413/1170094879142051912) version. (not needed with MultiworldGG)
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Sonic_Heroes

- Setup files: Sonic_Heroes/setup_en.md
- Connection hints:
  - After a world is hosted, in Reloaded, enable the Mod and click Configure. A UI will open up and you can set a server, port, slot name and password (if required).
  - Finally, launch the game through Reloaded and if the mod is enabled and the correct settings are set the Mod will connect to AP.
  - After a world is hosted, in Reloaded, enable the Mod and click Configure. A UI will open up and you can set a server, port, slot name and password (if required).
- Install/patch hints:
  - Once you've set up the Sonic Heroes application, go to the add mods page in Reloaded-II, search for Sonic Heroes MultiworldGG client, and install the mod.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Spyro_3

- Setup files: Spyro_3/setup_en.md
- Emulators: Duckstation, duckstation
- Patch extensions mentioned: apworld
- Connection hints:
  - 8. Enter your host, slot, and optionally your password.
- Install/patch hints:
  - # Setup Guide for Spyro 3 MultiworldGG
  - As the mandatory client runs only on Windows, no other systems are supported.
  - - MultiworldGG version 0.7.100 or later.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Star_Fox_64

- Setup files: Star_Fox_64/setup_en.md
- Emulators: bizhawk, Bizhawk, BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - * Once you have a room hosted, connect the `Star Fox 64 Client` to the room.
- Install/patch hints:
  - You will need the Star Fox 64 v1.1 ROM. MD5s of supported files:
  - This world is currently in development, which means you must manually place some files in certain locations. The [releases](https://github.com/Auztin/AP-Star-Fox-64/releases/latest) page has all of the files you need. Use the Archipelago Launcher's `Browse Files` button to find the Archipelago directory.
  - * `star_fox_64.apworld` - This needs to go in the `MultiworldGG/custom_worlds/` folder. You can also double click this file to have Archipelago do this for you.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Star_Wars_Episode_I_Racer

- Setup files: Star_Wars_Episode_I_Racer/setup_en.md
- Connection hints:
  - ## Join a Multiworld Room
  - * Run the game and a window should pop up asking if you want to connect to Archipelago. Enter the server address and port (e.g.: multiworld.gg:12345), your slot name, and a password if applicable.
- Install/patch hints:
  - * [SWR AP Client](https://github.com/wcolding/SWR_AP_Client/releases/tag/v0.6.0)
  - * Place the entire scripts folder from the client release into the main directory as well
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Starcraft_2

- Setup files: Starcraft_2/setup_en.md
- Patch extensions mentioned: AppImage
- Connection hints:
  - 2. Type `/connect [server ip]`.
  - - If you're running through the website, the server IP should be displayed near the top of the room page.
  - 3. Type your slot name from your YAML when prompted.
- Install/patch hints:
  - This guide contains instructions on how to install and troubleshoot the StarCraft 2 MultiworldGG client, as well as
  - - [The most recent MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases)
  - 1. Install StarCraft 2 and MultiworldGG using the links above. The StarCraft 2 MultiworldGG client is downloaded by the
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Stardew_Valley

- Setup files: Stardew_Valley/setup_en.md
- Mod loaders/tools: SMAPI
- Connection hints:
  - ### Connect to the MultiServer
  - The password is optional.
  - You will never need to enter this information again for this character, unless your room changes its ip or port.
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - that you can add to your yaml to include them with the MultiworldGG randomization
  - See the guide on setting up a basic YAML at the MultiworldGG setup
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Subnautica

- Setup files: Subnautica/setup_en.md
- Mod loaders/tools: BepInEx
- Connection hints:
  - Use the connect form in Subnautica's main menu to enter your connection information to connect to a MultiworldGG Multiworld.
  - - Host: the full url that you're trying to connect to, such as `multiworld.gg:38281`.
  - - PlayerName: your name in the multiworld. Can also be called "slot name" and is the name you entered when creating your options.
- Install/patch hints:
  - Use the connect form in Subnautica's main menu to enter your connection information to connect to a MultiworldGG Multiworld.
  - - For example, to use the [`!hint` command](/tutorial/MultiworldGG/commands/en#remote-commands), type `say !hint`.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Sudoku

- Setup files: Sudoku/setup_en.md
- Connection hints:
  - This is a HintGame client, which can connect to any multiworld slot, allowing you to play Sudoku to unlock random hints for that slot's locations.
  - - Enter the server address and port number
  - - Enter the name of the slot you wish to connect to
- Install/patch hints:
  - This is a HintGame client, which can connect to any multiworld slot, allowing you to play Sudoku to unlock random hints for that slot's locations.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Super_Mario_64

- Setup files: Super_Mario_64/setup_en.md
- Patch extensions mentioned: apsm64ex
- Connection hints:
  - To join, set the following launch options: `--sm64ap_name YourName --sm64ap_ip ServerIP:Port`.
  - For example, if you are hosting a game using the website, `YourName` will be the name from the Options Page, `ServerIP` is `multiworld.gg` and `Port` the port given on the MultiworldGG room page.
  - Optionally, add `--sm64ap_passwd "YourPassword"` if the room you are using requires a password.
- Install/patch hints:
  - - Super Mario 64 US or JP Rom (Europe and Shindou not supported)
  - - [SM64AP-Launcher](https://github.com/N00byKing/SM64AP-Launcher/releases) or
  - - Optional, for sending [commands](/tutorial/MultiworldGG/commands/en) like `!hint`: the TextClient from [the most recent MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases)
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Super_Mario_Land_2

- Setup files: Super_Mario_Land_2/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apsml2
- Connection hints:
  - ### Connect to the Multiserver
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
  - 7. To connect the client to the server, enter your room's address and port (e.g. `archipelago.gg:38281`) into the
- Install/patch hints:
  - - The built-in Archipelago client, which can be installed [here](https://github.com/ArchipelagoMW/Archipelago/releases)
  - - A Super Mario Land 2: 6 Golden Coins version 1.0 ROM file. The Archipelago community cannot provide this.
  - - Under Config > Customize, check the "Run in background" box. This will prevent disconnecting from the client while
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Super_Mario_Odyssey

- Setup files: Super_Mario_Odyssey/setup_en.md
- Emulators: Ryujinx, Yuzu
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Super_Mario_Sunshine

- Setup files: Super_Mario_Sunshine/setup_en.md
- Emulators: dolphin, Dolphin
- Connection hints:
  - 5. Click the "Create New Room" link.
  - the host will provide you with a link to the room or the address and port necessary to connect.
  - ### Connect to the client
- Install/patch hints:
  - - [Latest release of MultiworldGG Multiworld Suite](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Your **Legally** obtained American ISO file, likely named `Super Mario Sunshine (USA).iso`
  - 1. Download and install the latest release of MultiworldGG Multiworld from the link above.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.

## Super_Mario_World

- Setup files: Super_Mario_World/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: apsmw
- Connection hints:
  - ### Connect to the client
  - ### Connect to the Archipelago Server
  - The client will attempt to reconnect to the new server address, and should momentarily show "Server Status: Connected".
- Install/patch hints:
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your legally obtained Super Mario World ROM file, probably named `Super Mario World (USA).sfc`
  - 2. The first time you do local generation or patch your game, you will be asked to locate your base ROM file.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Symphony_of_the_Night

- Setup files: Symphony_of_the_Night/setup_en.md
- Emulators: bizhawk, Bizhawk
- Patch extensions mentioned: apsotn
- Connection hints:
  - Select Open Patch from MultiworldGGLauncher. PAY ATTENTION TO FILE DIALOG. During the process will be asked for Castlevania - Symphony of the Night (USA) (Track 1).bin or ROM File and Castlevania - Symphony of the Night (USA) (Track 2).bin or Audio File. You could be asked for a bizhawk binary also. You might get No handler found during Sony and Playstation logos. Don't forget to enter your server address and click connect after getting Symphony of the Night handler.
  - If you prefer to open manually, run the game with AP_SEED_PLAYER.cue, chooose Bizhawk client from MultiworldGGLauncher, on Bizhawk choose Tools->Lua Console, on Lua console choose Script->Open script and select connector_bizhawk_generic.lua from data\lua directory. Don't forget to enter your server address and click connect after getting Symphony of the Night handler.
- Install/patch hints:
  - [GRAB THE LAST RELEASE HERE](https://github.com/fdelduque/Archipelago/releases) (ships with MultiworldGG)
  - * MultiworldGG Client
  - * Symphony of the Night ROM file
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Terraria

- Setup files: Terraria/setup_en.md
- Connection hints:
  - [connect to a MultiworldGG game](#joining-an-archipelago-game-in-terraria).
- Install/patch hints:
  - # Terraria for MultiworldGG Setup Guide
  - [connect to a MultiworldGG game](#joining-an-archipelago-game-in-terraria).
  - The [basic multiworld setup guide](/tutorial/Archipelago/setup/en) can be found on MultiworldGG's website. Among other things, it explains what .yaml
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Tetris_Attack

- Setup files: Tetris_Attack/setup_en.md
- Emulators: BizHawk, BSNES, bsnes, RetroArch, retroarch, snes9x
- Patch extensions mentioned: aptatk
- Connection hints:
  - 3. Click the "Create New Room" link.
  - ### Connect to the client
  - ### Connect to the Archipelago Server
- Install/patch hints:
  - - Hardware or software capable of loading and playing SNES ROM files
  - 4. You will be presented with a server page, from which you can download your patch file.
  - 5. Double-click on your patch file, and the Tetris Attack Client will launch automatically, create your ROM from the
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Binding_of_Isaac_Repentance

- Setup files: The_Binding_of_Isaac_Repentance/setup_en.md
- Connection hints:
  - navigate to *AP Integration* and set the appropriate ip, port, slot name and optionally password for your Archipelago
  - server and slot and hit reconnect.
  - 6. Click "Create New Room". This will take you to the server page. Provide the link to this page to your players, so
- Install/patch hints:
  - they may download their patch files from there.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## The_Legend_of_Zelda_-_Oracle_of_Ages

- Setup files: The_Legend_of_Zelda_-_Oracle_of_Ages/ooa_setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apooa, apoos, apworld
- Install/patch hints:
  - - Your legally obtained Oracle of Ages US ROM file
  - 1. Put your **Oracle of Ages US ROM** inside your MultiworldGG install folder (named "Legend of Zelda, The - Oracle of Ages (USA).gbc")
  - 1. Put your **Oracle of Ages US ROM** inside your MultiworldGG install folder (named "Legend of Zelda, The - Oracle of Ages (USA).gbc")
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Legend_of_Zelda_-_Oracle_of_Seasons

- Setup files: The_Legend_of_Zelda_-_Oracle_of_Seasons/oos_setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apoos, apworld
- Install/patch hints:
  - - Your legally obtained Oracle of Seasons US ROM file
  - 1. Put your **Oracle of Seasons US ROM** inside your MultiworldGG install folder (named "Legend of Zelda, The - Oracle of Seasons (USA).gbc")
  - 1. Put your **Oracle of Seasons US ROM** inside your MultiworldGG install folder (named "Legend of Zelda, The - Oracle of Seasons (USA).gbc")
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Legend_of_Zelda_-_Phantom_Hourglass

- Setup files: The_Legend_of_Zelda_-_Phantom_Hourglass/setup_en.md
- Emulators: bizhawk, Bizhawk, BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - 5. Open the `generic bizhawk client` in MultiworldGG, and connect to the server
- Install/patch hints:
  - * [MultiworldGG 0.7.150+](/tutorial/Archipelago/setup/en)
  - * Legally acquired Phantom Hourglass EU rom (US support coming soon). Apparently it only works in English
  - 1. Find your MultiworldGG directory, and put `tloz_ph.apworld` in the `custom_worlds` folder (not needed with MWGG)
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Messenger

- Setup files: The_Messenger/setup_en.md
- Connection hints:
  - 1. Go to the room page of the MultiWorld you are going to join.
  - 2. Click on your slot name on the left side.
  - 4. Select the `Connect to Archipelago` button
- Install/patch hints:
  - 1. Download and install the latest [MultiworldGG release](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
  - 2. Launch the MultiworldGG Launcher (MultiworldGGLauncher.exe)
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## The_Minish_Cap

- Setup files: The_Minish_Cap/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: aptmc
- Connection hints:
  - 6. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - 5. Enter the MultiworldGG server address (the one you connected your client to), slot name, and password.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - An EU copy of The Legend of Zelda: The Minish Cap. The MultiworldGG community cannot provide this.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Simpsons_Hit_And_Run

- Setup files: The_Simpsons_Hit_And_Run/setup_en.md
- Patch extensions mentioned: apshar, apworld
- Connection hints:
  - - Optionally add more yamls for other players or other games you want to play. The multiworld will have 1 slot per yaml included here.
  - - Click Create Room and wait for it to spin up. This is the room. You're good to go once there's a line that says /connect multiworld.gg:{port}
  - - Download the Config file (*.apshar) by going to the MultiworldGG Room and clicking "Download Patch File"
- Install/patch hints:
  - - [MultiworldGG](https://multiworld.gg/) (Latest release [here](https://github.com/MultiworldGG/MultiworldGG/releases/latest))
  - - The latest release of the [client/memory manager and the lmlm mod](https://github.com/nmize1/AP-SHARRandomizer/releases/latest)
  - - The latest release of the [apworld](https://github.com/nmize1/Archipelago/releases/latest) (ships with MultiworldGG)
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## The_Sims_4

- Setup files: The_Sims_4/setup_en.md
- Patch extensions mentioned: apworld
- Connection hints:
  - - Open the MultiworldGG launcher and run The Sims 4 client and connect to the server.
- Install/patch hints:
  - - Install MultiworldGG
  - - Add sims4.apworld to custom_worlds folder in your MultiworldGG directory or double click the .apworld to install it automatically (not needed for MWGG)
  - - Follow MultiworldGG's tutorial on [how to generate a game.](https://multiworld.gg/tutorial/Archipelago/setup/en)
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## The_Wind_Waker

- Setup files: The_Wind_Waker/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: aptww, apworld
- Connection hints:
  - All players playing The Wind Waker must provide the room host with a YAML file containing the settings for their world.
  - world. Once you're happy with your settings, provide the room host with your YAML file and proceed to the next step.
  - ## Connecting to a Room
- Install/patch hints:
  - # Setup Guide for The Wind Waker MultiworldGG
  - Welcome to The Wind Waker MultiworldGG! This guide will help you set up the randomizer and play your first multiworld.
  - specifically for MultiworldGG and Archipelago.
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## The_Witness

- Setup files: The_Witness/setup_en.md
- Connection hints:
  - 4. Enter the Archipelago address, slot name and password
  - 4. Enter the AP address, slot name and password.
- Install/patch hints:
  - - [MultiworldGGTextClient](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ## MultiworldGG Text Client
  - It is recommended to have MultiworldGG's Text Client open on the side to keep track of what items you receive and send.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Timespinner

- Setup files: Timespinner/setup_en.md
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Toontown

- Setup files: Toontown/setup_en.md
- Connection hints:
  - * Where it says "Server IP", enter the IP address of the server. Then press enter. If you're running the server locally, just press enter without typing anything.
  - * When in game, you first need to type (in Toontown's chat) !slot <SLOT NAME> where you replace <SLOT NAME> with whatever your slot is in the Multiworld room.
- Install/patch hints:
  - * To start the game, run `start_client.bat`. This will open a window that will help you set up your client.
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Trackmania

- Setup files: Trackmania/setup_en.md
- Connection hints:
  - To connect to a server, simply run the **MultiworldGG Trackmania Client** from the **MultiworldGG Launcher**, and use the
  - /connect command or the header bar to connect to the server. Once the client is connected to the server, you must connect the Trackmania Plugin to the client. In the Openplanet overlay, click Scripts -> Archipelago to open the Archipelago Plugin window, and click Connect.
- Install/patch hints:
  - # Trackmania MultiworldGG Setup Guide!
  - - [MultiworldGG Launcher](https://multiworld.gg/tutorial/Archipelago/setup/en)
  - ## Connecting to a MultiworldGG Server
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## TUNIC

- Setup files: TUNIC/setup_en.md
- Mod loaders/tools: bepinex, BepInEx
- Connection hints:
  - Click the button labeled `Edit AP Config`, and fill in *Player*, *Hostname*, *Port*, and *Password* (if required) with the correct information for your room.
  - An error message will display if the game fails to connect to the server.
- Install/patch hints:
  - - [MultiworldGG Text Client](https://github.com/MultiworldGG/MultiworldGG/releases/latest)
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.

## Twilight_Princess

- Setup files: Twilight_Princess/setup_en.md
- Emulators: dolphin, Dolphin
- Patch extensions mentioned: aptp, apworld
- Connection hints:
  - Whether playing, generating, or hosting a MultiworldGG room with Twilight Princess, you must follow a few simple steps to
  - All players playing Twilight Princess must provide the room host with a YAML file containing the settings for their world.
  - Once you're happy with your settings, provide the room host with your YAML file and proceed to the next step.
- Install/patch hints:
  - # Setup Guide for Twilight Princess MultiworldGG
  - Welcome to Twilight Princess MultiworldGG! This guide will help you set up the randomizer and play your first multiworld.
  - Whether playing, generating, or hosting a MultiworldGG room with Twilight Princess, you must follow a few simple steps to
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Tyrian

- Setup files: Tyrian/setup_en.md
- Connection hints:
  - 4. Enter the address of the Archipelago server, and your slot name.
  - 5. Choose "Connect to Server", and enjoy.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - ## In-Game Text Client
  - APTyrian contains a fully-featured text client available from within the game. To bring it up, press the TAB key while
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## UFO_50

- Setup files: UFO_50/setup_en.md
- Connection hints:
  - - **server:port** (e.g. `multiworld.gg:38281`)
  - * If hosting on the website, this detail will be shown in your created room.
  - - **slot name** (e.g. `Player`)
- Install/patch hints:
  - 3. Open the MultiworldGG Launcher and click on the UFO 50 Client.
  - 6. To launch the game, run the UFO 50 Client from the MultiworldGG Launcher.
  - - A way to apply `.bsdiff4` patches, such as [bspatch](https://www.romhacking.net/utilities/929/).
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## ULTRAKILL

- Setup files: ULTRAKILL/setup_en.md
- Mod loaders/tools: r2modman, thunderstore
- Patch extensions mentioned: apworld
- Connection hints:
  - 6. To connect to the server, first select a new save file. Then open the options menu, click the PLUGIN CONFIG button, click Configure next to Archipelago, and open the PLAYER SETTINGS menu. Enter your name, the server's address in the form of `address:port`, and a password if necessary, then click the Connect button.
  - - `connect [address:port] [player] [password]` - Connect to an MultiworldGG server.
- Install/patch hints:
  - 1. Download and install [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - 5. Host a game, either manually, or by [uploading](https://multiworld.gg/uploads) it to the MultiworldGG website.
  - - `connect [address:port] [player] [password]` - Connect to an MultiworldGG server.
- Automation approach (draft):
  - Bundle or install mod loader/mods into game directory or profile; launch game via modded profile and inject connection config/command if supported.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Undertale

- Setup files: Undertale/setup_en.md
- Connection hints:
  - ### Connect to the MultiServer
  - Make sure both Undertale **from the MultiworldGG folder** and its client are running. (Undertale will ask for a save slot
  - The client will then ask for the slot name, input your slot name chosen during YAML creation in the text box at the
- Install/patch hints:
  - - MultiworldGG from the [MultiworldGG Releases Page](https://github.com/MultiworldGG/MultiworldGG/releases)
  - Start the Undertale client from your MultiworldGG folder and input `/auto_patch <Your Undertale Install Directory>` at the bottom.
  - Make sure both Undertale **from the MultiworldGG folder** and its client are running. (Undertale will ask for a save slot
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## VVVVVV

- Setup files: VVVVVV/setup_en.md
- Patch extensions mentioned: apv6
- Connection hints:
  - `-v6ap_ip server:port`
  - If the game you are joining requires a password, you should also add the following to your launch options:
  - `-v6ap_passwd secretPassword`
- Install/patch hints:
  - To join a MultiworldGG Multiworld game, you must set the game's launch options. The two mandatory launch options are:
- Automation approach (draft):
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Wario_Land

- Setup files: Wario_Land/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apwl, apworld
- Connection hints:
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected.\
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - 5. Enter the MultiworldGG server address (the one you connected your client to), slot name, and password.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A (World) ROM of Super Mario Land 3: Wario Land.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Wario_Land_4

- Setup files: Wario_Land_4/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apwl4
- Connection hints:
  - ### Connect to the Multiserver
  - press enter (if the server uses a password, type in the bottom text field
  - `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A Wario Land 4 ROM. Either US/Europe or Japanese is acceptable. You do not need one to generate a seed, only to play it.
  - It is strongly recommended to associate the GBA ROM extension (\*.gba) to the BizHawk we've just
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Xenoblade_X

- Setup files: Xenoblade_X/setup_en.md
- Emulators: cemu, Cemu
- Patch extensions mentioned: apworld
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases)
  - 7. Open the Archipelago Launcher
  - 1. Start the `Archipelago Launcher`
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Yacht_Dice

- Setup files: Yacht_Dice/setup_en.md
- Install/patch hints:
  - Press Archipelago, and after logging in, you are good to go. The website has a built-in client, where you can chat and send commands.
  - For more information on generating MultiworldGG games and connecting to servers, please see the [Basic Multiworld Setup Guide](/tutorial/Archipelago/setup/en).
- Automation approach (draft):
  - Prefer native game launch with client-side connect flow; check for CLI/config fields for server/slot/password.

## Yoshis_Island

- Setup files: Yoshis_Island/setup_en.md
- Emulators: BizHawk, BSNES, RetroArch, snes9x
- Patch extensions mentioned: apyi
- Connection hints:
  - ### Connect to the client
  - ### Connect to the Archipelago Server
  - The client will attempt to reconnect to the new server address, and should momentarily show "Server Status: Connected".
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Hardware or software capable of loading and playing SNES ROM files
  - - Your legally obtained Yoshi's Island English 1.0 ROM file, probably named `Super Mario World 2 - Yoshi's Island (U).sfc`
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Yu-Gi-Oh_2006

- Setup files: Yu-Gi-Oh_2006/setup_en.md
- Emulators: Bizhawk, BizHawk
- Patch extensions mentioned: apygo06
- Connection hints:
  - ### Connect to the Multiserver
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - - The built-in MultiworldGG client, which can be installed [here](https://github.com/MultiworldGG/MultiworldGG/releases)
  - - A US or European Yu-Gi-Oh! Ultimate Masters: World Championship Tournament 2006 Rom
  - continue playing in the background, even if another window is selected, such as the Client.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Yu-Gi-Oh_Dungeon_Dice_Monsters

- Setup files: Yu-Gi-Oh_Dungeon_Dice_Monsters/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - 5. Select your patch from the generated output or downloaded from the MultiworldGG site from the hosted game room.
  - 7. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 8. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases). Please use version 0.7.50 or later for integrated
  - - Yu-Gi-Oh! Dungeon Dice Monsters .GBA rom.
  - - Under `Config > Customize`, check the "Run in background" option to prevent disconnecting from the client while you're
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Yu-Gi-Oh_Forbidden_Memories

- Setup files: Yu-Gi-Oh_Forbidden_Memories/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apworld
- Connection hints:
  - 8. The emulator and client will eventually connect to each other. The BizHawk Client window should indicate that it
  - 9. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
- Install/patch hints:
  - - Yu-Gi-Oh! Forbidden Memories NTSC: ISO or BIN/CUE. Card-drop mods are expressly supported. The MultiworldGG
  - - Yu-Gi-Oh! Forbidden Memories NTSC: ISO or BIN/CUE. Card-drop mods are expressly supported. The MultiworldGG
  - not have to patch your ROM with MultiworldGG for it to work. Just make sure you launch a "New Game" for each seed.
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Zelda_II_The_Adventure_of_Link

- Setup files: Zelda_II_The_Adventure_of_Link/setup_en.md
- Emulators: BizHawk
- Patch extensions mentioned: apz2
- Connection hints:
  - 6. The emulator may freeze every few seconds until it manages to connect to the client. This is expected. The BizHawk
  - 7. To connect the client to the server, enter your room's address and port (e.g. `multiworld.gg:38281`) into the
  - server uses password, type in the bottom textfield `/connect <address>:<port> [password]`)
- Install/patch hints:
  - # Zelda II MultiworldGG Randomizer Setup Guide
  - ### Obtain your patch file and create your ROM
  - the host will provide you with either a link to download your patch file, or with a zip file containing everyone's patch
- Automation approach (draft):
  - Bundle BizHawk, auto-configure Lua core, launch EmuHawk with ROM/ISO and MWGG lua connector; start BizHawk client and auto-connect to room.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Zillion

- Setup files: Zillion/setup_en.md
- Emulators: retroarch, RetroArch
- Patch extensions mentioned: apzl
- Connection hints:
  - 3. Click the "Create New Room" link.
  - - If you activate the "room generation" option in your config (yaml), you might want to tell your host that the generation will take longer than normal. It takes approximately 20 seconds longer for each Zillion player that enables this option.
  - 3. Connect to the client.
- Install/patch hints:
  - - [MultiworldGG](https://github.com/MultiworldGG/MultiworldGG/releases).
  - - Your legally obtained Zillion ROM file, named `Zillion (UE) [!].sms`
  - Put your Zillion ROM file in the Archipelago directory in your home directory.
- Automation approach (draft):
  - Bundle RetroArch core(s), preconfigure network commands/core, launch RetroArch with ROM and core; auto-start client and connect.
  - Handle patch file ingestion; auto-apply patch to base ROM/ISO and launch patched output in emulator.

## Zork_Grand_Inquisitor

- Setup files: Zork_Grand_Inquisitor/setup_en.md
- Emulators: scummvm, ScummVM
- Connection hints:
  - - Enter the room's hostname and port number (e.g. `multiworld.gg:54321`) in the top box and press `Connect`.
  - - You should now be connected to the MultiworldGG room.
- Install/patch hints:
  - - Windows OS (Hard required. Client is using memory reading / writing through Win32 API)
  - - MultiworldGG 0.7.120+
  - No game modding is required to play Zork Grand Inquisitor with MultiworldGG. The client does all the work by attaching to
- Automation approach (draft):
  - Bundle emulator and launch with ROM/ISO; apply required settings via config files where possible; auto-start client and connect.
