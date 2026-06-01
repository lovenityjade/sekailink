# Tracker Status

Le module `ALTTP` dispose maintenant d'une baseline tracker clean-room-owned
materialisee au chemin canonique attendu.

Canonique ici:

- `tracker.manifest.json`
- `runtime-bindings.phase1.json`
- `default.bundle/manifest.json`
- `default.bundle/surface.complete.json`
- `default.bundle/tracker-flow.v1.json`
- `default.bundle/maps/*.ppm`
- `default.zip`

Etat reel:

- le metadata source continue de mentionner un pack externe via son UID
- le clean-room materialise maintenant un `tracker/default.zip` honnete et
  repo-owned
- la source de verite de ce zip est `tracker/default.bundle/`
- les assets statiques et donnees JSON du pack PopTracker epingle sont inclus
  sous `tracker/default.bundle/poptracker-adapted/`
- la cible "run complete" checks/items/settings est normalisee dans
  `tracker/default.bundle/surface.complete.json`
- les metadata d'items/icons, maps/pins, settings et autotabbing sont
  declarees comme donnees LinkedWorld dans `tracker/default.bundle/`
- les bindings runtime restent une projection du contenu ALTTP, meme si le
  chemin memoire consomme par `SKLMI` reste `phase1`
- la logique de flux tracker run-complete est maintenant decrite sous forme
  data-driven: feed items serveur, metadata seed/settings, application settings
  et autotabbing
- la presentation cible side-by-side et les metadata runtime attendues sont
  canonisees ici

Limites:

- ce bundle ne lance pas PopTracker, Lua, SNI ou le runtime du pack tiers
- les assets tiers inclus restent statiques et licencies MIT
- la traduction runtime demeure une implementation SekaiLink data-driven
- aucune garantie de compatibilite avec un host qui exigerait un vrai pack
  externe complet

Travail suivant:

- etendre la couverture ALTTP au-dela des families deja declarees
- prouver en live une plus grande part de la surface cible normalisee
- remplacer progressivement les placeholders utilises par Sekaiemu par les
  assets statiques inclus lorsque le renderer supporte les formats requis
- distinguer clairement reference historique externe et artefacts reellement
  portes par ce repo
