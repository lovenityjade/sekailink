# Change Record - Core Access Pack Repo Drafts

## Summary

- Title: SekaiLink Core Access pack repo drafts
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Admin cockpit needs pack repository administration
  primitives without hardcoding any pack.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add local pack repository drafts. Admins can add pack repo records and schedule
pack checks as local scheduler drafts. Service/Admin can list the local pack repo
catalog. Publishing, staging, deletion, and CDN writes remain planned only.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Draft repos are local under
  `~/.sekailink/core-access/pack-repos/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local pack repo draft commands to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local JSONL pack repo and scheduler drafts.
- Auth/capabilities: `pack repo add` and `pack schedule-check` require Admin.
- Frequency: command-driven.
- Retry: none.
- Errors: missing arguments are rejected locally.
- Compatibility: no runtime protocol or client protocol changes.

## Risk

- User risk: low; drafts are not published.
- Server risk: low; no service modification or remote execution.
- Runtime risk: low; no Client Core, Sekaiemu, or SKLMI modification.
- Data risk: low; no production database write.

## Rollback

- Rollback command: revert the commit containing this change.
- Backup required: no.
- Verification after rollback: `cargo test --manifest-path tools/core-access/Cargo.toml`.

## Tests

- Integration: `cargo test`, docs validation, pack repo add/list/schedule smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-pack-repo-drafts.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
