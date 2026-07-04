# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access ops doctor and local inventory
- Author: Codex SekaiLink Linux
- Related issue/incident: Core Access operators need a fast readiness check
  before public moderation shifts and release-day support.

## Summary

Add local Core Access operations commands:

- `ops doctor [--verbose]`
- `ops paths`
- `ops exports [query]`

`ops doctor` reports local cockpit readiness: data paths, local JSONL stores,
external tools, docs/PDF presence, known log/service counts, export count, and
Core Access environment gates. Sensitive token variables are reported only as
set or missing; their values are never printed.

## Scope

- Add local self-check and inventory commands under `ops`.
- Add command registry/autocomplete entries.
- Add help text and moderator documentation.
- Add generated PDF documentation updates.
- No server mutation is added.
- No Client Core, Sekaiemu, or SKLMI code is changed.

## Safety

- Commands read local files and environment variable presence only.
- Sensitive environment variables are redacted.
- `ops exports` lists local Core Access export files only.
- No SSH command is executed by this tranche.
- No Nexus DB record is changed.

## Platform Impact

- Linux impact: local Core Access readiness checks on the bastion.
- Windows impact: none for shipped client builds.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none; no SKLMI file is modified.

## Rollback

Revert this change record and the matching Core Access command commit. Existing
local exports and JSONL stores remain readable.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `cargo run --manifest-path tools/core-access/Cargo.toml -- --data-dir /tmp/sekailink-core-access-doctor-smoke --command "ops doctor --verbose"`
- `cargo run --manifest-path tools/core-access/Cargo.toml -- --data-dir /tmp/sekailink-core-access-doctor-smoke --command "ops paths"`
- `cargo run --manifest-path tools/core-access/Cargo.toml -- --data-dir /tmp/sekailink-core-access-doctor-smoke --command "ops exports"`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`

## Files

- `tools/core-access/src/app.rs`
- `tools/core-access/src/commands.rs`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/16-training-moderators.md`
- `docs/sekailink-core-access/17-admin-runbook.md`
- `docs/sekailink-core-access/quick-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-ops-doctor.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
