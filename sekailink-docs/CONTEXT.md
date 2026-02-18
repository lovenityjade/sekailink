# CONTEXT.md

## Regle d'or (priorite absolue)
**Facilite maximum pour l'utilisateur.** Le moins d'interaction possible avec des applications externes.  
SekaiLink est **cle en main** pour amener de nouveaux joueurs au multiworld sans leur faire lire 20 pages de setup par jeu.  
Tout doit etre automatise (patch, lancement, connexion, emu) avec une interaction minimale.

## Summary (session Feb 4, 2026 — Electron client + VPS)
- **Player Options React UI** now fully wired to API (no web HTML), with loading screen, two-column layout, scrollable columns, refined typography, and toast validation. Import button removed. Player name validation: only letters/numbers/`_`/`-`, no spaces.
- **Tracker drawer ported from web HTML to React** (no iframe). Tabs: My Tracker (default), All Players, Spheres. Search + hide checked + tables. Uses new VPS API endpoints:
  - `/api/tracker_view/<tracker>`
  - `/api/tracker_player/<tracker>/<team>/<player>`
  - `/api/sphere_tracker/<tracker>` (existing)
  - `/api/static_tracker/<tracker>` + `/api/datapackage/<checksum>` for names
  - VPS file changed: `/opt/multiworldgg/WebHostLib/api/tracker.py` (service restarted).
- **Lobby UX fixes**:
  - Controls/Room info/User list are collapsible.
  - Member actions moved to context menu (`⋯` + right‑click): Release/Slow Release (host only), Tracker, Launch, Kick (host).
  - Generate remains disabled after success; only host can generate. Tracker only opens after generation.
  - Ready fails with toast if no active game. Generate fails with toast if not all ready.
  - “YAML” wording replaced by “Game” in lobby UI.
- **Social + Lobby chat spam fix** (Linux client only):
  - Social drawer socket no longer reconnects on every action (prevents toast spam).
  - Lobby chat de-dup by message ID; polling suppressed while socket is connected.
- **Windows ZIP build** created and uploaded:
  - File: `SekaiLink-client-alpha-0.0.1.zip` in VPS `/opt/multiworldgg/WebHostLib/static/downloads/`
  - URL: `https://sekailink.com/static/downloads/SekaiLink-client-alpha-0.0.1.zip`
- **Windows build guide** added: `WINDOWS-BUILD.md` (Wine + electron-builder zip flow + upload URL).
- **Linux build** updated: `client/app/release/SekaiLink-client-alpha-0.0.1.AppImage`.

## Summary (current session)
- **Docs overhaul (world setup)**:
  - Built `GAMES-SETUP.md` (central per‑game setup + SekaiLink integration notes).
  - Built `SUPPORTED-GAMES.md` and added emulator categories (BizHawk, Dolphin, snes9x‑rr, Project64). Duplicates allowed to reflect emulator choice.
  - Added note: `tracker` (Universal Tracker) is not a game but must be supported.
  - Fixed **ROM server requirements**: all ROM‑based worlds now show `ROM serveur: Oui (requis pour generation cote host; pas necessaire cote client uniquement)`.
  - Moved **all root .md** files into `sekailink-docs/` (docs in `./docs` untouched).
- **Supported emulator scope**: BizHawk‑first strategy; Dolphin/snes9x‑rr/Project64 secondary.
- **Client runtime**:
  - Added **Pokemon Emerald BizHawk module** and ROM hash in manifest.
  - `patcher_wrapper.py` now supports `pokemon_emerald` ROM in settings.
  - Auto‑launch mapping now includes `.apemerald` in Lobby flow.
- **ROM management (zero‑interaction)**:
  - Added **Settings page** (gear icon) with **ROM folder scan**.
  - Scan hashes ROMs, **copies them into client storage** (`userData/roms`) and writes to `~/.sekailink/config.json`.
  - No hard dependency on external file paths after import.
  - New IPC: `app:pickFolder`, `roms:scan`, `config:get`, `config:setRom`.
- **Memory benchmarking (Jan 27, 2026)**:
  - Baseline after workers restart: **3.7 GiB used**.
  - Test A (UI, 10 rooms, 2 min): **7.1 GiB → 9.5 GiB** (+2.4 GiB) ≈ **0.24 GiB/room**.
  - Test B (NoKanto YAML via script, 10 rooms, 2 min, after workers restart): **3.7 GiB → 9.6 GiB** (+5.9 GiB) ≈ **0.59 GiB/room**.
  - Projection (order-of-magnitude): ~0.6 GiB/room ⇒ **~60–65 GiB** RAM for 100 active rooms; **~24–32 GiB** recommended for 25 rooms.
  - Note: memory did not drop after closing rooms without worker restart (hosters keep memory). Restarting workers resets baseline.
- **UFW cleanup + fix**:
  - Dynamic UFW rule deletion failed because we used `ufw delete <port>/tcp`. Fixed to `ufw --force delete allow <port>/tcp` in `WebHostLib/customserver.py`.
  - Manually removed stale UFW rules for previously opened room ports.
- Added **Lobby system** to landing + dedicated lobby page with chat, YAML selection, ready states, and owner-only generate flow.
- Implemented **Lobby chat** UI (IRC-like) with avatars + timestamps; added **room info box** with server/seed/room links and live stats.
- Added **per-user YAML selection** (dropdown), **Ready button**, **user list**, **patch download** links per player, and **tracker links** per user.
- Implemented **owner Generate**: gather all active YAMLs, build options, enqueue generation or generate immediately, create seed + room, and log to chat.
- Added **basic lobby moderation tools**: owner context menu with Kick (server-side removal + system chat message), Release player, and Report placeholder.
- Added **lobby bans + kick notification**: kicked users are added to a ban list, receive a live notice, and are redirected out of the lobby. Default ban length is 24 hours (configurable via kick payload).
- Added **lobby rules at creation**: host.yaml-style settings (release/collect/remaining/countdown, hint cost, spoiler, item cheat, plando modules) and **server password** (also used as lobby password + private indicator).
- Added **room settings panel** in the lobby page: visible to everyone, editable only by the host (with live updates).
- Added **terminal panel** for room logs + command input (owner).
- Added **Realtime via WebSocket + Redis** (Socket.IO): live chat, members, generation status, room stats, terminal output.
- Updated gunicorn to `geventwebsocket` worker and Apache to proxy `/socket.io/` (WebSocket).
- Multiple DB migrations: new lobby tables/columns; fixed missing columns (ready, ready_at).
- Added **slow release + release** controls (host can release/slow release anyone; player can release/slow release self). Release counts as completion and buttons disable after log confirmation.
- Added **seed timer** that starts when generation completes.
- Added **lobby/room timeouts** for stale/empty lobbies and room shutdown.
- Added **host transfer + voting** when host is inactive; host changes are announced.
- Added **join/leave activity feed** and online status tracking.
- Added **global user roles + bans** with ban reasons and support ticket storage.
- Added **UFW port management** (open on room start, close on shutdown/timeout).
- Added **banned page** with ban reason and appeal form.
- Created `UI-PLAN.md` and `TODO.md` to drive the beta UI/UX cleanup based on `ui-context.md`.
- Reworked UI layout to match mockups: new landing hero/nav, app shell sidebar, page background images, lobby/search styling, and DM profile card layout; added `boot.html` + `boot.css` template.
- Added a dedicated Room List page at `/rooms` with app shell and lobby creation modal; sidebar links updated to use `/rooms`.
- Applied the app shell + glass system to generate, host room, player options, supported games, and tutorial landing pages.
- Added a central SFX manager and wired lobby/social notifications to play audio cues.
- Added `/boot` route for the loading screen and wired auto-redirect with app-open SFX.
- QA fixes: admin tables now scroll on small screens, room list header stacks, lobby forms collapse on mobile, default SFX volume reduced, tutorial headings use theme accent.
- Updated typography to **Sora (headings/brand) + Inter (UI/body)**; removed the older display font usage across app styles.
- App UI is now more Electron-like (legacy headers/footers hidden), with higher background transparency, less rounded corners, and gradient corner borders.
- Sidebar standardized via `partials/appSidebar.html` across all app pages (icons, About/Contribute, mute toggle).
- Room list: full URL copy, Room Info modal, “Create Room” CTA in header, and fixed a JS crash (duplicate `const info`).
- Lobby: room info panel moved to top of side stack; Active YAML label next to Ready; chat font smaller; generation/system messages go to terminal.
- Social/DM: friend list scaled down for readability; profile card padded and less Discord-like; DM timestamps + typing indicator.
- Tracker embeds: purple background full-bleed, no header, glass container; tracker links use `?embed=1&theme=purple`.
- SFX unlock reliability improved (click/touchstart handlers added).
- Fixed realtime error: `_emit_generation_update` no longer shadows `to_url` (prevents UnboundLocalError).

## Key features added
- Landing page shows **lobby list** + create lobby form (Discord required).
- Lobby page sections:
  - Chatroom (WebSocket)
  - Active YAML (dropdown + Ready button)
  - User list (ready badge + tracker link + patch download)
  - Room info (room link, server:port, seed link, players count, checks %, completed)
  - Terminal (live log + command input)
- User list moderation: owner right-click menu with **Kick**, **Release player**, **Report** (placeholder).
- Release/Slow release updates completion and logs who triggered the release.
- Seed timer appears after successful generation.
- Host presence, transfer, and member voting for new host when inactive.
- Lobby auto-timeouts close stale/empty lobbies and shut down rooms.
- Generation flow:
  - Owner-only Generate; requires all users Ready + active YAML.
  - Creates `LobbyGeneration` records; posts system messages to chat.
  - On completion, creates room and posts seed/room links.
  - Server port message posted when room assigns port.

## Backend files touched/added
- `WebHostLib/models.py`: added `Lobby`, `LobbyMember`, `LobbyMessage`, `LobbyGeneration` plus reverse relations.
- `WebHostLib/lobbies.py`: lobby API + generation flow + realtime emits.
- `WebHostLib/lobbies.py`: added `/api/lobbies/<id>/kick` owner-only endpoint.
- `WebHostLib/models.py`: added `LobbyBan` (ban list storage).
- `WebHostLib/models.py`: added `LobbyHostVote`, `SupportTicket`, `User.role`, `User.banned`, `User.ban_reason`.
- `WebHostLib/lobbies.py`: ban checks on lobby join/chat/ready/YAML; kick now creates a ban + emits a direct kick notice.
- `WebHostLib/realtime.py`: user-scoped Socket.IO room + ban checks to block chat/join.
- `WebHostLib/models.py`: added lobby rule fields (server password + host options + plando toggles).
- `WebHostLib/lobbies.py`: lobby creation stores rule fields; private lobbies require password to join/chat; lobby list marks private.
- `WebHostLib/lobbies.py`: release/slow release endpoint, host transfer/vote endpoints, lobby close endpoint, host status endpoint.
- `WebHostLib/templates/landing.html` + `WebHostLib/static/styles/landing.css` + `WebHostLib/static/assets/lobbiesLanding.js`: lobby creation form now includes rule settings.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/assets/lobbyRoom.js` + `WebHostLib/static/styles/lobby.css`: private lobby password prompt + join flow.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/assets/lobbyRoom.js` + `WebHostLib/static/styles/lobby.css`: room settings panel + save flow.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/assets/lobbyRoom.js` + `WebHostLib/static/styles/lobby.css`: seed timer, host status UI, release controls, join/leave feed, settings lock on generate.
- `WebHostLib/lobbies.py`: new `/api/lobbies/<id>/settings` endpoint + realtime settings emit.
- `WebHostLib/realtime.py`: Socket.IO handlers, Redis-backed log tail + room stats emit.
- `WebHostLib/realtime.py`: join/leave system messages + online presence tracking.
- `WebHostLib/customserver.py`: emits server:port info to lobby via Socket.IO.
- `WebHostLib/api/room.py`: `players` now returns slot id/name/game.
- `WebHostLib/autolauncher.py`: generation success/failure now updates lobby + emits.
- `WebHostLib/autolauncher.py`: lobby cleanup timeouts and room shutdown on timeout.
- `WebHostLib/customserver.py`: UFW port open/close on room start/shutdown.
- `WebHostLib/__init__.py`: Socket.IO init + realtime module import.
- `WebHostLib/misc.py`: global ban enforcement, banned page route, ban appeal endpoint.
- `WebHostLib/templates/banned.html`: banned page with appeal form.
- UI: `WebHostLib/templates/landing.html`, `WebHostLib/templates/lobby.html`.
- JS/CSS: `WebHostLib/static/assets/lobbiesLanding.js`, `WebHostLib/static/assets/lobbyRoom.js`,
  `WebHostLib/static/styles/landing.css`, `WebHostLib/static/styles/lobby.css`.
  - `lobbyRoom.js` includes member context menu (kick/release/report).
  - `lobbyRoom.js` now handles `lobby_kicked` event and redirects to landing.
- `lobby.css` adds kick notification banner styles.

## Infra / Config changes
- **Gunicorn**: `/opt/sekailink/deploy/example_gunicorn.conf.py` now uses
  `geventwebsocket.gunicorn.workers.GeventWebSocketWorker`.
- **Apache**: `/etc/apache2/sites-available/sekailink.xyz-le-ssl.conf` now includes:
  - `ProxyPass /socket.io/ ws://127.0.0.1:8000/socket.io/`
  - `ProxyPassReverse /socket.io/ ws://127.0.0.1:8000/socket.io/`
- **Redis**: service running on `127.0.0.1:6379`. Socket.IO message queue uses `REDIS_URL` default `redis://127.0.0.1:6379/0`.
- **Dependencies** installed in venv + added to `requirements.txt`:
  `flask-socketio`, `redis`, `gevent`, `gevent-websocket`.
- **UFW sudoers**: `/etc/sudoers.d/sekailink-ufw` allows `sekailink` to run `/usr/sbin/ufw`.

## DB migrations performed
- Added `LobbyMember.ready` (default 0) and `ready_at`.
- Created `LobbyGeneration` table (SQLite).
- Manual fixes applied via python/sqlite for missing columns.
- Added lobby rule columns + lobby bans + host votes.
- Added `LobbyGeneration.completed_at`.
- Added user `role`, `banned`, `ban_reason`.
- Added `SupportTicket` table.

## Notable operational notes
- After a generation, **seed/room info is posted to lobby chat**.
- **Server:port** is posted when room port is assigned.
- Lobby realtime updates require Apache WebSocket proxy and gevent WebSocket worker.
- Lobby timeouts are configured in `WebHostLib/__init__.py`.
- Room ports are opened/closed via UFW for security.

## Commands used often
- Restart webhost: `systemctl restart multiworldgg-webhost.service`
- Restart workers: `systemctl restart multiworldgg-workers.service`
- Reload Apache: `systemctl reload apache2`

## Page / Template map (what each page does)
- `WebHostLib/templates/landing.html` + `WebHostLib/static/styles/landing.css` + `WebHostLib/static/assets/lobbiesLanding.js`
  - Public landing page. Hero + stats + links + lobby hub (list/create). Lobby list loads from `/api/lobbies`.
- `WebHostLib/templates/lobby.html` + `WebHostLib/static/styles/lobby.css` + `WebHostLib/static/assets/lobbyRoom.js`
  - Lobby experience: chat (WebSocket), YAML dropdown, Ready button, Generate (owner), room info + stats, terminal, user list.
  - Seed timer, host status/transfer, release controls, join/leave feed.
- `WebHostLib/templates/account.html` + `WebHostLib/static/styles/account.css`
  - Discord profile + terms status + YAML list (edit/delete/download) for the logged-in user.
- `WebHostLib/templates/yamlDashboard.html` + `WebHostLib/static/styles/yamlDashboard.css`
  - YAML dashboard landing (pick game).
- `WebHostLib/templates/playerOptions/playerOptions.html` + `WebHostLib/static/styles/playerOptions/playerOptions.css`
  - Options form for YAML creation/edit; used by dashboard and import flow.
- `WebHostLib/templates/generate.html` + `WebHostLib/static/styles/generate.css` + `WebHostLib/static/assets/generate.js`
  - Manual multiworld generation by uploading YAMLs.
- `WebHostLib/templates/hostGame.html` + `WebHostLib/static/styles/hostGame.css` + `WebHostLib/static/assets/hostGame.js`
  - Upload generated .zip/.archipelago to host a room manually.
- `WebHostLib/templates/hostRoom.html` + `WebHostLib/static/styles/hostRoom.css`
  - Room page (status/log view) for a generated room.
- `WebHostLib/templates/trackers/*` + `WebHostLib/static/styles/tracker*.css`
  - Trackers per game; generic tracker also exists.
- `WebHostLib/templates/tutorialLanding.html` + `WebHostLib/static/styles/tutorialLanding.css`
  - Guides landing (docs list).
- `WebHostLib/templates/supportedGames.html` + `WebHostLib/static/styles/supportedGames.css`
  - Supported games list + search.
- `WebHostLib/templates/startPlaying.html` + `WebHostLib/static/styles/startPlaying.css`
  - Getting started page.
- `WebHostLib/templates/legal.html` + `WebHostLib/static/styles/markdown.css`
  - Terms/Legal markdown rendering.
- `docs/sekailink.md`
  - SekaiLink webhost documentation (lobbies, generation, social, support, admin).

## Core feature flows (routes + logic)
- Lobby list/create: `GET/POST /api/lobbies` in `WebHostLib/lobbies.py`.
- Lobby view: `/lobby/<id>` in `WebHostLib/lobbies.py`.
- Lobby chat:
  - REST fallback: `GET/POST /api/lobbies/<id>/messages`.
  - Realtime: Socket.IO `chat_send` → emits `lobby_message`.
- Lobby members:
  - REST fallback: `GET /api/lobbies/<id>/members`.
  - Realtime: `members_update` emit on join/ready/yaml changes.
- Active YAML selection: `POST /api/lobbies/<id>/active-yaml`.
- Ready state: `POST /api/lobbies/<id>/ready`.
- Release/Slow release: `POST /api/lobbies/<id>/release`.
- Host transfer: `POST /api/lobbies/<id>/transfer-host`.

## Update (current session)
- Locked the **client emulator integration plan** (Linux first) with **BizHawk-only** for emu games (including ALttP) and **PopTracker always visible**.
- **CommonClient** will be integrated into Electron UI/UX as a **Python child process** (no user-entered commands; all actions are clickable/controller-friendly).
- **Per-game modules** will live in-repo and be dynamically loaded, avoiding full client updates for game tweaks.
- Added `CLIENT-EMULATOR-INTEGRATION.md` with the detailed plan and milestones.
- Host status/vote: `GET /api/lobbies/<id>/host-status`, `POST /api/lobbies/<id>/vote-host`.
- Lobby close: `POST /api/lobbies/<id>/close`.
- Generate flow: `POST /api/lobbies/<id>/generate`.
  - Collects all active YAMLs, builds options, enqueues or generates immediately.
  - Writes `LobbyGeneration` record, posts system messages.
- Generation status:
  - REST: `GET /api/lobbies/<id>/generation` (used as fallback).
  - Realtime: `generation_update` emit.
- Room info/patches: `GET /api/room_status/<room_id>` (updated to return slot/name/game).
- Terminal:
  - WebSocket: `terminal_output` from `realtime.py` tailer.
  - Command input: Socket.IO `terminal_command` (owner only).
  - REST fallback: `/log/<room_id>` + `POST /room/<room_id>` if needed.

## Realtime (Socket.IO + Redis) architecture
- `WebHostLib/realtime.py`:
  - Socket.IO handlers: `join_lobby`, `leave_lobby`, `disconnect`, `chat_send`,
    `terminal_command`, `watch_room`.
  - Redis used for message queue + cross-worker log tailing.
  - Background tasks: `_tail_room_log` (reads `logs/<room_id>.txt`),
    `_track_room_stats` (TrackerData stats).
- Client (`lobbyRoom.js`) connects to Socket.IO and listens for:
  - `lobby_message`, `members_update`, `generation_update`, `room_info`,
    `room_stats`, `terminal_output`, `lobby_kicked`, `lobby_closed`.
- Apache proxies `/socket.io/` to gunicorn (WebSocket).

## YAML / Player options system
- Dashboard routes in `WebHostLib/options.py`:
  - `/dashboard/yaml/new`, `/dashboard/yaml/<game>/create`, `/dashboard/yaml/<game>/edit/<yaml_id>`,
    `/dashboard/yaml/<game>/save`, `/dashboard/yaml/<yaml_id>/download`,
    `/dashboard/yaml/<yaml_id>/delete`, `/dashboard/yaml/import`.
- YAML stored per user in `/opt/sekailink/user_yamls/<user_uuid>/`.
- YAML is standard Archipelago options with SekaiLink metadata under `sekailink.title`.

## CSS/JS roles (important files)
- `WebHostLib/static/styles/globalStyles.css`: fonts (Sora + Inter), base theme.
- `WebHostLib/static/styles/themes/*`: themed headers; SekaiLink uses ocean-island header.
- `WebHostLib/static/assets/baseHeader.js`: mobile menu for header.
- `WebHostLib/static/assets/lobbyRoom.js`: main lobby client (Socket.IO + UI state).
- `WebHostLib/static/assets/lobbiesLanding.js`: lobby list polling + create.
- `WebHostLib/static/assets/termsModal.js`: terms popup logic.

## Current known fragile points
- WebSocket requires Apache proxy rules and gevent websocket worker.
- Redis must be running; Socket.IO uses Redis message queue to sync across workers.
- SQLite schema changes were applied manually; further model changes require manual ALTER TABLE.
- New `LobbyBan` table requires a manual migration in SQLite (`ap.db3`).
- New lobby rule fields and private lobby password columns require a manual migration in SQLite (`ap.db3`).

## Where to look first (if something breaks)
- Logs: `journalctl -u sekailink-webhost.service -n 120 --no-pager`
- Socket.IO / WebSocket errors: Apache error logs + browser console.
- Missing columns errors: SQLite `ap.db3` table schema.


## Update (current session)
- **Domains + SSL**:
  - Added `sekailink.com` and `admin.sekailink.com` Apache vhosts (HTTP→HTTPS redirect, SSL, proxy to `127.0.0.1:8000`, `/socket.io/` WebSocket rules).
  - `admin.sekailink.com` `/` redirects to `/admin`.
  - `HOST_ADDRESS` and `DISCORD_REDIRECT_URI` now use `sekailink.com` in `config.yaml`.
- **OAuth reliability**:
  - Added User-Agent/Accept headers for Discord OAuth requests; login redirect goes to `/rooms`.
- **UI shell**:
  - Sidebar standardized across app: removed Admin link; Dashboard moved to bottom with profile card + settings gear.
  - Added sidebar collapse toggle (icon-only mode) + smooth animation; About/Contribute now have icons.
  - Friends online count badge added after “Friends”.
  - App panel gets subtle “matrix” overlay; global modals now animate + draggable by header.
- **Room list**:
  - Removed duplicate “Create Room” CTA and empty helper text; hidden stray button dot.
- **Lobby UX**:
  - Room settings moved to host-only modal; button in hero meta.
  - Active YAML moved to modal and **supports multiple active YAMLs** with add/remove list (scrollable).
  - Room terminal moved to right-side drawer; “Room Terminal” button visible to all.
  - Right-side cards are collapsible; lobby scroll is internal with themed scrollbar.
  - Chat height fixed; lobby chat, terminal and tracker drawers now slide smoothly.
  - Player info modal added via context menu; context menu fixed (no auto-close).
  - Release/slow release actions now post to chat; generation messages appear in chat and terminal.
- **YAML dashboard**:
  - Added live search bar + internal scroll; fixed filter; cache-busted assets and no-cache headers.
- **Player options**:
  - Inputs and scrollbars styled to match theme.
- **Patreon (API prep)**:
  - OAuth routes: `/api/auth/patreon/login`, `/api/auth/patreon/callback`.
  - Webhook: `/api/patreon/webhook` with HMAC validation.
  - User fields for supporter status (tier, status, member id, etc.).
  - Placeholders added in `config.yaml` for Patreon secrets/URLs.
- **DB**:
  - Added `LobbyMemberYaml` table for multi-YAML per member.
- Added Patreon-related columns on `User`.
- **Game Manager + Custom YAMLs**:
  - Sidebar label “Create YAML” renamed to **Game Manager**.
  - New Game Manager page with tabs: **My Games**, **Add a Game**, **YAML Editor (Advanced)**.
  - Advanced editor saves as **new Custom YAML** (original unchanged) with a warning modal.
  - Custom YAML flag stored in DB (`UserYaml`), visible in lists and lobby selection.
  - Lobby rule **Allow Custom YAMLs** added; server blocks custom YAMLs if disabled.
  - Custom YAML label shown in lobby active YAML list and status.
  - Duplicate YAML button added (creates copy, appends to list).
- **Lobby cleanup**:
  - Periodic cleanup interval set to **5 minutes**.
  - Empty lobbies removed after **1 hour** with room shutdown.
- **Maintenance**:
  - Manual cleanup performed: removed all lobbies, rooms, seeds, and logs; restarted workers.
  - Added `sekailink.sh` helper script for `--start/--restart/--stop`.
  - Ensured webhost + workers are enabled at boot.

## Update (current session)
- **BizHawk v2.10** is now bundled in repo at `third_party/emulators/BizHawk-2.10-linux-x64` and used as default for launch.
- Lobby **Launch** now performs: patch -> BizHawk -> CommonClient auto-connect (self-only).
- Multi-patch support: Launch handles multiple patches, player modal shows patch count + Launch All (self only).
- PopTracker compiled on Linux and placed under `client/runtime/poptracker/` (submodules initialized; SDL2 + OpenSSL deps installed).
- PopTracker IPC runner added (launch/stop) and wired into Lobby auto-launch (non-blocking).
- Tracker pack installer added (GitHub releases -> `userData/poptracker/packs`) and Game Setup registry stubbed.
- Game Manager now shows setup checklist + Ready badge for Pokemon FR/LG and Emerald.
- Add tab now has Setup buttons for FR/LG/Emerald with inline wizard panel.
- Setup Test added (ROM + tracker pack + PopTracker presence + pack manifest validation).
- Lobby auto-launch now blocks if Game Setup is incomplete for FR/LG or Emerald.
- ROM import now uses direct file selection (copy into app-managed `userData/roms`) instead of folder scan.
- Rebuilt AppImage after swapping scan->import, but user still saw "Scan ROM Folder" in UI (likely stale build/cached AppImage). Needs verification.
