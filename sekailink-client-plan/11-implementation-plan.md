# Plan d'implementation (phases)

But: avancer par familles, avec des increments utilisables.

## Phase 0: Stabilisation des fondations (1-2 semaines)
- de-hardcoder `client/app/src/pages/Lobby.tsx` (remplacer `resolveModuleId()` par une resolution basee sur manifests)
- ajouter un "Session log" unifie (server terminal Socket.IO + commonclient logs)
- ajouter notes build/run Wayland+X11
- packaging: permettre `ELECTRON_OZONE_PLATFORM_HINT=auto`
- tester en session Wayland et en session X11

Livrables:
- `PatchResolver` + registry manifests
- UI logs visibles

## Phase 1: BizHawk family complete (2-4 semaines)
- supporter toutes les extensions BizHawk listables (source: `sekailink-docs/BIZHAWK-CONNECTORS.md`)
- etendre `client/runtime/patcher_wrapper.py` pour plus de jeux a ROM
- installer packs PopTracker automatiquement quand `tracker_pack_uid` est un repo/URL
- ajouter support pack variants (UI + config)

Livrable:
- one-button flow pour un ensemble de jeux BizHawk (au moins 10)

## Phase 2: Slot files et pipelines non-AP-patch (2-6 semaines)
- identifier les jeux qui renvoient `download_slot_file`
- definir un module runtime + driver pour chaque famille
- implementer le pipeline (extract, configure, launch)

Priorites:
- jeux populaires
- jeux avec guides clairs

## Phase 3: SNIClient + SNES family (2-4 semaines)
- creer wrapper headless SNIClient
- supporter attach/autoreconnect
- UI cliquable pour SNES connect

## Phase 4: Dolphin family (Twilight Princess, etc.) (4-8 semaines)
- external patchers + mod installs
- profiles Dolphin portables
- option gamescope pour limiter les fenetres

## Phase 5: Universal Tracker integration (optionnel mais utile)
- wrapper headless UT engine
- panel UI in-app

## Phase 6: Windows parity
- paths + process management
- bundling emulators
- installer experiences

