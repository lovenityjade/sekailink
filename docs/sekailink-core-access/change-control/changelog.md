# Core Access Change-Control Changelog

## Unreleased

- Initial documentation and governance for SekaiLink Core Access.
- Added local-only Rust MVP under `tools/core-access` with command registry,
  RBAC gates, persistent command history, local JSONL audit, notes, approval
  queue primitives, log catalog and local dashboard.
- Added bounded local exports for Core Access audit and note JSONL.
- Added dry-run plans for `server logs`, `logs tail`, and `health probe`.
- Aligned dry-run SSH output with documented `*-vps` bastion aliases.
- Added local Markdown incident snapshots with dashboard, log catalog, services,
  recent audit, notes, and approval queue.
- Added local draft management for the three client dashboard banner slots.
- Added local maintenance schedule/status/history drafts.
- Added local scheduler draft add/list/calendar/history commands.
- Added local pack repository drafts and pack check scheduler draft command.
- Added explicit `--execute` plus `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`
  gate for read-only remote log and health commands.
- Aligned Link service allowlist with read-only `link-vps` systemd discovery and
  labelled service status output in health probes.
- Aligned Nexus, Worlds, Evolution, and Pulse service allowlists with read-only
  systemd discovery.
- Added `server status <server|all> --execute` as a gated live health alias.
- Normalized child `TERM` for gated read-only remote execution to reduce noisy
  terminal initialization output.
- Added Nexus read-only wrapper plans for `nexus services`, `lobby list`,
  `lobby open`, and Identity user command planning.
- Connected Identity user read-only commands to the confirmed Nexus Identity
  HTTP admin GET routes with token-gated execution.
- Added the first full-screen terminal cockpit with panels, command line,
  autocomplete/history, F1-F12 hotkeys, and `--shell` fallback.
- Added local log evidence workflow commands for search/filter dry-run plans,
  local pins, log notes, and Markdown/JSONL/TXT evidence exports.
- Added local incident workspace commands for append-only open/list/status,
  notes, pins, exports, and close workflow.
- Added local ops timeline and shift handoff Markdown export for moderator/admin
  relay between shifts.
