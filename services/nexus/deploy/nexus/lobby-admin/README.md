# Nexus Lobby Admin Service

Private native C++ lobby-control surface for the admin CLI.

## Paths

- binary: `/opt/sekailink/nexus/lobby-admin/bin/sekailink_lobby_admin_service`
- config: `/opt/sekailink/nexus/lobby-admin/config/lobby_admin.json`
- data: `/opt/sekailink/nexus/lobby-admin/data/`

## Service

Systemd unit:

- `sekailink-nexus-lobby-admin.service`

## Routes

- `GET /health`
- `POST /admin/lobbies`
- `GET /admin/lobbies/{lobby_id}`
- `PATCH /admin/lobbies/{lobby_id}`
- `POST /admin/lobbies/{lobby_id}/close`

This service is intended to remain loopback/private and be reached through the local admin CLI over SSH tunnel, not exposed publicly.

## Optional bridge to link runtime

The config also supports a private runtime bridge:

- `runtime_bridge.enabled`
- `runtime_bridge.host`
- `runtime_bridge.port`
- `runtime_bridge.admin_token`

This is intended for the `Nexus -> link` tunnel case so private lobby metadata/control on `Nexus` can sync into the live lobby runtime on `link`.
