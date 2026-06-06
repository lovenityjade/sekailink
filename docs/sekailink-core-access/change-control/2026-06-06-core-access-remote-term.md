# Change Record - Core Access Remote TERM

## Summary

- Title: SekaiLink Core Access remote TERM normalization
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Read-only SSH smoke showed noisy `TERM=dumb` starship
  output before health results.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Set a non-dumb `TERM` for child shell execution when Core Access runs gated
read-only remote commands. This keeps health/status output cleaner in terminals
where the parent process runs with `TERM=dumb`.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: cleaner Core Access read-only remote output.
- Windows impact: none.

## Connectivity

- Endpoint/event: unchanged gated SSH path.
- Payload: unchanged read-only command output.
- Auth/capabilities: unchanged.
- Frequency: command-driven.
- Retry: none.
- Errors: unchanged.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; only child process environment changes.
- Server risk: low; read-only commands only.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: none.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, read-only status smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-remote-term.md`
- `docs/sekailink-core-access/change-control/changelog.md`
