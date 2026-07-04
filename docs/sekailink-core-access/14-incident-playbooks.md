# 14 - Playbooks incidents

## Room server down

1. `incident open room-<room> sev2 room server symptoms`
2. `room list`
3. `room summary <room>`
4. `room logs <room> --follow`
5. `server status link`
6. `logs filter room:<room> source:link:room-runtime`
7. `incident pin room-<room> link:room-runtime <important line>`
8. `incident note room-<room> <decision or next step>`
9. Si necessaire: `ops snapshot room-<room>`.

## SKLMI non connecte

1. `incident open sklmi-<room>-<player> sev3 SKLMI not connected`
2. `room summary <room>`
3. Verifier presence player et runtime.
4. `room events <room>`
5. `logs filter runtime:<player> source:link:room-runtime`
6. `incident pin sklmi-<room>-<player> link:room-runtime <important line>`
7. `client request-sklmi-reconnect <player>`
8. Si echec: `client diagnostics-request <player> sklmi-<room>-<player> <reason> --include sekaiemu,sklmi,client`
9. Exporter la demande si besoin avec `client diagnostics-export <player> --file diagnostics-<player>.md`.

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
3. `incident open emergency-<scope> sev1 <summary>`
4. Choisir scope minimal.
5. Taper cible + raison.
6. Broadcast.
7. Collect logs bundle.
8. `incident export emergency-<scope> --file emergency-<scope>.md`

## Releve apres incident

1. `ops timeline <label>`
2. `incident status <label>`
3. `incident export <label> --file <label>.md`
4. `ops handoff shift-<date> --file shift-<date>.md`

`incident export` isole un incident. `ops handoff` resume le shift complet:
incidents, notes, pins, approvals, maintenance, scheduler, packs, banners et
audit local.
