# Emulateurs, fenetres, Wayland/X11

But: limiter le nombre de fenetres ouvertes et rendre l'experience "console".

## Etat actuel
- BizHawk Linux est bundle (chemin hardcode): `third_party/emulators/BizHawk-2.10-linux-x64/EmuHawkMono.sh`
- BizHawk est lance via Electron: `client/app/electron/main.cjs` -> `launchBizHawk()`
- Prerequis BizHawk Linux: `mono` doit etre installe (check via `which mono`)

## Support Wayland et X11 (Linux)
Exigence:
- supporter Wayland et X11
- X11 peut etre primaire

Points pratiques:
- Electron sous Wayland peut tourner via Ozone ou via XWayland selon l'environnement
- recommandation packaging: permettre `ELECTRON_OZONE_PLATFORM_HINT=auto` (sans forcer)
- beaucoup d'emulateurs (Mono, SDL2, Qt) tourneront via XWayland en session Wayland
- objectif: ne pas casser Wayland, pas necessairement "Wayland natif" partout des le debut

## Strategie "single window" realiste
On ne peut pas embed tous les emulateurs "dans" la fenetre Electron de maniere generique.

Approche A (MVP): fullscreen + focus management
- lancer emulateur en fullscreen
- garder SekaiLink en background
- offrir un bouton "Return to SekaiLink" (hotkey configurable) et un overlay minimal

Approche B (Linux): gamescope pour encapsuler
- wrapper de launch pour executer sous `gamescope` quand disponible
- benefices: experience portable-console, resolution/scaling stable, conteneur fenetre
- implementation SekaiLink (actuel):
- `client/app/electron/main.cjs` -> `spawnMaybeGamescope()`
- config: `~/.sekailink/config.json`:
- `windowing.gamescope.enabled` (bool)
- `windowing.gamescope.mode` (`prefer` ou `require`)
- `windowing.gamescope.fullscreen` (bool)
- `windowing.gamescope.width` / `height` (nombre)
- `windowing.gamescope.args` (array de strings)

Approche C: RetroArch-first quand possible
- utiliser RetroArch comme "fenetre universelle" quand un world le permet
- limite: certains worlds exigent BizHawk (Lua connectors), donc pas universel

## Minimiser la friction des fenetres multiples
- regler chaque famille emulateur: auto fullscreen, run-in-background (quand applicable), disable confirmations
- PopTracker: option visible vs headless (si possible) ou launch minimized
- Universal Tracker: preferer integration dans UI Electron plutot qu'une fenetre Kivy

## Cross-platform (Windows)
- sur Windows, on ne peut pas compter sur gamescope
- objectif: launch fullscreen, tracker en arriere-plan, supervision process robuste (kill tree)

## Checklist par famille (a documenter par driver)
- BizHawk: config (RunInBackground, autosave), args `--lua=...` + rom
- Dolphin: profile portable, gecko codes / load folder
- RetroArch: core selection, config portable
- PC modloaders: per-game profile, EAC/anti-cheat disclaimers

Voir aussi:
- inventaire et plan des options emulateurs: `sekailink-client-plan/17-third-party-emulators.md`

## Note de faisabilite: "screen encapsulator" BizHawk (2026-02-18)
Objectif discute:
- lancer BizHawk "dans" une fenetre container SekaiLink plein ecran pour controler placement/focus et contourner les limites Wayland.

Conclusion pratique:
- Windows: faisable (controle fenetre externe via Win32, mode borderless/fullscreen, placement monitor).
- Linux X11: faisable (controle fenetre externe avec APIs X11/outils WM).
- Linux Wayland: pas fiable en "reparent/embed" d'une fenetre externe (bloque par design securite du protocole).

Decision technique:
- ne pas faire d'embed cross-platform direct de BizHawk.
- garder l'orchestration BizHawk dans SekaiLink et traiter Wayland avec strategie compositeur (ex: gamescope) plus tard.
- prioriser d'abord les modifications BizHawk; compositeur Wayland planifie ensuite.
