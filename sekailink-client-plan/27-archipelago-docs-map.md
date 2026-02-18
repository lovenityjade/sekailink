# Reference: `docs/` (Archipelago and WebHost documentation)

But: pointer vers les docs "upstream" presentes dans ce repo (`docs/`) qui servent de reference pour comprendre Archipelago, les worlds, le protocole reseau, et le WebHost.

Positionnement:
- `docs/` est la reference technique Archipelago.
- `sekailink-client-plan/` est la synthese SekaiLink (workflow, contrats, decisions).

## Docs clefs pour SekaiLink Desktop

AP network protocol:
- `docs/network protocol.md`
- utile pour comprendre les messages, ids, et limites eventuelles

World API (dev worlds):
- `docs/world api.md`
- utile pour comprendre comment un world declare:
- generation
- options
- webhost integration

APWorld specification:
- `docs/apworld specification.md`
- utile pour comprendre packaging `.apworld` (installation, versioning)

Settings API:
- `docs/settings api.md`
- important car le patcher SekaiLink applique aujourd'hui des chemins ROM via `settings.get_settings()`

Options API:
- `docs/options api.md`
- utile pour comprendre les options, validation, et comment introspecter les worlds

WebHost API:
- `docs/webhost api.md`
- decrit les endpoints server utilises par SekaiLink (generation, room_status, tracker)
- important car le code serveur ici n'est pas forcement identique au VPS, donc ce doc est un contrat de reference

Tests:
- `docs/tests.md`
- utile pour planifier des contract checks et des tests de non-regression (voir `sekailink-client-plan/25-contract-checks-and-testing.md`)

Shared cache:
- `docs/shared_cache.md`
- utile pour comprendre comment Archipelago met en cache des ressources (impact sur packaging et installs)

## Docs utiles mais secondaires

Entrances:
- `docs/entrance randomization.md`

Running from source:
- `docs/running from source.md`

Deploy:
- `docs/deploy using containers.md`

Adding games:
- `docs/adding games.md`

Style/contributing:
- `docs/style.md`
- `docs/contributing.md`
- `docs/apworld_dev_faq.md`

## Consequence pour SekaiLink

Quand SekaiLink doit "deviner" un comportement:
- verifier `docs/` avant de reverse engineer le code

Quand SekaiLink doit "documenter une decision produit":
- resumer dans `sekailink-client-plan/` avec un lien vers le doc `docs/` pertinent

