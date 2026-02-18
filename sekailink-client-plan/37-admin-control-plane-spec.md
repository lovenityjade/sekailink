# 37 - Admin Control Plane Spec (Electron admin app)

But: documenter le cote serveur pour livrer une application administratrice separee du client joueur, avec auth admin stricte, operations serveur, et observabilite.

Date de reference: 2026-02-11.

## 1) Etat serveur actuel (source de verite)

- VPS: Ubuntu
- projet: `/opt/multiworldgg`
- webhost: `WebHostLib/`
- DB: SQLite (`/opt/multiworldgg/ap.db3`, configure via `PONY.filename`)
- logs room/host: `/opt/multiworldgg/logs/*.txt`
- services systemd:
  - `multiworldgg-webhost` (gunicorn, bind `127.0.0.1:8000`)
  - `multiworldgg-workers` (`WebHost.py`, autogen/autohost)

Proxy/reseau observe:
- front HTTP(S) via Apache (`:80/:443`)
- webhost interne sur `127.0.0.1:8000`
- Redis local `127.0.0.1:6379`

Important:
- Les secrets d'acces (SSH, mots de passe) restent dans `sekailink-client-plan/XX-server-infos.md`.
- Ne jamais les recopier dans cette spec ni dans le code client/admin.

## 2) Capacites admin deja disponibles (serveur)

Auth/role:
- `WebHostLib/misc.py` enforce `admin` via `_require_admin_user()`.
- page admin web existante: `/admin` (template `WebHostLib/templates/admin.html` + JS `WebHostLib/static/assets/admin.js`).

Endpoints admin existants (deja exploitables):
- users:
  - `GET /api/admin/users`
  - `POST /api/admin/users/<discord_id>/role`
  - `POST /api/admin/users/<discord_id>/ban`
- lobbies:
  - `GET /api/admin/lobbies`
  - `GET /api/admin/lobbies/<lobby_id>`
  - `POST /api/admin/lobbies/<lobby_id>/close`
- rooms:
  - `GET /api/admin/rooms`
  - `POST /api/admin/rooms/<room_id>/close`
  - `POST /api/admin/rooms/purge`
- logs:
  - `GET /api/admin/logs`
  - `GET /api/admin/logs/<filename>?lines=...`
- support:
  - `GET /api/admin/support`
  - `POST /api/admin/support/<ticket_id>/status`

Monitoring tokenise (hors session admin):
- `GET /api/monitoring/rooms`
- `GET /api/monitoring/games`
- `POST /api/monitoring/broadcast`
- garde par `MONITORING_ADMIN_TOKEN` (Bearer), cf `WebHostLib/api/monitoring.py`.

Realtime dispo:
- Socket.IO dans `WebHostLib/realtime.py` (lobby chat, terminal output, room stats, etc.).

## 3) Gaps pour l'app admin cible

Fonctions demandees mais pas encore exposees proprement:
- reboot VPS (host-level)
- restart systemd granular (webhost/workers) via API securisee
- gestion process/health plus detaillee (CPU/RAM/disk, uptime, queue etat)
- gestion DB guidee (read-first, write controls, backup/restore)
- purge selective (par age, game, owner, status) au lieu de tout fermer
- workflow d'audit (qui a fait quelle action admin, quand, resultat)
- RBAC plus fin (admin vs ops vs mod) pour limiter les actions destructives

Point critique:
- les endpoints `/api/admin/*` actuels sont suffisants pour un panel web basique, mais pas pour un control plane ops complet.

## 4) Architecture recommandee (admin app separee)

## 4.1 Composants

1. `SekaiLink Admin` (Electron, app separee)
- UI meme theme que client joueur.
- aucune logique critique locale; tout passe par API serveur.

2. `Admin API` (dans `WebHostLib`)
- endpoints applicatifs (users/lobbies/rooms/support/logs/monitoring).
- auth basee sur session desktop token + role + scope.

3. `Admin Agent` (nouveau service local VPS)
- expose uniquement sur `127.0.0.1` (pas public).
- execute operations host-level:
  - `systemctl status/restart`
  - `journalctl tail`
  - `reboot` (si confirme)
  - metriques systeme.
- appele par `WebHostLib` cote serveur via bridge interne.

Pourquoi agent:
- evite d'executer des commandes OS depuis le process web principal.
- permet ACL stricte et audit par action.

## 4.2 Flux de securite

1. Login Discord desktop normal.
2. Verification serveur du role (`admin` requis).
3. Emission token admin scope court (ex: 15 min), refresh controle.
4. Pour action destructive (purge globale, reboot): challenge reauth + code confirme.
5. Ecriture audit log obligatoire (user, action, payload sanitize, resultat).

## 5) Spec fonctionnelle de l'app Electron admin

## 5.1 Modules UI (MVP)

1. Dashboard Ops
- etat services (`webhost`, `workers`)
- active rooms count, queue generation, erreurs recentes
- boutons restart services (avec modal confirmation)

2. Utilisateurs
- recherche
- role set
- ban/unban avec raison

3. Lobbies
- liste + detail
- close lobby
- purge inactive avec filtres

4. Room Servers
- liste + statut (`Active/Idle/Stopped/Error`)
- close room
- purge rooms filtrees (age/status)

5. Logs
- room logs
- webhost/workers logs (journalctl tail via agent)
- export texte

6. DB Explorer (read-first)
- tables principales (`User`, `Lobby`, `Room`, `LobbyGeneration`, `SupportTicket`)
- filtres, pagination
- write operations explicites et limitees (pas SQL libre en MVP)

7. Maintenance
- restart services
- reboot VPS (garde-fous)
- checklist post-action

## 5.2 Auth UX

- ecran d'entree: "Admin Access Required".
- si compte non-admin: blocage avec message clair.
- badge session: `admin`, expiration token, dernier refresh.

## 6) Contrat API cible (extensions necessaires)

Reutiliser d'abord les endpoints existants, puis ajouter:

Nouveaux endpoints proposes (serveur):
- `GET /api/admin/me`
  - retourne identite admin + role + scopes.
- `GET /api/admin/system/health`
  - webhost/workers status, uptime, version, disk, cpu, mem.
- `POST /api/admin/system/services/<name>/restart`
  - `<name>`: `webhost` ou `workers`.
- `POST /api/admin/system/reboot`
  - action destructrice, requiert challenge token.
- `GET /api/admin/system/journal`
  - stream tail (`service`, `lines`).
- `POST /api/admin/rooms/purge-filtered`
  - filtres: `older_than`, `status`, `game`, `owner`.
- `GET /api/admin/db/<entity>`
  - lecture paginee whitelistee.
- `POST /api/admin/audit/query`
  - lecture des actions admin historisees.

Contraintes:
- aucune execution shell libre exposee a l'UI.
- whitelists strictes cote serveur/agent.
- rate limit sur endpoints sensibles.

## 7) Modele de donnees utile (admin)

Entites principales (cf `WebHostLib/models.py`):
- `User` (role, banned, presence, terms)
- `Lobby`, `LobbyMember`, `LobbyGeneration`
- `Room`, `Command`, `Seed`, `Slot`
- `SupportTicket`, `DirectMessage`
- `DesktopToken`

Regle operationnelle:
- pour moderation/ops quotidienne, passer par API; eviter manip DB manuelle.
- DB manuelle reservee aux incidents + backup.

## 8) Operations VPS (runbook admin app)

Fallback CLI (si UI indisponible):

```bash
systemctl status multiworldgg-webhost multiworldgg-workers
systemctl restart multiworldgg-webhost multiworldgg-workers
journalctl -u multiworldgg-webhost -n 200 --no-pager
journalctl -u multiworldgg-workers -n 200 --no-pager
```

Health checks:

```bash
curl -fsS https://sekailink.com/api/client/version
curl -fsS https://sekailink.com/api/client/incremental-manifest?platform=windows
```

## 9) Plan d'implementation recommande

Phase A - Stabiliser le backend admin existant
- ajouter `GET /api/admin/me` avec role/scopes
- ajouter audit log admin central
- durcir erreurs/reponses des endpoints existants

Phase B - Control plane serveur
- creer `admin-agent` (localhost only) + service systemd dedie
- brancher endpoints `system/*` via agent
- ajouter purge filtree + DB read endpoints

Phase C - App Electron Admin (separee)
- base UI (theme client)
- auth + guard admin
- modules: dashboard/users/lobbies/rooms/logs/support
- module maintenance (restart/reboot) derriere confirmations

Phase D - Hardening
- tests e2e admin
- rotation tokens, timeout session
- limitation et monitoring des actions destructives

## 10) Definition of Done (admin app v1)

- App Electron admin separee du client joueur.
- acces refuse aux non-admin.
- users/lobbies/rooms/logs/support operationnels.
- restart services operationnel depuis UI.
- reboot VPS operationnel avec double confirmation + audit.
- logs et erreurs exploitables sans SSH.
- doc runbook tenue a jour dans `sekailink-client-plan/`.

## 11) Dette technique a traiter cote serveur

- Le repo serveur contient encore des fichiers lies au client desktop historique.
- Pour l'app admin, isoler clairement:
  - "serveur pur" (`WebHostLib`, workers, config, scripts ops)
  - "artefacts client" (downloads/static/update assets)
- Maintenir un dossier ops dedie (ex: `ops/`) pour:
  - scripts reboot/restart verifies
  - templates systemd
  - procedures backup DB/log rotation
- Ne pas melanger credentials/secret dans les docs fonctionnelles.
