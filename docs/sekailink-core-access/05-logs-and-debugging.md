# 05 - Logs et debugging

## Sources

Core Access doit lister deux vues:

- par serveur/service;
- par incident/correlation.

Sources prioritaires:

- journalctl systemd;
- fichiers logs app;
- nginx/CDN;
- Nexus auth/admin;
- Link chat/lobby;
- Worlds generation;
- Evolution packs/releases;
- Pulse;
- room multiserver logs;
- room events;
- DB/backup logs;
- client reports.

## Commandes de base

```text
logs list
logs list --by-server
logs list --by-incident
logs tail <source>
logs search <query> [source|all]
logs filter user:<id> lobby:<id> room:<id> correlation:<id> source:<source|all>
logs pin <source> <text>
logs note <source> <text>
logs export [query] --format md
```

Etat MVP actuel:

- `logs search` et `logs filter` produisent un plan dry-run par defaut;
- `--execute` reste bloque sans `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`;
- les prefixes `user:`, `lobby:`, `room:`, `correlation:`, `item:` et
  `runtime:` sont normalises en valeurs de recherche;
- `logs pin` ecrit une preuve locale dans `log-pins/pins.jsonl`;
- `logs note` ecrit une note locale ciblee `log:<source>`;
- `logs export` produit un fichier local Markdown, JSONL ou TXT sous
  `exports/`;
- les logs client Sekaiemu/SKLMI doivent passer par une demande
  `client diagnostics-request` avec consentement utilisateur;
- aucune ligne de log n'est ecrite dans Nexus DB dans cette tranche.

## Notes liees

Une note liee capture:

- extrait original;
- source;
- timestamps;
- tags;
- cible;
- ops_commit_id;
- auteur.

## Verbose filtre

Le cockpit doit etre verbose, mais filtre par defaut:

- erreurs et warnings visibles en haut;
- details disponibles en drilldown;
- bruit groupable par service, room, user ou correlation.
