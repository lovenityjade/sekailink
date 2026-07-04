# Change Record - Core Access SSH Alias Alignment

## Summary

- Title: SekaiLink Core Access SSH alias alignment
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Dry-run ops plans should match the server aliases from
  the BETA-3 handoff.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Keep human command targets short (`link`, `nexus`, `worlds`, `evolution`,
`pulse`) while rendering dry-run SSH plans with the documented bastion aliases
(`link-vps`, `nexus-vps`, `worlds-vps`, `evolution-vps`, `pulse-vps`).

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: dry-run command text now matches expected SSH aliases.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: rendered command plans only.
- Auth/capabilities: unchanged.
- Frequency: command-driven.
- Retry: none.
- Errors: unchanged.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; output is advisory.
- Server risk: low; no SSH or service mutation is performed.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: none.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Unit: dry-run log plan now expects `link-vps`.
- Integration: `cargo test` and dry-run smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-ssh-aliases.md`
- `docs/sekailink-core-access/change-control/changelog.md`
