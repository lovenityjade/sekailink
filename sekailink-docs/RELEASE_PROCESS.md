# SekaiLink Release Process

## Scope
This process covers desktop client releases (Linux/Windows artifacts) and server-facing compatibility checks.

## 1. Pre-release checks
Run from `client/app`:

```bash
npm ci
npm run lint
npm run build
```

Run smoke from repo root:

```bash
bash scripts/headless-smoke.sh --mode integration --wrapper common
```

Verify security gates:
- updater hash verification paths are enabled
- Electron IPC hardening paths are enabled
- no secrets logged in JSONL logs

## 2. Versioning
- Bump version in `client/app/package.json` (and lockstep places if required by packaging scripts).
- Keep a single monotonic prerelease stream (`beta.x-topaz`) until stable release policy is introduced.

## 3. Build artifacts
From `client/app`:

```bash
npm run electron:pack
npm run electron:pack:win
```

For UI prototype artifact:

```bash
npm run electron:pack:ui-prototype
npm run electron:pack:win:ui-prototype
```

## 4. Publish update payloads
- Upload release binaries and/or incremental patch payloads to server download paths.
- Update server config values for:
  - latest version
  - download URLs
  - release hashes
  - release signature (if enabled)
  - incremental manifest URL/path

If signature verification is enabled in the client:
- provide detached signature in update metadata
- ensure client has `SEKAILINK_UPDATE_PUBLIC_KEY` configured

## 5. Post-deploy validation
- Fresh install launch (Linux + Windows)
- Auto-update flow (download, apply, relaunch, version changed)
- Launch one BizHawk flow + one tracker flow
- Verify room connect, item send/receive, and terminal command path

## 6. Rollback
If release is bad:
- Repoint latest version + download URL/hash back to previous known-good build.
- Keep old artifacts available until all clients converge.
- Log incident summary in `sekailink-client-plan/` with root cause and corrective actions.

Linux self-update keeps `.bak` fallback during replace so failed relaunch can be restored.
