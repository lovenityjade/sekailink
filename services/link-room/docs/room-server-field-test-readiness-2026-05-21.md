# Link Room Field-Test Readiness - 2026-05-21

## Scope

This note captures the first real-field-test readiness audit for `sekailink-link-room`.
It intentionally avoids secrets and does not cover `aptest`.

## Local Build Gate

Result: pass.

Validated from the clean-room Link Room repo:

- `cmake -S . -B /tmp/sekailink-link-room-readiness-build`
- `cmake --build /tmp/sekailink-link-room-readiness-build -j2`
- `sekailink_room_linkedworld_surface_smoke`
- `sekailink_room_seed_package_dispatch_smoke`
- `sekailink_room_seed_package_dispatch_tcp_smoke`
- `deploy/link/room-server/run-local-alttp-loopback.sh`

The TCP smoke and local ALTTP loopback need an unrestricted local loopback
environment. Inside the Codex sandbox, socket bind can fail; outside the sandbox
the smoke passed.

## Link Host Gate

Result: pass for private loopback room-server readiness.

Observed on `link.sekailink.com`:

- `sekailink-room-server.service` is active and enabled.
- Native room-server service is bound to loopback only.
- HTTP health returns `ok=true`.
- TCP and HTTP loopback ports are listening.
- Runtime state reports:
  - `status=running`
  - `loopback_only=true`
  - `projection_backend=mysql`
  - `projection_root=sekailink_room_projection`
  - admin/runtime/client-report auth enabled
- Existing live room visible on Link HTTP admin:
  - `room-linkedworld-soh-live-official`

Important operational note:

- The current live unit logs to journal only. Some newer helper docs mention an
  optional file log path, but the active unit does not append to that file.
  `tail-private-live-logs.sh` can still fall back to `journalctl`.

## Nexus Read-Side Gate

Result: pass after service restart.

Initial state:

- `sekailink-nexus-room-query.service` was active and enabled.
- `/health` returned OK.
- `/rooms` returned `500` with `Server has gone away`.

Action taken:

- Restarted only `sekailink-nexus-room-query.service`.

After restart:

- `/health` returned OK.
- `/rooms?limit=5` returned room summaries.
- `room-linkedworld-soh-live-official` returned a full snapshot.
- Nexus room-query state reports MySQL projection target
  `sekailink_room_projection`.

Implication:

- The service can lose its long-lived MySQL connection after enough uptime.
  Before a field test, always run `/rooms` in addition to `/health`, or restart
  `sekailink-nexus-room-query.service` during the preflight window.

## Current Deployment Reality

The current Link host binary differs from the binary built locally during this
audit. That is expected if local source has moved since the last host deploy.
Do not assume the host is running the exact local tree until a staged binary
promotion is performed.

Recommended next deployment path:

1. Stage the current local build on Link under the existing staging path.
2. Run the private instance on alternate loopback ports.
3. Run `exercise-private-live-instance.sh`.
4. Run `check-private-live-readiness.sh`.
5. Run `validate-private-runtime-commands.sh`.
6. Confirm Nexus `/rooms` and the target room snapshot.
7. Only then wire a private tester-facing edge/tunnel.

## External Tester Gate

Not ready yet for arbitrary external testers.

The native daemon is intentionally private and loopback-only. A real field test
still needs a deliberate edge strategy:

- TCP relay or tunnel to the private room-server TCP port.
- Optional HTTP edge for read-only/tester-safe endpoints, if needed.
- No admin/runtime tokens shared with testers.

## Verdict

Link Room is clean enough for the next private deployment rehearsal.

It is not yet clean enough to invite external testers until the staged current
binary is promoted through the private-instance flow and the tester-facing edge
is explicitly configured.
