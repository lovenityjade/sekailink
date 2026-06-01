# Worlds Server

Date: 2026-03-26

## Role

`worlds` remains the heavy backend host during migration.

It owns:
- Generation Server
- SMART Server
- Git bare SSH server

## Logical layout

```text
/opt/sekailink/worlds/
  generation-server/
  smart-server/
  git/
  admin-agent/
  logs/
```

## Migration rule

Generation remains Python-backed for now to avoid breaking the current generation path while the rest of the server stack is refactored.

This is an intentional stability decision, not an architectural end state.

## Native generation orchestration

The first native C++ generation orchestration foundation now exists in:

- `server/native/sekailink_server_core/include/sekailink_server/generation_service.hpp`
- `server/native/sekailink_server_core/src/generation_service.cpp`
- `server/native/sekailink_server_core/include/sekailink_server/generation_server.hpp`
- `server/native/sekailink_server_core/src/generation_server.cpp`

This slice is intentionally an orchestrator, not a replacement for the Python generator itself.

Deployment/runtime templates for the first native `worlds` generation service now exist in:

- `deploy/worlds/generation-server/`

That generation slice now also writes a structured runtime state file.

Current state payload includes:

- `tcp_port`
- `running`
- `total_requests`
- `total_errors`
- `job_counts`
- `updated_at`

Current runtime path on `worlds`:

- binary:
  - `/opt/sekailink/worlds/generation-server/bin/sekailink_generation_server_service`
- config:
  - `/opt/sekailink/worlds/generation-server/config/generation_server.json`
- state:
  - `/opt/sekailink/worlds/generation-server/data/generation_server_state.json`
- systemd:
  - `sekailink-generation-server.service`

The first native `worlds` admin agent is now also deployed as a loopback-only ops surface.

Current runtime path:

- binary:
  - `/opt/sekailink/worlds/admin-agent/bin/sekailink_admin_agent_service`
- config:
  - `/opt/sekailink/worlds/admin-agent/config/admin_agent.json`
- systemd:
  - `sekailink-worlds-admin-agent.service`

Current private services exposed there:

- `generation-server`
- `smart-server`

Current admin CLI direction:

- private SSH-tunneled generation control is now part of the native admin terminal
- current command model:
  - `submitgenjob`
  - `listgenjobs [limit] [query] [status] [sort_by] [order] [requested_after] [requested_before] [offset]`
  - `genjobinfo`

Generation discovery on `worlds` now also supports bounded sort/time filters for read-only ops inspection:

- `sort_by`:
  - `job_id`
  - `requested_at`
  - `status`
- `order`:
  - `asc`
  - `desc`
- `requested_after`
- `requested_before`

This is intended for safer ops discovery and backlog inspection without submitting production jobs.
