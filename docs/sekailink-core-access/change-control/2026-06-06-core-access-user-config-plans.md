# Change Control - Core Access User Config Plans

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now handles user seed-config administration commands:

- `user configs`
- `user config open`
- `user config export`
- `user config diff`
- `user config edit`

## Connected Routes

The read/export commands use the Nexus seed-config API contract documented in
`services/nexus/docs/SEED_CONFIGS.md`:

- `GET /users/{user_id}/seed-configs`
- `POST /users/{user_id}/seed-configs/{config_id}/export-yaml`

The seed-config API currently expects a numeric Nexus `user_id`. Operators
should use `user open <username>` to resolve a username before reading configs.

Execution is gated by:

- `--execute`
- `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`
- `SEKAILINK_CORE_ACCESS_NEXUS_SEED_CONFIG_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` fallback

## Draft-Only Workflows

These commands are local drafts only:

- `user config diff <user_id> <config_id> <version>`
- `user config edit <user_id> <config_id> key=value ...`

No canonical Nexus config values are changed. Edit drafts require the exact
confirmation:

```text
user-config:<user_id>:<config_id>:edit
```

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "user configs 42 alttp"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "user config open 42 7"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "user config export 42 7 --format yaml"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "user config diff 42 7 6"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "user config edit 42 7 name=Race --confirm user-config:42:7:edit"
```

## Rollback

Code rollback: revert this change-control entry, the user config command
dispatch, Nexus seed-config plan helpers, command registry readiness, and docs.

Operational rollback: no live mutation occurs from this tranche. Supersede
incorrect diff/edit drafts with a new draft or incident note; do not rewrite
historical JSONL.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, room server, pack, or Lua logic was modified.
