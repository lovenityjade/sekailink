# Client Notes

## Update Checks (re-enabled)
- The client now enforces `min_supported` from `/api/client/version`.
- Update availability is shown in-app with a persistent update icon and startup notice.
- Update downloads run in-app via Electron IPC with progress events (`updater:event`).
- On Windows/Linux, downloaded client binaries are now applied in-place (self-replace) and the client restarts automatically (no external installer prompt).
- Startup can auto-download/apply/restart when a newer client version is available.
- Incremental sync is available through `updater:syncIncremental` using a server manifest.
- Incremental updates are constrained to overlay-safe paths:
  - `runtime/...`
  - `ap/...`
- Runtime/AP overlay root:
  - `<userData>/runtime/overlay`
- Update notes modal reads `update_notes_url` (Markdown) and shows once per `update_notes_version` (or content hash fallback).
- Server contract required for best UX:
  - `latest`
  - `min_supported`
  - `download_url`
  - optional `notes`
  - optional `sha256` (64 hex chars)
  - optional `incremental_manifest_url`
  - optional `update_notes_url`
  - optional `update_notes_version`
  - optional `update_notes_title`
