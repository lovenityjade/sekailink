# 34 - Updater Sprint (2026-02-11)

Objectif de ce sprint:
- implanter le systeme de mise a jour avant le prochain build Windows.
- documenter chaque action pour reprise rapide.

## Journal

### Etape 1 - Audit local
- verifier l'etat actuel du client updater.
- constater que `AuthGate.tsx` avait la logique de version mais banner/gate desactives (`setUpdateAvailable(false)`).
- constater qu'il n'y avait pas d'IPC updater dedie pour download progress + lancement du fichier telecharge.

### Etape 2 - Implementation client (local)
- `client/app/electron/main.cjs`
  - ajout canal `updater:event`.
  - ajout IPC:
    - `updater:download`
    - `updater:openDownloaded`
  - ajout telechargement in-app avec progression:
    - `downloadToDirWithProgress(...)`
    - redirections HTTP(S)
    - headers (Bearer token possible)
    - checksum SHA-256 optionnel
  - destination downloads: dossier utilisateur `Downloads/SekaiLink`.
- `client/app/electron/preload.cjs`
  - expose updater bridge:
    - `updaterDownload(...)`
    - `updaterOpenDownloaded(...)`
    - `onUpdaterEvent(...)`
- `client/app/src/vite-env.d.ts`
  - types des nouvelles APIs preload updater.
- `client/app/src/services/runtime.ts`
  - wrappers updater pour les appels renderer.
- `client/app/src/components/AuthGate.tsx`
  - reactivation de la verif version.
  - gate `update-required` selon `min_supported`.
  - notification de demarrage "update available".
  - icone update persistante + panneau details.
  - flux download/install/folder avec progression en direct.
- `client/app/src/styles/globalStyles.css`
  - styles du hub updater (toast, bouton, popover, progress bar).
- `client/app/CLIENT-NOTES.md`
  - notes mises a jour (updater re-active + contrat serveur).

### Etape 3 - A faire ensuite
- verifier/corriger endpoint serveur reel `/api/client/version` sur VPS.
- valider valeurs `latest`, `min_supported`, `download_url`, `sha256`.
- seulement apres: rebuild Windows et test du parcours complet.

### Etape 4 - Implementation serveur (VPS)
- hote: `root@sekailink.com`
- racine: `/opt/multiworldgg`
- endpoint modifie: `/opt/multiworldgg/WebHostLib/misc.py`
  - route `/api/client/version` mise a jour pour:
    - supporter `?platform=windows|linux|macos`
    - retourner url per-platform via config (`CLIENT_DOWNLOAD_URL_WINDOWS`, etc.)
    - supporter `sha256` optionnel
    - supporter un manifest optionnel via `CLIENT_VERSION_MANIFEST_PATH`
- config mise a jour: `/opt/multiworldgg/config.yaml`
  - `CLIENT_LATEST_VERSION: 0.0.2-beta.0.2-topaz`
  - URLs de download Linux/Windows vers `/static/downloads/...topaz...`
  - champs sha256 prepares (vides pour l'instant)
- redemarrage applique:
  - `systemctl restart multiworldgg-webhost multiworldgg-workers`
  - statut: `active` pour les deux services

### Etape 5 - Verification endpoint production
- `GET https://sekailink.com/api/client/version`
  - retourne latest `0.0.2-beta.0.2-topaz` + AppImage par defaut
- `GET https://sekailink.com/api/client/version?platform=windows`
  - retourne URL `.exe`
- `GET https://sekailink.com/api/client/version?platform=linux`
  - retourne URL `.AppImage`

### Etape 6 - Ajustement client post-serveur
- `AuthGate.tsx` envoie maintenant `platform` a l'endpoint version.
- compile client relancee avec succes apres ce patch.
- fix mineur: fallback navigateur pour download update remet bien l'etat `active=false`.
- compilations locales effectuees apres patchs code (`npm run build`) et OK.

### Etape 7 - Updater incremental + modal UPDATE.md
- client (`client/app/electron/main.cjs`):
  - ajout d'un moteur de sync incremental via manifest (`updater:syncIncremental`).
  - les updates incrementales sont appliquees en overlay local:
    - `userData/runtime/overlay/runtime/...`
    - `userData/runtime/overlay/ap/...`
  - seuls les fichiers listés dans le manifest et autorises (`runtime/` ou `ap/`) sont appliques.
  - verification SHA-256 par fichier avant activation.
  - reset des caches d'index modules apres sync (`_romIndexCache`, `_patchIndexCache`).
- client (`AuthGate`):
  - integration du bouton `Sync Runtime`.
  - affichage etats sync (actif, changed, deleted, bytes).
  - modal `UPDATE.md` au demarrage (apres boot), avec memorisation locale "deja lu".
  - si `UPDATE.md`/signature n'a pas change, le modal ne se reouvre pas.
- client UI:
  - nouveau composant: `client/app/src/components/UpdateNotesModal.tsx`
  - rendu markdown et scroll themed.
  - styles ajoutes dans `client/app/src/styles/globalStyles.css`.
- preload/types/runtime:
  - `updaterSyncIncremental(...)` expose cote renderer.

### Etape 8 - Serveur prod (VPS)
- endpoint `/api/client/version` enrichi avec:
  - `incremental_manifest_url`
  - `update_notes_url`
  - `update_notes_version`
  - `update_notes_title`
- nouvel endpoint `/api/client/incremental-manifest`.
- nouveaux assets prod:
  - `/opt/multiworldgg/WebHostLib/static/UPDATE.md`
  - `/opt/multiworldgg/WebHostLib/static/downloads/client-incremental-manifest.json`
  - `/opt/multiworldgg/WebHostLib/static/client-sync/` (racine des fichiers incrementaux servis)
- config prod maj (`/opt/multiworldgg/config.yaml`):
  - `CLIENT_INCREMENTAL_MANIFEST_*`
  - `CLIENT_UPDATE_NOTES_*`
- services redemarres et valides:
  - `multiworldgg-webhost: active`
  - `multiworldgg-workers: active`
