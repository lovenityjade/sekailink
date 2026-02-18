# Lua connectors (documenter les connecteurs)

Dans Archipelago/MWGG, "Lua connector" veut souvent dire:
- un script Lua charge dans l'emulateur
- qui expose une API (souvent TCP localhost)
- que le client Python utilise pour lire/ecrire memoire, hash ROM, etc.

## BizHawk generic connector (ce repo)

### Script bundle SekaiLink (runtime)
- chemin: `client/runtime/modules/bizhawk_base/lua/connector.lua`
- version: `SCRIPT_VERSION = 1`
- protocole: JSON over TCP
- ports: essaie `localhost:43055` jusqu'a `43060` (range size 5)
- comportement: serveur TCP + messages newline delimited; supporte `VERSION` (string) et sinon JSON array -> JSON array

Request types (extraits):
- `PING` -> `PONG`
- `SYSTEM` -> `SYSTEM_RESPONSE`
- `HASH` -> `HASH_RESPONSE`
- `READ` / `WRITE` -> base64 payload
- `LOCK` / `UNLOCK`
- `DISPLAY_MESSAGE`

Compatibilite:
- `worlds/_bizhawk/context.py` exige `EXPECTED_SCRIPT_VERSION = 1`

### Script upstream (AP data/lua)
- BizHawkClient Python pointe par defaut sur `data/lua/connector_bizhawk_generic.lua`

Dans SekaiLink, on a choisi une copie vendoree dans le runtime Electron.
Risque: divergence.

Recommandation: rendre la source unique.
- Option A: utiliser `data/lua/connector_bizhawk_generic.lua` comme source et la copier dans le runtime build
- Option B: faire pointer BizHawkClient vers le runtime connector (moins propre cote AP)

## Connecteurs BizHawk specifiques a certains worlds
`.lua` dans `worlds/` (en dehors du runtime SekaiLink):
- `worlds/banjo_tooie/assets/connector_banjo_tooie_bizhawk.lua`
- `worlds/star_fox_64/assets/connector_sf64_bizhawk.lua`

Note:
- les docs SekaiLink listent aussi des connecteurs dans `data/lua/` (cote AP): `sekailink-docs/BIZHAWK-CONNECTORS.md`

## Connecteurs "Lua" qui ne sont pas emulateur
Certains worlds (ex: Factorio) utilisent Lua pour des mods in-game.
- `worlds/factorio/data/mod_template/*.lua`
- `worlds/factorio_saws/data/mod_template/*.lua`

Implication runtime contract:
- `connector.type = bizhawk_lua` pour l'emulation BizHawk
- `connector.type = mod_lua` (ou `native_mod`) pour les jeux moddes (ex: Factorio)

## Documentation a produire (livrable)
Pour chaque connecteur family:
- ou il vit (path)
- comment il est charge (CLI emulateur, auto-load, mod install)
- protocole (ports, version)
- erreurs frequentes

