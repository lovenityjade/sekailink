# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access Identity read-only wrapper
- Author: Codex SekaiLink Linux
- Related issue/incident: Core Access user administration needed the live Nexus
  Identity admin endpoint before user search/detail/session/device/audit commands
  could execute safely.

## Summary

Connect read-only Core Access user commands to the live private Nexus Identity
HTTP admin surface:

- `user search`
- `user open`
- `user sessions`
- `user devices`
- `user audit`

The live endpoint was confirmed by source and read-only host probing:

- Service: `sekailink-nexus-identity.service`
- Bound address: `149.202.61.90:19095`
- Health: `GET /health`
- Auth: `Authorization: Bearer <admin token>`
- Read-only admin routes return `401 unauthorized` without a token, confirming
  route presence without exposing secrets.

## Connectivity Contract

- User search:
  - Method: `GET`
  - Path: `/admin/users`
  - Query: `limit`, `query`, `role`, `state`, `offset`
- User detail:
  - Method: `GET`
  - Path: `/admin/users/{username}`
- User sessions:
  - Method: `GET`
  - Path: `/admin/users/{username}/sessions`
- User devices:
  - Method: `GET`
  - Path: `/admin/users/{username}/devices`
- User audit:
  - Method: `GET`
  - Path: `/admin/users/{username}/audit`
  - Query: `limit`, `event_type`, `offset`

## Safety

- Dry-run remains default.
- Live execution requires:
  - command flag: `--execute`
  - environment gate: `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`
  - token environment variable:
    `SEKAILINK_CORE_ACCESS_NEXUS_IDENTITY_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` remains a compatibility fallback.
- Token values are never printed.
- Mutating user commands remain unimplemented.
- User detail/session/device/audit GET routes append admin audit records on the
  Identity service by design.

## Platform Impact

- Linux impact: Core Access command additions only.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert this Core Access commit. No account/lobby/room state is changed by the
wrapper. Identity admin audit entries created by successful read-only requests
are durable audit records and are not rolled back.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke dry-runs:
  - `user search certo`
  - `user open certo`
  - `user sessions certo`
  - `user devices certo`
  - `user audit certo`
- Smoke gated execution without token:
  - `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1 user search certo --execute`
  - Expected: blocked for missing token, no HTTP request executed.

## Files

- `tools/core-access/src/nexus.rs`
- `tools/core-access/src/app.rs`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-identity-readonly-wrapper.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/06-users-and-configs.md`
- `docs/sekailink-core-access/15-command-reference.md`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
