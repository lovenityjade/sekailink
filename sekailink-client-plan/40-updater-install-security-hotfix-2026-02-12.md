# 40 - Updater Install Security Hotfix (2026-02-12)

Objectif:
- corriger l'echec "Install" de l'updater sous Linux (erreur de securite lors du lancement).
- verifier le chemin d'installation Windows en parallele.

## Symptom

Sous Linux, clic sur `Install` apres telechargement update:
- erreur de type securite / open path refuse.

Contexte:
- le client utilisait seulement `shell.openPath(...)` pour ouvrir le binaire telecharge.
- ce mode depend de la politique desktop/portal et peut refuser les executables telecharges.

## Correctif

Fichier:
- `client/app/electron/main.cjs`

Changements:
1. `updater:openDownloaded`:
   - Linux `.AppImage`:
     - force `chmod +x` (bit executable) si necessaire.
     - lance directement le fichier via `spawn(..., detached=true)`.
   - Windows `.exe`:
     - lance directement via `spawn(..., detached=true)`.
2. Fallback:
   - si le `spawn` echoue, fallback sur `shell.openPath(...)` (comportement precedent).

Impact:
- contourne les blocages "security/open" des handlers desktop Linux.
- conserve un fallback compatible.

## Build + release

Version:
- `0.0.2-beta.0.4-topaz`

Artefacts:
- `SekaiLink-client-0.0.2-beta.0.4-topaz.AppImage`
- `SekaiLink-client-0.0.2-beta.0.4-topaz.exe`
- `SekaiLink-client-0.0.2-beta.0.4-topaz.exe.blockmap`

SHA256:
- AppImage: `102124ecffeab86d77fe6753023ab849a376beac7c4e0f4749d441170cbd50f5`
- EXE: `14822b121e100a6fbe14eaa749450fbb2afa00e7f1b66c1f5bd0950350a95d5d`
- EXE blockmap: `44b8cb00ec03406fb150319adbbe323f6d3935c34fa4cbd626e216bba3c892a7`

## Publication VPS

Upload:
- `/opt/multiworldgg/WebHostLib/static/downloads/`

Config:
- `/opt/multiworldgg/config.yaml`
  - `CLIENT_LATEST_VERSION: "0.0.2-beta.0.4-topaz"`
  - URLs Linux/Windows vers artefacts `0.0.2-beta.0.4-topaz`
  - SHA256 Linux/Windows mis a jour
  - notes release mises a jour

Services:
- `systemctl restart multiworldgg-webhost multiworldgg-workers`
- etat final: `active` / `active`

## Verification API

Valide:
- `GET https://sekailink.com/api/client/version`
- `GET https://sekailink.com/api/client/version?platform=windows`
- `GET https://sekailink.com/api/client/version?platform=linux`

Resultat:
- `latest: 0.0.2-beta.0.4-topaz`
- URLs correctes par plateforme
- SHA256 alignes avec les artefacts publies

