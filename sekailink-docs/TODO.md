# TODO.md

Note: Completed items should be moved into `CONTEXT.md`.

## UI/UX Beta Backlog (fidelity to ui-context.md)
- QA follow-up after live testing: validate landing/room list/friends/profile fidelity vs mockups (spacing, sizing, glass levels, gradients).
- Confirm SFX mapping in live environment (autoplay unlock, mute toggle state).
- Verify backgrounds and glass overlays remain correct on all app pages after cache refresh.
- Check tracker embed styling in real rooms (purple background, no header).
- QA Game Manager: tabs, duplicate, custom save modal, custom YAML badge.
- QA Lobby rule: custom YAMLs blocked when disabled.

## Platform/Feature Backlog
- Admin panel: allow administrators to change lobby and room timeout values.
- Patreon integration: wire secrets, add migrations, add link UI on account (optional).
- Basic DM chatbot to answer common questions and guide users.
- Localization (common app languages): English, Spanish, French, German, Portuguese (Brazil), Italian, Japanese, Korean, Chinese (Simplified), Arabic, Russian, Hindi, Indonesian.

## Electron Client (Emulator Integration)
- Integrate CommonClient as a Python child process with IPC to Electron UI (no user commands).
- Add Runtime Orchestrator + PatchService + EmulatorService (BizHawk) + TrackerService (PopTracker).
- Implement game modules in-repo (`client/runtime/modules/*`) with manifests and runners.
- Add global config file `~/.sekailink/config.json` for ROM paths and emulator overrides.
- Implement one-button flow: patch -> connect -> launch emu + lua -> launch tracker.
- Verify AppImage is loading latest UI (Settings should show "Import ROM File" instead of "Scan ROM Folder").
