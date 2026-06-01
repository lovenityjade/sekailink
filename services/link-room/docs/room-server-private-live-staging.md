# Room Server Private Live Staging

Date: 2026-05-05

## Goal

Stage updated Link Room runtime assets on the live `link` host without touching
the active service, active config, or public/private edge.

This runbook is intentionally non-destructive.

## Current live assumptions

- host: `link.sekailink.com`
- active root: `/opt/sekailink/link/room-server`
- active env file: `/opt/sekailink/link/room-server/config/room_server.env`
- active state file: `/opt/sekailink/link/room-server/runtime/room_server_state.json`
- projection backend: `mysql`
- projection target is shared with Nexus room-query
- logs may come from either:
  - `journalctl -u sekailink-room-server.service`
  - or `/opt/sekailink/link/logs/room-server/service.log` if file logging is enabled

## 1. Verify the active daemon first

```bash
systemctl status sekailink-room-server.service --no-pager
jq . /opt/sekailink/link/room-server/runtime/room_server_state.json
curl http://127.0.0.1:18081/health
```

Do not stage anything until the currently running daemon is already healthy.

## 2. Create a non-destructive staging bundle on the host

From the repo checkout on `link`, run:

```bash
bash deploy/link/room-server/stage-private-live-host.sh \
  --profile nexus-live \
  --room-root /opt/sekailink/link/room-server
```

That creates a timestamped tree under:

- `/opt/sekailink/link/room-server/staging/<timestamp>/`

and does not overwrite:

- `/opt/sekailink/link/room-server/config/room_server.json`
- `/opt/sekailink/link/room-server/config/room_server.env`
- `/etc/systemd/system/sekailink-room-server.service`

## 3. Compare staged files against the active files

Example:

```bash
STAGE_DIR="$(find /opt/sekailink/link/room-server/staging -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)"

diff -u \
  /opt/sekailink/link/room-server/config/room_server.json \
  "${STAGE_DIR}/config/room_server.json"

diff -u \
  /opt/sekailink/link/room-server/config/room_server.env \
  "${STAGE_DIR}/config/room_server.env"

diff -u \
  /etc/systemd/system/sekailink-room-server.service \
  "${STAGE_DIR}/ops/sekailink-room-server.service"
```

The goal of this step is review, not rollout.

## 4. Re-run private readiness against the active service

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --systemd-unit sekailink-room-server.service
```

Useful things to confirm:

- `projection_backend=mysql`
- `projection_target=sekailink_room_projection`
- the printed chain says:
  - `link-room -> mysql(room_records,room_event_records,client_report_records) -> nexus-room-query`
- `/health` is still healthy
- `/rooms` still answers with the admin token
- readiness prints a room-focused log helper command
- readiness prints a runtime-surface validation command

## 4b. Re-run the SKLMI-facing runtime surface locally on the private daemon

```bash
bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json
```

Useful things to confirm:

- `issue_ticket` returns a `session_token`
- `pending_items` returns at least one `delivery_id`
- `acknowledge_delivery` succeeds and can be safely replayed
- `summary` reports `runtime_ticket_issued=true`
- `summary` reports `last_delivery_ack_id`

## 5. Preserve a review artifact

Before any future rollout discussion, capture:

```bash
tar -C /opt/sekailink/link/room-server/staging -czf \
  "/opt/sekailink/link/room-server/staging/$(basename "${STAGE_DIR}").tar.gz" \
  "$(basename "${STAGE_DIR}")"
```

This gives the next agent a frozen staging candidate to inspect without
rebuilding it.

## 6. What this runbook does not do

It does not:

- restart `sekailink-room-server.service`
- replace the active env/config files
- edit the systemd unit in place
- alter the Nexus room-query service
- touch the external edge

Promotion to a tester-facing private edge should happen only after the runtime
surface replay above passes on the private daemon.
