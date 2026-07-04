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

## SSH explicite

Core Access garde l'ouverture SSH interactive derriere une confirmation et une
gate locale:

```text
ssh open <server> <reason> --confirm ssh:<server>:open
ssh open <server> <reason> --confirm ssh:<server>:open --execute
```

Pour executer vraiment, l'environnement local doit contenir:

```sh
export SEKAILINK_CORE_ACCESS_SSH_OPEN=1
```

Chaque tentative cree un draft audite local avant l'ouverture.

## Updates serveurs

Les commandes update ne lancent pas de package manager et ne redemarrent aucun
service. Elles servent a creer un plan audite et une trace d'intention:

```text
server update plan <server> <scope>
server update apply <server> <plan_id|reason> --confirm server-update:<server>:apply
```

Avant une application reelle hors Core Access, faire au minimum:

- `ops snapshot <label>`;
- `server agent-health <server> --execute`;
- `server agent-services <server> --execute`;
- backup des donnees pertinentes;
- maintenance/broadcast si le scope est visible utilisateur.
