# Room Server Live Test

Date: 2026-05-05

## Goal

Prepare `sekailink-link-room` for a real `link` live test host without changing the remote server directly from this repo.

## What is now in-repo

The room-server deployment/runtime assets now live in:

- `deploy/link/room-server/`

That directory includes:

- `room_server.json.example`
- `room_server.alttp-live.loopback.json.example`
- `room_server.env.example`
- `room-server.alttp-live.profile.env.example`
- `sekailink-room-server.service`
- `admin-agent.room-server-service.example.json`
- `send-room-command.sh`
- `check-private-live-readiness.sh`
- `stage-private-live-host.sh`
- `payloads/*.command.json`
- `README.md`

## Runtime contract

The native room server binary remains:

- `sekailink_room_server_service`

Supported runtime flags:

- `--config <room_server.json>`
- `--state-file <room_server_state.json>`

Runtime env overrides:

- `SEKAILINK_ROOM_SERVER_ADMIN_TOKEN`
- `SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN`
- `SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN`

Operator-only live profile helper:

- `deploy/link/room-server/room-server.alttp-live.profile.env.example`

Current profile split:

- `room_server.json.example` reflects the live MySQL/Nexus direction
- `room_server.alttp-live.loopback.json.example` remains the local SQLite proof profile

## New service state visibility

The room server state file now carries service lifecycle details in addition to effective ports:

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
- `requested_config`

Expected status progression:

- `running`
- `stopping`
- `stopped`
- `failed`

## Verbose logs

The in-repo systemd template appends both stdout and stderr to:

- `/opt/sekailink/link/logs/room-server/service.log`

The active live host may still rely primarily on:

- `journalctl -u sekailink-room-server.service`

The service now emits structured JSON lifecycle logs for:

- bootstrap
- successful start
- stopping
- stopped
- failed start / runtime failure
- TCP command handling
- HTTP inspection requests

The previous plain startup line is still kept for compatibility:

- `room_server_service_started tcp_port=... http_port=...`

Lifecycle logs now also carry:

- bind host visibility (`127.0.0.1`)
- loopback-only flag
- auth-enabled flags per channel
- current room count

TCP command logs now also carry live room context that is useful during ALTTP
tests:

- protocol channel
- command name
- `seed_name`
- `seed_id`
- `seed_hash`
- `tracker_pack`
- `tracker_variant`
- `slot_data_keys`
- runtime heartbeat identity such as `driver_instance_id`, `linkedworld_id`, `core_profile`
- latest received item summary when available

## Health visibility

The unauthenticated `GET /health` payload now includes lightweight ops clues that are safe to expose on loopback:

- `loopback_only`
- `http_bind_host`
- `room_count`
- `admin_auth_enabled`

That makes it easier to confirm whether the local daemon is ready before testing the protected routes.

Protected inspection routes that are now especially useful for an ALTTP live
test:

- `GET /rooms/<room_id>/snapshot`
- `GET /rooms/<room_id>/summary`
- `GET /rooms/<room_id>/sync`
- `GET /rooms/<room_id>/events`

## Access pattern

Current native bindings remain loopback-only:

- TCP: `127.0.0.1`
- HTTP: `127.0.0.1`

For an external live test, keep the daemon private and add an edge layer on the host:

- HTTP reverse proxy or tunnel
- TCP proxy / relay

The clean-room target host for that first test is currently assumed to be:

- logical host `link`
- hostname `link.sekailink.com`

## Service template hardening

The provided `systemd` unit now:

- checks that the binary exists and is executable before startup
- checks that `room_server.json` exists before startup
- creates runtime/log/projection directories with `sekailink:sekailink` ownership instead of root-owned placeholders

That avoids a common false-green boot where `systemd` starts but the service cannot write state, audit, or logs.

## Admin path

If `sekailink_admin_agent` is present on the same host, merge:

- `deploy/link/room-server/admin-agent.room-server-service.example.json`

into the admin agent `services` array so ops can fetch:

- service state
- recent verbose logs
- systemd control

## Important limitation

This clean-room repo still does not declare its own standalone build system yet. The deployment/runtime artifacts here assume the existing server-core build lane already produces the room server binary.

It also does not ship a direct SKLMI transport. The first downstream consumer
should read either the room HTTP inspection surface or the shared MySQL
projection produced by the room server and consumed by Nexus room-query.

## Deployment checklist

Use the local runbook for the actual preflight, rollout, smoke, external-tester gate, and rollback steps:

- `docs/room-server-live-deployment-checklist.md`
- `docs/room-server-alttp-live-runbook.md`
- `docs/room-server-archipelago-alignment.md`
- `docs/room-server-first-external-test.md`
- `docs/room-server-private-live-staging.md`
- `docs/room-server-nexus-room-query-chain.md`
