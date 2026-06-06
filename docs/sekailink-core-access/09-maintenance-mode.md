# 09 - Maintenance mode

## Scopes

- `full`
- `generation_only`
- `cdn_only`
- `read_only`
- `lobby_only`
- `client_update_only`

## Etat

Champs:

- enabled;
- scope;
- message;
- starts_at;
- ends_at;
- allow_admin_bypass;
- created_by;
- reason;
- ops_commit_id.

## Commandes

```text
maintenance status
maintenance enable <scope> <message> --confirm maintenance:<scope>:enable
maintenance disable [scope] [reason] --confirm maintenance:<scope>:disable
maintenance schedule <scope> <start> <end> <message>
maintenance broadcast <scope> <message> --confirm maintenance:<scope>:broadcast
maintenance history
```

Etat Core Access actuel:

- `maintenance schedule` cree un draft local dans `maintenance/current.txt` et
  `maintenance/history.jsonl`;
- `maintenance enable`, `maintenance disable` et `maintenance broadcast` creent
  des drafts locaux audites dans `drafts/maintenance.jsonl`;
- aucune commande maintenance ne modifie encore le mode maintenance serveur ou
  client;
- les actions sensibles exigent une confirmation exacte.

## F12 Panic

F12 ouvre un panneau controle:

- maintenance full;
- freeze generation;
- broadcast incident;
- snapshot active rooms;
- collect logs bundle.

Chaque action Panic demande confirmation forte.
