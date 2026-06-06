# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access local incident workspaces
- Author: Codex SekaiLink Linux
- Related issue/incident: Public opening preparation; operators need a concrete
  way to group live debugging evidence by incident.

## Summary

Add local append-only incident workspace commands:

- `incident open <label> <severity> <summary>`
- `incident list [query]`
- `incident status <label>`
- `incident note <label> <text>`
- `incident pin <label> <source> <text>`
- `incident export <label> [--file name]`
- `incident close <label> <resolution>`

Each incident action writes a local JSONL event under Core Access data. Notes use
the existing notes store with target `incident:<label>`. Pins use the existing
log pin store with text prefixed by `incident:<label>`.

## Scope

- Add local incident event storage under `~/.sekailink/core-access/incidents/`.
- Add incident status and Markdown export rendering.
- Update TUI F4 to prepare `incident note`.
- Update moderator/admin documentation and generated PDFs.
- No production service mutation is added.
- No SKLMI, Client Core, or Sekaiemu change.

## Safety

- Incident labels are restricted to letters, numbers, `_`, `-`, `.`, and `:`.
- Incident commands write local append-only files only.
- No Nexus DB incident record is changed in this tranche.
- No server command, broadcast, maintenance flag, or client signal is executed.

## Platform Impact

- Linux impact: improves bastion incident workflow.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert this change record and the matching Core Access incident workspace commit.
Existing audit, note, log pin, and export files remain readable because the
storage is plain JSONL/Markdown.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke:
  - `incident open stream-test sev4 smoke test`
  - `incident note stream-test first note`
  - `incident pin stream-test link:room-runtime sample evidence`
  - `incident status stream-test`
  - `incident export stream-test --file stream-test.md`
  - `incident close stream-test resolved`

## Files

- `tools/core-access/src/app.rs`
- `tools/core-access/src/audit.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/src/tui.rs`
- `tools/core-access/README.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-incident-workspaces.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/03-tui-navigation.md`
- `docs/sekailink-core-access/14-incident-playbooks.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/16-training-moderators.md`
- `docs/sekailink-core-access/17-admin-runbook.md`
- `docs/sekailink-core-access/quick-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
