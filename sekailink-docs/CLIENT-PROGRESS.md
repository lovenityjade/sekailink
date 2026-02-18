# CLIENT-PROGRESS.md

Date: 2026-01-29

## What I did
- Identified web UI sources to port: `WebHostLib/templates/{roomList.html,lobby.html,gameManager.html,account.html}`, `WebHostLib/templates/partials/appSidebar.html`, and shared wrapper in `WebHostLib/templates/pageWrapper.html`.
- Identified CSS + JS sources for app pages: `WebHostLib/static/styles/{globalStyles,appShell,roomList,lobby,gameManager,account,yamlDashboard,social,termsModal,tooltip}.css` and scripts `WebHostLib/static/assets/{lobbiesLanding,lobbyRoom,gameManager}.js`.
- Created Electron + React skeleton under `client/app/` with Vite + React Router.
- Copied app background assets into `client/app/public/assets/{img,sfx}`.
- Copied core CSS files into `client/app/src/styles/` and wired them in `src/main.tsx`.
- Added Electron main/preload: `client/app/electron/main.cjs` and `client/app/electron/preload.cjs`.
- Added base app shell and sidebar components:
  - `client/app/src/components/AppShell.tsx`
  - `client/app/src/components/Sidebar.tsx`
  - `client/app/src/components/AboutModal.tsx`
  - `client/app/src/components/ContributeModal.tsx`
  - `client/app/src/components/SocialDrawer.tsx`
- Added utilities:
  - `client/app/src/services/api.ts`
  - `client/app/src/hooks/useSfx.ts`
  - `client/app/src/hooks/useInterval.ts`
- Ported Room List page layout and modal logic to React: `client/app/src/pages/RoomList.tsx`.
- Added Lobby page skeleton with API polling + Socket.IO hooks: `client/app/src/pages/Lobby.tsx`.
- Added Game Manager page with YAML list, search, and editor logic: `client/app/src/pages/GameManager.tsx`.
- Added Account page with terms acceptance + social settings + YAML list: `client/app/src/pages/Account.tsx`.
- Added Help stub page: `client/app/src/pages/Help.tsx`.
- Switched routing to HashRouter for Electron file:// compatibility.
- Copied `sfx.js` into `client/app/public/assets/sfx.js`.
- Copied game registry into `client/app/src/data/games.generated.json` for Add a Game list.
- Added `/api/me` endpoint for client identity/terms data in `WebHostLib/misc.py`.
- Reworked Lobby to include room status, tracker drawer, terminal command handling, owner settings, release buttons, and seed timer.
- Replaced Social drawer with React state + Socket.IO integration for friends, requests, DMs, and toasts.
- Updated Account page to fetch `/api/me` and show real user/terms data.
- Fixed tracker/download URLs in Lobby to respect API base and added YAML import handling in Account.
- Added `/api/yamls/new` for client-side YAML imports (no prior YAML id required).
- Game Manager import now opens editor with local file content and saves as new Custom YAML.
- Lobby now includes host-status voting, generate/close confirm modals, and owner kick action.
- Adjusted asset paths for Electron file:// (`./assets/...`) and updated SFX paths.
- Added `client/app/.env.example` for `VITE_API_BASE_URL`.
- Socket.IO now uses `withCredentials` for cookie auth.
- Added `client/app/README.md` with Electron dev instructions and CORS note.
- Built AppImage `SekaiLink-client-alpha-0.0.1.AppImage` and copied to `WebHostLib/static/downloads/`.
- Added help page download button for the AppImage.
- Generated a 256x256 PNG icon from `favicon.ico` for the Linux build.

## What I’m doing now
- Verifying Lobby and Social wiring against server endpoints and Socket.IO events.

## What remains
- Validate lobby host-vote flow end-to-end and adjust UI copy if needed.
- Confirm API base URL + cookies with Electron build (Linux) against CORS rules.
- Final audit for missing interactions vs web (context menu, edge cases).

## Notes
- The Electron client is meant to mirror the web UI; server remains source of truth and still serves the web app.
- Admin page is intentionally excluded per scope.
