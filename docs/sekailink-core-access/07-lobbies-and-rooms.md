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

