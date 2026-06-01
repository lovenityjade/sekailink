# Room Server ALTTP Live Runbook

Date: 2026-05-05

## Goal

Run one loopback-native ALTTP room test with:

- readable room-server logs
- visible seed metadata propagation
- visible item receive flow
- clear health and inspection checkpoints

This runbook stays inside `sekailink-link-room` assumptions. It does not define
the external proxy/tunnel layer.

## Required binaries

Current build lane should already provide:

- `sekailink_room_server_service`
- `sekailink_room_server_tcp_cli`

Prepared helper assets in this repo:

- `deploy/link/room-server/send-room-command.sh`
- `deploy/link/room-server/payloads/*.command.json`
- `deploy/link/room-server/room-server.alttp-live.profile.env.example`

Useful runtime files:

- room server config:
  - `/opt/sekailink/link/room-server/config/room_server.json`
- env tokens:
  - `/opt/sekailink/link/room-server/config/room_server.env`
- runtime state:
  - `/opt/sekailink/link/room-server/runtime/room_server_state.json`
- service log:
  - `journalctl -u sekailink-room-server.service`

## 1. Start and verify the daemon

```bash
systemctl restart sekailink-room-server.service
curl http://127.0.0.1:18081/health
jq . /opt/sekailink/link/room-server/runtime/room_server_state.json
journalctl -u sekailink-room-server.service -n 20 --no-pager
```

Healthy hints:

- `/health` returns `200`
- state file `status` is `running`
- service log shows `room_server_service_bootstrap`
- service log shows `room_server_service_started`

If you need a review-only staging bundle before replaying these steps on the
live host, create it first with:

```bash
bash deploy/link/room-server/stage-private-live-host.sh \
  --profile nexus-live \
  --room-root /opt/sekailink/link/room-server
```

Export the effective loopback ports from the state file before sending traffic:

```bash
ROOM_TCP_PORT="$(jq -r '.effective_tcp_port' /opt/sekailink/link/room-server/runtime/room_server_state.json)"
ROOM_HTTP_PORT="$(jq -r '.effective_http_port' /opt/sekailink/link/room-server/runtime/room_server_state.json)"
```

## 2. Create one ALTTP room

```bash
bash deploy/link/room-server/send-room-command.sh \
  --channel admin \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/create-room.alttp-live.command.json
```

Expected log clue in `journalctl`:

- `room_server_tcp_command`
- `cmd=create_room`
- `seed_name`
- `seed_id`
- `seed_hash`
- `tracker_pack`
- `tracker_variant`

## 3. Register runtime heartbeat

```bash
sekailink_room_server_tcp_cli \
  --host 127.0.0.1 \
  --port "${ROOM_TCP_PORT}" \
  --payload '{"channel":"runtime","auth_token":"'"${SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN}"'","command":{"cmd":"runtime_heartbeat","room_id":"alttp-live-1","runtime_kind":"sklmi","runtime_session_name":"alttp-live-1","driver_instance_id":"sklmi-driver-1","linkedworld_id":"alttp","core_profile":"snes_v1","client_name":"sekailink_sklmi_runtime","client_version":"0.1","connected":true}}'
```

Expected visibility:

- `journalctl` contains `cmd=runtime_heartbeat`
- the same log line carries `driver_instance_id`, `linkedworld_id`, `core_profile`
- `/rooms/alttp-live-1/summary` shows `runtime_connected=true`

## 4. Push seed/runtime metadata into `slot_data`

```bash
bash deploy/link/room-server/send-room-command.sh \
  --channel runtime \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/set-slot-data.alttp-live.command.json
```

Expected visibility:

- `journalctl` contains `room_server_tcp_command` with `cmd=set_slot_data`
- the same log line carries `slot_data_keys`
- `/rooms/alttp-live-1/events` later includes `slot_data_updated`

## 5. Simulate one received item

```bash
bash deploy/link/room-server/send-room-command.sh \
  --channel runtime \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/enqueue-item.alttp-live.command.json
```

Expected visibility:

- `journalctl` contains `cmd=enqueue_received_item`
- that log line carries `item_name`, `location_id`, `sender_alias`
- room event stream includes `item_received`

## 6. Simulate one checked location

```bash
bash deploy/link/room-server/send-room-command.sh \
  --channel runtime \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/record-check.alttp-live.command.json
```

Expected visibility:

- `journalctl` contains `cmd=record_check`
- the room context on later TCP logs shows rising `checked_count`

## 7. Inspect the room over HTTP

```bash
curl -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/snapshot"

curl -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/sync"

curl -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/events"

curl -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${ROOM_HTTP_PORT}/rooms/alttp-live-1/summary"
```

Useful things to confirm:

- snapshot contains `seed_name`, `seed_id`, `seed_hash`, `tracker_pack`, `tracker_variant`
- snapshot contains `slot_data`
- sync contains `runtime_state`
- sync contains `items.next_index`
- sync contains `checked_locations` and `missing_locations`
- events contain `slot_data_updated` and `item_received`
- summary `items_received` increments

## 8. Inspect the live logs

Good targeted filters:

```bash
journalctl -u sekailink-room-server.service --no-pager | grep 'room_server_tcp_command' | tail -n 20
journalctl -u sekailink-room-server.service --no-pager | grep 'room_server_http_request' | tail -n 20
journalctl -u sekailink-room-server.service --no-pager | grep 'slot_data_updated\|item_received' | tail -n 20
```

The TCP command logs now carry:

- channel
- cmd
- ok/error
- room summary
- `seed_name`
- `seed_id` / `seed_hash`
- `tracker_pack` / `tracker_variant`
- `slot_data_keys`
- runtime heartbeat identity
- latest received item summary when available

Before exposing any private edge for testers, run:

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --systemd-unit sekailink-room-server.service
```

Then replay the SKLMI-facing runtime surface against the same private daemon:

```bash
bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json
```

And keep a room-focused log tail ready during the test window:

```bash
bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

## 9. Failure triage order

1. `/health`
2. `room_server_state.json`
3. `service.log`
4. `GET /rooms/<room_id>/snapshot`
5. `GET /rooms/<room_id>/events`

If the room exists but item flow looks stale, check whether:

- `slot_data_updated` happened at all
- `enqueue_received_item` produced both a TCP log line and a room event
- `runtime_heartbeat` is recent and still marked connected
- the runtime token is correct on the TCP command

## External handoff reminder

This runbook stays on the private loopback surface. Before sharing anything with
external testers, map the public edge values in:

- `deploy/link/room-server/room-server.alttp-live.profile.env.example`

and keep the admin token private.

## Nexus relationship

For the live host, the official room-information path is already:

`Link Room -> MySQL projection (sekailink_room_projection) -> Nexus room-query`

So a successful ALTTP smoke should be treated as complete only if the room
state remains coherent on the `Link Room` side and the projection stays usable
for `Nexus`.
