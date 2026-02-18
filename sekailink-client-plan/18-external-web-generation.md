# Worlds avec generation externe (sites web, patchers, local-only)

But: documenter les worlds qui ne rentrent pas dans le flow "WebHost genere une seed, le client telecharge une patch `.ap*`, lance le jeu", et definir une strategie SekaiLink pour minimiser les inputs utilisateur.

Ce doc est volontairement pragmatique:
- on ne re-implemente pas les randomizers
- on automatise seulement ce qui est stable/legal
- on garde un fallback guide (et documente) pour le reste

## Definitions (artefacts)

Artefacts typiques qu'un player/host manipule:
- `player.yaml`: config AP par joueur.
- `patch.ap*`: patch Archipelago a appliquer localement (format depend du world).
- `patched_rom`: ROM/ISO/jeu modifie qui correspond a `player.yaml` (souvent un "pair" inseparable).
- `seed_file`: fichier de seed/data a placer dans un dossier specifique (ex: save data Dolphin).
- `external_assets`: loader, mod, plugin, scripts (Lua/JS), etc.

## Taxonomie (types de fournisseurs de generation)

SekaiLink doit traiter la "generation" comme une etape pluggable.

Types utilises dans ce doc:
- `AP_INTERNAL`: generation standard (WebHost/Generate produit patchs), le client telecharge et lance.
- `LOCAL_ONLY`: ne peut pas etre genere via un webhost public; generation doit tourner "localement". Un serveur SekaiLink peut quand meme etre ce "local", mais ca impose des contraintes.
- `EXTERNAL_WEB_GENERATOR_PER_PLAYER`: chaque joueur doit aller sur un site externe qui genere un `player.yaml` et un `patched_rom` apparies.
- `EXTERNAL_PATCH_APPLY`: le webhost genere une patch AP, mais l'application de la patch (creation du ROM) se fait via un outil/site externe.
- `EXTERNAL_RESOURCE_FETCH`: generation AP ok, mais l'execution exige un ou plusieurs fichiers externes a telecharger et placer.

## Strategie UX SekaiLink (objectif: minimum d'inputs)

Principes:
- Ne jamais demander a l'utilisateur de comprendre le tooling. On traduit en "etapes" et on valide les prerequis.
- Ne jamais automatiser un upload de ROM vers un tiers. Si un site externe demande un ROM, l'action doit etre explicite et locale.
- Ne jamais stocker des ROMs cote serveur. Seuls des metadata et artefacts non sensibles (yaml/patch) peuvent transiter.

## UX proposee (comportement du client)

General:
- Ecran "Play": un statut par prerequis: `OK`, `Missing`, `Manual Step`.
- Bouton `Fix` pour chaque prerequis (download assets, ouvrir dossier, ouvrir URL, importer fichier).

Worlds `EXTERNAL_WEB_GENERATOR_PER_PLAYER`:
- bouton "Open Generator Website"
- bouton "Import YAML"
- bouton "Select Patched ROM"
- verifications best-effort que `yaml` et `rom` appartiennent a la meme seed (sinon warning fort)

Worlds `EXTERNAL_PATCH_APPLY`:
- bouton "Download AP patch" (si SekaiLink fournit un `.ap*`)
- bouton "Apply patch" avec 2 chemins
- chemin 1: patcher local si un patcher CLI officiel existe et est redistribuable
- chemin 2: open website (fallback)

Worlds `EXTERNAL_RESOURCE_FETCH`:
- telechargement automatique si source stable (releases publiques), sinon import manuel guide

## Catalog (worlds confirmes dans ce repo)

### Chrono Trigger Jets of Time (CTJoT)

Type: `EXTERNAL_WEB_GENERATOR_PER_PLAYER`

Sources:
- docs: `worlds/ctjot/docs/multiworld_en.md`, `worlds/ctjot/docs/en_Chrono Trigger Jets of Time.md`
- code: `worlds/ctjot/__init__.py` (validation `seed_share_link`), `worlds/ctjot/Options.py` (option `seed_share_link`)

Flow reel:
- Chaque joueur utilise `https://multiworld.ctjot.com/options`.
- Le site genere un duo `player.yaml` + ROM patchee (`.sfc`).
- Le duo est inseparable: chaque multiworld exige un nouveau duo.
- Le host recolte les `player.yaml` et genere la multiworld AP.

Implications SekaiLink:
- SekaiLink orchestre l'import YAML et la selection du ROM patche; il ne peut pas "auto-patcher" sans devenir le randomizer.
- SekaiLink peut valider au minimum:
- que `seed_share_link` contient `multiworld.ctjot.com`
- que le slot name correspond

### Final Fantasy 1 (NES)

Type: `EXTERNAL_WEB_GENERATOR_PER_PLAYER`

Sources:
- docs: `worlds/ff1/docs/multiworld_en.md`
- code: `worlds/ff1/__init__.py` (options_page et assertions), `worlds/ff1/Options.py`

Flow reel:
- Chaque joueur utilise `https://finalfantasyrandomizer.com/`.
- Active le flag `Archipelago`, donne un player name, upload sa ROM, clique `GENERATE ROM`.
- Le site telecharge deux fichiers: ROM randomisee + `player.yaml`.

Implications SekaiLink:
- Meme pattern que CTJoT: SekaiLink doit eviter les erreurs de pair `yaml/rom` (seed mismatch).

### Final Fantasy Mystic Quest (SNES)

Type: `EXTERNAL_PATCH_APPLY`

Sources:
- docs: `worlds/ffmq/docs/setup_en.md`
- code: `worlds/ffmq/__init__.py` (mention de `https://api.ffmqrando.net/` a investiguer)

Flow reel:
- Le webhost AP genere une patch `.apmq`.
- Chaque joueur utilise `https://ap.ffmqrando.net/Archipelago` avec ROM vanilla + patch `.apmq`, puis recupere la ROM patchee.

Implications SekaiLink:
- Minimum viable: telecharger `.apmq`, ouvrir le site, guider l'utilisateur.
- Option future: patcher local si un outil officiel existe (ou via API) pour supprimer l'etape website.

### Twilight Princess (Dolphin)

Type: `EXTERNAL_RESOURCE_FETCH` + `LOCAL_ONLY` (contexte MultiworldGG)

Source:
- docs: `worlds/tp/docs/setup_en.md`

Points speciaux observes:
- MultiworldGG dit que l'online generator ne supporte pas TP, donc generation "locale" cote MWGG.
- Execution exige des ressources externes:
- REL loader: `https://tprandomizer.com/`
- seed file custom: `https://generator.tprandomizer.com/s/aptest`
- placement dans le save data Dolphin avec `RandomizerAP.US.gci` fourni par le zip

Implications SekaiLink:
- Cote server SekaiLink: "local-only" peut etre acceptable si notre serveur sait generer ce world.
- Cote client: SekaiLink doit offrir un flow "Installer prerequisites TP" (download + placement dans le user dir Dolphin).

### SM64 Romhack

Type: `EXTERNAL_WEB_GENERATOR_PER_PLAYER` (json) + out-of-band tooling

Source:
- docs: `worlds/sm64hacks/docs/setup_en.md`

Flow reel:
- Generation d'un `.json` via `http://dnvic.com/ArchipelagoGenerator/index.html` a partir d'un `.jsml`.
- Setup depend de tooling externe (Project64, scripts JS, StarDisplay).

Implications SekaiLink:
- A traiter comme un world "hautement manuel" au depart.
- SekaiLink peut seulement structurer les imports (json) et clarifier les prerequis.

### Paper Mario

Type: `LOCAL_ONLY` (generation) + patch distribution special-case

Source:
- docs: `worlds/papermario/docs/setup_en.md`

Points speciaux observes:
- "You will not be able to generate games with Paper Mario on the Archipelago site, only locally."
- Patch `.appm64` ne s'upload pas sur MultiworldGG dans ce contexte, donc echange out-of-band.
- Contrainte BizHawk: `2.7` a `2.9.1`, `2.10` release ne marche pas.

Implications SekaiLink:
- Un serveur SekaiLink peut satisfaire "local-only generation", mais il faut verifier l'integration reelle.
- Le client SekaiLink doit gerer la contrainte BizHawk par world (voir `sekailink-client-plan/17-third-party-emulators.md`).

## Impacts sur le runtime contract SekaiLink

Le manifest par module/jeu (voir `sekailink-client-plan/04-runtime-contract.md`) doit pouvoir exprimer:
- `generation.provider_type`: un des types de ce doc.
- `generation.external_urls[]`: liens a ouvrir (generator/patcher/docs).
- `generation.required_artifacts[]`: ex `yaml`, `patched_rom`, `seed_file`.
- `generation.validation`: regles simples (domain match, extension, presence).
- `legal.requires_user_rom`: bool.

## Open questions (a investiguer)

- Pour chaque site externe, existe-t-il un repo GitHub, une API, ou un patcher CLI redistribuable?
- Peut-on verifier automatiquement "yaml/rom pair" sans reverse engineer?
- Quelle part de ces steps peut etre guidee en "in-app browser" vs "open system browser" (Wayland/Steam Deck)?
