# Change Record - Core Access Incident Snapshots

## Summary

- Title: SekaiLink Core Access incident snapshots
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Public opening preparation; admins need a quick way to
  preserve operational context during live debugging.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add `ops snapshot <label>`, a local Markdown snapshot containing the dashboard,
log sources, service allowlist, recent audit lines, notes, and approval queue.
This supports live incident handoff, paper notes, and postmortem preparation.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Snapshot files are local exports.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local snapshot command to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local Markdown export.
- Auth/capabilities: same local bootstrap role as the MVP.
- Frequency: command-driven.
- Retry: none.
- Errors: missing local logs/notes are represented as empty sections.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; snapshots are local.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, snapshot smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-incident-snapshots.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/commands/registry.txt`
- `tools/core-access/README.md`
