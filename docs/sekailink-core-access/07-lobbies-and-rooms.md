# 07 - Lobbies et rooms

## Lobbies

Commandes:

```text
lobby list
lobby open <lobby>
lobby create
lobby edit <lobby>
lobby close <lobby>
lobby lock <lobby>
lobby unlock <lobby>
lobby chat <lobby>
lobby join-secret <lobby>
lobby broadcast <lobby> <message>
```

`lobby join-secret` est invisible pour les joueurs ordinaires, mais jamais pour
l'audit. Il doit apparaitre comme `admin_observer`.

Etat MVP actuel:

- `lobby list [limit] [query] [visibility] [status] [offset]` prepare un GET
  prive vers Nexus lobby-admin;
- `lobby open <lobby>` prepare un GET prive vers Nexus lobby-admin;
- l'execution live est read-only, bloquee par defaut, et exige `--execute`,
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` et
  `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN`;
- `lobby create`, `lobby edit`, `lobby close`, `lobby lock`, `lobby unlock`,
  `lobby broadcast` et `lobby join-secret` restent non connectees dans cette
  tranche pour eviter toute mutation non gouvernee.

## Rooms

Commandes:

```text
room list
room open <room>
room summary <room>
room events <room>
room logs <room> --follow
room snapshot <room>
room sync <room>
room pending-items <room>
room client-reports <room>
room request-sklmi-reconnect <room> <player>
room disconnect-runtime <room> <player>
room give-item <room> <target> <item>
```

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
