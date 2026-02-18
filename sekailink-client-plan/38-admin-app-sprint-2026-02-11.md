# 38 - Admin App Sprint (2026-02-11)

Objectif:
- livrer une application administratrice separee du client joueur.
- connecter moderation/ops au serveur reel.
- renforcer le backend admin cote WebHostLib quand necessaire.

## Resultat

Nouvelle app desktop admin:
- chemin: `client/admin-app`
- stack: Electron + React + Vue (Vue utilise via custom element `service-health-widget` dans le dashboard)
- build valide localement avec `npm run build`

Backend admin renforce sur VPS:
- fichier: `/opt/multiworldgg/WebHostLib/misc.py`
- nouvelles routes:
  - `GET /api/admin/me`
  - `GET /api/admin/system/health`
  - `POST /api/admin/system/services/<service_key>/restart`
  - `GET /api/admin/system/journal`
  - `POST /api/admin/system/reboot`
  - `POST /api/admin/rooms/purge-filtered`
  - `GET /api/admin/db/<entity>` (read-only explorer)
- OAuth desktop multi-app:
  - `GET /api/auth/desktop-login?scheme=sekailink-admin`
  - redirect adapte selon le `state` prefix (`sekailink` ou `sekailink-admin`)

Sudo policy ajoutee (serveur):
- fichier: `/etc/sudoers.d/90-multiworldgg-admin`
- user: `multiworldgg`
- commandes autorisees sans mot de passe:
  - restart `multiworldgg-webhost`
  - restart `multiworldgg-workers`
  - reboot via `systemctl`
  - `journalctl` pour webhost/workers

Audit admin:
- fichier de log: `/opt/multiworldgg/logs/admin_audit.log`
- format: JSON lines

## Details app admin (`client/admin-app`)

Fichiers cle:
- `client/admin-app/package.json`
- `client/admin-app/electron/main.cjs`
- `client/admin-app/electron/preload.cjs`
- `client/admin-app/src/services/api.ts`
- `client/admin-app/src/services/adminApi.ts`
- `client/admin-app/src/components/AuthGate.tsx`
- `client/admin-app/src/pages/DashboardPage.tsx`
- `client/admin-app/src/pages/UsersPage.tsx`
- `client/admin-app/src/pages/LobbiesPage.tsx`
- `client/admin-app/src/pages/RoomsPage.tsx`
- `client/admin-app/src/pages/LogsPage.tsx`
- `client/admin-app/src/pages/SupportPage.tsx`
- `client/admin-app/src/vue/service-health.ce.ts`
- `client/admin-app/src/styles/global.css`
- `client/admin-app/.env.example`

Fonctions livrees:
- auth desktop OAuth via protocole `sekailink-admin://`
- guard admin (`/api/admin/me`)
- dashboard ops (health + journal + restart service + reboot)
- moderation users (role, ban/unban)
- gestion lobbies (inspect + close)
- gestion rooms (close + purge all + purge filtered)
- logs serveur (liste + tail)
- tickets support (reply/close)
- DB explorer read-only (users/lobbies/rooms/generations/tickets)

## Validation executee

Build local:
- `cd client/admin-app && npm install`
- `cd client/admin-app && npm run build` => OK

Validation serveur:
- `python3 -m py_compile /opt/multiworldgg/WebHostLib/misc.py` => OK
- `systemctl restart multiworldgg-webhost multiworldgg-workers` => OK
- `systemctl is-active ...` => `active`, `active`

Validation API (token admin temporaire):
- `GET /api/admin/me` => OK
- `GET /api/admin/system/health` => OK
- `GET /api/admin/system/journal?service=webhost&lines=10` => OK
- `POST /api/admin/system/services/workers/restart` => OK
- `POST /api/admin/rooms/purge-filtered` => OK

Validation OAuth scheme:
- `GET /api/auth/desktop-login?scheme=sekailink-admin` retourne un `state=sekailink-admin:<token>`
- `GET /api/auth/desktop-redirect?...&state=sekailink-admin:...` renvoie vers `sekailink-admin://auth?...`

Artefacts app admin publies:
- `/opt/multiworldgg/WebHostLib/static/downloads/SekaiLink-admin-0.0.1-alpha.0.AppImage`
- `/opt/multiworldgg/WebHostLib/static/downloads/SekaiLink-admin-0.0.1-alpha.0.exe`
- `/opt/multiworldgg/WebHostLib/static/downloads/SekaiLink-admin-0.0.1-alpha.0.exe.blockmap`
- SHA256:
  - `6120c01f4fda7f5752722e66fd6f6ecd699cbfad0682fec2c5fc030cfbebaaaf` AppImage
  - `3dd3159fd482cbf5d1d4838f40b87f506ef2ba698a205f4211724b085fe4151b` EXE
  - `506366ef9f5f2fa5ab29a9d301dc75ef63efb2de2a2a51eff9171b195130b942` EXE blockmap

## Notes de robustesse

- Les endpoints destructifs (`restart`, `reboot`, purge) sont admin-only et audites.
- `reboot` demande `confirm=REBOOT` + raison.
- Le dashboard lit des metriques systeme sans dependance externe (`/proc`, `disk_usage`, `systemctl show`).
- Le parsing `systemctl show` a ete corrige en key=value (plus stable).

## Prochaines etapes recommandees

1. Ajouter un scope RBAC plus fin (`admin`, `ops`, `moderator`) sur les endpoints systeme.
2. Ajouter un endpoint d'audit query (`/api/admin/audit/query`) pour lire `admin_audit.log` directement dans l'app.
3. Produire un package desktop admin (`electron:pack`, `electron:pack:win`) et publier dans `/static/downloads`.
