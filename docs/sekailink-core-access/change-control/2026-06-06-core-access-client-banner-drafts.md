# Change Record - Core Access Client Banner Drafts

## Summary

- Title: SekaiLink Core Access client banner drafts
- Date: 2026-06-06
- Author: Codex
- Related issue/incident: Admin cockpit needs a safe way to prepare dashboard
  banner/publicity copy before production publishing exists.
- Approval: Requested by Jade in-session as part of the Core Access STAT build.

## Purpose

Add local draft management for the three SekaiLink client dashboard banner slots.
Admins can edit local drafts and Service can list/preview them. Publishing to
the client-facing service remains intentionally unimplemented until the Nexus
integration and publish contract are documented.

## Surfaces touched

- Services: none.
- APIs/events: none.
- Data/DB: none. Draft files are local under
  `~/.sekailink/core-access/client-banners/`.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.
- Linux impact: adds local banner draft commands to the bastion MVP.
- Windows impact: none.

## Connectivity

- Endpoint/event: none.
- Payload: local draft text and local JSONL history only.
- Auth/capabilities: `client-banner edit` requires Admin in the MVP RBAC.
- Frequency: command-driven.
- Retry: none.
- Errors: invalid slots outside 1-3 are rejected.
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

- Unit: banner slot validation.
- Integration: `cargo test`, docs validation, banner list/edit/preview smoke.

## Docs updated in same commit

- `docs/sekailink-core-access/change-control/2026-06-06-core-access-client-banner-drafts.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `tools/core-access/README.md`
