# Nexus Server

Date: 2026-03-27

## Role

`Nexus` is the central core host.

It owns:
- MariaDB/MySQL central database
- native identity and account services
- native room-query services
- native admin-agent
- durable API and persistence responsibilities moved off `evolution`

## Logical layout

```text
/opt/sekailink/nexus/
  auth/
  api-gateway/
  db/
  admin-agent/
  logs/
  run/
/opt/sekailink/src/
/srv/nexus-data/
  mysql/
  backups/
  logs/
  spool/
  artifacts/
```

## Nexus Configs tier

The Nexus Configs tier owns committed service config templates and their
promotion path to live host config files.

Canonical template roots:

- `deploy/nexus/auth/identity/identity_service.json.example`
- `deploy/nexus/api-gateway/room-query/nexus_room_query.json.example`
- `deploy/nexus/lobby-admin/lobby_admin.json.example`
- `deploy/nexus/admin-agent/admin_agent.json.example`

Live configs belong under `/opt/sekailink/nexus/*/config/`; generated state
belongs under each service `data/` directory or under `/srv/nexus-data` for
host-level durable data.

Detailed ownership and build handoff rules are in `docs/nexus-configs.md`.

## Current bootstrap state

- hostname: `nexus.sekailink.com`
- OS: Fedora 43 Cloud Edition
- CPU/RAM: 4 vCores / 8 GB RAM
- storage:
  - root disk: ~73 GB mounted on `/`
  - dedicated data SSD: 100 GB mounted on `/srv/nexus-data`
- swap:
  - `zram0` 7.6 GB active

## Baseline hardening

- `firewalld` enabled
- public services currently allowed:
  - `ssh`
  - `dhcpv6-client`
- `fail2ban` enabled with `sshd` jail
- SSH hardening overlay installed in:
  - `/etc/ssh/sshd_config.d/90-sekailink-hardening.conf`
- root login kept enabled by explicit ops choice
- dedicated ops user:
  - `sekaiops`

## Data placement

The extra 100 GB SSD is now the persistent SekaiLink data volume:

- device: `/dev/sdb1`
- mountpoint: `/srv/nexus-data`
- filesystem: `ext4`

MariaDB has already been moved there:

- datadir: `/srv/nexus-data/mysql/data`
- bind-address: `127.0.0.1`

## Planned service role

`Nexus` is the target backend host for:

- native identity service
- native room-query service
- central MariaDB
- private admin-agent

Public traffic should still terminate on `link`, then proxy internally to `Nexus`.

## Current live state

`Nexus` is now live for the public API backend split.

Current native services:

- `sekailink-nexus-identity.service`
- `sekailink-nexus-room-query.service`
- `sekailink-nexus-admin-agent.service`
- `sekailink-nexus-lobby-admin.service`
- `sekailink-link-lobby-runtime-tunnel.service`

Current identity hashing baseline:

- password hashing: `Argon2id`
- current live parameters:
  - `password_time_cost = 3`
  - `password_memory_kib = 65536`
  - `password_parallelism = 1`
  - `password_hash_length = 32`
  - `password_salt_length = 16`

Current private identity-admin surface:

- private admin user control on the native identity service
- intended for admin CLI use through SSH-tunneled private access
- current command model foundation:
  - `adduser`
  - `listusers [limit] [query] [role] [state] [offset]`
  - `edituser`
  - `disableuser`
  - `enableuser`
  - `deluser` (soft disable)
  - `forcepasswordreset`
  - `userinfo`
  - `useraudit <username> [limit] [event_type] [offset]`
  - `listsessions`
  - `listdevices`
  - `revokesession`
  - `revokeothersessions`
  - `revokedevicesessions`
- current audit model:
  - `adduser`
  - `edituser`
  - `deluser`
  - `userinfo`
  - `useraudit`
  now persist contextual admin audit events with request metadata from the native admin CLI
- current session/device inventory coverage:
  - public `me/sessions` now returns additive session inventory summaries
  - public `me/security` now exposes the same session inventory summary for account-security views
  - private `userinfo` now includes a structured session inventory block for ops/moderation
  - private `listsessions` now exposes a dedicated per-user session list for ops/moderation
  - private `revokesession` now allows targeted session invalidation by session id
  - current per-session additive fields:
    - `session_state`
    - `client_summary`
    - `device.device_key`
    - `device.display_name`

Current private lobby-admin direction:

- first native lobby-control service is now live privately on `Nexus`
- exposed through SSH-tunneled admin CLI access, not publicly
- current command model:
  - `addlobby`
  - `listlobbies [limit] [query] [visibility] [status] [offset]`
  - `editlobby`
  - `closelobby`
  - `lobbyinfo`
- current live bridge:
  - `Nexus` now tunnels privately to the loopback-only `link` lobby-runtime service
  - add/edit/close on `Nexus` sync into the live lobby runtime on `link`
  - `lobbyinfo` on `Nexus` now includes runtime state when available
- current audit model:
  - `addlobby`
  - `editlobby`
  - `closelobby`
  - `lobbyinfo`
  now persist contextual admin audit events with request metadata from the native admin CLI

Current public path:

- `link` terminates TLS for `sekailink.com`
- `link` proxies `/api/identity/*` to `Nexus`
- `link` proxies `/api/room/*` to `Nexus`

Current mail path for identity flows:

- native identity service on `Nexus`
- local Postfix queue on `Nexus`
- SMTP relay to `link`
- outbound delivery from `link`
- identity/security emails must follow the user locale with fallback through SekaiLink i18n

## Migration note

This server replaces the DB/API role previously concentrated on `evolution`.

After cutover:

- `evolution` keeps web + mail
- `Nexus` becomes DB/API/core
- `link` keeps TLS/public edge and room runtime
