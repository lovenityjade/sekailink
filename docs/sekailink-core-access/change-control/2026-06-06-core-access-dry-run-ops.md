# Change Record - Core Access Dry-Run Ops Plans

## Summary

- Title: SekaiLink Core Access dry-run ops plans
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Public opening preparation; admins need safe command
  routing for logs and health checks before remote execution is enabled.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add dry-run operational plans for remote log tails and server health probes.
Core Access can now validate known servers/services and print the SSH/journalctl
commands that a future remote executor will run, without making any network
connection or changing any production state.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds dry-run command planning to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: rendered command plans only.
- Auth/capabilities: same local bootstrap role as the MVP.
- Frequency: command-driven.
- Retry: none.
- Errors: unknown servers, services, or sources are rejected locally.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; command output is advisory.
- Server risk: low; no SSH or service mutation is performed.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; audit remains local JSONL only.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit: allowlisted service planning and rejection.
- Integration: `cargo test`, docs validation, dry-run command smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-dry-run-ops.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
