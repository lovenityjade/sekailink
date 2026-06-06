# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access admin-agent commands
- Author: Codex SekaiLink Linux
- Related issue/incident: Core Access needs production server visibility and
  controlled service actions before public release.

## Summary

Add Core Access commands for existing loopback admin-agent routes:

- `server agent-health [server|all]`
- `server agent-system [server|all]`
- `server agent-services [server|all]`
- `server agent-service <server> <service>`
- `server agent-logs <server> <service>`
- `server restart <server> <service>`
- `server start <server> <service>`
- `server stop <server> <service>`

Read-only commands execute only with `--execute` and
`SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`. Mutating commands require Admin role,
`--execute`, `SEKAILINK_CORE_ACCESS_REMOTE_MUTATION=1`, and an exact
`--confirm <server>:<service>:<action>` value.

## Scope

- Add a new Rust `admin_agent` module for dry-run and gated execution plans.
- Add command registry/autocomplete entries.
- Add CLI help and docs.
- Use existing admin-agent HTTP contract: `/health`, `/system`, `/services`,
  `/services/{service}`, `/services/{service}/logs`,
  `/services/{service}/{restart|start|stop}`.
- No server deployment is performed.
- No Client Core, Sekaiemu, or SKLMI code is changed.

## Safety

- Tokens are read from local environment variables and never printed.
- Dry-run is the default.
- Read-only and mutation gates are separate.
- Mutation requires a second exact confirmation string.
- Pulse is intentionally left without an admin-agent profile until a Pulse agent
  is declared/deployed.

## Token Environment

- `SEKAILINK_CORE_ACCESS_NEXUS_AGENT_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_LINK_AGENT_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_WORLDS_AGENT_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_EVOLUTION_AGENT_ADMIN_TOKEN`
- `SEKAILINK_CORE_ACCESS_AGENT_ADMIN_TOKEN` as a generic non-Nexus fallback
- `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN` remains Nexus fallback compatible.

## Platform Impact

- Linux impact: Core Access can now plan and gate admin-agent HTTP calls over SSH.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert this change record and the matching Core Access admin-agent command
commit. No server state is changed by reverting.

## Validation Plan

- `cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check`
- `cargo test --manifest-path tools/core-access/Cargo.toml`
- Smoke dry-run:
  - `server agent-health all`
  - `server agent-system nexus`
  - `server agent-logs link room-server`
  - `server restart link room-server --confirm link:room-server:restart --execute`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`

## Files

- `tools/core-access/src/admin_agent.rs`
- `tools/core-access/src/app.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/src/main.rs`
- `docs/sekailink-core-access/05-logs-and-debugging.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/17-admin-runbook.md`
- `docs/sekailink-core-access/quick-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
