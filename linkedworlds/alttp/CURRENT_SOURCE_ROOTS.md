# Current Source Roots

Source module actuelle:

- `sekaiemu/client/linkedworlds/alttp/`

Sources secondaires:

- `sekaiemu/worlds/alttp/`
- `sklmi/manifests/alttp.phase1.json`
- `sklmi/tests/sklmi_alttp_golden_main.cpp`

Ce que chaque racine apporte:

- `sekaiemu/client/linkedworlds/alttp/`: linkedworld runtime, metadata d'entree, conventions tracker/preset attendues
- `sekaiemu/worlds/alttp/`: patching, options, logique de generation, validations ROM
- `sklmi/manifests/alttp.phase1.json`: snapshot de forme de transport memoire,
  a redeclarer ici comme contenu ALTTP
- `sklmi/tests/sklmi_alttp_golden_main.cpp`: tests de reference pour un
  runtime consommant ce contenu

Etat actuel:

- `linkedworld.json` present
- base generation `worlds/alttp` presente
- manifest `SKLMI` present

Travail attendu:

- normaliser `manifest/ metadata/ bridge/ tracker/ patch/ presets/ docs/`
- faire vivre ici le linkedworld `ALTTP`, meme si certains contrats utilisent
  une forme compatible `SKLMI`

Priorite de reprise:

1. considerer `sekaiemu/client/linkedworlds/alttp/linkedworld.json` comme source de comparaison, pas comme fichier canonique de ce repo
2. considerer `sklmi/manifests/alttp.phase1.json` comme snapshot de transport,
   pas comme proprietaire metier d'ALTTP
3. extraire ensuite les contrats de patch depuis `sekaiemu/worlds/alttp/`
