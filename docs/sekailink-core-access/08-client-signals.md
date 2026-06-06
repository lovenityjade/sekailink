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
client diagnostics-list
client diagnostics-export
```

## Diagnostics

Regles:

- consentement utilisateur obligatoire;
- logs et configs redacted;
- upload lie a un incident ou ops commit;
- jamais de secret client brut.

Etat Core Access actuel:

- `client diagnostics-request` cree une demande locale `draft-consent-required`;
- `client broadcast`, `client maintenance-banner`, `client force-update-prompt`,
  `client request-relogin`, `client refresh-lobby`, `client refresh-room`,
  `client request-sklmi-reconnect`, `client restart-runtime` et
  `client clear-cache-request` creent des drafts locaux audites dans
  `drafts/client-signal.jsonl`;
- les signaux sensibles exigent `--confirm client:<action>:<target>`;
- `client refresh-lobby` et `client refresh-room` sont des drafts sans
  confirmation forte, car ils ne sont pas destructifs;
- `client clear-cache-request` reste marque approval-required dans le detail du
  draft;
- aucun signal client n'est envoye par Core Access actuellement;
- aucun fichier Client Core, Sekaiemu ou SKLMI n'est collecte par Core Access;
- `client diagnostics-list` liste les demandes locales;
- `client diagnostics-export` exporte les demandes et le contrat de bundle
  attendu.

Bundle attendu quand l'integration client sera implementee:

- `client-core/main.log`, `client-core/renderer.log`, `client-core/updater.log`;
- `sekaiemu/sekaiemu.log`, `sekaiemu/stdout.log`, `sekaiemu/stderr.log`;
- `sklmi/sklmi.log`, `sklmi/stdout.log`, `sklmi/stderr.log`;
- configs redacted et contexte de lancement;
- informations systeme sans secrets.

Toute modification SKLMI necessaire pour produire ces fichiers demande accord
explicite ecrit de Jade avant implementation.

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
broadcast lobby pas-pour-certo "Maintenance de room dans 5 minutes." --confirm broadcast:lobby:pas-pour-certo
```

Etat actuel: les commandes `broadcast ...` creent des drafts locaux audites dans
`drafts/broadcast.jsonl`. Aucun message n'est envoye aux clients, rooms,
lobbies, Discord ou Twitch tant que le transport serveur n'est pas connecte.
