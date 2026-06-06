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
maintenance enable <scope> --message <msg>
maintenance disable
maintenance schedule <scope> <start> <end>
maintenance broadcast
maintenance history
```

## F12 Panic

F12 ouvre un panneau controle:

- maintenance full;
- freeze generation;
- broadcast incident;
- snapshot active rooms;
- collect logs bundle.

Chaque action Panic demande confirmation forte.

