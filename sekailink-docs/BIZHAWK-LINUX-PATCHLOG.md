# BizHawk Linux Patchlog (MVP)

Objectif: tracer les correctifs appliques pendant le debug Linux BizHawk, et identifier ce qui est deja global vs ce qui reste a propager aux autres worlds BizHawk.

## Probleme racine observes

- Erreur EROFS sur `config.ini` dans un runtime AppImage monte en lecture seule.
- BizHawk qui ne charge pas certains modules Lua (`lua_5_3_compat`, `base64`, `json`, `socket`).
- Echec du module natif LuaSocket (`socket-linux-5-4.so` introuvable).
- Logs wrapper non JSON qui cassaient le flux de status.
- Crashs Python cote client AP avec websocket moderne sans attribut `.open`/`.closed`.
- Boucle de game watcher qui mourait apres exception, laissant le client dans un etat instable.

## Patches appliques

## 1) Runtime BizHawk en dossier writable (Linux/AppImage)

- **Fichier:** `client/app/electron/main.cjs`
- **Fonctions:** `getBizHawkInstalledDir` (`:1119`), `stageBizHawkToDir` (`:1124`), `ensureBizHawkInstalled` (`:1146`)
- **Effet:** copie BizHawk dans `~/.config/sekailink-client/runtime/tools/bizhawk/BizHawk-2.10-linux-x64`, evite les ecritures dans `/tmp/.mount_*`, et gere `SEKAILINK_BIZHAWK` en lecture seule (staging auto).

## 2) Ecriture config BizHawk dans runtime installe

- **Fichier:** `client/app/electron/main.cjs`
- **Fonction:** `ensureBizHawkConfig` (`:1364`)
- **Effet:** ecrit `config.ini` dans le dossier staged writable, plus dans l’image montee.

## 3) Injection des dependances Lua manquantes

- **Fichier:** `client/app/electron/main.cjs`
- **Fonction:** `ensureBizHawkLuaCompat` (`:1391`)
- **Fichiers assures:** `lua_5_3_compat.lua`, `base64.lua`, `json.lua`, `socket.lua`
- **Effet:** corrige les erreurs `module '...' not found` dans la console Lua BizHawk.

## 4) Copie des `.so` LuaSocket pour Linux

- **Fichier:** `client/app/electron/main.cjs`
- **Fonction:** `ensureBizHawkLuaCompat` (`:1391`)
- **Bibliotheques assurees:** `x64/socket-linux-5-4.so`, `x64/socket-linux-5-1.so`
- **Effet:** copie vers `BizHawkRoot/x64` et `BizHawkRoot/Lua/x64` pour couvrir les resolutions de chemin runtime differents; corrige `socket.lua: ... cannot open shared object file`.

## 5) Staging du connector Lua dans BizHawk runtime

- **Fichier:** `client/app/electron/main.cjs`
- **Fonction:** `stageBizHawkConnectorLua` (`:1464`)
- **Effet:** copie le script connector vers `Lua/sekailink_connector_<module>.lua` dans le runtime BizHawk actif, sans dependre d’un chemin externe fragile.

## 6) Tolerance aux lignes stdout non JSON du wrapper

- **Fichier:** `client/app/electron/main.cjs`
- **Bloc:** parsing stdout du `bizhawkclient_wrapper.py` (`:1965`)
- **Effet:** une ligne non JSON n’arrete plus la pipeline; elle est loggee en warning puis relayee comme evenement texte.

## 7) Compat websocket moderne dans CommonClient

- **Fichier:** `CommonClient.py`
- **Ajouts:** `_websocket_closed_compat` (`:50`), `_websocket_open_compat` (`:66`), `_install_websocket_compat_shim` (`:91`), `websocket_is_closed` (`:119`), `websocket_is_open` (`:127`)
- **Remplacements cibles:** `disconnect` (`:568`), `send_msgs` (`:575`), `shutdown` (`:714`), `update_death_link` (`:851`)
- **Effet:** evite les crashs `AttributeError: 'ClientConnection' object has no attribute 'open'/'closed'` et rend le client tolerant aux differences de version websocket.

## 8) Compat websocket cote watcher BizHawk

- **Fichier:** `worlds/_bizhawk/context.py`
- **Changement:** import et usage de `websocket_is_open` (`:14`, `:267`, `:298`)
- **Effet:** le coeur watcher BizHawk n’utilise plus directement `.socket.closed` a ces points.

## 9) Relance automatique du game watcher sur crash

- **Fichier:** `client/runtime/bizhawkclient_wrapper.py`
- **Fonction:** `_resilient_game_watcher` (`:330`)
- **Effet:** en cas d’exception game watcher, le processus ne meurt plus silencieusement; log explicite + reset handler/rom + tentative de reprise.

## 10) Status wrapper en mode websocket-compat

- **Fichier:** `client/runtime/bizhawkclient_wrapper.py`
- **Changement:** poll status serveur via `websocket_is_open` (`:302`)
- **Effet:** etat UI plus fiable avec stack websocket recente.

## 11) Mono Linux portable prioritaire

- **Fichier:** `client/app/electron/main.cjs`
- **Fonction:** `ensureMonoRuntime` (`:418`)
- **Effet:** priorite a `SEKAILINK_MONO`, puis mono bundle dans `third_party/mono`, puis mono systeme; evite la dependance obligatoire a une installation systeme manuelle.

## Applicabilite aux autres worlds BizHawk

Ce qui est deja global (benefice immediat pour tous les worlds BizHawk):

- Staging BizHawk writable.
- Patch config writable.
- Injection Lua + `.so` Linux.
- Staging du connector lua.
- Parse stdout non-JSON robuste.
- Helpers websocket dans `CommonClient`.
- Compat dans `worlds/_bizhawk/context.py` (niveau coeur BizHawk).

Ce qui reste a migrer (direct `.socket.open/.closed` encore presents dans des clients de world):

- `worlds/pokemon_rb/client.py:119`
- `worlds/pokemon_rb/client.py:304`
- `worlds/pokemon_crystal/client.py:316`
- `worlds/pokemon_crystal/client.py:845`
- `worlds/marioland2/client.py:226`
- `worlds/tmc/client.py:159`
- `worlds/mzm/client.py:535`
- `worlds/pokemon_emerald/client.py:202`
- `worlds/pokemon_frlg/client.py:343`
- `worlds/star_fox_64/client.py:419`
- `worlds/trackmania/client.py:70`
- `worlds/trackmania/client.py:83`
- `worlds/trackmania/client.py:89`
- `worlds/trackmania/client.py:92`
- `worlds/sotn/client.py:114`
- `worlds/sm64hacks/client.py:251`
- `worlds/cvcotm/client.py:181`

## Regle de migration pour ces worlds

- Remplacer les checks directs `ctx.server.socket.closed` et `ctx.server.socket.open`.
- Utiliser `websocket_is_open(ctx.server.socket)` et `websocket_is_closed(ctx.server.socket)`.
- Importer les helpers depuis `CommonClient`.
- Retester chaque world avec un smoke local de connexion + watcher.

## Outils de verification deja ajoutes

- `client/runtime/tests/headless_ap_smoke.py`
- `scripts/headless-smoke.sh`
- `client/runtime/tests/README.md`
- `client/app/package.json` script `headless:smoke`

## Notes connues (non bloquantes)

- Warning `_speedups not available` possible dans certains environnements Python.
- Message `Game name for validate rom ...` peut apparaitre en warning selon ROM/metadata, sans etre la cause principale des crashs websocket.
