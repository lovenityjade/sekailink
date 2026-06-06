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

### Exercice 3 - Creer une note

1. Ouvrir un log.
2. Selectionner une plage.
3. F4.
4. Ajouter tags.
5. Exporter en Markdown.

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
- Je sais creer une note.
- Je sais demander approval.
- Je sais utiliser F12 Panic en lecture avant action.

