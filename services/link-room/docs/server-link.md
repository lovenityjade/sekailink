# Link Server

Date: 2026-03-26

## Role

`link` is the realtime host.

It owns:
- Room Server
- Async Room Server
- live lobby runtime
- social realtime
- chatrooms
- private chat daemon foundation
- room gateway
- CDN
- room and service verbose logs

## Logical layout

```text
/opt/sekailink/link/
  room-server/
  lobby-runtime/
  async-room-server/
  social-server/
  cdn/
  admin-agent/
  logs/
  spool/
```

## Design notes

- room semantics should remain anchored to MultiServer behavior during migration
- client-facing protocols may change
- product clients read structured state, not verbose logs
- admin client can stream verbose logs from here

## Data responsibilities

Stored or cached from `link`:
- live room state
- live lobby state and presence
- async room state
- room activity summaries
- room log streams
- social presence

Persistent truth remains in central MySQL where required.

For the updated target topology, that central persistence host is `Nexus`.

## Native room service runtime

The first native deployment/runtime artifacts for `link` now exist in:

- `deploy/link/room-server/`

They include:

- JSON runtime config template
- environment/token template
- `systemd` unit template
- admin-agent service descriptor example
- install/runtime README

The current long-lived service binary is:

- `build-server-core/sekailink_room_server_service`

For first-room smoke and scripted live prep, the companion CLI should also be
available from the same build lane:

- `build-server-core/sekailink_room_server_tcp_cli`

Supported runtime inputs:

- `--config <room_server.json>`
- `--state-file <room_server_state.json>`

The service currently exposes:

- loopback TCP JSON-lines runtime/admin surface
- loopback HTTP inspection surface
- audit/projection restore at startup
- periodic expired-room purge
- structured lifecycle logs on stdout/stderr
- structured runtime state file updates on running/stopping/stopped/failed

Recommended runtime observability paths:

- state:
  - `/opt/sekailink/link/room-server/runtime/room_server_state.json`
- log:
  - `/opt/sekailink/link/logs/room-server/service.log`
- env:
  - `/opt/sekailink/link/room-server/config/room_server.env`

Recommended first ALTTP room-test operator profile:

- `deploy/link/room-server/room-server.alttp-live.profile.env.example`

Recommended first ALTTP room-test config profile:

- live MySQL/Nexus profile:
  - `deploy/link/room-server/room_server.json.example`
- local loopback proof profile:
  - `deploy/link/room-server/room_server.alttp-live.loopback.json.example`

## Current cutover state

The deployed native room server on `link` is the public-facing runtime host.

Cutover status:

- the room server itself is deployed and healthy
- the live TCP mutation path is now restored
- DB/API responsibilities are being moved off `evolution` onto `Nexus`

## TCP stability hardening

The deployed native room server now protects its TCP loop against incomplete or stalled clients.

What changed:

- per-client receive timeout on accepted TCP sockets
- explicit `incomplete_request` response for partial JSON-line clients
- regression coverage for the single-client stall case

Result:

- a malformed or abandoned TCP client no longer wedges the room server until manual restart

## Public edge routing

`link` is also the live public HTTPS entrypoint for `sekailink.com`.

Current public cutover state:

- `/api/identity/` stays publicly stable under `sekailink.com`
- `/api/room/` stays publicly stable under `sekailink.com`
- the target backend for both routes is now `Nexus`
- the public website continues to terminate on the web stack served by `evolution`

This keeps the public hostname stable while letting native C++ server slices replace the old stack incrementally.

## Native admin agent

The first native `link` admin agent is now also deployed as a loopback-only ops surface.

Current runtime path:

- binary:
  - `/opt/sekailink/link/admin-agent/bin/sekailink_admin_agent_service`
- config:
  - `/opt/sekailink/link/admin-agent/config/admin_agent.json`
- systemd:
  - `sekailink-admin-agent.service`

Current private service inventory exposed there:

- `room-server`
- `lobby-runtime`

## Native lobby runtime

The first native live lobby runtime slice on `link` is now deployed.

Current runtime path:

- binary:
  - `/opt/sekailink/link/lobby-runtime/bin/sekailink_lobby_runtime_service`
- config:
  - `/opt/sekailink/link/lobby-runtime/config/lobby_runtime.json`
- systemd:
  - `sekailink-lobby-runtime.service`

Current scope:

- open runtime lobbies
- list runtime lobbies, with bounded filtering for terminal discovery
- runtime lobby info
- presence join/leave
- close runtime lobbies

Current private discovery commands now support:

  - `listruntimelobbies [limit] [query] [visibility] [status] [offset]`
  - `listrooms [limit] [query] [room_type] [connection_state] [offset]`
- `roomevents <room_id> [limit] [event_type] [severity] [offset]`
- `clientreports <room_id> [limit] [report_type] [severity] [source] [offset]`
- `roomsummary <room_id>`
- `expiredrooms <now_utc>`
- `purgeexpiredrooms <now_utc>`
- `setroomallowed <room_id> <slot_csv>`
- `setroomexpires <room_id> <expires_at|none>`
- `setroomdailysummary <room_id> <state|none>`
- `setroomnotifications <room_id> <state|none>`
- `setroomsuspend <room_id> <state|none>`

Current storage/state:

- SQLite:
  - `/opt/sekailink/link/lobby-runtime/data/lobby_runtime.sqlite3`
- state file:
  - `/opt/sekailink/link/lobby-runtime/data/lobby_runtime_state.json`

This is the first live bridge between:

- private lobby-control on `Nexus`
- realtime lobby presence/runtime on `link`

## Private chat daemon foundation

`link` now has a loopback-only InspIRCd-based daemon installed as:

- `sekailink-chat-daemon.service`

It also has the first loopback-only SekaiLink chat gateway installed as:

- `sekailink-chat-gateway.service`

And the public-edge-safe chat API adapter installed as:

- `sekailink-chat-api.service`

See `docs/chat-daemon.md`.

This is not public yet. It is a secure foundation for the SekaiLink chat/auth integration.

Current state source:

- `/opt/sekailink/link/room-server/runtime/room_server_state.json`

## Outbound mail edge

`link` is also the current outbound mail edge for SekaiLink.

Current mail role:

- receives SMTP relays from internal SekaiLink services
- delivers application mail to the Internet
- signs outbound mail for `sekailink.com` with DKIM on the real outbound host
- aligns outbound HELO as `mail.sekailink.com`
