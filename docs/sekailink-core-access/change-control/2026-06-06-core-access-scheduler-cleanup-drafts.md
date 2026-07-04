# Change Control - Core Access Scheduler And Cleanup Drafts

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now has audited local draft workflows for scheduler and cleanup
administration:

- `schedule add`
- `schedule edit`
- `schedule pause`
- `schedule resume`
- `schedule run-now`
- `cleanup plan logs|db|spool|all`
- `cleanup apply`
- `cleanup history`
- `cleanup rollback`

## Safety

No scheduler job is armed, modified, paused, resumed, or executed. No cleanup
scan, deletion, DB mutation, backup, or server action is executed.

Admin commands require exact confirmations:

- `schedule:<name>:add`
- `schedule:<job>:edit`
- `schedule:<job>:pause`
- `schedule:<job>:resume`
- `schedule:<job>:run-now`
- `cleanup:<plan_id>:apply`
- `cleanup:<id>:rollback`

Cleanup plan commands are local Admin dry-run drafts and do not require a
confirmation because they do not inspect or mutate server state.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "schedule add release-check daily release verify latest --fast --confirm schedule:release-check:add"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "schedule pause release-check maintenance --confirm schedule:release-check:pause"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "cleanup plan all before release"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "cleanup apply cleanup-plan-1 reason --confirm cleanup:cleanup-plan-1:apply"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "cleanup apply cleanup-plan-1 reason --confirm cleanup:cleanup-plan-1:apply"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "cleanup history"
```

The Service `cleanup apply` command must be blocked by RBAC.

## Rollback

Code rollback: revert this change-control entry and the scheduler/cleanup draft
workflow changes.

Operational rollback: no production mutation occurs from this tranche. Supersede
incorrect drafts with a new note or incident record; do not rewrite historical
JSONL.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, room server, pack, or Lua logic was modified.
