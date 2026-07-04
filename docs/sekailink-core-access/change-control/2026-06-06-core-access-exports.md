# Change Record - Core Access Local Exports

## Summary

- Title: SekaiLink Core Access local exports
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Public opening preparation; admins need quick audit and
  note extraction during live incidents.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add local export commands for Core Access audit and notes. The goal is to let an
operator extract filtered JSONL into a file immediately during an incident,
without touching production services or the Nexus database.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Exports write local files under
  `~/.sekailink/core-access/exports/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local export commands to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local JSONL audit/note lines.
- Auth/capabilities: same local bootstrap role as the MVP.
- Frequency: command-driven.
- Retry: none.
- Errors: missing source logs produce empty/export messages.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; exports are local and bounded to the Core Access data dir.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit: export path sanitization.
- Integration: `cargo test`, docs validation, and non-interactive export smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-exports.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/commands/registry.txt`
- `tools/core-access/README.md`
