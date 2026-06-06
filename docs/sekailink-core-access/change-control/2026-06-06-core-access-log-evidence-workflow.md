# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access log evidence workflow
- Author: Codex SekaiLink Linux
- Related issue/incident: Core Access operators need to isolate, pin, annotate,
  and export important log evidence during live support without mutating
  production services.

## Summary

Add implemented Core Access log evidence commands:

- `logs search <query> [source|all] [--execute]`
- `logs filter <term...> [source:<source|all>] [--execute]`
- `logs pin <source> <text>`
- `logs note <source> <text>`
- `logs export [query] [--format md|jsonl|txt] [--file name]`

Search and filter commands produce dry-run read-only SSH plans by default.
Execution remains gated by `--execute` and
`SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`.

Pins and notes are local Core Access records. Exports write local evidence files
under `~/.sekailink/core-access/exports/`.

## Scope

- Add local log pins under Core Access data.
- Add local evidence export rendering in Markdown, JSONL, and TXT.
- Add read-only log search/filter plan generation from the existing allowlisted
  log catalog.
- Normalize common filter labels such as `user:`, `lobby:`, `room:`, and
  `correlation:` to their search values before building grep pipelines.
- Update docs and PDFs.
- No production service mutation is added.
- No SKLMI, Client Core, or Sekaiemu change.

## Safety

- Remote log search/filter cannot run unless the operator passes `--execute`
  and the environment gate is set.
- Search/filter plans use the existing allowlisted log source catalog.
- Pins, notes, and exports are local files only.
- No Nexus DB incident record is changed in this tranche.

## Platform Impact

- Linux impact: improves bastion incident workflow.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert this change record and the matching Core Access log evidence workflow
commit. Existing notes, audit, and prior command shell behavior remain
compatible.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke:
  - `logs search route_not_found link:chat-api`
  - `logs filter user:certo source:link:room-runtime`
  - `logs pin link:room-runtime "sample line"`
  - `logs note link:room-runtime "sample note"`
  - `logs export sample --format md`

## Files

- `tools/core-access/src/app.rs`
- `tools/core-access/src/audit.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/src/system.rs`
- `tools/core-access/README.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-log-evidence-workflow.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/03-tui-navigation.md`
- `docs/sekailink-core-access/05-logs-and-debugging.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/quick-reference.md`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
