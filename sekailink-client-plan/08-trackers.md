# Tracking (PopTracker + Universal Tracker)

## PopTracker (etat)
- binaire bundle: `client/runtime/poptracker/poptracker`
- IPC Electron: `tracker:launch`, `tracker:stop`, `tracker:installPack`, `tracker:status`, `tracker:validatePack`
- `tracker:launch` doit supporter: pack uid ou pack path; `--pack-variant`; `--ap-host`; `--ap-slot`; `--ap-pass`
- `tracker:installPack` telecharge une release GitHub (asset zip) et extrait dans `userData/poptracker/packs/<gameId>`

Docs SekaiLink:
- `sekailink-docs/POPTRACKER.md`

## Packs avec plusieurs options (variants)
Exigence:
- un pack de tracking peut avoir plus qu'une option (ex: vertical/horizontal)

Etat:
- le binaire supporte `--pack-variant` (patch SekaiLink)
- le binaire supporte `--list-pack-variants` (patch SekaiLink)

A faire (UI/UX):
- exposer "variant" dans le runtime contract (manifest)
- bouton "Tracker settings" par jeu
- selection persistante dans `~/.sekailink/config.json`

## Packs avec plusieurs sources (resolution)
Clarification:
- ce n'est pas ce que tu voulais dire par "multi-source" (toi = variants/layouts).
- par contre, SekaiLink peut quand meme beneficier d'une strategie de "sources" pour les power users et pour des fallbacks.

Exemples de sources possibles (optionnel):
- pack deja installe localement (cache SekaiLink)
- pack dans `~/PopTracker/packs` (utilisateur power user)
- pack via GitHub releases (repo officiel par jeu)
- pack via repository PopTracker (packlist JSON) si on decide d'integrer un index
- pack via URL directe (zip) pour debug

Approche recommande (SekaiLink):
- prendre un "repo canonique" par jeu (source de verite) pour install/update automatique
- permettre des overrides pour power users (dossier local) sans casser le chemin canonique
- SekaiLink resolve vers un `packPath` local stable, puis lance PopTracker avec ce `packPath`

Note:
- l'approche "sources via flags PopTracker" n'est pas necessaire pour atteindre l'objectif SekaiLink.
- on garde PopTracker simple (packPath + variant) et SekaiLink orchestre l'installation/cache.

## Universal Tracker (UT)
- code: `worlds/tracker/`
- docs: `worlds/tracker/docs/client-integration.md`, `worlds/tracker/docs/setup.md`

Ce que UT apporte:
- un tracker "universel" base sur la logique des worlds
- peut servir de text client aussi

Risques et limites:
- desync si logique non-deterministe (entrance rando, random start, etc.)
- depend d'une generation interne basee sur les yamls

Integration dans SekaiLink Desktop

Option A (court terme): UT comme app externe
- lancer `python Launcher.py "Universal Tracker" -- --nogui ...`
- moins bon pour "limiter les fenetres"

Option B (recommande): UT engine headless dans SekaiLink
- ne pas embarquer la UI Kivy
- ajouter un wrapper python headless `universal_tracker_wrapper.py`
- wrapper: se connecte au MultiServer, calcule in-logic, renvoie des snapshots JSON
- renderer React: affiche un panel "Universal Tracker"

Contract UT dans manifest
- `universal_tracker.supported = true`
- `universal_tracker.flags = ["disable_ut", "ut_can_gen_without_yaml"]`
