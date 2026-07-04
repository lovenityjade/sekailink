# SekaiLink Canonical Root Migration - 2026-06-27

## Status

Completed.

SekaiLink's active source tree now lives at:

```text
<local-home>/SekaiLink/canonical
```

The old `DevSSD` canonical location was removed:

```text
<local-home>/DevSSD/sekailink-canonical
```

## Active Tree

```text
<local-home>/SekaiLink/
  canonical/      active Git working tree
  archives/       cold archives, not part of Git
  reports/        external reports
  scratch/        local scratch space
```

## Archived Legacy Sources

Legacy and quarantine sources were compressed to:

```text
<local-home>/SekaiLink/archives/cold-legacy-2026-06-27/
```

Archives created and verified:

```text
_sekailink_quarantine.tar.zst
_sekailink_quarantine.tar.zst.sha256
_sekailink_quarantine.manifest.tsv

sekailink-legacy-quarantine-2026-05-17.tar.zst
sekailink-legacy-quarantine-2026-05-17.tar.zst.sha256
sekailink-legacy-quarantine-2026-05-17.manifest.tsv
```

Both archives were validated with `sha256sum -c`, `zstd -t`, and a `tar` listing check before deleting the source directories.

## Removed Source Directories

These directories were deleted after archive verification:

```text
<local-home>/DevSSD/_sekailink_quarantine
<local-home>/DevSSD/sekailink-legacy-quarantine-2026-05-17
```

## Runtime Path Cleanup

Active runtime fallbacks were updated to avoid legacy roots:

- Sekaiemu Electron runtime now resolves SKLMI dev files from the current canonical repo.
- Sekaiemu native SKLMI bridge registry now uses `~/SekaiLink/canonical` dev paths.
- Resource import tools now derive the repo path from their own file location.
- `scripts/doctor-source-roots.sh` now blocks the old canonical path and quarantine paths.

## Verification

Current verification command:

```bash
./scripts/doctor-source-roots.sh
```

Expected result:

```text
No blocked legacy source roots found outside migration docs.
```

## Git Notes

The `legacy-disabled` remote was removed. Active remotes are:

```text
origin
windows-build
```

Archives remain outside the Git repo and should not be committed.
