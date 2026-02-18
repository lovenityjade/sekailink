# Changelog

## 2026-02 (Topaz Sprint)

### UI / UX
- Reworked the client UI to the neon teal/cyan SekaiLink visual language across main pages.
- Added animated panel borders and subtle animated atmospheric background.
- Added styled tabs in lobby panels (`Users`, `Controls`, `Info`) and streamlined right-side layout.
- Replaced placeholder game selection UI with a production carousel (`Previous` / `Next`) in **Game Manager > Add a Game**.
- Added modal-based error surfaces for critical startup/service failures.
- Improved global and lobby chat presentation, scrolling, and persistent toasts.
- Added profile/context menus and cleaned interaction model (left-click status/context, right-click moderation actions).

### Social / Friends
- Implemented friend system wiring across sidebar, social drawer, and lobby context menus.
- Added **Send Friend Request** / **Accept Friend Request** logic in lobby context menu.
- Added matching friend actions in lobby profile modal.
- Synchronized friend state from `/api/social/friends` + `/api/social/requests` with socket refresh hooks.

### Lobby / Room Flow
- Added `Create` access from home filtering area.
- Added solo mode flow from create modal (single-player room flow without room list exposure).
- Added room lock/rejoin behavior foundations and moderated exit handling (kick/ban modal + redirect).
- Added room timer activation from generation completion.
- Added spoiler log visibility controls in room info when enabled.
- Added/expanded moderation actions (kick, ban, release, slow release, block, report).

### Auto-update / Runtime
- Migrated toward self-patching updates (no installer interaction for regular updates).
- Added launch progress and patching status reporting improvements.
- Integrated incremental manifest sync handling on startup.
- Improved runtime packaging and dependency staging for Linux AppImage.

### Emulator / Tracker Integration
- Stabilized PopTracker startup flow and fixed missing runtime dependencies.
- Added SNI bridge improvements for SNES flows and BizHawk connector interoperability.
- Added stronger setup checks and diagnostics around tracker/runtime modules.
- Continued BizHawk simplification work (SekaiLink-oriented UX direction).

### Boxart / Assets
- Integrated SteamGridDB-sourced boxart set for currently supported games.
- Added server-side boxart endpoints:
  - `/api/client/boxart`
  - `/api/client/gamebox`
- Synced local boxart assets into WebHost static hosting and client fallback resolution.

### Audio / Visual Polish
- Integrated SFX/BGM option wiring in settings (mute, per-event, bgm track/volume controls groundwork).
- Added boot sound timing alignment with loading flow.
- Updated loading screen branding to use SekaiLink logo assets.

### Internationalization
- Extended multilingual support across new features (EN, FR, ES, JA, ZH-CN, ZH-TW).
- Added translation keys for new social actions, carousel controls, toasts, and errors.
- Continued player-options i18n groundwork for world-by-world option translation strategy.

### Server / Infra
- Hardened critical API surface for client-only sensitive operations (ongoing).
- Added/maintained VPS services for:
  - WebHost
  - Workers
  - Social bots
  - Local LLM service
- Added legal page groundwork (TOS/Privacy) and web theme alignment with client design.

### Security / Hygiene
- Redacted committed secret/token tracker files and replaced with safe templates.
- Added local-only secret file patterns to `.gitignore`.
- Continued no-secrets logging and release hygiene tasks.

## Notes
- This changelog summarizes the Topaz sprint scope documented in `sekailink-client-plan/` and implemented across client/server during February 2026.
