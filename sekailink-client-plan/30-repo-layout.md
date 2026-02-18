# Repo contents for GitHub

This document describes the files that should be committed to the public repository and the files that should remain excluded or archived elsewhere so the repo stays under a reasonable size.

## Logical structure
- `client/`
  - `client/app/`: Electron renderer + main/preload sources, build scripts, packaging config, and release artifacts (AppImage). Keeps the UI, IPC helpers, and API glue.
  - `client/runtime/`: Python wrappers for the Archipelago clients (`commonclient_wrapper.py`, `patcher_wrapper.py`, etc.), plus per-game modules and Lua connectors.
- `CommonClient.py`, `BizHawkClient.py`, `SNIClient.py`, `ManualClient.py`, `MultiServer.py`: the Archipelago client entry points that SekaiLink orchestrates instead of re-implementing the protocol.
- `WebHostLib/` & `worlds/`: the server-side API (generation, room status, downloads, realtime logs) and the Multiworld worlds that define patches, autolaunch metadata, and trackers.
- `third_party/`: bundled emulators, `gamescope` helpers, and patched PopTracker CLI so the desktop client can control window layouts and tracker variants.
- `sekailink-client-plan/`: the architecture, workflow, and analysis documentation written during this sprint. Files such as `28-integration-workflow.md`, `29-status-workflow-snapshot.md`, `08-trackers.md`, and `10-server-apis-and-logs.md` explain contracts, features, and next actions.
- `sekailink-docs/`: support documentation (`CLIENT.md`, `POPTRACKER.md`, `ELECTRON-APP-PLAN.md`, `UI-PLAN.md`, etc.) that details the UI, tracker handling, emulator research, and autolaunch strategies.
- Configuration and metadata: `application.yaml`, `config.yaml`, `CLIENT_AUTOLAUNCH_RAW.json`, `CLIENT_EXTERNAL_PATCH_URLS.json`, `license`, `requirements.txt`, and delivery scripts (`ModuleUpdate.py`, `Updater.py`, `Launcher.py`).

## What to avoid committing
- `docs/`: this directory is heavy and redundant once the dedicated doc folders above are available. Do not tag it in the public repo.
- Generated or environment-specific files: `venv/`, `client/app/node_modules/`, `release/` outputs, `__pycache__/`, `logs/`, `userData/`, and compressed bundles such as `sekailink-laptop-pack.zip` belong to the build artifacts or cache.
- Platform-specific installers: `inno_setup.iss`, `ci-requirements.txt`, etc. should only live in the repository if they are consumed by CI; otherwise keep them in a separate archive.

## Summary
Commit the code and configuration that define the core client/server portions (`client/`, `CommonClient.py`, `WebHostLib/`, `worlds/`, `third_party/`, documentation folders), keep large binaries or generated content out of Git. This list is the starting point for the next GitHub repository snapshot—refer back to `sekailink-client-plan/30-repo-layout.md` whenever the structure evolves.
