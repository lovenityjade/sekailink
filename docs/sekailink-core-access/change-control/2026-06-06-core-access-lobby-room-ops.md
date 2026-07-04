# Change Control - Core Access Lobby And Room Ops

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now supports the lobby/room operator cockpit commands:

- `lobby list`
- `lobby open`
- `lobby create`
- `lobby edit`
- `lobby close`
- `lobby lock`
- `lobby unlock`
- `lobby chat`
- `lobby join-secret`
- `lobby broadcast`
- `room list`
- `room open`
- `room summary`
- `room events`
- `room logs`
- `room snapshot`
- `room pending-items`
- `room client-reports`
- `room sync`
- `room request-sklmi-reconnect`
- `room disconnect-runtime`
- `room give-item`

## Connected Routes

Lobby read-only and governed mutations use Nexus lobby-admin:

- `GET /admin/lobbies`
- `GET /admin/lobbies/{lobby_id}`
- `POST /admin/lobbies`
- `PATCH /admin/lobbies/{lobby_id}`
- `POST /admin/lobbies/{lobby_id}/close`

Room read-only commands use Nexus room-query:

- `GET /rooms`
- `GET /rooms/{room_id}`
- `GET /rooms/{room_id}/diagnostics`
- `GET /rooms/{room_id}/events`
- `GET /rooms/{room_id}/client-reports`

Execution gates:

- read-only routes require `--execute`,
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`, and the matching Nexus token env;
- lobby mutations require role policy, `--execute`,
  `SEKAILINK_CORE_ACCESS_NEXUS_MUTATION=1`, the lobby token env, and exact
  confirmation;
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` remains the generic fallback.

## Draft-Only Workflows

The following commands write append-only local drafts and do not mutate live
runtime state:

- `lobby lock`
- `lobby unlock`
- `lobby join-secret`
- `lobby broadcast`
- `room sync`
- `room request-sklmi-reconnect`
- `room disconnect-runtime`
- `room give-item`

`room request-sklmi-reconnect` is only a signal draft. No SKLMI code, protocol,
Lua, pack, or runtime behavior was changed.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "lobby list"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "lobby create lobby-alpha Alpha public jade --confirm lobby:lobby-alpha:create"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "room list 25 alttp"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "room summary room-alpha"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "room request-sklmi-reconnect room-alpha player1 --confirm room:room-alpha:request-sklmi-reconnect"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "room give-item room-alpha player1 Hookshot --confirm room:room-alpha:give-item"
```

## Rollback

Code rollback: revert this change-control entry, lobby/room command dispatch,
Nexus lobby/room plan helpers, command registry readiness, and docs.

Operational rollback: live lobby mutations require execution gates. Draft-only
room/lobby records can be superseded by new drafts or incident notes; do not
rewrite historical JSONL.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, pack, or Lua logic was modified.
