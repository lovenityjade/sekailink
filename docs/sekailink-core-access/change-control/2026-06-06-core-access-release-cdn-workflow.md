# Change Control - Core Access Release/CDN Workflow

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now has a local release/CDN workflow:

- `release current`
- `release list`
- `release verify`
- `release verify-cdn`
- `release compare`
- `release publish`
- `release rollback`
- `release schedule`
- `release notes`
- `release audit`

The workflow reads local Client Core update-bundle manifests under
`apps/client-core/release/update-bundles/YYYYMMDD`.

## Safety

`release publish`, `release rollback`, and `release schedule` create local JSONL
drafts only. They do not upload CDN files, do not replace the Link
`client_release_latest.json`, and do not restart `sekailink-chat-api.service`.

Admin commands still require RBAC and exact confirmation:

- `release:<version>:publish`
- `release:<version>:rollback`
- `release:<version>:schedule`

## Connectivity

`release verify-cdn --execute` calls the public HTTPS
`/api/client/release-latest` endpoint. It does not require a token and does not
use private server access.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "release current"
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "release list"
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "release verify latest --fast"
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "release verify-cdn test all"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "release publish 20260605 --confirm release:0.3.1-prebeta3.20260605.13:publish"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "release schedule 20260605 2026-06-07T09:00:00-04:00 --confirm release:0.3.1-prebeta3.20260605.13:schedule"
```

## Rollback

Code rollback: revert this change-control entry and the Core Access release
workflow implementation.

Operational rollback: no production mutation occurs from this tranche. Delete or
supersede unwanted local release drafts in the Core Access audit trail according
to moderation policy; do not edit historical JSONL entries in place.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core runtime, room server, pack logic, or Lua logic
was modified.
