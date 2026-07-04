# Change Record - Core Access MVP

## Summary

- Title: SekaiLink Core Access bastion MVP
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Public opening preparation; administration cockpit needed STAT.
- Approval: Requested by Jade in-session.

## Purpose

Create the first executable Core Access tool without touching production server
code, Client Core, Sekaiemu, or SKLMI. This MVP gives a bastion-side shell,
command registry, autocompletion, persistent command history, RBAC checks, local
audit JSONL, approval queue primitives, notes, log catalog, service allowlist,
dashboard stubs, and documentation-aligned command names.

## Surfaces touched

- Services: none.
- APIs/events: none implemented in production services.
- Data/DB: none. MVP writes local bastion files under `~/.sekailink/core-access/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds a Linux-oriented Rust CLI/TUI MVP for bastion usage.
- Windows impact: none for this MVP. Future client/runtime sync remains required
  before any client-facing release work.

## Connectivity

- Endpoint/event: none yet.
- Payload: local audit/notes/approval JSONL only.
- Auth/capabilities: bootstrap role via local environment/CLI for MVP; Nexus auth
  integration remains a required follow-up before production trust.
- Frequency: command-driven.
- Retry: none.
- Errors: command handlers return local error messages; no server mutation.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; no production calls.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; local audit/notes files only.

## Rollback

- Rollback command: remove `tools/core-access` and this change record.
- Backup required: no.
- Verification after rollback: `git status --short` and docs validation.

## Tests

- Unit: command parser/RBAC.
- Integration: `cargo test` and basic non-interactive command execution.
- Manual: run `sekailink-core-access --command "server status all"`.
- Linux: required for MVP.
- Windows: not required for this MVP.

## Docs updated in same commit

- `docs/sekailink-core-access/README.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-mvp.md`
- `tools/core-access/README.md`
