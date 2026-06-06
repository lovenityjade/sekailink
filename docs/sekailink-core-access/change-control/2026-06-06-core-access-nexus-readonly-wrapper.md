# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access Nexus read-only wrapper
- Author: Codex SekaiLink Linux
- Related issue/incident: Core Access needs private Nexus administration access
  for launch operations without exposing mutation commands too early.

## Summary

Add Core Access wrappers for known private Nexus read-only surfaces:

- `nexus services`
- `lobby list`
- `lobby open`
- read-only planning for `user search`, `user open`, `user sessions`,
  `user devices`, and `user audit`

Protected Nexus HTTP routes remain dry-run by default. Live execution requires
`--execute`, the read-only execution gate, and a local admin token environment
variable. Identity user commands do not execute yet because the live private
Identity admin CLI/API path has not been found in the current checkout or on the
Nexus host through read-only discovery.

## Scope

- Add local Rust command routing in `tools/core-access`.
- Add no server mutation.
- Add no SKLMI, Client Core, or Sekaiemu change.
- Add documentation and command reference updates.

## Connectivity Contract

- Nexus admin-agent:
  - Host: `nexus-vps`
  - URL: `http://127.0.0.1:19091/services`
  - Method: `GET`
  - Auth: `Authorization: Bearer <admin token>`
  - Capability: Service/Admin read-only service inventory
- Nexus lobby-admin:
  - Host: `nexus-vps`
  - URL: `http://127.0.0.1:19096/admin/lobbies`
  - Method: `GET`
  - Auth: `Authorization: Bearer <admin token>`
  - Capability: Service/Admin read-only lobby inventory
- Nexus lobby-admin single lobby:
  - Host: `nexus-vps`
  - URL: `http://127.0.0.1:19096/admin/lobbies/{lobby_id}`
  - Method: `GET`
  - Auth: `Authorization: Bearer <admin token>`
  - Capability: Service/Admin read-only lobby detail
- Nexus identity admin:
  - Status: planned only in this tranche
  - Reason: documented command model exists, but exact live CLI/API entrypoint
    is not present in the repo checkout and was not discovered through
    read-only host inspection.

## Safety

- Default mode is dry-run.
- Live read-only execution requires:
  - command flag: `--execute`
  - environment gate: `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`
  - local token variable: `SEKAILINK_CORE_ACCESS_NEXUS_ADMIN_TOKEN`
- Token values are not printed in dry-runs or execution logs.
- Mutating commands remain planned and blocked.

## Platform Impact

- Linux impact: Core Access command additions only.
- Windows impact: none; this is an ops tool in the Linux bastion workspace.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert the Core Access wrapper commit. Since the change only adds local command
plans and gated read-only HTTP execution, there is no server data rollback.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke dry-runs:
  - `nexus services`
  - `lobby list`
  - `lobby open <id>`
  - `user search <query>`

## Files

- `tools/core-access/src/nexus.rs`
- `tools/core-access/src/app.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/src/main.rs`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-nexus-readonly-wrapper.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/06-users-and-configs.md`
- `docs/sekailink-core-access/07-lobbies-and-rooms.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
