# Change Control - Core Access Server Update And SSH Gates

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now supports the final server access commands from the registry:

- `server update plan`
- `server update apply`
- `ssh open`

## Safety

`server update plan` writes an append-only local draft and prints an update
checklist. It does not execute package managers, git commands, service restarts,
or remote mutations.

`server update apply` also writes an append-only local draft only. It requires
role Admin and exact confirmation:

```text
server-update:<server>:apply
```

`ssh open` writes an audit draft before opening any interactive session. It
requires role Admin and exact confirmation:

```text
ssh:<server>:open
```

The real interactive SSH process is blocked unless both are present:

- `--execute`
- `SEKAILINK_CORE_ACCESS_SSH_OPEN=1`

## Server Scope

The command resolves the same configured server aliases as the admin-agent
surface:

- `nexus` -> `nexus-vps`
- `link` -> `link-vps`
- `worlds` -> `worlds-vps`
- `evolution` -> `evolution-vps`

Pulse is intentionally not included until the Pulse admin-agent/server profile
is declared in Core Access.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "server update plan link release-check"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "server update apply link plan-1 --confirm server-update:link:apply"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "ssh open link inspect --confirm ssh:link:open"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "ssh open link inspect --confirm ssh:link:open"
```

The Service `ssh open` command must be blocked by RBAC.

## Rollback

Code rollback: revert this change-control entry, server update/SSH command
dispatch, command registry readiness, and docs.

Operational rollback: no production update mutation occurs from this tranche.
Supersede incorrect drafts with new drafts or incident notes; do not rewrite
historical JSONL.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, room server, pack, or Lua logic was modified.
