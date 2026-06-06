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
