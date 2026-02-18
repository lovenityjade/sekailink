You are continuing work on the SekaiLink webhost in /opt/multiworldgg.

Current status (updated Jan 2026):
- WebHost runs via systemd: `multiworldgg-webhost.service` (gunicorn) and workers via `multiworldgg-workers.service`.
- Gunicorn uses WebSocket worker: `geventwebsocket.gunicorn.workers.GeventWebSocketWorker`.
- Redis is running on `127.0.0.1:6379`; Socket.IO uses `REDIS_URL` (default `redis://127.0.0.1:6379/0`).
- Apache vhost proxies to `127.0.0.1:8000` and includes WebSocket rules for `/socket.io/`.
- Domain: `sekailink.com` points to VPS; `admin.sekailink.com` redirects to `/admin`.
- HOST_ADDRESS set to `sekailink.com` in `config.yaml`.
- Theme: dark green glass, gamer style, no MWGG logo; footer shows "SekaiLink v0.01-alpha" and "Powered by Archipelago".
- Discord OAuth implemented with scopes `identify email` and routes:
  - `/api/auth/login`, `/api/auth/callback`, `/api/auth/logout`, `/api/auth/terms`.
- Terms popup on first login; acceptance stored with IP + user agent and version `TERMS_VERSION = v1`.
- Account dashboard at `/account` with profile, terms status, and YAML management.
- YAML creator:
  - `Create a YAML` → select game → player options → Save YAML.
  - YAML list with Edit/Download/Delete.
  - Import YAML button uploads file and opens correct options page pre-filled.
  - Storage in `/opt/sekailink/user_yamls/<user_uuid>/`.
  - DB has `User.user_uuid` and other fields; migration already done.

Lobby system (new):
- Landing page shows lobby list + create lobby form (Discord required).
- Lobby page: chat (IRC-style), YAML dropdown, Ready button, user list, patch download links, tracker links.
- Owner-only Generate: gathers active YAMLs, queues/executes generation, creates seed + room, posts system messages.
- Room info box: room/seed link, server:port, players, checks %, completed count.
- Terminal panel: live room logs + command input (owner).
- Realtime updates via Socket.IO (`WebHostLib/realtime.py`).
- Owner moderation (basic): right-click context menu with Kick from lobby, Release player, and Report (placeholder).
- Kick now creates a lobby ban and sends a real-time notice + redirect to the kicked user (default 24 hours).
- Lobby creation now includes host.yaml-style rule settings + server password (also used as lobby password and private flag).
- Lobby page now shows room settings to everyone; only the host can edit.
- Release/Slow release controls (host or player), counts as completion and disables buttons after log confirmation.
- Seed timer starts after generation completes and is visible to everyone.
- Host transfer + vote flow when host is inactive; host changes are announced.
- Lobby timeouts close stale/empty lobbies and shut down rooms.
- Join/leave activity feed + online presence tracking in lobby.
- Global user roles + bans with ban reasons; banned users are redirected to `/banned` with appeal form.
- Room ports open/close via UFW on start/shutdown for VPS security.
- OAuth redirect after login lands on `/rooms`.
- Admin subdomain root redirects to `/admin`.
- Game Manager replaces “Create YAML”: tabs for My Games / Add a Game / YAML Editor (Advanced).
- Advanced YAML editor saves as a **new Custom YAML** (original untouched) with a warning modal.
- Custom YAMLs are tracked in DB and can be blocked per lobby via “Allow Custom YAMLs”.

If issues occur:
- Webhost logs: `journalctl -u multiworldgg-webhost.service -n 120 --no-pager`.
- Worker logs: `journalctl -u multiworldgg-workers.service -n 120 --no-pager`.
- Apache proxy + WebSocket rules: `/etc/apache2/sites-available/sekailink.xyz-le-ssl.conf`.
- Verify `/opt/sekailink/user_yamls` permissions are `sekailink:sekailink`.
- Confirm Socket.IO works through Apache (WebSocket upgrade).

Likely next steps:
- Replace any remaining REST polling with WebSocket events.
- Add admin tools for lobby moderation or reset generation.
- UI polish for lobby terminal and status boxes.
- Add ban management tools (view/unban).
- Patreon integration (OAuth + webhook) and supporter badge surface.
- Validate Game Manager UX (duplicate, custom warning, lobby blocking).

Checklist de vérif (après déploiement):
- Page lobby: chat temps réel (messages instantanés, avatars ok).
- Boutons Ready/Generate: état synchro sans refresh.
- Terminal: output live + commandes envoyées.
- Room info: server:port + seed/room links + stats (checks/complete).
- Patch download links par joueur.
- WebSocket OK via Apache (pas d’erreurs 502/upgrade).

Commandes utiles:
- Restart webhost: `systemctl restart multiworldgg-webhost.service`
- Restart workers: `systemctl restart multiworldgg-workers.service`
- Reload Apache: `systemctl reload apache2`
- Logs webhost: `journalctl -u multiworldgg-webhost.service -n 120 --no-pager`
- Logs workers: `journalctl -u multiworldgg-workers.service -n 120 --no-pager`

Rollback rapide (WebSocket):
- Gunicorn worker: remettre `worker_class = "gthread"` dans `deploy/example_gunicorn.conf.py`
- Apache vhost: retirer les règles `/socket.io/` si besoin
- Redémarrer webhost + reload Apache

Mini changelog (this session):
- `WebHostLib/models.py`: tables lobby + generation + ready/ready_at + relations.
- `WebHostLib/lobbies.py`: lobby API + generation + realtime emits.
- `WebHostLib/lobbies.py`: added `/api/lobbies/<id>/kick` owner-only endpoint.
- `WebHostLib/realtime.py`: Socket.IO (chat/members/generation/terminal/room stats).
- `WebHostLib/models.py`: added `LobbyBan` storage.
- `WebHostLib/models.py`: added `LobbyHostVote`, `SupportTicket`, `User.role`, `User.banned`, `User.ban_reason`.
- `WebHostLib/lobbies.py`: ban checks + kick creates timed ban + emits kick notice.
- `WebHostLib/realtime.py`: user-scoped Socket.IO rooms + ban checks for join/chat.
- `WebHostLib/models.py`: added lobby rule fields (server password + host options + plando toggles).
- `WebHostLib/lobbies.py`: lobby creation stores rule fields; private lobbies require password; lobby list marks private.
- `WebHostLib/templates/landing.html` + `WebHostLib/static/styles/landing.css` + `WebHostLib/static/assets/lobbiesLanding.js`: lobby create form includes rule settings.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/assets/lobbyRoom.js` + `WebHostLib/static/styles/lobby.css`: private lobby password prompt + join flow.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/assets/lobbyRoom.js` + `WebHostLib/static/styles/lobby.css`: room settings panel + save flow.
- `WebHostLib/lobbies.py`: `/api/lobbies/<id>/settings` endpoint + realtime settings emit.
- `WebHostLib/customserver.py`: emit server:port to lobby.
- `WebHostLib/api/room.py`: players now include slot/name/game.
- `WebHostLib/autolauncher.py`: generation completion/error updates lobby.
- `WebHostLib/templates/lobby.html`: lobby UI sections + terminal + status boxes.
- `WebHostLib/static/assets/lobbyRoom.js`: live client (Socket.IO) + UI logic + member context menu.
- `WebHostLib/static/styles/lobby.css`: lobby layout, room info, terminal, buttons.
- `WebHostLib/templates/landing.html` + `WebHostLib/static/assets/lobbiesLanding.js` + `WebHostLib/static/styles/landing.css`: lobby hub.
- `WebHostLib/__init__.py`: Socket.IO init + realtime import.
- `deploy/example_gunicorn.conf.py`: gevent websocket worker.
- `requirements.txt`: flask-socketio, redis, gevent, gevent-websocket.
- `/etc/apache2/sites-available/sekailink.xyz-le-ssl.conf`: /socket.io proxy.
- `WebHostLib/autolauncher.py`: lobby timeouts cleanup.
- `WebHostLib/customserver.py`: UFW open/close for room ports.
- `WebHostLib/misc.py`: global ban enforcement + ban appeal endpoint.
- `WebHostLib/templates/banned.html`: banned page with appeal form.
- Added `sekailink.com` + `admin.sekailink.com` vhosts (SSL + proxy); certs via Let's Encrypt.
- `WebHostLib/misc.py`: OAuth user-agent header fix + redirect to `/rooms`; admin subdomain redirect.
- Sidebar standardized: removed Admin link; Dashboard moved bottom with profile + settings gear; friends online count.
- Room list: removed duplicate “Create Room”; removed empty helper text; hide stray button.
- Lobby UI: room settings modal (host-only), Active YAML modal (multi-select), terminal drawer, collapsible side cards, fixed chat height, internal lobby scroll.
- Lobby: player info modal via context menu; context menu fixed; release/slow release now posts to chat; generation messages shown in chat + terminal.
- YAML dashboard: search bar with live filter + internal scroll; styled inputs; cache bust on assets + no-cache headers.
- Player options: inputs + scrollbars themed.
- App shell: sidebar collapse toggle with animation; About/Contribute icons; modal animations + draggable headers; subtle matrix background on main app panel.
- Patreon: new OAuth routes `/api/auth/patreon/login` + `/api/auth/patreon/callback`, webhook `/api/patreon/webhook`, and user fields for supporter status; placeholders in `config.yaml`.
