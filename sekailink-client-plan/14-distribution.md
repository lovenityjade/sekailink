# Distribution, installs, updates (Linux/Windows)

But: atteindre "si l'emulateur/mods ne sont pas installes, on telecharge et on installe" sans exploser la taille du client.

## Options de distribution

Option A: "fat client" (bundle beaucoup de choses)
- on ship un client tres gros (emulateurs + tools + trackers)
- pros: offline, moins de soucis de download/CDN
- cons: taille enorme, updates lourdes

Option B (recommandee): base client + content packs on-demand
- on ship SekaiLink Desktop + un runtime minimal
- on telecharge des "packs" (emulateur, cores, tools, connectors, tracker packs) au besoin
- pros: taille initiale faible, updates ciblees, plus scalable
- cons: plus de logique d'install, besoin reseau

Option C: hybrid
- bundle les indispensables (ex: PopTracker)
- on-demand pour emulateurs lourds (Dolphin, PCSX2) et tools specifiques

## Content pack model (propose)
- chaque family (bizhawk/dolphin/retroarch/modloader) a un "installer" dans Electron main
- chaque module runtime declare ses dependances (ex: bizhawk + mono)

Exemples de dependances declarables:
- `runtime_deps`: `mono`, `gamescope` (Linux)
- `bundled_assets`: `poptracker`, `bizhawk_connector`
- `remote_assets`: repo release, checksum, taille, OS

## Updates
- les versions d'emulateurs et tools changent vite
- proposer un systeme de "channel": stable / beta
- garder la compat du connecteur Lua (script version) comme contrainte de version

## Linux
- format actuel: AppImage (`client/app/release/*.AppImage`)
- Wayland/X11: preferer comportement auto (XWayland ok)
- attention sandbox: si on veut download/install, il faut des chemins accessibles (userData)

## Windows
- installer ou zip
- attention paths + quoting
- attention kill tree (process supervision)

## Observations sur le repo actuel
- il existe des vendored assets dans `third_party/`
- il existe un binaire PopTracker bundle dans `client/runtime/poptracker/poptracker`
- BizHawk Linux semble present sous `third_party/emulators/` (chemin hardcode dans Electron main)

Decisions a prendre:
- veut-on inclure BizHawk dans le build par defaut
- ou veut-on downloader BizHawk au premier usage

