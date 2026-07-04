# 04 - Roles et permissions

## Admin

Admin a le controle complet, avec confirmations et audit:

- reboot/update services;
- publish/rollback release;
- modifier users et roles;
- modifier configs utilisateur;
- give item;
- maintenance full;
- secrets set/rotate;
- approval queue.

## Service

Service est quasi-admin live, mais les actions dangereuses demandent approval.

Service peut:

- lire logs, users, configs, lobbies, rooms;
- moderer lobby/chat;
- demander diagnostics client;
- request SKLMI reconnect;
- preparer un give item;
- preparer un rollback;
- preparer une fermeture de room/lobby sensible;
- poster annonces Discord/Twitch selon policy.

Service ne peut pas directement:

- reboot/update serveur;
- publier/rollback release;
- delete user;
- modifier roles;
- modifier secrets;
- modifier DB critique;
- give item sans approval.

## Confirmation forte

Toute action dangereuse exige:

- cible tapee exactement;
- raison;
- resume d'impact;
- ops commit.

Exemple:

```text
Type target to confirm: link
Reason: emergency restart after stuck release-latest deploy
```

