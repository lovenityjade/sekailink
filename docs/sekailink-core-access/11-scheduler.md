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
schedule add
schedule edit <job>
schedule pause <job>
schedule resume <job>
schedule run-now <job>
schedule history <job>
```

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

