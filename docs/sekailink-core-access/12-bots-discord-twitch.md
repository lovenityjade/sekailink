# 12 - Bots Discord et Twitch

Les configs bots vivent dans Nexus et sont auditees. Les secrets sont set/rotate
seulement, jamais affiches en clair.

## Discord

Commandes:

```text
discord status
discord reload
discord announce <channel> <message>
discord sync-roles
discord command list
discord command enable <name>
discord command disable <name>
discord command edit <name>
discord timer list
discord timer edit <id>
discord incident-post <incident>
discord logs
```

## Twitch

Commandes:

```text
twitch status
twitch connect <channel>
twitch disconnect <channel>
twitch announce <channel> <message>
twitch command list
twitch command enable <name>
twitch command disable <name>
twitch command edit <name>
twitch timer list
twitch timer edit <id>
twitch lobby announce <lobby>
twitch stream set-title-hint
twitch logs
```

## Rollback

Toute modification de commande, timer ou mapping doit etre versionnee et
rollbackable.

