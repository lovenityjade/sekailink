# Room Server Private Live Promotion

Date: 2026-05-05

## Goal

Provide one exact, non-destructive promotion path from a healthy private modern
Link Room daemon on `link` to a tester-facing private edge.

## Promotion gate

Do not invite testers until all of these are true on the private daemon:

1. `check-private-live-readiness.sh` passes.
2. `validate-private-runtime-commands.sh` passes.
3. A room-focused log tail is available through `tail-private-live-logs.sh`.
4. The projection chain remains:
   - `Link Room -> MySQL projection -> Nexus room-query`

## Exact operator sequence

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --systemd-unit sekailink-room-server.service

bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json

bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

## Tester-facing handoff

Testers should receive only:

- the private TCP hostname and port prepared for them
- the optional private HTTPS base URL if intentionally exposed
- the room id
- the expected seed metadata
- the exact client/runtime build to run

They should not receive:

- admin token
- runtime token
- loopback hostnames or ports
- state-file paths
- MySQL credentials

## Promotion evidence to capture

Before the invite, save:

- the readiness output
- the runtime-surface validation output
- the latest room summary for the target room
- the latest room snapshot for the target room
- a short room-focused log tail

Recommended capture commands:

```bash
curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:$(jq -r '.effective_http_port' /opt/sekailink/link/room-server/runtime/room_server_state.json)/rooms/alttp-live-1/summary" | jq .

curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:$(jq -r '.effective_http_port' /opt/sekailink/link/room-server/runtime/room_server_state.json)/rooms/alttp-live-1/snapshot" | jq .

bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

## What “good” looks like

- `issue_ticket` returns a session token
- `pending_items` returns delivery ids
- `pending_items` includes LinkedWorld item semantics (`event_key` or
  `tracker_semantic_id`, plus `mapped_value`) after
  `apply_linkedworld_surface`
- `runtime_event` records the check or returns a safe duplicate result
- `acknowledge_delivery` succeeds and can be replayed safely
- `summary` shows:
  - `runtime_ticket_issued=true`
  - `pending_delivery_count`
  - `acknowledged_delivery_count`
  - `last_delivery_ack_id`

## If promotion is blocked

Stop before touching the tester-facing edge and inspect in this order:

1. `tail-private-live-logs.sh`
2. `/rooms/<room_id>/summary`
3. `/rooms/<room_id>/snapshot`
4. the shared MySQL projection target
5. Nexus room-query visibility

## Residual risks

- the private modern daemon can be healthy while the long-lived prod service on
  `link` still runs an older binary; do not confuse the two instances
- the tester-facing TCP/HTTP edge remains out of this repo, so a wrong relay
  target can bypass the validated private daemon
- Link Room can be healthy while Nexus room-query still points at the wrong
  projection target or stale credentials
- loopback HTTP inspection can pass while the shared MySQL projection is lagging
  or blocked
- operator evidence can be lost if the room-focused log tail is not captured
  before a restart or rollback discussion
