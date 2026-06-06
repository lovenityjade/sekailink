# Change Control - Core Access Signal And Maintenance Drafts

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now creates append-only local drafts for operational commands that
will later connect to a realtime transport:

- `broadcast global|server|lobby|room|role|version|game`
- `client broadcast`
- `client maintenance-banner`
- `client force-update-prompt`
- `client request-relogin`
- `client refresh-lobby`
- `client refresh-room`
- `client request-sklmi-reconnect`
- `client restart-runtime`
- `client clear-cache-request`
- `maintenance enable`
- `maintenance disable`
- `maintenance broadcast`
- `client-banner publish`
- `client-banner rollback`
- `client-banner disable`

Drafts are written under Core Access `drafts/*.jsonl`, included in `ops paths`,
`ops timeline`, and shift handoff exports.

## Safety

No realtime signal is sent. No client dashboard banner config is published. No
production maintenance mode is changed. No Discord or Twitch message is sent.

Sensitive actions require exact confirmation strings, for example:

- `broadcast:global:all`
- `client:request-sklmi-reconnect:<target>`
- `maintenance:<scope>:enable`
- `banner:<slot>:publish`

RBAC still applies before draft creation.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, room server, pack, or Lua logic was modified.
`client request-sklmi-reconnect` is only a Core Access draft and does not call or
restart SKLMI.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "broadcast global Stream maintenance dans 10 minutes --confirm broadcast:global:all"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "client request-sklmi-reconnect certo diagnostic live --confirm client:request-sklmi-reconnect:certo"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "maintenance enable full Release window --confirm maintenance:full:enable"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "client-banner publish 1 --confirm banner:1:publish"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "client refresh-lobby all windows-lobby"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "client restart-runtime certo nope --confirm client:restart-runtime:certo"
cargo run --manifest-path tools/core-access/Cargo.toml -- --command "ops timeline draft"
```

The `client restart-runtime` Service command must be blocked by RBAC.

## Rollback

Code rollback: revert this change-control entry and the Core Access draft
workflow changes.

Operational rollback: no production mutation occurs from this tranche. Supersede
incorrect drafts with a new note or incident record; do not rewrite historical
JSONL.
