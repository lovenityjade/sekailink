# Runtime installers, updates, et licences

But: decrire une strategie d'installation/update des dependances (emulateurs, mod loaders, clients externes, seed files, trackers) compatible Linux + Windows, et compatible avec les contraintes de licences.

Contrainte produit:
- "0 interaction" veut dire "pas de recherches manuelles de zips", mais pas "violer des licences".
- SekaiLink ne distribue pas de ROMs/BIOS/keys/firmware.

## Principe: ne pas compiler les repos third_party en production

Dans ce repo, `third_party/emulators/*` et `third_party/mod_loaders/*` sont surtout des sources.

Regle:
- pour l'utilisateur final, installer depuis des releases officielles (AppImage, zip, msi, tar.gz, Flatpak quand pertinent)
- stocker dans un cache SekaiLink versionne

Exception:
- des binaries vendores peuvent exister pour du MVP (ex: `third_party/emulators/BizHawk-2.10-linux-x64/`), mais ne doivent pas etre le seul plan.

## Cache SekaiLink (proposition)

Racine:
- Linux: `~/.sekailink/runtime/`
- Windows: `%LOCALAPPDATA%\\SekaiLink\\runtime\\` (ou `userData/runtime/`)

Structure:
- `runtime/emulators/<emu_id>/<version>/...`
- `runtime/modloaders/<loader_id>/<version>/...`
- `runtime/tools/<tool_id>/<version>/...`
- `runtime/trackers/poptracker/<version>/...` (si on decide de ne plus vendor)

Objectif:
- multi-versions (BizHawk, Dolphin, etc.)
- rollback simple (ne pas ecraser une version en place)
- verification checksum

## Sources et verification

Sources permises:
- GitHub releases officielles
- sites officiels (ex: dolphin-emu.org)
- Flathub (Flatpak) si l'utilisateur l'a deja, ou si on veut un mode "system-managed"

Verification:
- preferer SHA256 fournie par upstream si disponible
- sinon, version pinnee + download URL stable
- logging: enregistrer `source_url`, `download_time`, `hash`, `size`

## Licences et redistribution (rappel)

Exemples concrets observes dans ce repo:
- DuckStation: le README precise que redistribuer des releases non modifiees est permis, mais que des packages pre-configures sont des modifications.

Regles SekaiLink:
- preferer installer "a la demande" plutot que bundler des archives modifiees
- ne pas modifier les artefacts upstream; stocker la config dans un user dir/profil separe
- documenter les licences (GPL, LGPL, Apache) avant de vendorer des bins

## Policy d'update

Modes:
- `stable`: utiliser `releases/latest` (avec checks)
- `pinned`: version specifique par module (necessaire si contraintes)
- `manual`: l'utilisateur choisit une version (power user)

Multi-version:
- support obligatoire pour BizHawk (ranges par world)
- utile pour d'autres runtimes (compat regressions)

Update atomique:
- telecharger dans un dossier temporaire
- verifier hash
- decomprimer
- switcher un symlink ou un pointeur "active"

## Integration avec les manifests modules

Le module manifest doit declarer:
- runtime requis (id + constraints)
- policy (prefer bundled, prefer installed, allowed fallbacks)

Exemple (idee):
- `driver.family = bizhawk`
- `driver.constraints = {min: "2.7.0", max: "2.9.1"}`
- `driver.sources = [{type: "github_release", repo: "..."}]`

## Linux specifics (Bazzite, Wayland, gamescope)

Objectif:
- ne pas dependre d'un packaging systeme particulier
- supporter Wayland et X11

Notes:
- gamescope peut etre absent sur des desktops; fallback sans casser
- Flatpak runtimes peuvent etre sandboxes (paths BIOS/ROM a gerer via portals)
- si un emulator est mieux en Flatpak (ex Dolphin sur Flathub), SekaiLink doit pouvoir:
- detecter `flatpak run ...` comme runtime
- ou installer une release non-flatpak

## Windows specifics

Objectif:
- pas d'admin requis
- pas d'installation system-wide obligatoire

Notes:
- preferer zip portable quand possible
- sinon, un installer silencieux est un plus mais non requis
- kill tree et process management sont plus difficiles (a documenter)

## Trackers packs (PopTracker)

Etat actuel:
- SekaiLink installe des packs via GitHub releases et les garde dans `userData/poptracker/packs/<gameId>`
- source et timestamp sont notes dans `~/.sekailink/config.json`

Exigence:
- chaque jeu declare son "repo canonique" (source de verite)
- l'utilisateur ne doit pas chercher des zips

Voir:
- `sekailink-client-plan/08-trackers.md`

