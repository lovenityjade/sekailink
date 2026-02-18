# Server APIs, downloads, logs

But: le client desktop doit pouvoir:
- consommer les APIs SekaiLink pour lobbies/rooms
- telecharger les patches/slot files
- afficher les logs serveur (room)
- exposer les commandes serveur de facon accessible

Important:
- le code serveur dans ce repo n'est pas garanti a jour par rapport au VPS.
- on traite ces endpoints comme un "contrat" a verifier contre le serveur reel.
- action proposee: ecrire des "contract checks" cote client (schema + cas reels) au lieu de dependre des details d'implementation.

## APIs utilisees par l'UI desktop (actuel)
Lobby endpoints:
- `GET /api/lobbies`
- `GET /api/lobbies/<id>/messages`
- `POST /api/lobbies/<id>/messages`
- `GET /api/lobbies/<id>/members`
- `POST /api/lobbies/<id>/active-yaml`
- `POST /api/lobbies/<id>/ready`
- `POST /api/lobbies/<id>/generate`

Room status endpoint:
- `GET /api/room_status/<room_id>` (downloads + last_port + tracker)

Code:
- client: `client/app/src/pages/Lobby.tsx`
- serveur: `WebHostLib/lobbies.py`, `WebHostLib/api/room.py`

## Downloads: patch vs slot file
`WebHostLib/api/room.py` renvoie une liste `downloads` pour tous les slots.
- `download_patch` quand `supports_apdeltapatch(slot.game)`
- `download_slot_file` sinon

Implication: le client doit detecter et router les downloads.

## Auth et downloads (important pour le desktop)
Le renderer utilise `apiFetch()` (voir `client/app/src/services/api.ts`) qui ajoute:
- `Authorization: Bearer <desktopToken>` si disponible
- un fallback `?token=...` uniquement en mode `file:` (packaged) et seulement pour les requetes faites via `apiFetch`

Probleme actuel:
- `Lobby.tsx` passe un `patchUrl` construit par `apiUrl()` (pas de token)
- Electron main telecharge via `downloadToFile()` (pas de cookies, pas d'Authorization)

Si les endpoints de download exigent une auth, ce flow va echouer.

Recommandations:
- Option A (simple): faire en sorte que les URLs de download incluent `?token=` en mode desktop (au moment de construire `downloadsBySlot`)
- Option B (propre): ajouter un IPC `downloads:download` qui prend un path et un token et telecharge avec headers
- Option C: rendre les downloads publics apres generation (si acceptable securite)

## Recommandation serveur (si on change l'API)
- exposer explicitement `download_type: patch|slot_file`
- exposer `filename` et `content_type` (ou au moins extension)

Sinon (sans changer serveur):
- faire un HEAD/GET et inferer par URL/headers

## Logs serveur (room terminal)
Le webhost envoie deja output via Socket.IO.
- client recoit `terminal_output` et append dans `terminalLog`
- input host-only via event `terminal_command`

Objectif client:
- "output des serveurs quelque part dans l'application"

Etat:
- deja present dans `Lobby.tsx` (Room Terminal drawer)

Ameliorations:
- conserver un buffer ring par room
- offrir export log
- afficher statut connection socket (deja partiel via `socketStatusMsg`)

## Logs client (AP clients)
Le CommonClient wrapper emet:
- `event: log`
- `event: print_json`

A faire:
- afficher ces logs dans une vue UI (ex: "Client Console")
- correlation ID: associer logs a une session (room_id + slot)

## Commandes serveur et commandes client
Deux types:
- commandes "serveur" (dans le terminal room, host-only)
- commandes "client AP" (envoyees via CommonClient, ex: /sync)

Besoin:
- rendre ca cliquable

Proposition UI:
- "Commands" panel avec boutons de macros (Sync, Hint, Remaining, Release, etc)
- "Commands" panel avec command palette

Proposition runtime:
- `CommonClient wrapper` expose un `exec` generique (interprete comme input texte)
- mais: le wrapper actuel n'execute pas les commandes `/...` du ClientCommandProcessor

Options:
- Option A: implementer une commande IPC `{"cmd":"raw","text":"/sync"}` qui appelle un parser de commandes cote Python
- Option B: exposer des commandes "first class" cote IPC (plus verbeux mais stable)
