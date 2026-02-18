# Contract checks et tests (robustesse)

But: documenter une strategie de tests qui garantit que SekaiLink reste robuste quand:
- l'API serveur change
- un world renvoie `download_slot_file` au lieu d'un patch `.ap*`
- un runtime change (emulateur update)
- l'environnement change (Wayland vs X11, multi-ecran)

Contrainte:
- le code serveur dans ce repo n'est pas forcement a jour par rapport au VPS
- on doit donc tester le "contrat" (comportement observe), pas l'implementation locale

## Contrats a figer

Server:
- `/api/room_status/<room_id>`:
- structure `downloads[]` et format des URLs
- presence `players[]`, `last_port`, `tracker`

Downloads:
- distinguer `download_patch` et `download_slot_file`
- fournir `filename` et `content_type` serait ideal, sinon le client doit inferer

Runtimes:
- modules `manifest.json` valides (schema)
- `rom_requirements` coherents (hashes)
- connecteurs (Lua) existent sur disque

Trackers:
- PopTracker executable present
- pack manifest valide (attention UTF-8 BOM possible)

## Contract checks (client)

Checks proposes au demarrage (best-effort, non bloquants):
- verifier que `runtime/modules/*/manifest.json` sont parsables
- verifier que les extensions patch map sur au moins un module (pas de hardcode)
- verifier que le patcher wrapper peut s'executer (`python -m ...`)

Checks a l'ouverture d'une room:
- appeler `/api/room_status/<room_id>`
- classifier chaque download en:
- `ap_patch` (extension connue)
- `slot_file` (inconnu ou non patch)
- afficher un statut par download

## Tests automatisables (repo)

Unit tests:
- validation schema JSON pour les manifests
- resolution extension -> moduleId (registry)
- parsing et validation packs PopTracker

Integration tests (headless):
- `patcher_wrapper.py` sur un patch fixture (quand legal et possible)
- `commonclient_wrapper.py` connect sur un serveur test (local)

E2E (manuel au debut):
- Wayland session:
- launch BizHawk + connect + tracker
- X11 session:
- idem
- multi-ecran:
- appliquer un preset layout
- streamer mode:
- verifier absence de secrets en argv

## Matrix de compat OS

Linux:
- X11 (primaire)
- Wayland (ne pas casser)
- gamescope: present sur Bazzite/SteamOS, fallback sinon

Windows:
- process tree kill
- paths et espaces

## Definition of Done (DoD) par module runtime

Pour declarer un module "supporte":
- patch/slot file resolu automatiquement
- prereqs detectes et guides (Fix buttons)
- launch + connect stable
- tracker fonctionne ou degrade proprement
- logs accessibles

