# SekaiLink Repo Recovery Map

Date: 2026-06-20

Purpose: reduce cognitive load without moving or deleting files.

This document is a map of the current repository state after the intense
Beta-3 push. It does not decide the future of SekaiLink. It only separates the
work into understandable shelves.

## Current Git State

- Branch: `main`
- Remote: `origin/main`
- Published latest commit: `b9c0322 Restore landing with hiatus notice`
- Public site state: original landing restored with a temporary health-hiatus
  banner.
- Worktree: intentionally dirty; do not mass-stage.

## High-Level Shelves

### 1. Published / Stable Enough

These are already pushed to GitHub or deployed:

- Public website hiatus banner.
- Earlier Core Access documentation commits.
- Earlier game support registry commits.
- Earlier generic BizHawk protocol work already on `origin/main`.

### 2. Dirty But Potentially Valuable

These are uncommitted changes that may contain useful work and should not be
discarded blindly:

- `apps/client-core/`
  - redesigned UI work;
  - settings work;
  - chat page;
  - Pulse page;
  - runtime lab page;
  - bootloader packaging experiments;
  - native bootloader source.
- `apps/sekaiemu/`
  - ImGui/runtime menu experiments;
  - layout preview;
  - tracker rendering experiments;
  - bug report client.
- `runtime/`
  - Archipelago client wrappers;
  - wrapper supervisor;
  - bootchain supervisor;
  - downloaded APWorld/resource inventory;
  - Windows cores and PopTracker runtime experiments.
- `services/`
  - Nexus/Link/SKLMI compatibility and report/API changes.
- `apps/admin.sekailink.com/`
  - admin web panel prototype.
- `tools/`
  - Pulse console;
  - resource import tools;
  - Windows worker scripts.

### 3. Heavy Vendor / Archive Material

These are large and should not be committed casually:

- `third_party/upstream/dolphin/`
- `third_party/upstream/shipwright-harkipellago/`
- `third_party/upstream/sm64ex/`
- `third_party/upstream/_archives/`
- `runtime/ap/worlds/`
- `runtime/downloaded-resources/`

Current rough sizes:

- `apps/client-core`: about 20G, mostly `release/`, `dist/`, and
  `node_modules/`.
- `apps/sekaiemu`: about 1.8G.
- `third_party`: about 1.2G.
- `runtime`: about 626M.
- `services`: about 447M.

## Do Not Do Yet

- Do not `git add -A`.
- Do not delete generated folders.
- Do not move `third_party`, APWorlds, or downloaded resources.
- Do not resume CDN/client releases.
- Do not decide tracker/native/runtime strategy while tired or distressed.
- Do not collapse Client Core, Sekaiemu, SKLMI, PopTracker, Admin, and servers
  into one task.

## Safe Next Cleanup Steps

These are safe because they are non-destructive:

1. Create focused inventory documents per shelf.
2. Run targeted `git diff -- <area>` before any commit.
3. Commit documentation-only cleanup first.
4. Decide later whether heavy vendor trees should be:
   - ignored;
   - converted to submodules;
   - stored outside git;
   - archived as local-only reference.
5. Decide later whether APWorld/downloaded resources should be source,
   generated cache, or external package metadata.

## Suggested Re-entry Order

If SekaiLink resumes, resume in this order:

1. **Project state only**
   - Read this file.
   - Read `docs/ACTIVE_DIRECTIVE.md`.
   - Do not code yet.

2. **Client Core**
   - Treat as frozen product shell.
   - Only fix obvious crash/blocking issues.
   - Avoid new feature work.

3. **Bootloader**
   - Treat as frozen unless install/update is broken.

4. **Sekaiemu**
   - Keep as emulator decision.
   - Only one question at a time: window/layout, memory bridge, or menu.

5. **SKLMI / wrappers**
   - Keep wrapper strategy as compatibility-first.
   - Avoid native replacement work until there is a live test plan.

6. **Admin**
   - Important later, but not part of release anxiety.

7. **Tracker**
   - PopTracker Edition is the Beta-3-compatible path.
   - Native tracker remains research, not release blocker.

## Mental Model

SekaiLink is not one giant problem.

It is five smaller systems:

- Website / public communication.
- Client shell.
- Emulator runtime.
- Compatibility wrappers.
- Server/admin operations.

Only one of those should be active at a time.

For now, the active system is:

**Recovery and clarity.**
