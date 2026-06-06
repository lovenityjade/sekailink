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
2. Ouvrir workspace stream.
3. Panels recommandes: Link logs, room logs, lobby list, alerts, Discord/Twitch.
4. Garder F12 Panic pret.
5. Tout incident important devient une note.

## Incident majeur

1. Stabiliser: maintenance scope minimal.
2. Broadcast.
3. Snapshot rooms.
4. Collect logs bundle.
5. Identifier service.
6. Appliquer rollback seulement si impact clair.
7. Clore avec ops commit et postmortem.

## Audit review

1. `audit search`
2. Filtrer par date/role/action.
3. Exporter Markdown/JSON.
4. Verifier actions sans rollback.
5. Creer notes de follow-up.

