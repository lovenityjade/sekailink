# Room Server First External Test

Date: 2026-05-05

## Goal

Prepare one coherent first external ALTTP live test around the native link room
server without pretending that this repo already ships a full SKLMI or public
edge integration.

## Canonical target

Current clean-room target assumptions:

- logical host: `link`
- expected hostname: `link.sekailink.com`
- room-server root: `/opt/sekailink/link/room-server`
- room-server logs: `journalctl -u sekailink-room-server.service`
- native bindings stay loopback-only:
  - TCP `127.0.0.1:32071`
  - HTTP `127.0.0.1:18081`

An adjacent live service already exists on the same host:

- `sekailink-lobby-runtime.service`
- loopback HTTP `127.0.0.1:19097`

That lobby runtime is useful for same-host coordination, but it is not a hard
dependency for the first room-server-only ALTTP test described here.

## Runtime inputs that are really consumed

Consumed by `sekailink_room_server_service` today:

- `--config <room_server.json>`
- `--state-file <room_server_state.json>`
- env:
  - `SEKAILINK_ROOM_SERVER_ADMIN_TOKEN`
  - `SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN`
  - `SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN`
  - `SEKAILINK_MYSQL_*` when `projection_backend=mysql`

Recommended first-live-test config profile:

- active live profile:
  - `deploy/link/room-server/room_server.json.example`
- local proof profile:
  - `deploy/link/room-server/room_server.alttp-live.loopback.json.example`

Recommended operator profile:

- `deploy/link/room-server/room-server.alttp-live.profile.env.example`

That profile is intentionally an operator helper, not a binary input file.

## SKLMI / ALTTP bridge reality

What this repo can prepare today:

- room creation and runtime mutation flow over TCP
- room inspection over HTTP
- structured room event history
- projection persistence suitable for downstream polling or replay

What this repo does not implement today:

- a direct SKLMI transport inside this repo
- a public TCP/HTTP edge
- an ALTTP runtime launcher or emulator wrapper

So the first live bridge should be treated like this:

1. ALTTP runtime or wrapper sends room mutations to the private TCP surface.
2. Operators inspect room state through private HTTP.
3. SKLMI or another downstream consumer reads either:
   - the private HTTP admin inspection surface
   - or the shared MySQL projection path read by Nexus room-query

The current live topology is stronger than that older assumption:

- `Link Room` writes to the MySQL projection target `sekailink_room_projection`
- `Nexus room-query` reads from that same target to expose official room data

So the preferred first live bridge is:

`runtime -> Link Room TCP -> MySQL projection -> Nexus room-query`

This keeps the live test aligned with the official room-information path.

## First deployable test pack

The repo now ships a small operator bundle under `deploy/link/room-server/`:

- `room_server.json.example`
- `room_server.alttp-live.loopback.json.example`
- `room_server.env.example`
- `room-server.alttp-live.profile.env.example`
- `sekailink-room-server.service`
- `send-room-command.sh`
- `check-private-live-readiness.sh`
- `stage-private-live-host.sh`
- `prepare-live-test-layout.sh`
- `payloads/*.command.json`

Use `prepare-live-test-layout.sh` to stage a local deployment tree without
touching the real host yet.

## First room mutation flow

Use `send-room-command.sh` with the prepared command files instead of hand-built
JSON quoting.

Examples:

```bash
bash deploy/link/room-server/send-room-command.sh \
  --channel admin \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/create-room.alttp-live.command.json

bash deploy/link/room-server/send-room-command.sh \
  --channel runtime \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/set-slot-data.alttp-live.command.json
```

The helper now infers the token env variable from the channel and can derive the
loopback TCP host:port directly from `room_server_state.json`.

Before any tester handoff, validate the private SKLMI-facing runtime command
surface end to end:

```bash
bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json
```

That confirms the live daemon can currently handle:

- `issue_ticket`
- `runtime_event`
- `pending_items`
- `acknowledge_delivery`

Before handing anything to external testers, run the private-live readiness
check on the host:

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --systemd-unit sekailink-room-server.service
```

For non-destructive host-side staging before any rollout discussion:

```bash
bash deploy/link/room-server/stage-private-live-host.sh \
  --profile nexus-live \
  --room-root /opt/sekailink/link/room-server
```

## What external testers should receive

For a first external test, do not hand testers the private loopback endpoints or
admin token.

They should receive only:

- one public TCP host:port that forwards to the room-server loopback TCP port
- one public HTTPS base URL if HTTP inspection is intentionally exposed
- one room identifier
- one test seed description
- the exact client/runtime build they should run

Operators keep:

- admin token
- raw logs
- state file
- shared MySQL projection target read by Nexus room-query
- direct loopback access

Operators should also keep one ready-to-run room-focused log command:

```bash
bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

## Exit criteria for a usable first test

- room server starts cleanly on `link`
- `/health` is healthy on loopback
- one ALTTP room can be created with seed metadata
- `set_slot_data` and `enqueue_received_item` produce both logs and room events
- at least one downstream consumer can confirm the room through HTTP or through
  the official Nexus room-query path backed by the shared MySQL projection
- the public edge can be disabled quickly without touching the private daemon
