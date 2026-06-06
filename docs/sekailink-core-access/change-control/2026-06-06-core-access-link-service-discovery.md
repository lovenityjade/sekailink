# Change Record - Core Access Link Service Discovery

## Summary

- Title: SekaiLink Core Access Link service discovery
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Live read-only health probe found stale Link service
  names in the Core Access allowlist.
- Approval: Jade authorized read-only SSH service discovery in-session.

## Purpose

Align the Core Access Link service allowlist with real `link-vps` systemd units.
The logical log source `link:room-runtime` remains stable for operators, but it
now maps to `sekailink-room-server.service`. Health probe output also labels
each service status line to make live readings usable in a cockpit view.

## Surfaces touched

- Services: read-only discovery only; no service changes.
- APIs/events: none.
- Data/DB: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: Link dry-run/read-only plans use real systemd service names.
- Windows impact: none.

## Connectivity

- Endpoint/event: read-only SSH to `link-vps` using `systemctl list-unit-files`
  and `systemctl list-units`.
- Payload: service names and active states.
- Auth/capabilities: operator SSH access.
- Frequency: one-time discovery during implementation.
- Retry: none.
- Errors: none; discovery showed `sekailink-llama` failed and
  `sekailink-social-bots` auto-restarting, but no changes were made.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; allowlist now reflects production names.
- Server risk: low; read-only SSH commands only.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: none.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit: existing dry-run service tests.
- Integration: `cargo test`, docs validation, read-only health dry-run/smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-link-service-discovery.md`
- `docs/sekailink-core-access/change-control/changelog.md`
