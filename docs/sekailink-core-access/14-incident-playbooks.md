# 14 - Playbooks incidents

## Room server down

1. `room list`
2. `room summary <room>`
3. `room logs <room> --follow`
4. `server status link`
5. `logs filter room:<room>`
6. F4 pour noter les lignes importantes.
7. Si necessaire: F12 -> snapshot rooms.

## SKLMI non connecte

1. `room summary <room>`
2. Verifier presence player et runtime.
3. `room events <room>`
4. `client request-sklmi-reconnect <player>`
5. Si echec: demander diagnostics avec consentement.

## Client update brise

1. `release current`
2. `release verify-cdn`
3. `logs filter release:<version>`
4. `maintenance enable client_update_only --message "..."`
5. `release rollback <version>` si Admin.

## Generation bloquee

1. `server status worlds`
2. `logs tail worlds:generation`
3. `schedule history health-probe-worlds`
4. `maintenance enable generation_only --message "..."`

## CDN pack invalid

1. `pack repo list`
2. `pack validate <id>`
3. `pack rollback <id> <version>`
4. `broadcast game <game> "..."`

## Bot Discord/Twitch down

1. `discord status` ou `twitch status`
2. `discord logs` ou `twitch logs`
3. `discord reload` ou `twitch connect <channel>`
4. Creer note incident si recurring.

## Maintenance d'urgence

1. F12 Panic.
2. Lire impact.
3. Choisir scope minimal.
4. Taper cible + raison.
5. Broadcast.
6. Collect logs bundle.

