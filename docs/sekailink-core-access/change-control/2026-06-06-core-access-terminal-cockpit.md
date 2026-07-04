# Change Record

## Metadata

- Date: 2026-06-06
- Title: SekaiLink Core Access terminal cockpit
- Author: Codex SekaiLink Linux
- Related issue/incident: The first Core Access binary exposed a command shell,
  but operators need an actual full-screen terminal cockpit.

## Summary

Add the first full-screen terminal UI layer for SekaiLink Core Access.

The cockpit uses the existing command engine as its backend and provides:

- alternate-screen full terminal layout;
- colored status header;
- server matrix, hotkey panel, command/output panel, and command line;
- editable command input with left/right arrows, history up/down, backspace,
  Ctrl+A/E, Ctrl+W, and Tab completion;
- F1-F12 quick actions;
- `--shell` fallback to the previous line-oriented command shell;
- `--command` direct command mode remains unchanged.

## Scope

- Add local Rust TUI code under `tools/core-access`.
- No server mutation is added.
- No SKLMI, Client Core, or Sekaiemu change.
- Existing read-only gates and token behavior remain unchanged.

## Safety

- The TUI runs commands through the existing Core Access command path.
- Remote read-only commands still require explicit `--execute` and
  `SEKAILINK_CORE_ACCESS_REMOTE_READONLY=1`.
- Protected Nexus commands still require their token environment variable.
- Long-running command execution is bounded by a local timeout in the cockpit
  output panel; use `--shell` for manual log-follow sessions.

## Platform Impact

- Linux impact: adds a terminal cockpit for bastion usage.
- Windows impact: none.
- Client Core impact: none.
- Sekaiemu impact: none.
- SKLMI impact: none.

## Rollback

Revert the Core Access terminal cockpit commit. The existing command shell and
direct command mode are preserved as fallback behavior.

## Validation Plan

- `cargo test --manifest-path tools/core-access/Cargo.toml`
- `bash docs/sekailink-core-access/scripts/validate-docs.sh`
- `bash docs/sekailink-core-access/scripts/build-pdf.sh`
- Smoke:
  - `cargo run --manifest-path tools/core-access/Cargo.toml -- --command "help"`
  - `cargo run --manifest-path tools/core-access/Cargo.toml -- --shell`
  - manual full-screen launch in a terminal:
    `cargo run --manifest-path tools/core-access/Cargo.toml`

## Files

- `tools/core-access/src/tui.rs`
- `tools/core-access/src/app.rs`
- `tools/core-access/src/main.rs`
- `docs/sekailink-core-access/change-control/2026-06-06-core-access-terminal-cockpit.md`
- `docs/sekailink-core-access/change-control/changelog.md`
- `docs/sekailink-core-access/03-tui-navigation.md`
- `docs/sekailink-core-access/README.md`
- `docs/sekailink-core-access/quick-reference.md`
- Generated PDFs under `docs/sekailink-core-access/dist/pdf/`
