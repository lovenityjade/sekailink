# Emulateurs (third_party/emulators) et plan des options SekaiLink

But:
- documenter ce qui existe dans `third_party/emulators/` (binaire vs source)
- en deduire une strategie "Settings" pour SekaiLink (paths, graphics, inputs, portable mode, etc.)
- preparer le terrain Linux (X11 + Wayland) et Windows

Contrainte produit:
- "one-button Play" doit rester vrai. Le menu options existe, mais il doit etre optionnel et safe.
- pas de distribution de ROM/ISO/BIOS/keys/firmware. SekaiLink ne peut gerer que les chemins et la verification.

## Etat du repo (important)
Dans `third_party/emulators/`, on a majoritairement des repositories source (pour build/dev), pas des binaires redistribuables prets.
Exception notable dans ce repo:
- `third_party/emulators/BizHawk-2.10-linux-x64/` contient un build Linux utilisable (Mono).

Implication:
- court terme (MVP): integrer proprement BizHawk (deja fait cote SekaiLink) + planifier un systeme d'installation/updates pour les autres emulateurs.
- moyen terme: SekaiLink telecharge des releases officielles (zip/AppImage/msi) et installe dans un cache SekaiLink, plutot que de compiler ces repos.

## Placeholder repos (attention)
Dans ce repo, certains dossiers existent mais sont vides hormis `.git`:
- `third_party/emulators/Cemu/`
- `third_party/emulators/RetroArch/`

Implication:
- ne pas planifier "on a deja RetroArch/Cemu" a partir de ce repo
- l'installation devra venir de releases officielles (ou Flatpak) via un installer SekaiLink

## Inventaire (dossiers observes)
Emulateurs/engines (sources):
- `third_party/emulators/Azahar/` (3DS, base Citra)
- `third_party/emulators/BizHawk/` (multi-system, source)
- `third_party/emulators/Dolphin/` (GC/Wii, source)
- `third_party/emulators/DuckStation/` (PS1, source)
- `third_party/emulators/PCSX2/` (PS2, source)
- `third_party/emulators/PPSSPP/` (PSP, source)
- `third_party/emulators/Ryujinx/` (Switch, source, .NET)
- `third_party/emulators/Eden/` (Switch, source, derive de Yuzu/Sudachi)
- `third_party/emulators/mGBA/` (GBA/GB/GBC, source)
- `third_party/emulators/VisualBoyAdvance-M/` (GBA/GB/GBC, source)
- `third_party/emulators/melonDS/` (NDS, source)
- `third_party/emulators/DeSmuME/` (NDS, source)
- `third_party/emulators/bsnes/` (SNES, source)
- `third_party/emulators/Snes9x/` (SNES, source)
- `third_party/emulators/DOSBox-Staging/` (DOS, source)
- `third_party/emulators/ScummVM/` (adventure engines, source)
- `third_party/emulators/MAME/` (arcade, source)
- `third_party/emulators/OpenGOAL/` (Jak 1/2 PC port, source)
- `third_party/emulators/OpenRCT2/` (RCT2 reimplementation, source)
- `third_party/emulators/gzdoom/` (Doom engine source port, source)
- `third_party/emulators/OpenEmu/` (macOS front-end + cores, source)

Build binaire present:
- `third_party/emulators/BizHawk-2.10-linux-x64/`

## Objectif "Options" cote SekaiLink
On veut un menu qui couvre 80% des besoins avec des controles simples, sans exposer l'integralite des settings de chaque emulateur.

Proposition: 3 niveaux d'options
1) Bibliotheque (global, cross-emulators)
- chemins "Jeux" (ROM/ISO) par systeme, ou dossiers scannes
- chemins "BIOS/Firmware/Keys" par systeme
- validation: hash, extension, taille, presence (sans copier)

2) Emulateurs (installation + selection)
- quel emulateur utiliser pour une "famille" (ex: PS1 = DuckStation)
- install/update/uninstall (telecharger releases officielles)
- mode portable (quand supporte) pour garder config dans un dossier SekaiLink

3) Avance (par emulateur)
- graphics backend (Vulkan/OpenGL/Software) quand l'emulateur le supporte via CLI ou config modifiable
- resolution scaling, vsync, fullscreen/borderless (best-effort)
- input: mapping minimal + bouton "Open emulator settings" quand non scriptable

## Mode portable et "user dir" (cle pour une UX console)
Pourquoi:
- garantir un comportement reproductible
- eviter de polluer les configs globales de l'utilisateur
- permettre des "profiles" par jeu/slot si necessaire

Regle:
- preferer les emulateurs qui supportent explicitement un user dir portable (fichier sentinel, argument CLI).
- sinon, fallback sur user dir standard XDG/Windows et documenter le risque.

## Detail par emulateur (points utiles pour SekaiLink)

### BizHawk (binaire present: `BizHawk-2.10-linux-x64`)
Entree:
- Linux: `third_party/emulators/BizHawk-2.10-linux-x64/EmuHawkMono.sh`
- binaire: `EmuHawk.exe` (lance via `mono`)

Dependances Linux (a verifier en packaging):
- Mono (runtime "complete")
- libs systeme (le script calcule `libpath` via `lsb_release` et exporte `LD_LIBRARY_PATH`)
- logging: `EmuHawkMono.sh` capture stdout/stderr via FIFO et ecrit `EmuHawkMono_laststdout.txt` et `EmuHawkMono_laststderr.txt` dans le dossier BizHawk

Config "portable" (reellement utile pour SekaiLink):
- `config.ini` est un JSON dans le dossier BizHawk (portable par conception ici)
- `defctrl.json` contient des mappings par defaut (clavier + joystick + XInput)

Champs observes dans `config.ini` (exemples):
- chemins: `PathEntries.Paths[]` inclut `Type=ROM`, `Type=Firmware`, `Type=Lua` avec paths relatifs
- fenetre: `StartFullscreen`, `MainWindowSize`, `SaveWindowPosition`
- focus: `RunInBackground`, `AcceptBackgroundInput`
- Lua: `RecentLua`, `DisableLuaScriptsOnLoad` (dans les sous-settings)
- hygiene: `RecentRoms.recentlist`, `LastRomPath`, et les positions de fenetre contiennent souvent des paths/coordonnees user-specific

Implications SekaiLink:
- "ROM paths": soit on passe directement le fichier ROM en argument, soit on configure `PathEntries` pour faciliter le browsing.
- "Firmware paths": le build vendore n'a pas de dossier `Firmware/` present, mais `config.ini` pointe sur `./Firmware`. SekaiLink doit fournir une strategie:
- option A: creer un dossier `Firmware/` a cote du binaire (cache SekaiLink) et y pointer
- option B: pointer `PathEntries` vers une librairie firmware globale user-provided
- "Lua connector": BizHawk supporte `--lua=<script>`. Sur Linux, les paths doivent etre absolus ou relatifs au dossier BizHawk (le script `EmuHawkMono.sh` fait `cd`).
- "Fullscreen": `StartFullscreen` est modifiable via `config.ini` (donc scriptable).
- "Inputs": on peut shipper un `defctrl.json` adapte handheld (ABXY, sticks) et offrir un bouton "Edit mapping" qui ouvre BizHawk.

### Dolphin (source: `third_party/emulators/Dolphin`)
Support portable documente:
- `portable.txt` dans `Binaries/` (build portable)
- CLI `-u <path>` / `--user=<path>` pour choisir le user folder

CLI utile (extrait Readme):
- `--exec=<file>` pour lancer un jeu
- `--batch` pour run sans UI (avec `--exec`)
- `--config <System>.<Section>.<Key>=<Value>` pour overrides

Implications SekaiLink:
- ideal pour "options" car `--user` permet un profil SekaiLink propre.
- expose des settings "scriptables" via `--config`, sans editer des `.ini` au debut.

### DuckStation (source: `third_party/emulators/DuckStation`)
User directory:
- Linux: `~/.local/share/duckstation` (XDG data)
- portable mode: creer `portable.txt` dans le dossier du binaire

Proprietary assets:
- BIOS PS1 requis (a placer dans le user dir, ex: `.../duckstation/bios`)

Input:
- bindings via UI; base SDL controller DB peut etre override en placant `gamecontrollerdb.txt` dans le user dir.

Implications SekaiLink:
- bon candidat pour isoler les donnees (BIOS, memory cards, saves, settings).
- portable mode existe (via `portable.txt`), mais attention aux contraintes de distribution et au fait que ca implique d'ecrire a cote de l'exe.
- on peut automatiser l'import BIOS (copie dans user dir SekaiLink) si l'utilisateur fournit un chemin.

Note distribution/licence (important):
- le README DuckStation indique que la redistribution de releases **non modifiees** est permise, mais que des packages pre-configures sont consideres comme des modifications
- pour SekaiLink: preferer un telechargement a la demande (release officielle) et garder la config dans un user dir separe, sans modifier l'archive release

### PCSX2 (source: `third_party/emulators/PCSX2`)
Portable mode:
- `portable.txt` ou `portable.ini` dans le dossier du programme
- argument `-portable` existe (force portable)

Linux config:
- utilise XDG (`$XDG_CONFIG_HOME/PCSX2` si present) avec fallback `~/.config/PCSX2` (observe dans `pcsx2/Pcsx2Config.cpp`)

Implications SekaiLink:
- tres bon candidat pour "profile SekaiLink" via portable.txt / -portable.
- BIOS PS2 requis (a gerer dans options "BIOS/Firmware").

Note integration future:
- presence de PINE (IPC) dans le code source; potentiellement interessant pour des integrations (telemetrie, controle), mais pas un besoin MVP.

### PPSSPP (source: `third_party/emulators/PPSSPP`)
Proprietary assets:
- pas de BIOS requis (HLE), mais jeux requis (ISO/CSO/CHD etc.)

Memstick / user data:
- dans le code headless, sur Windows: `memstick/` a cote de l'exe
- sur Linux (non-Android): `~/.ppsspp` par defaut

Implications SekaiLink:
- pour une UX portable, preferer une install PPSSPP avec un `memstick/` dans un dossier SekaiLink (cache), et lancer l'exe depuis ce dossier.
- options exposees: resolution scaling, backend, controller mapping (principalement via UI/config).

### mGBA (source: `third_party/emulators/mGBA`)
Config:
- XDG config: `$XDG_CONFIG_HOME/mgba/config.ini` (sinon `~/.config/mgba/config.ini`)
- portable mode: `portable.ini` dans le current dir force un `config.ini` local (doc `doc/mgba.6`)

Implications SekaiLink:
- bon candidat portable (portable.ini) avec config dans un dossier SekaiLink.
- support Lua et patches (IPS/UPS/BPS) selon README, utile pour certains worlds.

### VisualBoyAdvance-M (source: `third_party/emulators/VisualBoyAdvance-M`)
Notes utiles:
- existe sur Flathub/snap et releases Windows/macOS
- project GUI riche, mais la doc ici est surtout build/contrib

Implications SekaiLink:
- probable "fallback emulator" GBA/GB/GBC si BizHawk n'est pas souhaite.
- a evaluer plus tard pour un vrai mode portable + CLI.

### melonDS (source: `third_party/emulators/melonDS`)
Proprietary assets:
- firmware boot necessite BIOS/firmware d'une DS/DS Lite

Implications SekaiLink:
- options "BIOS/Firmware" doivent inclure DS (bios7, bios9, firmware).
- bon candidat pour plan "imports" (l'utilisateur pointe vers ses dumps).

### DeSmuME (source: `third_party/emulators/DeSmuME`)
Notes:
- readme minimal, a traiter comme option future.

### bsnes et Snes9x (sources: `third_party/emulators/bsnes`, `third_party/emulators/Snes9x`)
Notes:
- candidats SNES cote emulateur pur.
- pour Archipelago, l'enjeu est souvent le "connector" (SNI/usb2snes ou autre) et la stabilite input/video.

Implications SekaiLink:
- la selection "SNES family" doit etre pensee avec l'integration SNI (client) et les connecteurs (Lua ou protocole).
- a documenter plus tard en lien avec `sekailink-client-plan/09-lua-connectors.md` et un doc SNI.

### Azahar (3DS, source: `third_party/emulators/Azahar`)
Wayland/X11:
- le README mentionne deux AppImages: `azahar.AppImage` et `azahar-wayland.AppImage`
- recommandation upstream: preferer la variante non-Wayland sauf besoin explicite (issues Wayland)

User data dir (Linux, observe dans `src/common/common_paths.h` et `src/common/file_util.cpp`):
- data: `$XDG_DATA_HOME/azahar-emu/` (sinon `~/.local/share/azahar-emu/`)
- config: `$XDG_CONFIG_HOME/azahar-emu/` (sinon `~/.config/azahar-emu/`)
- cache: `$XDG_CACHE_HOME/azahar-emu/` (sinon `~/.cache/azahar-emu/`)
- portable local: si un dossier `user/` existe dans le current dir, il est utilise

Fichiers systeme (constantes):
- `keys.txt`, `boot9.bin`, `sector0x96.bin` (voir `common_paths.h`)

Implications SekaiLink:
- excellent exemple d'emulateur qui suit XDG proprement + supporte portable local.
- pour handheld Linux, suivre l'approche: X11/XWayland first, Wayland natif best-effort.

### Ryujinx (Switch, source: `third_party/emulators/Ryujinx`)
Config/user dir (observe dans `Ryujinx.Common/Configuration/AppDataManager.cs`):
- Linux: `Environment.SpecialFolder.ApplicationData` -> typiquement `~/.config/Ryujinx`
- `Config.json` dans le user folder

Proprietary assets:
- keys: `prod.keys` (et possiblement `title.keys`, `console.keys`) (observe dans `VirtualFileSystem.cs`)
- firmware Switch requis (le code gere installation/verification)

Implications SekaiLink:
- pour "0 interaction", SekaiLink doit avoir un flow d'import keys/firmware (l'utilisateur selectionne ses fichiers, SekaiLink copie dans le bon dossier, verifie presence).
- attention securite streamer: ne jamais mettre keys/paths sensibles dans argv.

### Eden (Switch, source: `third_party/emulators/Eden`)
Notes:
- derive de Yuzu/Sudachi, license GPLv3 (d'apres README)

Implications SekaiLink:
- meme probleme que Ryujinx: keys/firmware/romfs, mais avec contraintes license et distribution a verifier.

### DOSBox Staging (source: `third_party/emulators/DOSBox-Staging`)
Ce que SekaiLink doit retenir:
- DOSBox Staging a une CLI bien definie, utile pour du "one-button play" par jeu.
- son comportement depend beaucoup d'un fichier `.conf` et/ou de commandes `-c`.

CLI observee (man page):
- `--conf <config_file>` (supporte plusieurs `--conf`)
- `--printconf` (trouver le chemin du conf primaire)
- `--set <setting>=<value>` (override ponctuel, repeatable)
- `--working-dir <path>`
- `--fullscreen`
- `-c <command>` (repeatable)
- `--exit`
- `PATH` en argument: directory -> mount C:, exe -> mount+run, image -> boot/mount (doc `third_party/emulators/DOSBox-Staging/docs/dosbox.1`)

Implications SekaiLink:
- modeler les jeux DOS comme un runtime "data path" + "launch recipe" (conf + commandes `-c`).
- pour un menu Options:
- `dosbox.fullscreen` bool
- `dosbox.conf_path` (per-game ou global)
- `dosbox.commands[]` (liste de commandes DOS a executer)

### ScummVM (source: `third_party/emulators/ScummVM`)
Ce que SekaiLink doit retenir:
- ScummVM est un launcher/engine: on ne lance pas une "ROM", on pointe vers un dossier de data.
- La CLI est riche, et la doc est dans le repo (`third_party/emulators/ScummVM/doc/docportal/...`).

CLI observee (doc ScummVM):
- lancement par target: `scummvm <target>`
- lancement par chemin: `scummvm -p <path> <game id>` ou `scummvm -p <path> --auto-detect`
- `--list-targets`, `--list-games`
- `--config=FILE` / `-c` (alternate config file)
- `--fullscreen` / `-f`
- (doc `third_party/emulators/ScummVM/doc/docportal/advanced_topics/command_line.rst`)

Portable mode observe:
- a partir de ScummVM 2.6.0: creer un fichier vide `scummvm.ini` dans le dossier de l'exe (doc `third_party/emulators/ScummVM/doc/docportal/use_scummvm/install_computer.rst`)

Implications SekaiLink:
- runtime "data path" (dossier jeu) + options `-p` / `--auto-detect` est suffisant pour un MVP.
- pour "limiter les fenetres": ScummVM est deja une seule fenetre; l'enjeu est surtout le layout du tracker.

### GZDoom (source: `third_party/emulators/gzdoom`)
Ce que SekaiLink doit retenir:
- ce n'est pas un emulateur, mais un engine source port.
- la "donnee proprietaire" est l'IWAD (et souvent des PWAD/mods).

Config/user data (Linux, observe dans `third_party/emulators/gzdoom/src/common/platform/posix/unix/i_specialpaths.cpp`):
- utilise `$HOME/.config/<game>` (ex: `$HOME/.config/gzdoom`) comme base
- maintient aussi un chemin legacy `$HOME/.<game>/` (migration)

Implications SekaiLink:
- runtime "Game Data Path" (IWAD + mods) plutot que "ROM path".
- support "profiles" via un dossier de config dedie (idealement via env HOME sandbox, ou via un wrapper si l'engine le supporte clairement).

### Mesen (source: `third_party/emulators/Mesen`)
Etat upstream:
- le README indique que ce repo est une archive de Mesen `0.9.9` et que le developpement s'est arrete en 2020
- le README recommande d'utiliser Mesen2 a la place

Implications SekaiLink:
- traiter ce dossier comme historique/placeholder, pas comme un runtime cible
- si SekaiLink supporte Mesen: viser Mesen2 (releases officielles) et documenter ses options/restrictions

### BizHawk: contraintes de versions par world (critique)

Observation:
- les guides dans `worlds/*/docs` contiennent des contraintes contradictoires sur BizHawk

Exemples concrets dans ce repo:
- `worlds/papermario/docs/setup_en.md`: BizHawk `2.7` a `2.9.1`, et "2.10's release does not work"
- `worlds/apeescape/docs/setup_en.md`: BizHawk `2.7` a `2.9.1`, et BizHawk `2.10` non supporte
- `worlds/tloz/docs/multiworld_en.md`: "Versions 2.10 and higher are supported."

Implication SekaiLink:
- on ne peut pas assumer "un seul BizHawk" pour tous les jeux
- le manifest runtime doit pouvoir exprimer une contrainte de version par module, ex:
- `emulator_id = bizhawk`
- `emulator_version_min`, `emulator_version_max` (ou un range)
- SekaiLink doit etre capable d'installer plusieurs versions en parallele et choisir la bonne version pour un jeu/slot

Conseil pragmatique:
- garder BizHawk vendore uniquement comme fallback dev/MVP
- pour production: installer BizHawk depuis releases officielles par version (Windows + Linux) dans un cache SekaiLink

### OpenGOAL et OpenRCT2 (sources)
Nature:
- ce sont souvent des "engine ports" plus que des emulateurs classiques.
Dependance commune:
- exigent des assets originaux (data files) et/ou des installations Steam/GOG.

Implications SekaiLink:
- dans l'UI, ce sont des "runtimes" au meme titre qu'un emulateur.
- les options doivent inclure "Game Data Path" (ou detection automatique Steam) plutot que "ROM path".

## Consequence sur la strategie SekaiLink
1) Ne pas compiler ces repos dans SekaiLink.
- trop lourd, trop fragile, trop lent, trop de dependances

2) Installer depuis releases officielles (per emulator, per OS).
- Linux: AppImage/Flatpak (selon projet), ou tar/zip
- Windows: zip/msi/installer portable
- garder un cache `~/.sekailink/runtime/emulators/<id>/<version>/...`

3) Preferer portable mode/user-dir override.
- permet "profiles" et permet d'appliquer des settings sans toucher le systeme.

4) Exposer des options "communes" dans SekaiLink, et minimiser le reste.
- exemple "Graphics backend": Vulkan/OpenGL/Software (quand possible)
- exemple "Fullscreen on launch": bool
- exemple "Input preset": handheld / desktop

## Actions de documentation (a faire plus tard)
- ecrire un doc "schema settings SekaiLink" (mapping vers manifest runtime): ou stocker quels paths, quelles options, comment valider.
- ecrire un doc "emulator installers" (sources de releases, checksums, licenses, update policy).
- ecrire un doc SNI (emulateur + connector + client) distinct, car c'est un ecosysteme.
