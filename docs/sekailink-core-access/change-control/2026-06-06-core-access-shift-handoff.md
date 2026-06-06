# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access shift handoff
- Author: Codex SekaiLink Linux
- Related issue/incident: Admin and Service users need a quick way to hand off
  recent local ops context between live shifts.

## Summary

Add local ops handoff commands:

- `ops timeline [query]`
- `ops handoff [label] [--file name]`

`ops timeline` merges local audit, incident events, notes, log pins, approvals,
maintenance drafts, scheduler drafts, and pack repo drafts into one chronological
terminal view.

`ops handoff` exports a Markdown shift report with dashboard, open incident
labels, recent timeline, recent local evidence, approvals, maintenance,
scheduler, pack repo drafts, banner drafts, and audit context.

## Scope

- Add local timeline rendering from existing Core Access JSONL stores.
- Add local Markdown handoff export under `~/.sekailink/core-access/exports/`.
- Update moderator/admin documentation and generated PDFs.
- No production service mutation is added.
- No SKLMI, Client Core, or Sekaiemu change.

## Safety

- Commands read local Core Access data only.
- Handoff writes a local export file only.
- No Nexus DB record is changed in this tranche.
- No server command, broadcast, maintenance flag, client signal, or bot action is
  executed.

## Platform Impact

- Linux impact: improves bastion shift handoff workflow.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert this change record and the matching Core Access shift handoff commit.
Existing JSONL stores remain unchanged and readable.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke:
  - `ops timeline`
  - `ops timeline stream-test`
  - `ops handoff stream-shift --file stream-shift.md`

## Files

- `tools/core-access/src/app.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/README.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-shift-handoff.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/14-incident-playbooks.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/16-training-moderators.md`
- `docs/sekailink-core-access/17-admin-runbook.md`
- `docs/sekailink-core-access/quick-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
