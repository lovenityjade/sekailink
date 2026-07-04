# 12 - Bots Discord et Twitch

Les configs bots vivent dans Nexus et sont auditees. Les secrets sont set/rotate
seulement, jamais affiches en clair. Core Access ne lit pas les fichiers
`XX-API*.local.md` et ne montre aucun token.

La premiere integration est volontairement defensive:

- `status` et `logs` preparent des lectures via les services allowlistes.
- `--execute` reste bloque par `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`.
- Les annonces, reloads, commandes, timers et connexions creent des drafts
  audites dans `~/.sekailink/core-access/drafts/`.
- Aucune API Discord/Twitch n'est appelee par ces commandes.

## Discord

Commandes:

```text
discord status [--execute]
discord logs [--follow] [--execute]
discord reload --confirm discord:reload:bot
discord announce <channel> <message> --confirm discord:announce:<channel>
discord sync-roles --confirm discord:sync-roles:all
discord command list
discord command enable <name> [detail] --confirm discord:command:<name>:enable
discord command disable <name> [detail] --confirm discord:command:<name>:disable
discord command edit <name> key=value [key=value...] --confirm discord:command:<name>:edit
discord timer list
discord timer edit <id> key=value [key=value...] --confirm discord:timer:<id>:edit
discord incident-post <incident> [channel] [message] --confirm discord:incident-post:<incident>
```

`discord status` pointe vers le service `sekailink-social-bots` sur Link.
`discord logs` pointe vers la source `discord:bot`.

## Twitch

Commandes:

```text
twitch status [--execute]
twitch logs [--follow] [--execute]
twitch connect <channel> [reason] --confirm twitch:connect:<channel>
twitch disconnect <channel> [reason] --confirm twitch:disconnect:<channel>
twitch announce <channel> <message> --confirm twitch:announce:<channel>
twitch command list
twitch command enable <name> [detail] --confirm twitch:command:<name>:enable
twitch command disable <name> [detail] --confirm twitch:command:<name>:disable
twitch command edit <name> key=value [key=value...] --confirm twitch:command:<name>:edit
twitch timer list
twitch timer edit <id> key=value [key=value...] --confirm twitch:timer:<id>:edit
twitch lobby announce <lobby> [message] --confirm twitch:lobby:<lobby>:announce
twitch stream set-title-hint [channel] <title>
```

`twitch status` pointe vers le service `sekailink-twitch-assistant` sur Link.
`twitch logs` pointe vers la source `twitch:assistant`.

## Drafts

Les drafts bot sont append-only et peuvent etre relus avec:

```text
discord command list
discord timer list
twitch command list
twitch timer list
ops timeline discord
ops timeline twitch
```

Chaque draft inclut `id`, `ts`, `state`, `author`, `domain`, `action`,
`target` et `detail`. Pour appliquer plus tard ces drafts vers Nexus ou vers
les bots live, il faudra ajouter une integration serveur documentee avec gate
d'environnement et confirmations exactes.

## Rollback

Toute modification de commande, timer ou mapping doit etre versionnee et
rollbackable.
