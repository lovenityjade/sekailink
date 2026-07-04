# 11 - Scheduler

## Types de jobs

- release client;
- pack check;
- pack publish;
- maintenance window;
- backup;
- health probe;
- Discord announcement;
- Twitch announcement;
- cleanup;
- release verification.

## Commandes

```text
schedule list
schedule calendar
schedule add <name> <when> <command> --confirm schedule:<name>:add
schedule edit <job> key=value [key=value...] --confirm schedule:<job>:edit
schedule pause <job> [reason] --confirm schedule:<job>:pause
schedule resume <job> [reason] --confirm schedule:<job>:resume
schedule run-now <job> [reason] --confirm schedule:<job>:run-now
schedule history
```

Etat Core Access actuel:

- `schedule add` ajoute un draft local dans `scheduler/jobs.jsonl`;
- `schedule edit`, `schedule pause`, `schedule resume` et `schedule run-now`
  creent des drafts locaux audites dans `drafts/schedule.jsonl`;
- aucun job n'est arme, modifie ou execute automatiquement;
- toutes les actions Admin exigent une confirmation exacte.

## Champs job

- owner;
- type;
- target;
- interval ou cron;
- next_run;
- last_run;
- last_success;
- last_error;
- enabled;
- approval_required;
- ops_commit_id.

## Calendrier

Le calendrier doit etre lisible par jour, semaine et mois. Les jobs sensibles
affichent leur approval status.
