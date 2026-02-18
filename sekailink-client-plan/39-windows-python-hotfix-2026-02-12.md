# 39 - Windows Python Hotfix (2026-02-12)

Objectif:
- corriger le crash Launch Windows:
  - `python_missing: no usable Python interpreter found`
- publier la correction via update serveur (sans workflow manuel d'installation locale).

## Bug observe

Sur Windows, `session:autoLaunch` pouvait echouer au demarrage de runtime Python.

Erreur UI:
- `Launch Failed`
- `Error invoking remote method 'session:autoLaunch': Error: python_missing: no usable Python interpreter found`

Cause probable:
- le client pouvait ignorer un Python portable deja present dans le runtime/overlay.
- en cas d'echec bootstrap ponctuel (download/reseau), le cache promise restait en echec jusqu'au redemarrage du client.

## Correctif code

Fichier:
- `client/app/electron/main.cjs`

Changements:
1. Detection Python portable elargie (Windows):
   - recherche prioritaire des executables deja presents dans:
     - `runtime/tools/python/portable-win` (userData)
     - `runtime/overlay/runtime/tools/python/portable-win` (overlay update)
   - fallback avec scan `findPathByBasename(..., "python.exe")`.
2. Bootstrap candidate update:
   - les executables portables detectes sont ajoutes avant les commandes systeme (`py`, `python`).
3. Retry apres echec:
   - `ensurePythonRuntime()` reset `_pythonRuntimePromise` en cas d'erreur pour permettre une nouvelle tentative (sans redemarrage obligatoire).

## Build publie

Version:
- `0.0.2-beta.0.3-topaz`

Artefacts:
- `SekaiLink-client-0.0.2-beta.0.3-topaz.AppImage`
- `SekaiLink-client-0.0.2-beta.0.3-topaz.exe`
- `SekaiLink-client-0.0.2-beta.0.3-topaz.exe.blockmap`

SHA256:
- AppImage: `58ba2094e84aca9fc70def7593cad76b500cedfd0676c45d27893c542155d0c5`
- EXE: `b26b2837efe8d5f48b2eaa979d9b3b0897a16607401cb13243a9e829202fbb70`
- EXE blockmap: `911c2bb1570106cefd4c321fb4604192441000eb793157dc43ed1940596fb4de`

## Publication VPS

Cible:
- `/opt/multiworldgg/WebHostLib/static/downloads/`

Fichiers uploades:
- `SekaiLink-client-0.0.2-beta.0.3-topaz.AppImage`
- `SekaiLink-client-0.0.2-beta.0.3-topaz.exe`
- `SekaiLink-client-0.0.2-beta.0.3-topaz.exe.blockmap`

Config mise a jour:
- `/opt/multiworldgg/config.yaml`
  - `CLIENT_LATEST_VERSION: "0.0.2-beta.0.3-topaz"`
  - URLs download Linux/Windows vers `...0.0.2-beta.0.3-topaz...`
  - SHA256 Linux/Windows alignes sur les artefacts publies
  - notes release: fix Python bootstrap Windows

Services:
- `systemctl restart multiworldgg-webhost multiworldgg-workers`
- statut final: `active` / `active`

## Verification endpoint

Valide:
- `GET https://sekailink.com/api/client/version`
- `GET https://sekailink.com/api/client/version?platform=windows`
- `GET https://sekailink.com/api/client/version?platform=linux`

Chaque endpoint retourne:
- `latest: "0.0.2-beta.0.3-topaz"`
- URL de download correcte par plateforme
- `sha256` coherent avec les checksums publies

