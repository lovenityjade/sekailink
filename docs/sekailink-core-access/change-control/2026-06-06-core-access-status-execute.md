# Change Record - Core Access Status Execute Alias

## Summary

- Title: SekaiLink Core Access status execute alias
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Operators naturally expect `server status all` to be
  the live health entry point.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Make `server status <server|all> --execute` use the gated read-only health probe
path. Also make `health probe --execute` default to `all` instead of treating
`--execute` as a server name.

## Surfaces touched

- Services: optional read-only SSH health checks when explicitly gated.
- APIs/events: none.
- Data/DB: local audit only.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: improves operator command ergonomics in the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: same gated SSH health path as previous read-only exec change.
- Payload: read-only health output.
- Auth/capabilities: `--execute` plus `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`.
- Frequency: command-driven.
- Retry: none.
- Errors: printed command status; no automatic retry.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; default remains dry-run/local unless gated.
- Server risk: low; read-only commands only.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, dry-run status smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-status-execute.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
