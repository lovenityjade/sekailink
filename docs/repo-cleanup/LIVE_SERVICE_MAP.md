# Live Service Map

Last updated: 2026-06-26

This document is the required first stop before touching a live SekaiLink
service. If a service is not mapped here, do not deploy it from an assumed
source path.

## link.sekailink.com

### Chat API

Systemd unit:

```text
sekailink-chat-api.service
```

Live binary:

```text
/opt/sekailink/link/chat-api/bin/sekailink_chat_api_service
```

Live config:

```text
/opt/sekailink/link/chat-api/config/chat_api.json
```

Runtime data:

```text
/opt/sekailink/link/chat-api/data/chat_api.sqlite3
/opt/sekailink/link/chat-api/data/generation-spool/
```

Backups:

```text
/opt/sekailink/link/chat-api/backups/
```

Known good rollback from 2026-06-25:

```text
/opt/sekailink/link/chat-api/backups/sekailink_chat_api_service.before-generation-reset-20260625T235800Z
```

Current caution:

The VPS-local source tree under `/opt/sekailink-server/src/sekailink-link/` is
not guaranteed to match the live binary. It was used accidentally on
2026-06-25 and produced a risky rollback situation. Do not deploy from that
tree without reconciliation.

Canonical repo source:

```text
UNCONFIRMED
```

Candidate repo source:

```text
services/link-social/src/
services/link-social/source-snapshots/monolith-core/
```

Required before next deploy:

- confirm canonical source;
- build in a clean directory;
- confirm generation handoff behavior still works;
- confirm `POST /api/lobbies/:id/generation/reset` route exists;
- backup live binary;
- restart only `sekailink-chat-api.service`;
- verify `systemctl is-active sekailink-chat-api.service`.

### Room Server

Systemd unit:

```text
sekailink-room-server.service
```

Live binary:

```text
/opt/sekailink/link/room-server/bin/sekailink_room_server_service
```

Live config:

```text
/opt/sekailink/link/room-server/config/room_server.json
```

State file:

```text
/opt/sekailink/link/room-server/runtime/room_server_state.json
```

Canonical repo source:

```text
UNCONFIRMED
```

Candidate repo source:

```text
services/link-room/
services/link-room/source-snapshots/monolith-core/
```

### Lobby Runtime

Systemd unit:

```text
sekailink-lobby-runtime.service
```

Live binary:

```text
/opt/sekailink/link/lobby-runtime/bin/sekailink_lobby_runtime_service
```

Canonical repo source:

```text
UNCONFIRMED
```

### Admin Agent

Systemd unit:

```text
sekailink-admin-agent.service
```

Live binary:

```text
/opt/sekailink/link/admin-agent/bin/sekailink_admin_agent_service
```

Canonical repo source:

```text
UNCONFIRMED
```

## worlds.sekailink.com

### Generation Server

Canonical repo source candidate:

```text
services/worlds/server/native/sekailink_server_core/
```

Operational notes:

```text
services/worlds/CHANGELOG.md
```

Known recent live changes:

- Archipelago 0.6.7 generation stabilization.
- `SEKAILINK_DISABLE_SPEEDUPS=1`.
- DKC3 ROM installation and `host.yaml` path.

## nexus.sekailink.com

### Seed Config API / Identity / Lobby Admin

Canonical repo source candidate:

```text
services/nexus/server/native/sekailink_server_core/
```

Operational notes:

```text
services/nexus/CHANGELOG.md
```

## Rule

Every mapped service must eventually have:

- host;
- systemd unit;
- live binary;
- live config;
- canonical repo source;
- build command;
- deploy command;
- smoke test;
- rollback command.
