# Autolaunch source data (CLIENT_AUTOLAUNCH_RAW.json)

But: expliquer ce fichier et comment l'utiliser sans tomber dans des mauvaises assumptions.

## Qu'est-ce que c'est
- `CLIENT_AUTOLAUNCH_RAW.json` est un dict par jeu (188 entrees actuellement)
- il est derive de textes dans les setup guides (MWGG style)
- champs typiques (heuristiques): `files`, `emulators`, `mod_loaders`, `patch_exts`, `connect_notes`, `install_notes`, `downloads`

Important:
- `patch_exts` n'est pas fiable comme "liste definitive d'extensions patch"
- exemple: beaucoup de jeux mentionnent `apworld` (fichier world), pas un patch joueur
- parfois, c'est un type de release (`AppImage`), pas un patch

## Comment l'utiliser correctement
- le fichier est un "signal" pour classer les jeux en familles
- il ne doit pas etre utilise directement comme registry runtime

Approche recommandee:
1. Construire un registry primaire a partir du code (worlds + patch register)
2. Utiliser `CLIENT_AUTOLAUNCH_RAW.json` comme couche d'indices (family + notes UX)
3. Maintenir une liste d'overrides manuelle pour les cas tricky (external patchers, multi-asset, etc.)

Exemples de sources plus fiables que le texte des guides:
- `worlds.Files.AutoPatchRegister.patch_types`
- manifests runtime SekaiLink: `client/runtime/modules/*/manifest.json`

## Stats utiles (observations)
- dans ce repo, `patch_exts` contient souvent `apworld`
- il contient aussi des tokens type `apred`, `apblue`, `apcrystal`, etc.

Ces tokens doivent etre normalises (ex: `.apred`) et verifies cote serveur.

## Livrable suggere
- creer un script de build qui genere `client/registry/runtime.generated.json`
- combiner: manifests `client/runtime/modules/*/manifest.json`, mapping manuel, hints optionnels de `CLIENT_AUTOLAUNCH_RAW.json`

