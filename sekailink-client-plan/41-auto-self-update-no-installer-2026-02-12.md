# 41 - Auto Self-Update (No Installer Prompt) (2026-02-12)

Objectif:
- supprimer la dependance au lancement manuel d'un installateur pour les updates client.
- reduire les interactions utilisateur (Windows + Linux).
- lancer la verification update au demarrage et appliquer automatiquement si une version plus recente existe.

## Changements client

Fichiers:
- `client/app/electron/main.cjs`
- `client/app/electron/preload.cjs`
- `client/app/src/components/AuthGate.tsx`
- `client/app/src/vite-env.d.ts`
- `client/app/src/services/runtime.ts`
- `client/app/CLIENT-NOTES.md`

Comportement:
1. Nouveau flux `updater:downloadAndApply`:
   - telecharge l'artefact update avec progression.
   - verifie SHA-256 si fourni.
   - applique la mise a jour automatiquement.
2. Application update sans installateur:
   - Windows: copie l'EXE telecharge sur le binaire courant puis relance le client.
   - Linux: remplace l'AppImage courante puis relance le client.
   - pas de fallback `shell.openPath` sur Windows/Linux (evite les blocages KIO/security et prompts installateur).
3. Au lancement:
   - apres `checkVersion`, si version plus recente disponible, tentative auto update + restart.
   - fallback normal si l'auto update echoue.
4. UI updater:
   - le bouton `Download` utilise aussi le flux download+apply quand disponible.

## Build

Version:
- `0.0.2-beta.0.6-topaz`

Artefacts:
- `SekaiLink-client-0.0.2-beta.0.6-topaz.AppImage`
- `SekaiLink-client-0.0.2-beta.0.6-topaz.exe`
- `SekaiLink-client-0.0.2-beta.0.6-topaz.exe.blockmap`

SHA256:
- AppImage: `eff66172e8ded87eea70cbd10e279208d1c97f312a00f0a798c2cf26be6cffc8`
- EXE: `c352910bc8966a3d96c3d6d56688d42f5d77e4c9fb679497cc36d9ffa985c61c`
- EXE blockmap: `868b77ee9fea3bd5fbdaa136ef5ba7bd549f98adaa60e92b32b6298f32815f7b`

## Publication VPS

Upload:
- `/opt/multiworldgg/WebHostLib/static/downloads/`

Config:
- `/opt/multiworldgg/config.yaml`
  - `CLIENT_LATEST_VERSION: "0.0.2-beta.0.6-topaz"`
  - URLs Linux/Windows vers `0.0.2-beta.0.6-topaz`
  - SHA256 Linux/Windows mis a jour
  - notes release: auto-update in-place + restart

Services:
- `systemctl restart multiworldgg-webhost multiworldgg-workers`
- etat final: `active` / `active`

## Verification endpoint

Valide:
- `GET https://sekailink.com/api/client/version`
- `GET https://sekailink.com/api/client/version?platform=windows`
- `GET https://sekailink.com/api/client/version?platform=linux`

Resultat:
- `latest: 0.0.2-beta.0.6-topaz`
- URLs et SHA256 alignes avec les artefacts publies

## Incident YAML (rappel)

Erreur deja vue pendant une mise a jour precedente:
- une valeur YAML avec `:` non quotee (`CLIENT_RELEASE_NOTES`) a casse le parsing de `config.yaml`.
- correction retenue: toujours quoter les valeurs texte de notes release.
