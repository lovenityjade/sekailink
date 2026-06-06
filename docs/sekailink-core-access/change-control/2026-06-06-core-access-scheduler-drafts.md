# Change Record - Core Access Scheduler Drafts

## Summary

- Title: SekaiLink Core Access scheduler drafts
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Admin cockpit needs local scheduling primitives for
  release, maintenance, pack checks, and operational reminders.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add local scheduler draft commands. Admins can add a scheduled job draft, while
Service/Admin can list and view the calendar/history. No job is executed
automatically in this MVP.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Draft jobs are local under
  `~/.sekailink/core-access/scheduler/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local scheduler draft commands to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local JSONL schedule drafts.
- Auth/capabilities: `schedule add` requires Admin in the MVP RBAC.
- Frequency: command-driven.
- Retry: none.
- Errors: missing arguments are rejected locally.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; jobs are not executed.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, schedule add/list/calendar smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-scheduler-drafts.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
