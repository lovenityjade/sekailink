# AppImage Runtime Debug (Packaging + Python + BizHawk)

Date: 2026-02-09

But: documenter le debug qui a debloque le passage de "Manual mode" a un vrai autorun (patch -> ROM -> emulateur -> client AP -> tracker) dans l'AppImage.

## Symptomes observes
- Autorun tombait en `Manual mode` meme pour des jeux BizHawk deja declares dans `client/runtime/modules/*/manifest.json`.
- En AppImage, erreurs Python du style:
- `ModuleNotFoundError: No module named 'ModuleUpdate'`
- `ModuleNotFoundError: No module named 'yaml'` (PyYAML)
- `ModuleNotFoundError: No module named 'pathspec'`
- `ModuleNotFoundError: No module named 'pkg_resources'` (setuptools)
- `ModuleNotFoundError: No module named 'orjson'`
- `FileNotFoundError: .../resources/ap/Players` (bootstrap user_path)
- Erreur world import:
- `ModuleNotFoundError: No module named 'worlds.generic'`

Notes:
- Warning non bloquant vu pendant l'exec: `_speedups not available` (LocationStore pure python). Ce n'est pas un blocage fonctionnel.

## Causes racines
1) Packaging AppImage incomplet (runtime dans ASAR)
- `client/app/electron-builder.yml` ne shipait que `dist/**`, `electron/**`, `public/**`.
- Le runtime SekaiLink (`client/runtime/**`) et BizHawk (`third_party/emulators/BizHawk-2.10-linux-x64`) n'etaient pas embarques en "vrais fichiers".
- Les chemins dans `client/app/electron/main.cjs` utilisaient `__dirname` vers `../../runtime` et `../../../third_party`, ce qui casse une fois package (ASAR / mount AppImage).

2) Runtime Python non portable
- Les wrappers (`client/runtime/patcher_wrapper.py`, `commonclient_wrapper.py`, `bizhawkclient_wrapper.py`) importent le code AP/MWGG.
- En AppImage, il faut:
- un "AP python root" accessible sur disque (pas dans ASAR),
- un `PYTHONPATH` pointe vers ce root,
- des dependances pip presentes (PyYAML, pathspec, setuptools/pkg_resources, etc.).

3) Bundle worlds incomplet
- On bundle une selection de `worlds/` pour l'MVP (GBA/SNES/NES/GB/GBC + OOT, etc.).
- Certains worlds ont des deps source vers `worlds/generic`, donc il faut l'inclure aussi.

4) Bootstrap AP `user_path()` sur AppImage read-only
- Dans `Utils.user_path()`:
- si `local_path()` n'est pas writable (cas AppImage), AP copie `Players`, `data/sprites`, `data/lua` vers le dossier user.
- Si `local_path("Players")` n'existe pas dans le bundle, crash.
- Si `manifest.json` n'existe pas, ce bootstrap peut se declencher trop souvent.

## Fixes implementes (etat actuel)

### A) Extra resources (hors ASAR)
Fichier: `client/app/electron-builder.yml`
- ajout `extraResources` pour ship:
- `client/runtime` -> `resources/runtime`
- `third_party/emulators/BizHawk-2.10-linux-x64` -> `resources/third_party/emulators/BizHawk-2.10-linux-x64`
- un "AP python root" -> `resources/ap`:
- `*.py` au root + `requirements.txt`
- `data/`
- `worlds/` (subset MVP + infra)
- ajout `resources/ap/manifest.json` (placeholder SekaiLink)
- ajout `resources/ap/Players/` (placeholder) pour satisfaire `user_path()` bootstrap
- ajout `worlds/generic/**` (fix `worlds.generic` missing)

### B) Resolve paths en packaged build
Fichier: `client/app/electron/main.cjs`
- nouveaux helpers:
- `getBundledRuntimeDir()` -> `process.resourcesPath/runtime` (packaged) ou `client/runtime` (dev)
- `getBundledThirdPartyDir()` -> `process.resourcesPath/third_party` (packaged) ou `third_party` (dev)
- `getBundledApRootDir()` -> `process.resourcesPath/ap` (packaged) ou repo root (dev)
- wrappers python et BizHawk pointent maintenant sur ces chemins.

### C) PYTHONPATH + AP root env
Fichier: `client/app/electron/main.cjs`
- `withApPythonEnv()` ajoute:
- `SEKAILINK_AP_ROOT=<resources/ap>`
- `PYTHONPATH=<resources/ap>:<prior>`
- `SKIP_REQUIREMENTS_UPDATE=1` (ne jamais lancer du pip interactif depuis ModuleUpdate)

Fichier: `client/runtime/patcher_wrapper.py`
- ajoute `SEKAILINK_AP_ROOT` dans `sys.path` si present.

### D) Patcher wrapper: import explicite par extension
Fichier: `client/runtime/patcher_wrapper.py`
- Avant: `Patch.create_rom_file(.ap*)` dependait de l'import side-effect d'un world pour enregistrer le handler AutoPatch.
- Maintenant: le wrapper detecte l'extension `.ap*` et importe le module world correspondant avant `Patch.create_rom_file()`.
- Ca evite de depend d'un import global de `worlds` juste pour patcher un jeu.

### E) Python venv "runtime tools" + auto-healing deps
Fichier: `client/app/electron/main.cjs`
- Ajout d'un bootstrap venv:
- venv: `app.getPath("userData")/runtime/tools/python/venv`
- installe un set minimal de deps pip (pas de deps lourdes GUI/server).
- ajoute un mode auto-healing:
- si un import AP/World échoue sur `ModuleNotFoundError: X`, le client mappe `X` -> pip spec (ex: `yaml` -> `pyyaml`, `pkg_resources` -> `setuptools`), installe, et retry.

Note importante:
- Ce flow requiert reseau au moins une fois (pip).
- Un mode offline (wheels bundle) est a prevoir plus tard pour handheld/offline.

## Impact taille
- L'AppImage a grossi (runtime + BizHawk + AP sources).
- Taille observee pendant debug: ~116MB -> ~219MB.

## Etat actuel (apres debug)
- Les problemes de type "Manual mode parce que runtime/emu/python manquants" sont traites par:
- extraResources + resolvers `process.resourcesPath`
- AP python root bundle + `PYTHONPATH`
- venv auto deps
- worlds infra `generic` + placeholders `Players`/`manifest.json`
- Le blocage residuel attendu est maintenant dans "launch":
- orchestration (ordre des steps),
- demarrage BizHawk sous mono,
- connexion BizHawk <-> Lua connector,
- focus/layout, trackers, etc.

## Paths utiles (pour debug manuel)
- App logs: `~/.config/sekailink-client/logs/sekailink-*.log`
- Config SekaiLink (roms + games + layout): `~/.sekailink/config.json`
- ROM library: `~/.config/sekailink-client/roms/`
- Runtime downloads: `~/.config/sekailink-client/runtime/downloads/`
- Python venv: `~/.config/sekailink-client/runtime/tools/python/venv/`

## Next actions recommandees
- Stabiliser la phase `LAUNCH_GAME_RUNTIME` + `CONNECT_TO_SERVER` (BizHawk) avant d'etendre a Dolphin/PC/mods.
- Ajouter un "preflight" UI: verif rom present + verif mono present + verif gamescope (si enabled) + verif tracker pack (si declared).
- Ajouter une option "offline install pack" (bundled wheels) pour handheld sans internet.

