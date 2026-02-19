# SekaiLink Update

## New this patch
- Incremental updater foundation is now enabled.
- The client can sync changed runtime/AP files from the server manifest.
- Update hub now includes runtime sync status and manual resync.

## Client UX
- Added startup update notes modal (this file).
- The modal supports Markdown rendering and themed scrolling.
- Notes are shown once per `CLIENT_UPDATE_NOTES_VERSION`.

## For players
- Full installer updates still work for core app changes.
- Runtime content (world connectors, wrappers, support files) can now be shipped as incremental file updates.

## For operators
- Manifest endpoint: `/api/client/incremental-manifest`.
- Notes source: `/static/UPDATE.md`.
