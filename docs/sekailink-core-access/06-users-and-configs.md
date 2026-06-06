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

- `user search`, `user open`, `user sessions`, `user devices` et `user audit`
  sont connectees au Nexus Identity admin HTTP en read-only;
- l'execution live exige `--execute`,
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` et
  `SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN`;
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` reste un fallback local compatible;
- aucune mutation utilisateur n'est disponible sans contrat Nexus explicite.

Note importante: `user open`, `user sessions`, `user devices` et `user audit`
declenchent une entree d'audit admin cote Identity, meme si elles ne modifient
pas le compte cible.

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
