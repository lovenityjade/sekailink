# 35 - Server Reboot / Update Runbook

But: garder une procedure simple pour remettre le serveur SekaiLink en route et verifier l'updater.

## Acces
- infos d'acces: `sekailink-client-plan/XX-server-infos.md`
- projet serveur: `/opt/multiworldgg`
- stack serveur: `WebHostLib` (webhost) + workers.

## Services systemd (production)
- `multiworldgg-webhost.service`
- `multiworldgg-workers.service`

## Restart standard
```bash
systemctl restart multiworldgg-webhost multiworldgg-workers
systemctl is-active multiworldgg-webhost multiworldgg-workers
```

Resultat attendu:
- deux lignes `active`.

## Health checks rapides
```bash
curl -fsS "https://sekailink.com/api/client/version"
curl -fsS "https://sekailink.com/api/client/version?platform=windows"
curl -fsS "https://sekailink.com/api/client/version?platform=linux"
curl -fsS "https://sekailink.com/api/client/incremental-manifest?platform=windows"
curl -fsS "https://sekailink.com/static/UPDATE.md"
```

Resultat attendu:
- `latest` coherent avec la version publiee.
- URL windows en `.exe`.
- URL linux en `.AppImage`.

## Fichiers critiques updater
- endpoint:
  - `/opt/multiworldgg/WebHostLib/misc.py`
  - route: `@app.route(\"/api/client/version\")`
  - route: `@app.route(\"/api/client/incremental-manifest\")`
- config:
  - `/opt/multiworldgg/config.yaml`
  - cles:
    - `CLIENT_LATEST_VERSION`
    - `CLIENT_MIN_VERSION`
    - `CLIENT_DOWNLOAD_URL`
    - `CLIENT_DOWNLOAD_URL_WINDOWS`
    - `CLIENT_DOWNLOAD_URL_LINUX`
    - `CLIENT_RELEASE_NOTES`
    - `CLIENT_RELEASE_SHA256*` (optionnel)
    - `CLIENT_VERSION_MANIFEST_PATH` (optionnel)
    - `CLIENT_INCREMENTAL_MANIFEST_URL*`
    - `CLIENT_INCREMENTAL_MANIFEST_PATH*`
    - `CLIENT_UPDATE_NOTES_URL`
    - `CLIENT_UPDATE_NOTES_VERSION`
    - `CLIENT_UPDATE_NOTES_TITLE`
- fichiers statiques updater:
  - `/opt/multiworldgg/WebHostLib/static/UPDATE.md`
  - `/opt/multiworldgg/WebHostLib/static/downloads/client-incremental-manifest.json`

## Publish update checklist (client)
1. deposer les artefacts dans `/static/downloads` cote web.
2. (incremental) deposer/mettre a jour les fichiers sync sous `/static/client-sync`.
3. mettre a jour `client-incremental-manifest.json` (paths + sha256 + version).
4. mettre a jour `/static/UPDATE.md` si patch majeur.
5. mettre a jour `config.yaml` (ou manifest JSON externe).
6. restart services.
7. verifier endpoint `api/client/version`.
8. ouvrir le client SekaiLink et verifier:
   - notification update visible.
   - download in-app avec progression.
   - bouton install/folder fonctionnel.
   - sync runtime incremental fonctionne.
   - modal UPDATE.md apparait une seule fois.

## Notes de prudence
- ce repo serveur contient encore des fichiers historiques lies au client; ne pas les considerer comme source de verite pour la prod.
- pour les operations urgentes, prioriser:
  - `WebHostLib/misc.py`
  - `config.yaml`
  - et les services systemd ci-dessus.
