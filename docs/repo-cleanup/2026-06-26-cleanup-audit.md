# SekaiLink Repository Cleanup Audit

Date: 2026-06-26

## Why This Exists

The repository currently contains several historical source surfaces for the
same live services. On 2026-06-25, a live `sekailink-chat-api` reset-route fix
was accidentally rebuilt from an older source tree on `link.sekailink.com`.
The binary was restored from backup, but the incident proves the repo and live
host need a stricter source-of-truth map before more server work.

## Immediate Rule

Do not deploy from any `source-snapshots/` tree until that tree has been
explicitly reconciled against the active service source and marked current.

Do not rebuild a live binary from a host-local source tree merely because it
contains a matching filename. First identify:

- the active systemd unit;
- the live binary path;
- the source tree used for the last successful deployment;
- the config file in use;
- the service changelog entry matching that deployment.

## Current Live Incident State

Service: `sekailink-chat-api.service`

Live binary:

```text
/opt/sekailink/link/chat-api/bin/sekailink_chat_api_service
```

Backup restored:

```text
/opt/sekailink/link/chat-api/backups/sekailink_chat_api_service.before-generation-reset-20260625T235800Z
```

Result:

- The service is active again.
- The accidentally deployed older rebuild was reverted.
- The stuck SNES lobby generation row was manually cleared from
  `lobby_generation_state`.
- The live binary does not currently contain the new `generation/reset` route.

Follow-up observation:

- A later retry recreated the SNES lobby row as `failed`.
- The reported generator error was the DKC3 base ROM path:
  `Donkey Kong Country 3 - Dixie Kong's Double Trouble! (USA) (En,Fr).sfc`
  was not found by `/opt/sekailink-generate`.
- The DKC3 ROM/config path was reconciled on the active live generator path.
- A GB/GBC retry also exposed an older Wario Land APWorld issue:
  `Utils.get_options() is deprecated. Use the settings API instead.`
- The live Wario Land loader was patched to use `get_settings()`, the validated
  ROM was installed under `/opt/sekailink-generate/roms/`, and the active
  `/opt/sekailink-generate/host.yaml` now points to it with an absolute path.
- A live generator venv smoke now confirms both DKC3 and Wario Land base ROM
  loaders succeed.

## Known Source Surfaces

### Client Core

Primary working surface:

```text
apps/client-core/
```

Notes:

- Very large diff.
- Contains recent BETA-3 UI/runtime/lobby changes.
- Needs separate cleanup pass after server source-of-truth is stabilized.

### Sekaiemu

Primary working surface:

```text
apps/sekaiemu/
```

Notes:

- Active runtime/debug UI work lives here.
- NES is frozen validated and should not be modified without reproducible logs.

### SKLMI

Primary working surface:

```text
services/sklmi/
```

Notes:

- Runtime bridge helper refactor has begun.
- Continue with small, testable changes only.

### Link Social / Chat API

Candidate current repo surface:

```text
services/link-social/src/
```

Candidate split snapshot:

```text
services/link-social/source-snapshots/monolith-core/
```

Deployment helpers:

```text
services/link-social/deploy/link/chat-api/
```

Risk:

- `services/link-social/source-snapshots/monolith-core/` looks more complete
  than the active live host source, but its local build dependencies are not
  currently cleanly available in the repo root.
- The live host also has older source trees under `/opt/sekailink-server/src/`.
  Those are not guaranteed to match the live binary.

Required before next deploy:

- Establish a single build recipe for chat-api from repository source.
- Build in a clean temp directory.
- Confirm the output binary contains expected new symbols/routes.
- Smoke test locally or against loopback before replacing the live binary.

### Link Room

Historical/snapshot surface:

```text
services/link-room/source-snapshots/monolith-core/
```

Risk:

- Contains another `chat_api_service.cpp` compatibility layer.
- It is easy to confuse with `link-social`.
- Do not patch this for live chat-api unless the active systemd service proves
  it is using this binary/source path.

### Worlds

Primary service notes:

```text
services/worlds/CHANGELOG.md
services/worlds/server/native/sekailink_server_core/
```

Notes:

- Recent live generator fixes are documented.
- DKC3 ROM installation is documented and should not be repeated blindly.

### Nexus

Primary service notes:

```text
services/nexus/CHANGELOG.md
services/nexus/server/native/sekailink_server_core/
```

Notes:

- Seed config schemas live here.
- Do not mix Nexus config work with runtime launch debugging.

## Cleanup Buckets

### Keep Active

These are active working surfaces:

- `apps/client-core/`
- `apps/sekaiemu/`
- `services/sklmi/`
- `services/nexus/`
- `services/worlds/`
- `services/link-social/src/`
- `services/link-social/deploy/`
- `runtime/game-registry/`
- `runtime/modules/`
- `runtime/ap/`
- `tools/room-admin-tui/`

### Quarantine Candidates

Do not delete yet. Mark as historical or move later only after diffing:

- `services/link-room/source-snapshots/`
- `services/link-social/source-snapshots/`
- `services/evolution/source-snapshots/`
- `services/sklmi/source-snapshots/`
- `services/worlds/source-snapshots/`
- `linkedworlds/*/source-snapshots/`

### Generated/Local Build Artifacts

These should not be committed unless explicitly intended:

- `apps/client-core/tsconfig.tsbuildinfo`
- `runtime/bin/sekaiemu_libretro_spike`
- `runtime/platforms/*/bin/sekaiemu_libretro_spike*`
- `runtime/poptracker/sekailink-poptracker*`
- `apps/client-core/imgui.ini`
- any local `build/` output

### Vendored Runtime Assets

These may be intentional, but need a manifest:

- `runtime/cores/`
- `runtime/platforms/*/cores/`
- `runtime/tools/python/wheelhouse/`
- `runtime/downloaded-resources/`
- `runtime/tracker-bundles/`

Before committing, each asset group needs:

- source URL or upstream package;
- version/date;
- license note;
- reason it is embedded.

## Required Cleanup Sequence

1. Write a live service map.
   - One table: service name, systemd unit, host, live binary, live config,
     canonical repo source, deploy command, rollback path.

2. Freeze all `source-snapshots/`.
   - Add README warnings to every active snapshot directory.
   - State whether it is historical, candidate, or reconciled.

3. Fix chat-api reset properly.
   - Reconcile `services/link-social/src` with the live binary behavior.
   - Build from canonical repo source, not VPS-local source.
   - Add a smoke test for `POST /api/lobbies/:id/generation/reset`.
   - Deploy only after the smoke proves the route exists and generation handoff
     behavior is unchanged.

4. Split the dirty worktree by domain.
   - Client Core UI/runtime changes.
   - Sekaiemu runtime/debug UI changes.
   - SKLMI bridge changes.
   - Runtime/APWorld/resources changes.
   - Server/Nexus/Worlds/Link changes.
   - Docs and operational notes.

5. Add commit guards.
   - Ignore local build outputs.
   - Keep generated binaries out unless they are intentional release artifacts.
   - Avoid committing live cache/state files.

6. Only then continue runtime validation.
   - NES remains frozen.
   - Next validation should use fresh rooms and clear core/system labels.

## Open Questions

- Which repo surface is the true canonical source for the live
  `sekailink-chat-api.service` binary?
- Should `link-room/source-snapshots` remain in this repo or be archived outside
  the active development tree?
- Which generated runtime binaries are release artifacts versus local test
  outputs?
- Should server deploys be blocked unless a `LIVE_SERVICE_MAP.md` entry exists?

## Current Recommendation

Do not keep coding new runtime behavior until the live service map and snapshot
warnings are in place. The cost of another wrong-source deploy is now higher
than the cost of pausing for cleanup.
