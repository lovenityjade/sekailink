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
logs search <query>
logs filter user:<id> lobby:<id> room:<id> correlation:<id>
logs pin
logs note
logs export --format md
```

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

