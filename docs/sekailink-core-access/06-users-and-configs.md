# 06 - Users et configs

## Users

Commandes principales:

```text
user search <query>
user open <user>
user sessions <user>
user devices <user>
user audit <user>
user revoke-sessions <user>
user force-password-reset <user>
```

Etat MVP actuel:

- ces commandes produisent un plan Nexus Identity read-only;
- `--execute` reste bloque tant que l'entree live Identity admin CLI/API n'est
  pas documentee;
- aucune mutation utilisateur n'est disponible sans contrat Nexus explicite.

Le modele prive documente cote Nexus existe (`listusers`, `userinfo`,
`listsessions`, `listdevices`, `useraudit`), mais Core Access ne l'executera pas
tant que l'entree exacte n'est pas confirmee dans le checkout ou sur le host.

## Configs utilisateur

Nexus reste la source de verite. Les YAML generes ne sont pas edites comme
source.

Commandes:

```text
user configs <user>
user config open <user> <config>
user config diff <user> <config> <version>
user config export <user> <config> --format yaml
user config edit <user> <config>
```

## Regles d'edition

- Admin peut modifier directement avec diff et ops commit.
- Service peut lire/diff/export.
- Service peut preparer une edition via approval.
- Chaque edition documente impact Client Core, generation, room et rollback.
