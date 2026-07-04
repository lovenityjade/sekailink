# Room Server Live Deployment Checklist

Date: 2026-05-05

Use this checklist when preparing a real `link` host for external ALTTP testers.

## Preflight

- Confirm the target binary exists at `/opt/sekailink/link/room-server/bin/sekailink_room_server_service`.
- Confirm the TCP CLI exists at `/opt/sekailink/link/room-server/bin/sekailink_room_server_tcp_cli` if you want scripted room mutation smoke.
- Confirm `room_server.json` and `room_server.env` are present under `/opt/sekailink/link/room-server/config/`.
- Confirm all three room-server tokens are replaced with real secrets.
- Confirm the chosen TCP and HTTP ports are free on the host.
- Confirm `/opt/sekailink/link/room-server/` is writable by `sekailink`.
- If using file logs, confirm `/opt/sekailink/link/logs/room-server/` is writable too.
- Confirm the first-test operator profile is filled:
  - `deploy/link/room-server/room-server.alttp-live.profile.env.example`
- Confirm the operator profile includes the room seed identifiers you intend to share:
  - `SEKAILINK_ALTTP_TEST_SEED_NAME`
  - `SEKAILINK_ALTTP_TEST_SEED_ID`
  - `SEKAILINK_ALTTP_TEST_SEED_HASH`
- Confirm the edge plan is ready because the native daemon remains loopback-only:
  - HTTP reverse proxy or tunnel
  - TCP proxy or relay
- Confirm the tester-facing room id and runtime identity are frozen for the
  promotion window:
  - `SEKAILINK_ALTTP_TEST_ROOM_ID`
  - `SEKAILINK_ALTTP_TEST_SLOT_ID`
  - `SEKAILINK_ALTTP_TEST_SEED_NAME`

## Deploy

1. Copy the current templates from `deploy/link/room-server/` to the host paths.
2. Run `systemctl daemon-reload`.
3. Run `systemctl enable --now sekailink-room-server.service`.
4. Run `systemctl status sekailink-room-server.service --no-pager`.
5. Run `journalctl -u sekailink-room-server.service -n 50 --no-pager`.

## Non-Destructive Staging

- Before changing any active file, run:

```bash
bash deploy/link/room-server/stage-private-live-host.sh \
  --profile nexus-live \
  --room-root /opt/sekailink/link/room-server
```

- Confirm the staged bundle lands only under:
  - `/opt/sekailink/link/room-server/staging/`
- Diff the staged config/env/unit against the active files before discussing any rollout.

## Local Smoke

- Run `curl http://127.0.0.1:18081/health`.
- Confirm the response includes:
  - `"ok":true`
  - `"loopback_only":true`
  - `"http_bind_host":"127.0.0.1"`
- Inspect `/opt/sekailink/link/room-server/runtime/room_server_state.json`.
- Confirm the state file includes:
  - `"status":"running"`
  - `"effective_tcp_host":"127.0.0.1"`
  - `"effective_http_host":"127.0.0.1"`
  - `"loopback_only":true`
  - auth-enabled flags matching the env file
- Inspect `journalctl -u sekailink-room-server.service -n 50 --no-pager`.
- Confirm the journal contains `room_server_service_started`.

## Protected Route Smoke

- Run:

```bash
curl \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  http://127.0.0.1:18081/rooms
```

- Confirm the route returns `200`.
- If rooms exist, confirm the room list matches expectations.
- Optionally run one scripted mutation with:
  - `deploy/link/room-server/send-room-command.sh`
- Prefer the helper with:
  - `--env-file /opt/sekailink/link/room-server/config/room_server.env`
  - `--state-file /opt/sekailink/link/room-server/runtime/room_server_state.json`

## Private-Ready Check

- Run:

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --systemd-unit sekailink-room-server.service
```

- Confirm it reports:
  - `state_status=running`
  - `state_loopback_only=true`
  - `projection_backend=mysql`
  - `projection_target=sekailink_room_projection` or the expected shared target
  - one healthy `/health` payload
  - one authorized `/rooms` payload
  - the next `create-room` and `set-slot-data` commands ready to replay
  - the next `validate-private-runtime-commands.sh` replay command
  - the next `tail-private-live-logs.sh` log command

## Runtime Surface Gate

- Run:

```bash
bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json
```

- Confirm the replay succeeds for:
  - `issue_ticket`
  - `pending_items`
  - `runtime_event`
  - `acknowledge_delivery`
- Confirm HTTP `summary` shows:
  - `runtime_ticket_issued=true`
  - `pending_delivery_count`
  - `acknowledged_delivery_count`
  - `last_delivery_ack_id`

## External Tester Gate

- Confirm the reverse proxy or tunnel only exposes the intended HTTP surface.
- Confirm the TCP relay only forwards to the loopback room-server TCP port.
- Confirm no secret token is present in public-facing configs or logs.
- Confirm the admin token is not shared with external testers.
- Confirm testers have the exact hostnames/ports they should use and nothing more.
- Confirm the external handoff contains the room id and test seed metadata, but
  not the operator-only loopback paths.
- Confirm one operator can tail the private room logs with:
  - `deploy/link/room-server/tail-private-live-logs.sh --filter <room_id>`

## During Test

- Watch:
  - `systemctl status sekailink-room-server.service --no-pager`
  - `journalctl -u sekailink-room-server.service -f --no-pager`
  - `jq . /opt/sekailink/link/room-server/runtime/room_server_state.json`
- Track room count changes through `/health` and the state file.
- If the process restarts, capture the surrounding `journalctl` and `service.log` window before retrying.

## Rollback

1. Remove external tester access at the edge first.
2. Run `systemctl stop sekailink-room-server.service`.
3. Preserve:
   - `journalctl -u sekailink-room-server.service --since '5 minutes ago' --no-pager`
   - `/opt/sekailink/link/room-server/runtime/room_server_state.json`
   - `/opt/sekailink/link/room-server/data/`
4. Revert to the previous binary or config only after the logs/state are saved.

## Nexus consistency gate

- Confirm the live room server projection backend remains `mysql`.
- Confirm the projection target remains `sekailink_room_projection`.
- Confirm `Nexus room-query` still points at the same projection target before
  any external test invite is sent.
- Treat `Link Room -> MySQL projection -> Nexus room-query` as the official
  source path for room information during the live test.
- Capture one explicit verification bundle before promotion:
  - Link Room `/summary`
  - Link Room `/snapshot`
  - room-focused log tail
  - the currently intended Nexus room-query read result
