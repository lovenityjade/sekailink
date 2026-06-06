# 06 - Users et configs

## Users

Commandes principales:

```text
user search <query>
user open <user>
user create <user> <email> <role> --password-env ENV --confirm user:<user>:create
user edit <user> key=value --confirm user:<user>:edit
user disable <user> --confirm user:<user>:disable
user sessions <user>
user devices <user>
user audit <user>
user revoke-sessions <user> --confirm user:<user>:revoke-sessions
user force-password-reset <user> --confirm user:<user>:force-password-reset
```

Etat MVP actuel:

- `user search`, `user open`, `user sessions`, `user devices` et `user audit`
  sont connectees au Nexus Identity admin HTTP en read-only;
- l'execution live exige `--execute`,
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` et
  `SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN`;
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` reste un fallback local compatible;
- `user create`, `user edit`, `user disable`, `user revoke-sessions` et
  `user force-password-reset` utilisent les routes HTTP admin Nexus Identity
  confirmees;
- toute mutation exige le role Admin, `--execute`,
  `SEKAILINK_CORE_ACCESS_NEXUS_MUTATION=1`, le token Identity admin, et la
  confirmation exacte affichee dans le dry-run;
- `user create` lit le mot de passe depuis `--password-env ENV` pour eviter de
  l'inscrire dans l'historique shell ou dans l'audit local; le body affiche en
  dry-run remplace toujours le mot de passe par `<redacted>`.

Note importante: `user open`, `user sessions`, `user devices` et `user audit`
declenchent une entree d'audit admin cote Identity, meme si elles ne modifient
pas le compte cible.

Les mutations utilisateur declenchent egalement l'audit admin cote Nexus
Identity. Core Access garde une trace locale de la commande et de son statut,
mais ne journalise jamais la valeur des tokens ni le mot de passe env.

Champs `user edit` supportes:

```text
email=<email>
display_name=<name>
avatar_url=<url>
bio=<text>
locale=<locale>
role=<player|service|moderator|admin>
email_verified=<true|false>
disabled=<true|false>
permissions=<permission-a,permission-b>
```

Ces commandes ne modifient pas SKLMI, Sekaiemu, Client Core, les packs, ni les
serveurs room. Elles appellent uniquement le contrat Nexus Identity admin deja
present cote serveur.

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
