# SekaiLink Room Admin TUI

Console locale de diagnostic pour les rooms SekaiLink.

## Lancement

```bash
sekailink-room-admin
```

Alias court:

```bash
skl-room
```

## Configuration

Les secrets ne sont pas stockés dans le repo. Utilise des variables d'environnement:

```bash
export SEKAILINK_API_BASE="https://sekailink.com"
export SEKAILINK_TOKEN="token-client-ou-admin-si-requis"
export SEKAILINK_ROOM_ADMIN_TOKEN="token-room-admin"
export SEKAILINK_ROOM_RUNTIME_TOKEN="token-runtime-optionnel"
export SEKAILINK_ROOM_ADMIN_TOOL_TOKEN="token-outil-admin-pour-secrets-room"
export SEKAILINK_AP_ADMIN_PASSWORD="mot-de-passe-remote-admin-archipelago"
```

Ou lance `config` dans le TUI pour créer:

```text
~/.config/sekailink-room-admin/config.json
```

Si tu n'as pas de token, lance simplement:

```text
login
```

Le TUI utilise le même endpoint que le Client Core (`/api/identity/login`), demande le mot de passe sans l'afficher, puis garde seulement le token de session localement.

## Commandes utiles

- `login [identity]`: ouvre une session Nexus/API.
- `whoami`: teste le token courant.
- `lobbies`: liste les lobbies.
- `select <index|lobby_id>`: sélectionne un lobby et résout sa room.
- `status`: recharge `/api/room_status/<room_id>`.
- `ap-info`: lit le paquet `RoomInfo` Archipelago et l'affiche en tableau.
- `ap-log [limit]`: affiche les derniers paquets vus par la connexion AP persistante.
- `ap-connect [slot game]`: ouvre/reconnecte le moniteur AP persistant.
- `ap-disconnect`: ferme le moniteur AP persistant.
- `secrets`: recharge les secrets admin de la room via `/api/room_admin_secrets/<room_id>`.
- `events [limit]`: affiche les événements room server.
- `watch [seconds]`: surveille les événements en live polling.
- `reports [limit]`: affiche les rapports client.
- `check <location_id>`: injecte `record_check` via le canal admin room server.
- `item <slot> <item_id> <name> [location_id]`: injecte `enqueue_received_item`.
- `players`: liste les noms de slots de la room sélectionnée.
- `items [game]`: liste les items connus depuis le datapackage Archipelago.
- `give <slotname> <item name...>`: valide le nom d'item via le datapackage, suggère les noms proches si besoin, puis envoie `!admin /send <slotname> <item>` au serveur AP. Requiert un mot de passe AP admin; le TUI le récupère automatiquement si `SEKAILINK_ROOM_ADMIN_TOOL_TOKEN` est configuré, sinon il utilise `SEKAILINK_AP_ADMIN_PASSWORD`.
- `raw <json>`: envoie une commande admin brute.
- `ap-check [slot_name game] <location_id...>`: se connecte au serveur AP de la room et envoie `LocationChecks`. Si une room est sélectionnée, le slot et le jeu sont détectés automatiquement.
- `ap-say [slot_name game] <message...>`: envoie un message `Say` au serveur AP. Si une room est sélectionnée, le slot et le jeu sont détectés automatiquement.
- `export`: exporte l'historique local en JSONL dans `~/SekaiLinkReports/room-admin/`.

## Notes

Le TUI masque les tokens dans ses exports. Il ne remplace pas les garde-fous serveur: si un token admin est requis par la room, il doit être fourni localement.

Actuellement, `/api/room_status/<room_id>` expose le port public Archipelago de la room. Les commandes `snapshot`, `summary`, `events`, `reports`, `check` et `item` nécessitent un endpoint admin room server distinct. Si cet endpoint n'est pas exposé, le TUI le dit clairement et propose les commandes AP (`ap-info`, `ap-check`, `ap-say`) ou `status`.

Après `select`, le TUI charge les secrets admin de la room si possible, puis ouvre aussi un moniteur Archipelago persistant sur le slot par défaut. Ce moniteur utilise `items_handling=0` et des tags tracker/admin: il peut lire les paquets serveur et envoyer les commandes admin comme `!admin /send`, mais il ne doit pas consommer les items destinés au vrai client du joueur.

Le prompt supporte Tab completion et historique. Après `select`, Tab complète notamment les noms de joueurs et les items pour `give`.
