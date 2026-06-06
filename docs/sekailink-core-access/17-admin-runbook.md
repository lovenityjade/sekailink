# 17 - Runbook Admin

## Release day

1. `ops doctor --verbose`
2. `release current`
3. `release verify-cdn`
4. `schedule calendar`
5. `maintenance schedule client_update_only <start> <end>` si necessaire.
6. Publier via `release publish <manifest>`.
7. Verifier clients Linux/Windows.
8. Creer ops note avec resultat.

## Stream/live event

1. `ops doctor`
2. Ouvrir dashboard dense.
3. `incident open stream-<date> sev4 live monitoring`
4. Panels recommandes: Link logs, room logs, lobby list, alerts, Discord/Twitch.
5. Garder F12 Panic pret.
6. Tout incident important devient `incident note` ou `incident pin`.
7. `ops exports`
8. `ops handoff stream-<date> --file stream-<date>.md` en fin de stream.

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
5. `ops paths`
6. `ops exports [query]`
7. Verifier actions sans rollback.
8. Creer notes de follow-up.
