# sekailink-link-room live test runtime

This directory prepares the native room server slice for a real `link` test host without touching the remote machine directly.

## Scope

Included here:

- runtime config template
- live MySQL/Nexus config profile
- ALTTP live loopback config profile
- systemd environment template
- operator profile for first live test
- systemd unit example
- admin-agent service descriptor example
- payload files for room mutations
- helper scripts for local staging, host-side staging, private-live preflight,
  TCP command sending, room-focused log access, and runtime-surface validation

Not included here:

- remote host changes
- TLS / TCP edge proxy setup
- standalone build system for this clean-room repo

The current build assumption remains the existing server-core pipeline that already produces:

- `sekailink_room_server_service`
- `sekailink_room_server_tcp_cli`

The live host may temporarily have only the service binary installed. The
readiness helper tolerates that and reports the TCP CLI as missing instead of
failing hard, because HTTP/state/journal validation remains useful before the
mutation CLI is copied.

## Expected host layout

```text
/opt/sekailink/link/
  room-server/
    bin/
    config/
    data/
      audit/
      projection/
    runtime/
  logs/
    room-server/
      service.log
```

Recommended runtime paths:

- binary:
  - `/opt/sekailink/link/room-server/bin/sekailink_room_server_service`
- config:
  - `/opt/sekailink/link/room-server/config/room_server.json`
- env:
  - `/opt/sekailink/link/room-server/config/room_server.env`
- state file:
  - `/opt/sekailink/link/room-server/runtime/room_server_state.json`
- logs:
  - current live host default: `journalctl -u sekailink-room-server.service`
  - optional file log if a future unit adds one: `/opt/sekailink/link/logs/room-server/service.log`

Secrets stay in the env file, not in `room_server.json`.

Recommended first ALTTP live profile files:

- `room_server.json.example`
- `room_server.alttp-live.loopback.json.example`
- `room-server.alttp-live.profile.env.example`
- `room_server.env.example`

Recommended token generation on the host:

```bash
openssl rand -hex 24
```

## Install flow

1. Build the room server binary through the current server-core build lane.
2. Build the TCP CLI through the same build lane if you want scripted smoke commands.
3. Copy the binaries to `/opt/sekailink/link/room-server/bin/`.
4. Copy `room_server.json.example` to `/opt/sekailink/link/room-server/config/room_server.json` for the live MySQL/Nexus profile.
5. Copy `room_server.env.example` to `/opt/sekailink/link/room-server/config/room_server.env` and replace the tokens.
6. Copy `sekailink-room-server.service` to `/etc/systemd/system/`.
7. Run `systemctl daemon-reload`.
8. Run `systemctl enable --now sekailink-room-server.service`.

Make sure the runtime tree remains writable by the `sekailink` service user:

- `/opt/sekailink/link/room-server/`
- `/opt/sekailink/link/logs/room-server/`

The provided unit now pre-creates the runtime directories with `sekailink:sekailink` ownership, which is important because the service writes:

- the runtime state file
- audit files
- projection files
- appended service logs

## Runtime behavior

Current service behavior that matters for live testing:

- TCP and HTTP both bind loopback only
- verbose lifecycle logs go to stdout/stderr
- the provided systemd unit appends both stdout and stderr into one file
- the service writes a structured state file on `running`, `stopping`, `stopped`, and `failed`
- auth tokens can stay out of JSON config and come only from the env file

The runtime state file now includes:

- `status`
- `started_at`
- `updated_at`
- `effective_tcp_host`
- `effective_tcp_port`
- `effective_http_host`
- `effective_http_port`
- `boot_room_count`
- `room_count`
- `stop_requested`
- `loopback_only`
- `admin_auth_enabled`
- `runtime_auth_enabled`
- `client_report_auth_enabled`
- `last_error`

The unauthenticated `/health` route now returns:

- `service`
- `time`
- `loopback_only`
- `http_bind_host`
- `room_count`
- `admin_auth_enabled`

## Live projection reality

The active live host should use:

- `projection_backend=mysql`
- `projection_root=<shared MySQL projection target>`
- `room_server.env` for both room-server auth tokens and `SEKAILINK_MYSQL_*`

Keep the SQLite profile only for local loopback proof:

- `room_server.alttp-live.loopback.json.example`

## First external test helpers

To avoid hand-editing long JSON payloads, this directory now includes:

- `send-room-command.sh`
- `check-private-live-readiness.sh`
- `tail-private-live-logs.sh`
- `validate-private-runtime-commands.sh`
- `stage-private-live-host.sh`
- `start-private-live-instance.sh`
- `exercise-private-live-instance.sh`
- `payloads/create-room.alttp-live.command.json`
- `payloads/set-slot-data.alttp-live.command.json`
- `payloads/enqueue-item.alttp-live.command.json`
- `payloads/record-check.alttp-live.command.json`
- `payloads/room-summary.alttp-live.command.json`

Example:

```bash
export SEKAILINK_ROOM_SERVER_ADMIN_TOKEN=replace-admin-token

bash deploy/link/room-server/send-room-command.sh \
  --channel admin \
  --env-file /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --command-file deploy/link/room-server/payloads/create-room.alttp-live.command.json
```

The helper requires `jq` and `sekailink_room_server_tcp_cli`. If `--token-env`
is omitted, it now infers the correct token variable from `--channel`. If
`--state-file` is provided, it can also derive the effective loopback TCP host
and port from `room_server_state.json`. It accepts both:

- `room_server.env`
- `room-server.env`

## Private-live preflight helper

Once the host paths exist on `link.sekailink.com`, you can run:

```bash
bash deploy/link/room-server/check-private-live-readiness.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json \
  --service-log /opt/sekailink/link/logs/room-server/service.log
```

That helper validates the expected private-live files, curls `/health` and
authorized `/rooms`, tails the live log if a file exists, otherwise falls back
to `journalctl`, prints the next `create-room` and `set-slot-data` commands
ready to replay, prints the next room-focused log tail command, prints the next
runtime-surface validation command, and reports the live projection chain when
`mysql` is active.

## Runtime surface validation

Before inviting testers, validate the private modern instance with:

```bash
bash deploy/link/room-server/validate-private-runtime-commands.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --state-file /opt/sekailink/link/room-server/runtime/room_server_state.json
```

That helper replays and verifies:

- `create_room`
- `set_slot_data`
- `enqueue_received_item`
- `issue_ticket`
- `pending_items`
- `runtime_event`
- `acknowledge_delivery`
- HTTP `summary` and `snapshot`

## Private log access

To focus logs on one private test room without remembering whether the service
is writing to a file or only to `journalctl`, use:

```bash
bash deploy/link/room-server/tail-private-live-logs.sh \
  --profile-env deploy/link/room-server/room-server.alttp-live.profile.env.example \
  --room-env /opt/sekailink/link/room-server/config/room_server.env \
  --filter alttp-live-1
```

## Private modern-binary instance on `link`

For a non-destructive verification of the modern room-server binary on the real
`link` host, run:

```bash
bash deploy/link/room-server/start-private-live-instance.sh
bash deploy/link/room-server/exercise-private-live-instance.sh
```

These scripts assume the staged modern binaries exist under:

- `/home/debian/sekailink-room-live-staging/bin/`

They launch a separate loopback-only instance with its own runtime/log tree and
leave the official service untouched.

## Host-side non-destructive staging

On the live host, create a review-only staging bundle with:

```bash
bash deploy/link/room-server/stage-private-live-host.sh \
  --profile nexus-live \
  --room-root /opt/sekailink/link/room-server
```

That writes only under:

- `/opt/sekailink/link/room-server/staging/`

and leaves the active service/config untouched.

## Local staging helper

If you want a safe local staging tree before touching the remote `link` host:

```bash
bash deploy/link/room-server/prepare-live-test-layout.sh --profile loopback
```

That creates a local bundle with config, payloads, helper scripts and the
service unit, but does not mutate the live host.

## One-command loopback proof

If the native room server binaries are already built locally, replay the proven
ALTTP loopback flow with:

```bash
bash deploy/link/room-server/run-local-alttp-loopback.sh
```

The script drives:

- `create_room`
- `set_slot_data`
- `enqueue_received_item`
- `record_check`

and then prints:

- `/health`
- `/rooms`
- `/rooms/<room_id>/summary`
- `/rooms/<room_id>/snapshot`
- `/rooms/<room_id>/events`
- the state file
- the service log tail

## Live test notes

Because the native room server is loopback-only today, a real external test host still needs an explicit edge strategy:

- HTTP: reverse proxy or tunnel on the host
- TCP: TCP proxy / edge relay on the host

That keeps the native daemon private while still allowing controlled external testing.

## Nexus projection relationship

The live `link` room server and the live `nexus` room-query service already
share the same MySQL projection target:

- `Link Room` writes room projections into `sekailink_room_projection`
- `Nexus room-query` reads from that same projection target to expose official
  room information

So the live data path is:

`Link Room -> MySQL projection -> Nexus room-query`

This is the current official bridge to `Nexus`. A first live ALTTP test should
preserve that relationship instead of inventing a parallel side channel.

See also:

- `docs/room-server-private-live-staging.md`
- `docs/room-server-nexus-room-query-chain.md`

## Fast checks

```bash
systemctl status sekailink-room-server.service --no-pager
journalctl -u sekailink-room-server.service -n 50 --no-pager
jq . /opt/sekailink/link/room-server/runtime/room_server_state.json
journalctl -u sekailink-room-server.service -f --no-pager
curl http://127.0.0.1:18081/health
```

Authorized room inspection example:

```bash
curl \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  http://127.0.0.1:18081/rooms
```

Expected healthy hints:

- `/health` returns `200`
- state file `status` is `running`
- state file `loopback_only` is `true`
- state file auth flags match the env file you loaded
- `journalctl` shows `room_server_service_started`

## Live rollout note

Use `docs/room-server-live-deployment-checklist.md` for the full preflight and external-tester gate instead of improvising on the host.

Use `docs/room-server-first-external-test.md` for the first ALTTP live profile
and the expected handoff to external testers / SKLMI-side consumers.

## Admin agent integration

If the admin agent is running on the same host, merge `admin-agent.room-server-service.example.json` into its `services` list.

That gives the admin agent access to:

- the room server state file
- the appended verbose log file
- the systemd unit name for restart/start/stop operations
