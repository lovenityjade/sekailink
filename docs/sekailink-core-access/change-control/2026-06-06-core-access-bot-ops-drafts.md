# Change Control - Core Access Bot Ops Drafts

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now has first-pass Discord and Twitch cockpit commands:

- `discord status`
- `discord logs`
- `discord reload`
- `discord announce`
- `discord sync-roles`
- `discord command list|enable|disable|edit`
- `discord timer list|edit`
- `discord incident-post`
- `twitch status`
- `twitch logs`
- `twitch connect|disconnect`
- `twitch announce`
- `twitch command list|enable|disable|edit`
- `twitch timer list|edit`
- `twitch lobby announce`
- `twitch stream set-title-hint`

## Safety

Only `status` and `logs` can use remote read-only execution, and only with the
existing `--execute` plus `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1` gate.

All bot-changing commands create append-only local drafts under
`~/.sekailink/core-access/drafts/`. No Discord API, Twitch API, Nexus bot config
mutation, bot process reload, role sync, channel connection, timer edit, command
edit, or announcement is executed.

Exact confirmations are required for sensitive drafts, for example:

- `discord:announce:<channel>`
- `discord:command:<name>:edit`
- `discord:timer:<id>:edit`
- `discord:incident-post:<incident>`
- `twitch:connect:<channel>`
- `twitch:announce:<channel>`
- `twitch:command:<name>:edit`
- `twitch:timer:<id>:edit`
- `twitch:lobby:<lobby>:announce`

## Connectivity

New log sources:

- `discord:bot` maps to Link service `sekailink-social-bots`
- `twitch:assistant` maps to Link service `sekailink-twitch-assistant`

These are read-only journalctl plans through the existing Link server allowlist.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "discord status"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "discord logs"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "discord announce general hello --confirm discord:announce:general"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "twitch connect sekailink --confirm twitch:connect:sekailink"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "twitch stream set-title-hint sekailink Pre-beta live support"
```

## Rollback

Code rollback: revert this change-control entry, bot command dispatch, bot
command registry readiness, and the two bot log source mappings.

Operational rollback: no production mutation occurs from this tranche. Supersede
incorrect drafts with a new draft or incident note; do not rewrite historical
JSONL.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, room server, pack, or Lua logic was modified.
