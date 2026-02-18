# UI Prototype (sekailink-UI-prototype) - 2026-02-14

Scope:
- UI-only rework (fonctionnalites preservees)
- build separe `sekailink-UI-prototype` (AppImage) pour validation design avant merge
- mises a jour automatiques desactivees dans ce prototype
- landing page (apres loading) = reproduction mockup (glass, neon cyan/teal, bordures fines, glow, scanlines/noise)

## Prototype isolation (entry + routing)
Files:
- `client/app/ui-prototype.html` (nouvelle entry Vite)
- `client/app/src/ui-prototype/main.tsx` (bootstrap prototype)
- `client/app/src/ui-prototype/prototypeApp.tsx` (routes isolees)
- `client/app/src/ui-prototype/ui/shell/PrototypeShell.tsx`
- `client/app/src/ui-prototype/ui/shell/PrototypeSidebar.tsx`

Notes:
- Vite build multi-entry: `index.html` + `ui-prototype.html`
- Routes prototype:
- `/` -> Dashboard (landing page mockup)
- `/lobby/:lobbyId` -> page Lobby existante (restylee via CSS prototype)
- autres pages: Game Manager, Settings, Help, Account, Player Options (routes existantes)

## Prototype theming (tokens + overlays + scrollbars)
Files:
- `client/app/src/ui-prototype/styles/tokens.css` (design tokens + mapping vers tokens legacy)
- `client/app/src/ui-prototype/styles/base.css` (background/atmosphere + scanlines/noise + scrollbars)
- `client/app/src/ui-prototype/styles/dashboard.css` (layout 3 colonnes et panels home)
- `client/app/src/ui-prototype/styles/overrides.css` (restyle pages existantes, sans toucher la logique)
- `client/app/src/ui-prototype/styles/lobby.css` (restyle lobby pour theme prototype)

Notes:
- pas de scroll global: scroll interne par panel
- scrollbars uniformes et subtils (Room List, Chat, Game Manager, User List, Lobby tabs)
- Orbitron: best-effort via stack fonts (TODO: packager la fonte pour 1:1 garanti)

## Dashboard (home landing)
Components:
- `client/app/src/ui-prototype/ui/components/RoomListPanel.tsx`
- `client/app/src/ui-prototype/ui/components/ChatroomPanel.tsx`
- `client/app/src/ui-prototype/ui/components/GameManagerPanel.tsx`
- `client/app/src/ui-prototype/ui/components/UserListPanel.tsx`
- `client/app/src/ui-prototype/ui/components/CreateRoomModal.tsx`

UX decisions:
- Room List: action par room = `Join` (au lieu de Ready/Join Lobby)
- Ajout chip `CREATE` a cote de `ALL/OPEN/FN` (ouvre create room modal)
- Sidebar: suppression entree `Room List` (home suffit)

## Lobby (UI-only + UX polishing)
Files:
- `client/app/src/pages/Lobby.tsx`
- `client/app/src/ui-prototype/styles/lobby.css`

Changements UI:
- right column: 3 boxes -> 1 panel avec onglets techno `Users / Controls / Info`
- suppression UI retract/collapse des 3 boites (tabs a la place)
- chat lobby:
- autoscroll force vers le bas sur nouveaux messages
- champ message 1 ligne (input) + envoi sur Enter
- bouton `Launch` dans le header du chat (a droite), retire du menu contextuel user
- Controls: la section "Local Game Client" est rabattable (Hide/Show)
- User list: suppression du bouton download `⬇`
- menu moderation/utilisateur: clic droit -> menu overlay sur toute l'interface (Release/Slow Release/Tracker/Hints/Kick)

## Disable automatic updates (prototype only)
File:
- `client/app/src/ui-prototype/ui/auth/AuthGatePrototype.tsx`

Notes:
- prototype ne branche pas le pipeline updater (pas d'incremental-sync / pas de check auto)
- auth/boot minimal conserve pour rester fonctionnel

## Electron packaging + fixes
Files:
- `client/app/electron-builder.ui-prototype.yml` (productName/executableName: `sekailink-UI-prototype`)
- `client/app/electron/main.cjs`

Changements:
- selection automatique `ui-prototype.html` en packaged mode si build prototype detecte (`app.getName()` ou `process.execPath` contient `ui-prototype`)
- fix crash runtime: regex invalide `devServerUrl.replace(/\\/$/, ...)` -> `devServerUrl.replace(/\/$/, ...)`
- window size prototype:
- base 1280x820
- prototype: +250px width, +100px height

Build:
- `cd client/app && npm run electron:pack:ui-prototype`
- output: `client/app/release/sekailink-UI-prototype-<version>.AppImage`

## Server: Online Users Endpoint (global user list)
Requirement:
- user list home doit afficher des users reels (pas placeholders)

Work:
- ajout endpoint sur VPS WebHost: `GET /api/online-users` (auth required)
- renvoie les users online (presence != offline) avec `display_name`, `avatar_url`, `status`, `last_seen`

Client:
- dashboard prototype fetch `apiJson("/api/online-users")` pour remplir le panel User List

Note:
- ce changement serveur a ete fait directement sur le VPS (pas via repo git local), a re-appliquer proprement dans le depot serveur si besoin.

