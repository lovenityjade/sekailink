# Change Record - Core Access Read-Only Remote Exec Gate

## Summary

- Title: SekaiLink Core Access read-only remote exec gate
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Admin cockpit needs a safe path from dry-run plans to
  real read-only log/health commands.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add an explicit read-only remote execution gate for log and health commands.
`server logs`, `logs tail`, and `health probe` remain dry-run by default. They
only execute when the operator passes `--execute` and sets
`SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`.

## Surfaces touched

- Services: read-only SSH/journalctl/health command execution can be invoked by
  the operator when explicitly enabled.
- APIs/events: none.
- Data/DB: local audit only.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds gated read-only remote execution to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: SSH aliases from the BETA-3 handoff (`*-vps`).
- Payload: read-only command stdout/stderr in the operator terminal.
- Auth/capabilities: local MVP role plus explicit environment gate.
- Frequency: command-driven.
- Retry: none.
- Errors: command exit status is printed; no automatic retry.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: medium-low; commands can stream logs but do not mutate state.
- Server risk: low; no restart, write, deploy, or database mutation.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; remote output is shown in terminal only unless separately exported.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit/integration: `cargo test`, docs validation, dry-run smoke. Live SSH smoke
  requires operator authorization and server reachability.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-readonly-remote-exec.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
