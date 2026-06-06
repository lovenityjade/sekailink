# 08 - Signaux clients

Les signaux clients sont realtime. Le fallback poll peut exister pour robustesse,
mais le cockpit doit viser l'immediat.

## Signaux

```text
client broadcast
client maintenance-banner
client force-update-prompt
client request-relogin
client refresh-lobby
client refresh-room
client request-sklmi-reconnect
client restart-runtime
client clear-cache-request
client diagnostics-request
```

## Diagnostics

Regles:

- consentement utilisateur obligatoire;
- logs et configs redacted;
- upload lie a un incident ou ops commit;
- jamais de secret client brut.

## Broadcast

Ciblage:

- global;
- server;
- lobby;
- room;
- role;
- version;
- game.

Exemple:

```text
broadcast lobby pas-pour-certo "Maintenance de room dans 5 minutes."
```

