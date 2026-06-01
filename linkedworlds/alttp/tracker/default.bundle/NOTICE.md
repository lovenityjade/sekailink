# SekaiLink Tracker Bundle Notice

Le contenu SekaiLink de `tracker/default.bundle/` materialise les chemins
canoniques `tracker/default.zip` et `config/default.yaml`.

Contraintes:

- aucun runtime PopTracker copie
- aucun script Lua execute ou integre comme runtime SekaiLink
- aucune integration SNI/BizHawk
- les assets tiers inclus sont statiques et gardent leur provenance

Provenance des assets statiques inclus:

- source: `https://github.com/StripesOO7/alttp-ap-poptracker-pack`
- revision locale: `bff7a80d7cfb53de5c5b49923ddedb3344108b96`
- licence: MIT
- details: `poptracker-adapted/sekailink-adaptation.json`
- licence source: `poptracker-adapted/LICENSE`

Ce bundle reste une adaptation SekaiLink data-driven: `Sekaiemu` consomme les
assets et metadata directement, sans lancer PopTracker ni Lua.
