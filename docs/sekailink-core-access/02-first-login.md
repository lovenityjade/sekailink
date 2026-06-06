# 02 - Premier login

## Preconditions

- Avoir un compte Linux autorise sur le bastion Nexus.
- Avoir un compte SekaiLink avec role Admin ou Service.
- Avoir un mapping Linux user -> SekaiLink user_id dans Nexus.

## Connexion

```sh
ssh nexus-vps
sekailink-core-access
```

Core Access affiche ensuite un prompt de login SekaiLink.

## Verification apres login

Executer:

```text
auth whoami
```

Resultat attendu:

- Linux user courant.
- SekaiLink user_id.
- Role.
- Capabilities.
- Heure d'expiration de session.

## Premier controle

Executer:

```text
server status all
release current
maintenance status
```

Si un serveur est en rouge, ouvrir ses logs avec F8 ou:

```text
server logs <server> <service> --follow
```

