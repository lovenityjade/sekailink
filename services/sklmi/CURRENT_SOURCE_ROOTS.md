# Current Source Roots

Source monorepo courante:

- `sklmi/include/`
- `sklmi/src/`
- `sklmi/tests/`
- `sklmi/docs/`
- `sklmi/fixtures/`
- `sklmi/source-snapshots/`

Zones a trier:

- manifests de jeu hors repo
  - les manifests specifiques a un jeu doivent vivre dans les repos
    `LinkedWorld`, pas dans `sklmi`
- gouvernance de contrat hors repo
  - les schemas, versions et politiques partagees doivent finir dans
    `Contracts`
- `sklmi/fixtures/`
  - support de test uniquement
- `sklmi/source-snapshots/`
  - references de migration uniquement

Etat:

- le coeur actif du runtime vit dans `include/` et `src/`
- `tests/` valide le comportement public et la compatibilite migration
- `docs/` sert d'orientation et de handoff
- il n'y a pas de dossier `manifests/` runtime actif dans ce clean-room
