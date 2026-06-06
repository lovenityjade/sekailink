# 13 - Cleanup et backups

## Scope

Cleanup v1 couvre:

- logs;
- caches;
- fichiers temporaires;
- old update bundles;
- generation spool;
- sessions expirees;
- vieux lobbies;
- anciens audits archives;
- stale room artifacts.

## Commandes

```text
cleanup plan logs
cleanup plan db
cleanup plan spool
cleanup plan all
cleanup apply <plan_id>
cleanup history
cleanup rollback <id>
```

## Regles

- Dry-run obligatoire.
- Estimation fichiers/lignes/bytes/rows.
- Backup gate pour DB cleanup.
- Confirmation cible + raison.
- Ops commit avec rapport detaille.

## Backup gate

Avant une action dangereuse, Core Access verifie le dernier backup. Si le backup
est absent ou trop vieux, l'action est bloquee ou demande une approval Admin
explicite selon la policy.

