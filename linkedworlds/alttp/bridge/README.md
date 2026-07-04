# Bridge Status

Le chemin `bridge/sklmi.phase1.json` est le seul contrat memoire concret
actuellement pose dans ce repo.

Canonique actuel:

- `sklmi.phase1.json`
- `bridge.manifest.json`

Important:

- `sklmi.phase1.json` reste canonique pour cette etape de reprise
- son contenu appartient au linkedworld `ALTTP`
- il declare des checks, items, actions et ecritures memoire jeu-specifiques
- `SKLMI` n'est ici qu'une famille de runtime capable de consommer cette forme
  de contrat
- il reste transitoire vis-a-vis d'une future forme bridge finale plus complete
- `source-snapshots/sklmi.phase1.current.json` reste une capture amont

Travail suivant:

- inventorier les checks/actions encore manquants au-dela du runtime-core deja
  declare
- relier la couverture bridge aux besoins tracker et room
- definir ensuite une migration eventuelle hors du profil `phase1`
