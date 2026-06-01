# Nexus Configs

Date: 2026-05-10

## Role

`Nexus Configs` is the configuration and deployment tier for native
services hosted by `Nexus`.

It owns:

- committed config templates under `deploy/nexus`
- runtime path conventions under `/opt/sekailink/nexus`
- service-to-config mapping for systemd units
- documentation for safe promotion from example config to live config

It does not own:

- live secrets
- generated state files
- service business logic
- public TLS or edge routing, which stays on `link`

## Source Of Truth

Authoritative config templates are committed as `*.json.example` files in
`deploy/nexus`. Live JSON files are derived from those templates during
deployment and must stay outside git.

Current template owners:

- `deploy/nexus/auth/identity/identity_service.json.example`
  - live path: `/opt/sekailink/nexus/auth/config/identity_service.json`
  - service: `sekailink-nexus-identity.service`
- `deploy/nexus/api-gateway/room-query/nexus_room_query.json.example`
  - live path: `/opt/sekailink/nexus/api-gateway/config/nexus_room_query.json`
  - service: `sekailink-nexus-room-query.service`
- `deploy/nexus/lobby-admin/lobby_admin.json.example`
  - live path: `/opt/sekailink/nexus/lobby-admin/config/lobby_admin.json`
  - service: `sekailink-nexus-lobby-admin.service`
- `deploy/nexus/admin-agent/admin_agent.json.example`
  - live path: `/opt/sekailink/nexus/admin-agent/config/admin_agent.json`
  - service: `sekailink-nexus-admin-agent.service`

Tunnel services may use shell/systemd deployment artifacts instead of JSON
configuration. The current tunnel tier is documented in `deploy/nexus/tunnel`.

## Runtime Path Contract

Use this layout for new Nexus-managed services:

```text
/opt/sekailink/nexus/<domain>/
  bin/
  config/
  data/
  logs/
```

Use `/srv/nexus-data` for durable host-level data such as MariaDB data,
backups, logs, spools, and large artifacts.

Config files should reference state files under the service-owned `data`
directory unless the state belongs to a shared host service such as MariaDB.

## Secret Handling

Committed templates must use placeholder values such as
`replace-nexus-identity-admin-token`.

Do not commit:

- live admin tokens
- OAuth client secrets
- database passwords
- private tunnel tokens
- machine-local bind overrides

The native service state writers redact known token fields when serializing
runtime state. Config templates should still treat every token-like field as
secret input.

## Build Integration Contract

Native config parsers live with the service implementation in
`server/native/sekailink_server_core`.

When a new config implementation exists, wire it into
`server/native/sekailink_server_core/CMakeLists.txt` only by adding:

- the new non-main `src/*.cpp` file to `sekailink_server_core`
- the new `src/*_smoke_main.cpp` file as a smoke executable

Do not add placeholder targets before the files exist. This keeps parallel
agent work safe and avoids breaking configure for partially delivered modules.

Current scan result for this handoff:

- no existing `src/*.cpp` file is missing from `CMakeLists.txt`
- no new `Nexus Configs` smoke source is present yet
- no CMake change is required until another agent lands those files

Useful local check:

```sh
for f in server/native/sekailink_server_core/src/*.cpp; do
  b=${f##*/}
  rg -q "src/$b" server/native/sekailink_server_core/CMakeLists.txt || printf '%s\n' "$b"
done
```

## Add A New Nexus Config

For a new Nexus-managed service:

1. Add the config parser and service defaults to the native service module.
2. Add a focused smoke source that parses a representative config template.
3. Add the parser `.cpp` and smoke executable to `CMakeLists.txt`.
4. Add a `deploy/nexus/<domain>/<service>.json.example` template.
5. Add or update the service README with live config, data, binary, and systemd paths.
6. Update this document when the service becomes part of the canonical Nexus config tier.

