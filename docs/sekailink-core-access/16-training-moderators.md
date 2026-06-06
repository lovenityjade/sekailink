# 16 - Formation moderateurs

## Objectif

Former le role Service pour aider en live sans mettre la production en danger.

## Parcours

1. Lire Concepts.
2. Lire Premier login.
3. Lire Navigation TUI.
4. Lire Roles et permissions.
5. Faire les exercices logs.
6. Faire les exercices lobbies/rooms.
7. Faire un incident simule.
8. Lire les limites Service.

## Exercices

### Exercice 1 - Diagnostiquer un joueur

```text
user search <name>
user open <user>
user sessions <user>
user configs <user>
```

But: trouver si le joueur est connecte, sur quel device et avec quelle config.

### Exercice 2 - Suivre une room

```text
room summary <room>
room events <room>
room logs <room> --follow
```

But: identifier qui est connecte, quel runtime parle, et s'il y a des pending
items.

### Exercice 3 - Creer un incident local

```text
incident open training-room sev4 training incident
incident note training-room player reported a reconnect issue
incident pin training-room link:room-runtime sample runtime line
incident status training-room
incident export training-room --file training-room.md
incident close training-room resolved during training
```

But: comprendre le journal append-only, les notes, les pins et l'export
Markdown sans toucher a la production.

### Exercice 4 - Faire une releve de shift

```text
ops timeline training-room
ops handoff training-shift --file training-shift.md
```

But: produire un rapport de releve lisible pour un autre moderateur.

## A ne jamais faire sans Admin

- publier une release;
- rollback release;
- reboot/update serveur;
- modifier role;
- modifier secret;
- injecter item;
- modifier DB critique;
- ignorer un backup gate.

## Checklist avant premier shift

- Je sais me connecter au bastion.
- Je sais verifier mon role.
- Je sais lister logs et rooms.
- Je sais creer un incident local, une note et un pin.
- Je sais produire une releve de shift.
- Je sais demander approval.
- Je sais utiliser F12 Panic en lecture avant action.
