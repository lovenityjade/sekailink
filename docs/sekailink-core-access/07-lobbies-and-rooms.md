# 07 - Lobbies et rooms

## Lobbies

Commandes:

```text
lobby list
lobby open <lobby>
lobby create <lobby_id> <name> [visibility] [owner_username] [description] --confirm lobby:<lobby_id>:create
lobby edit <lobby_id> key=value [key=value...] --confirm lobby:<lobby_id>:edit
lobby close <lobby_id> --confirm lobby:<lobby_id>:close
lobby lock <lobby_id> [reason] --confirm lobby:<lobby_id>:lock
lobby unlock <lobby_id> [reason] --confirm lobby:<lobby_id>:unlock
lobby chat <lobby_id>
lobby join-secret <lobby_id> [reason] --confirm lobby:<lobby_id>:join-secret
lobby broadcast <lobby_id> <message> --confirm lobby:<lobby_id>:broadcast
```

`lobby join-secret` est invisible pour les joueurs ordinaires, mais jamais pour
l'audit. Il doit apparaitre comme `admin_observer`.

Etat MVP actuel:

- `lobby list [limit] [query] [visibility] [status] [offset]` prepare un GET
  prive vers Nexus lobby-admin;
- `lobby open <lobby>` prepare un GET prive vers Nexus lobby-admin;
- `lobby create`, `lobby edit` et `lobby close` utilisent les routes privees
  Nexus lobby-admin confirmees, avec `--execute`,
  `SEKAILINK_CORE_ACCESS_NEXUS_MUTATION=1` et confirmation exacte;
- `lobby lock`, `lobby unlock`, `lobby broadcast` et `lobby join-secret`
  creent des drafts audites locaux;
- `lobby chat` prepare une recherche de logs sur `link:chat-api`;
- l'execution live est read-only, bloquee par defaut, et exige `--execute`,
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` et
  `SEKAILINK_CORE_ACCESS_NEXUS_LOBBY_ADMIN_TOKEN`;
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` reste un fallback local compatible;
- les mutations lobby live sont bloquees sans role/gate/confirmation.

## Rooms

Commandes:

```text
room list [limit] [query] [room_type] [connection_state] [offset]
room open <room_id>
room summary <room_id>
room events <room_id> [limit] [event_type] [severity] [offset] [source]
room logs <room_id> --follow
room snapshot <room_id>
room sync <room_id> [reason] --confirm room:<room_id>:sync
room pending-items <room_id>
room client-reports <room_id> [limit] [report_type] [severity] [source] [offset]
room request-sklmi-reconnect <room_id> <player> [reason] --confirm room:<room_id>:request-sklmi-reconnect
room disconnect-runtime <room_id> <player|runtime> [reason] --confirm room:<room_id>:disconnect-runtime
room give-item <room_id> <target> <item> [reason] --confirm room:<room_id>:give-item
```

Etat actuel:

- `room list` utilise `GET /rooms` sur Nexus room-query;
- `room open` et `room snapshot` utilisent `GET /rooms/{room_id}`;
- `room summary` utilise `GET /rooms/{room_id}/diagnostics`;
- `room events` utilise `GET /rooms/{room_id}/events`;
- `room client-reports` utilise `GET /rooms/{room_id}/client-reports`;
- `room pending-items` lit le snapshot et demande d'inspecter
  `pending_delivery_count`/`received_items`;
- `room logs` prepare une recherche `link:room-runtime` ou un follow
  `sekailink-room-server`;
- `room sync`, `room request-sklmi-reconnect`, `room disconnect-runtime` et
  `room give-item` creent des drafts audites locaux, sans mutation runtime.

Execution read-only room-query:

- `--execute`
- `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`
- `SEKAILINK_CORE_ACCESS_NEXUS_ROOM_QUERY_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` fallback

## Give item

Policy:

- Admin ou approval Admin.
- Snapshot automatique avant action.
- Raison obligatoire.
- Impact live affiche.
- Ops commit.

## SKLMI reconnect

`room request-sklmi-reconnect` envoie un signal client/runtime. Une modification
SKLMI pour supporter une nouvelle forme de reconnect demande accord explicite.
