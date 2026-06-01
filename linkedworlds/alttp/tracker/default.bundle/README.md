# ALTTP Default Bundle Source

Ce dossier est la source canonique clean-room du bundle runtime par defaut
materialise en `tracker/default.zip`.

Ce bundle est un artefact de contenu linkedworld `ALTTP`.

Ce bundle est:

- repo-owned
- coherent avec le contrat linkedworld de ce repo
- enrichi avec les assets, donnees JSON et scripts Lua tracker du pack
  PopTracker epingle
- destine a etre consomme par `Sekaiemu`, pas a specialiser un moteur generique

But:

- donner une materialisation honnete de `tracker/default.zip`
- rendre visibles les surfaces side-by-side attendues
- figer une premiere surface run-complete checks/items/settings
- figer le flux tracker run-complete sous forme de metadata data-driven
- declarer les metadata items/icons, maps/pins, settings et autotabbing
  comme donnees LinkedWorld consommables par Sekaiemu
- fixer une base de packaging et de doc testable avec provenance claire des
  assets statiques inclus

Source de verite:

- `manifest.json`
- `surface.complete.json`
- `tracker-flow.v1.json`
- `item-icon-metadata.json`
- `map-pin-metadata.json`
- `settings-metadata.json`
- `autotabbing-hints.json`
- `location-groups.complete.json`
- `item-slots.complete.json`
- `dungeon-progress.complete.json`
- `room-metadata.complete.json`
- `slot-data.complete.json`
- `maps/*.ppm`
- `poptracker-adapted/`
- `NOTICE.md`

Limites:

- aucune promesse de compatibilite byte-for-byte avec un pack externe
- runtime PopTracker non inclus
- les assets, donnees et scripts Lua PopTracker inclus sont licencies MIT et
  documentes dans `poptracker-adapted/sekailink-adaptation.json`
- Lua est autorise ici uniquement pour la logique tracker du pack PopTracker.
  Il ne doit pas servir a l'emulation, au patching, a SKLMI, a la memoire ou au
  transport reseau.
- aucune garantie que ce zip satisfasse un host attendant un vrai pack
  PopTracker complet
