# Source Snapshots Warning

These directories are historical or candidate source snapshots, not guaranteed
canonical deployment sources.

Do not deploy live services directly from a snapshot unless it has been
reconciled against `docs/repo-cleanup/LIVE_SERVICE_MAP.md` and explicitly
marked as current.

The 2026-06-25 `sekailink-chat-api` reset-route incident happened because a
live binary was rebuilt from a source tree that looked plausible but did not
match the active deployment behavior.
