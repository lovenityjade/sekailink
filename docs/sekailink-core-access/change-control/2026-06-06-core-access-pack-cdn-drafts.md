# Change Control - Core Access Pack CDN Drafts

- Date: 2026-06-06
- Owner: Core Access
- Scope: `tools/core-access`, Core Access docs
- Status: Implemented

## Summary

Core Access now has audited local draft workflows for pack administration:

- `pack repo add`
- `pack repo edit`
- `pack repo disable`
- `pack repo delete`
- `pack check`
- `pack validate`
- `pack stage`
- `pack publish`
- `pack rollback`
- `pack schedule-check`

## Safety

No pack repo is fetched. No pack is downloaded. No CDN artifact is staged,
published, deleted, or rolled back. No Lua logic is executed.

Mutating Admin commands require exact confirmations:

- `pack-repo:<id>:add`
- `pack-repo:<id>:edit`
- `pack-repo:<id>:disable`
- `pack-repo:<id>:delete`
- `pack:<id>:stage`
- `pack:<id>:publish`
- `pack:<id>:rollback:<version>`
- `pack:<id>:schedule-check`

`pack check` and `pack validate` are Service-level local drafts only.

## SKLMI Impact

No SKLMI, Sekaiemu, Client Core, tracker pack runtime, PopTracker-adapted pack,
or Lua logic was modified. This workflow intentionally avoids hardcoding a pack
repo and stores only the operator-provided repo id/URL/game/notes.

## Verification

Local validation:

```sh
cargo fmt --manifest-path tools/core-access/Cargo.toml -- --check
cargo test --manifest-path tools/core-access/Cargo.toml
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "pack repo add alttp-default https://example.invalid/alttp.zip alttp dry-run --confirm pack-repo:alttp-default:add"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "pack repo edit alttp-default url=https://example.invalid/new.zip --confirm pack-repo:alttp-default:edit"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role service --command "pack validate alttp-default smoke"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "pack stage alttp-default candidate --confirm pack:alttp-default:stage"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "pack rollback alttp-default v1 --confirm pack:alttp-default:rollback:v1"
cargo run --manifest-path tools/core-access/Cargo.toml -- --role admin --command "pack schedule-check alttp-default hourly --confirm pack:alttp-default:schedule-check"
```

## Rollback

Code rollback: revert this change-control entry and the pack draft workflow
changes.

Operational rollback: no production mutation occurs from this tranche. Supersede
incorrect drafts with a new note or incident record; do not rewrite historical
JSONL.
