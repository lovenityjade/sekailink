# 17 - Runbook Admin

## Release day

1. `release current`
2. `release verify-cdn`
3. `schedule calendar`
4. `maintenance schedule client_update_only <start> <end>` si necessaire.
5. Publier via `release publish <manifest>`.
6. Verifier clients Linux/Windows.
7. Creer ops note avec resultat.

## Stream/live event

1. Ouvrir dashboard dense.
2. `incident open stream-<date> sev4 live monitoring`
3. Panels recommandes: Link logs, room logs, lobby list, alerts, Discord/Twitch.
4. Garder F12 Panic pret.
5. Tout incident important devient `incident note` ou `incident pin`.
6. `ops handoff stream-<date> --file stream-<date>.md` en fin de stream.

## Incident majeur

1. `incident open major-<date>-<scope> sev1 <summary>`
2. Stabiliser: maintenance scope minimal.
3. Broadcast.
4. Snapshot rooms.
5. Collect logs bundle.
6. Identifier service.
7. Appliquer rollback seulement si impact clair.
8. `incident close major-<date>-<scope> <resolution>`
9. Clore avec ops commit et postmortem.

## Audit review

1. `audit search`
2. Filtrer par date/role/action.
3. Exporter Markdown/JSON.
4. `ops timeline [query]`
5. Verifier actions sans rollback.
6. Creer notes de follow-up.
