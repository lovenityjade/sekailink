# Change Record - Core Access Maintenance Drafts

## Summary

- Title: SekaiLink Core Access maintenance drafts
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Admin cockpit needs maintenance planning before
  production maintenance signaling is connected.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add local maintenance draft planning. Admins can schedule a maintenance draft,
and Service/Admin can view current local status and history. Production
maintenance enable/disable/broadcast remains intentionally unimplemented until
the Nexus and client signal contracts are documented.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Draft files are local under
  `~/.sekailink/core-access/maintenance/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local maintenance draft commands to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local draft state and JSONL history.
- Auth/capabilities: `maintenance schedule` requires Admin in the MVP RBAC.
- Frequency: command-driven.
- Retry: none.
- Errors: missing arguments are rejected locally.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; drafts are not broadcast or enforced.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, maintenance schedule/status/history smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-maintenance-drafts.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
