# Integration des clients Archipelago

But: "integrer dans le client SekaiLink tout ce qui est supporte par les differents clients Archipelago".

Dans ce repo, les clients Python pertinents incluent:
- `CommonClient.py`
- `BizHawkClient.py`
- `SNIClient.py`
- `ManualClient.py`
- clients par world (varient: Python, exe, mod in-game, etc.)

## 1) CommonClient (etat)
- deja integre headless via `client/runtime/commonclient_wrapper.py`
- IPC JSON stable et deja consomme par Electron (`commonclient:*`)

Actions exposees aujourd'hui:
- connect/disconnect
- ready
- say
- prompts (slot/password)
- output log et `print_json`

Gap:
- pas de support "command palette" pour les commandes client (ex: /sync, /hint, /forfeit)

## 2) BizHawkClient (etat)
- `BizHawkClient.py` est un thin wrapper qui appelle `worlds._bizhawk.context.launch`
- la logique BizHawk (TCP vers Lua) est dans `worlds/_bizhawk/context.py`
- le script Lua connector utilise un protocole JSON (voir `sekailink-client-plan/09-lua-connectors.md`)

Pourquoi ca compte:
- pour beaucoup de jeux BizHawk, le "client" n'est pas seulement CommonClient
- il faut un bridge (Python) qui parle a BizHawk via le Lua connector

Options:
- Option A: CommonClient + launch BizHawk (sans BizHawkClient). Risque: certains worlds attendent BizHawkClient et on manquera des cas.
- Option B (recommande): wrapper headless BizHawkClient.

## 3) SNIClient
- `SNIClient.py` est un client complet avec des commandes (ex: /snes) et un handler `worlds.AutoSNIClient`
- typiquement utilise pour SNES via SNI (websocket) et pour certains connectors

Recommandation:
- creer `client/runtime/sniclient_wrapper.py` (analogue a `commonclient_wrapper.py`)
- wrapper: connect AP + attach SNI + status + logs
- UI: commandes cliquables (connect/disconnect, attach, reset)

## 4) ManualClient
- `ManualClient.py` existe, mais il est minimal

Besoins SekaiLink:
- permettre au joueur de jouer un manual world
- garder le tracking (PopTracker/UT)
- commandes serveur accessibles

## 5) Clients par world (heterogene)
On doit classifier les worlds en familles, parce qu'il n'existe pas "un" client.

Familles frequentes:
- ROM/emulation + connector (BizHawk, RetroArch, Dolphin, etc.)
- mod in-game qui fait client (BepInEx/SMAPI/Fabric)
- exe externe (randomizer launcher) qui accepte args server/slot/pass
- web based (rare)

Indice de depart:
- `CLIENT_AUTOLAUNCH_RAW.json` (heuristiques extraites des guides)

Recommendation engineering:
- formaliser un registry "family" qui map `game_id` vers pipeline
- implementer les familles une a une

## 6) Commandes serveur "facilement accessibles"
Besoin: exposer les commandes AP (cote client) dans l'app, cliquables, avec output visible.

Approche:
- dans le wrapper Python (CommonClient/SNI/BizHawk), ajouter une commande IPC `exec`
- exemple IPC (stdin vers wrapper):

```json
{"cmd":"exec","text":"/sync"}
```

Bonus:
- "command palette" avec suggestions (liste statique au debut, puis auto-gen par introspection du `ClientCommandProcessor`)

## 7) Output serveur dans l'app
Etat:
- la lobby UI affiche deja output serveur via Socket.IO `terminal_output`
- input "terminal_command" est envoye au serveur pour execution (host only)

Ameliorations:
- unifier client logs + server logs dans une vue unique "Session"
- conserver un buffer par room et ecrire un log file local exportable

