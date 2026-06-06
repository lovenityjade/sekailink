# Change Record - Core Access Server Service Discovery

## Summary

- Title: SekaiLink Core Access server service discovery
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Core Access should use real systemd service names for
  all five SekaiLink servers.
- Approval: Jade authorized read-only SSH service discovery in-session.

## Purpose

Align Nexus, Worlds, Evolution, and Pulse service allowlists with read-only
systemd discovery. This makes `server services`, `health probe`, and log source
plans reflect the actual production service names.

## Surfaces touched

- Services: read-only discovery only; no service changes.
- APIs/events: none.
- Data/DB: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: service allowlists and log mappings now match discovered units.
- Windows impact: none.

## Connectivity

- Endpoint/event: read-only SSH to `nexus-vps`, `worlds-vps`, `evolution-vps`,
  and `pulse-vps` using `systemctl list-units`.
- Payload: service names and active states.
- Auth/capabilities: operator SSH access.
- Frequency: one-time discovery during implementation.
- Retry: none.
- Errors: discovery showed Evolution postfix snapshot units failed/inactive, but
  no changes were made.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; service names are more accurate.
- Server risk: low; read-only SSH commands only.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: none.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit: existing dry-run service tests.
- Integration: `cargo test`, docs validation, health dry-run smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-server-service-discovery.md`
- `docs/sekailink-core-access/change-control/changelog.md`
