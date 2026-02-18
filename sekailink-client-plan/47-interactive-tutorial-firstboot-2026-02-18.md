# 47) Interactive Tutorial (First Boot)

Date: 2026-02-18

## Scope
- Add an interactive tutorial flow after the loading screen.
- Gate by a DB user flag:
  - `tutorial=true` => show tutorial.
  - tutorial skipped/completed => set `tutorial=false`.

## Client Changes
- Added `InteractiveTutorial` modal wizard:
  - `client/app/src/ui-prototype/ui/components/InteractiveTutorial.tsx`
- Integrated tutorial trigger in auth/boot gate:
  - `client/app/src/ui-prototype/ui/auth/AuthGatePrototype.tsx`
  - Uses `/api/me` to read `tutorial`.
  - Calls `POST /api/me/tutorial` with `{"tutorial": false}` on skip/finish.
- Added tutorial styling:
  - `client/app/src/ui-prototype/styles/overrides.css`
- Added i18n keys (all supported locales):
  - `client/app/src/i18n/locales/en.json`
  - `client/app/src/i18n/locales/fr.json`
  - `client/app/src/i18n/locales/es.json`
  - `client/app/src/i18n/locales/ja.json`
  - `client/app/src/i18n/locales/zh-CN.json`
  - `client/app/src/i18n/locales/zh-TW.json`

## Server/VPS Changes
- Updated user model with tutorial fields:
  - `tutorial` (bool, default true)
  - `tutorial_completed_at` (datetime, nullable)
- Updated `/api/me` response to include `tutorial`.
- Added endpoint:
  - `POST /api/me/tutorial`
  - Requires auth.
  - Accepts `tutorial` bool (defaults to false if omitted/invalid).
  - Updates `tutorial` and `tutorial_completed_at`.

## Database Migration (VPS)
- MariaDB `sekailink.user`:
  - add `tutorial TINYINT(1) NOT NULL DEFAULT 1`
  - add `tutorial_completed_at DATETIME NULL`
  - initialize all users with `tutorial=1`

## Validation
- Client build succeeds.
- Webhost restarts cleanly.
- `POST /api/me/tutorial` returns `401` unauthenticated (route exists and auth enforced).
