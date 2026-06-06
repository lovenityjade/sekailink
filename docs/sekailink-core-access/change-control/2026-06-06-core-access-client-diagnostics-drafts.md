# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access client diagnostics drafts
- Author: Codex SekaiLink Linux
- Related issue/incident: Operators need a way to request Sekaiemu and SKLMI
  client-side logs from a user for bug diagnosis.

## Summary

Add local client diagnostics commands:

- `client diagnostics-request <user> <incident> <reason> [--include ...]`
- `client diagnostics-list [query]`
- `client diagnostics-export [query] [--file name]`

The request command records a local `draft-consent-required` diagnostics request
with an include set such as `client-core`, `sekaiemu`, `sklmi`, `configs`, and
`system`.

The export command writes local Markdown containing the queued requests and the
expected future client diagnostics bundle contract.

## Scope

- Add local diagnostics request storage under
  `~/.sekailink/core-access/client-diagnostics/`.
- Add local request/list/export commands.
- Add diagnostics entries to `ops timeline` and `ops handoff`.
- Document the expected Client Core, Sekaiemu, and SKLMI bundle files.
- No client signal is sent in this tranche.
- No production service mutation is added.
- No SKLMI, Client Core, or Sekaiemu code change.

## Safety

- User consent is required before any future client-side upload.
- Commands write local Core Access JSONL/Markdown only.
- No Nexus DB record is changed in this tranche.
- No client file is collected by Core Access in this tranche.
- Future SKLMI collection hooks require explicit Jade approval before any SKLMI
  file is modified.

## Platform Impact

- Linux impact: improves bastion diagnostics planning.
- Windows impact: none.
- Client Core impact: none in this tranche; future implementation requires the
  linked connectivity contract.
- Sekaiemu impact: none in this tranche; future implementation must expose or
  collect logs without changing runtime behavior.
- SKLMI impact: none in this tranche; future implementation requires explicit
  Jade approval if SKLMI code must change.

## Rollback

Revert this change record and the matching Core Access diagnostics draft commit.
Existing diagnostics request JSONL files remain local and readable.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke:
  - `client diagnostics-request JoueurSansFromage sklmi-room-1 "SKLMI runtime failed" --include sekaiemu,sklmi,client`
  - `client diagnostics-list JoueurSansFromage`
  - `client diagnostics-export JoueurSansFromage --file diagnostics-smoke.md`
  - `ops timeline diagnostics`

## Files

- `tools/core-access/src/app.rs`
- `tools/core-access/src/audit.rs`
- `tools/core-access/src/commands.rs`
- `tools/core-access/README.md`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-client-diagnostics-drafts.md`
- `docs/sekailink-core-access/change-control/2026-06-06-client-diagnostics-connectivity-contract.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/05-logs-and-debugging.md`
- `docs/sekailink-core-access/08-client-signals.md`
- `docs/sekailink-core-access/14-incident-playbooks.md`
- `docs/sekailink-core-access/15-command-reference.md`
- `docs/sekailink-core-access/quick-reference.md`
- `docs/sekailink-core-access/commands/registry.txt`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
