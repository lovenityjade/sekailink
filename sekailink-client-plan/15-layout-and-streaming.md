# Layout multi-ecran + streamer mode (handheld/desktop)

Objectif: une UX "console" sur handheld, mais configurable sur desktop multi-ecrans, et streamer-friendly.

## Principe: Layout Manager + Session Orchestrator
- chaque "session" (room+slot) decrit ou vont:
  - fenetre Jeu (emulateur ou jeu)
  - fenetre Tracker (PopTracker ou panel UT)
  - fenetre SekaiLink (UI principale)
- le main process garde le controle: spawn, stop, relaunch, logs, cleanup

## Mode handheld (Linux, Wayland ou X11)
Approche recommande: `gamescope`.
- lancer le jeu sous gamescope (fullscreen, scaling stable)
- SekaiLink reste derriere
- un bouton "Return to SekaiLink" (et hotkey) bascule le focus

Pourquoi c'est mieux qu'Archipelago:
- pas besoin de jongler avec plusieurs fenetres
- focus stable
- meme experience sur Steam Deck/ROG/desktop Bazzite

Tracker dans ce mode:
- ideal: Universal Tracker (headless) rendu dans SekaiLink (aucune fenetre tracker)
- fallback: PopTracker lance, mais option "minimized" ou "hidden" si possible

## Mode desktop (X11 ou Wayland) avec presets d'affichage
Presets proposes:
- "Single monitor": jeu fullscreen + tracker en overlay/panel SekaiLink
- "Dual monitor": jeu fullscreen ecran 1, tracker ecran 2, SekaiLink ecran 2
- "Side-by-side": jeu borderless 16:9 a gauche, tracker a droite (meme ecran)

Implementation (realisable):
- SekaiLink window: positionnable facilement (Electron `screen.getAllDisplays()`)
- Jeu window:
  - si gamescope: choisir l'output cible (monitor) via args gamescope
  - sinon: lancer en fullscreen sur le bon display quand l'emulateur le supporte (best-effort)
- Tracker window:
  - PopTracker: best-effort pour le placer

Note Wayland:
- repositionner une fenetre externe (PopTracker, emulateur) n'est pas toujours fiable sous Wayland (depend du compositor)
- donc "Side-by-side" automatique est plus fiable si:
  - on utilise gamescope pour contenir le jeu
  - et on garde le tracker dans SekaiLink (UT panel)

## Streamer mode (prioritaire)
Objectif streamer:
- fenetres stables, capture OBS facile, pas d'infos sensibles, pas de surprises focus.

Features proposes:
- fenetres avec titres stables:
  - "SekaiLink - Game"
  - "SekaiLink - Tracker"
  - "SekaiLink - Logs"
- une fenetre "Tracker" optionnelle en BrowserWindow (si UT panel ou tracker web) pour OBS Window Capture
- un "Overlay" minimal (BrowserWindow always-on-top) pour stats utiles (slot, room, checks)

## Securite streamer: mot de passe et argv
Probleme actuel:
- PopTracker est lance avec `--ap-pass`.
- sur Linux/Windows, les args de process peuvent etre visibles localement.

Recommandation:
- modifier PopTracker patch pour accepter le password via:
  - env var (ex: `SEKAILINK_AP_PASS`) ou
  - stdin (ex: `--ap-pass-stdin`) ou
  - fichier temporaire chmod 600
- streamer mode: ne jamais mettre le password dans argv.

## Config (a stocker)
- `~/.sekailink/config.json`:
  - `windowing.gamescope`: `{enabled, mode, fullscreen, width, height, args}`
  - `layout.preset`: string libre, presets recommandes: `handheld`, `desktop`, `desktop_dual`, `streamer_dual`
  - `layout.mode`: `side_by_side` | `separate_displays` | absent (auto)
  - `layout.game_display`: index display (Electron)
  - `layout.tracker_display`: index display (Electron)
  - `layout.split`: float `0.1..0.9` (ratio du jeu en side-by-side)
  - `trackerVariants[game_id]`: variante PopTracker (layout pack)
