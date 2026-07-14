# SekaiLink PC Packages

This directory is the source-of-truth staging area for the optional Games & Mods catalog. It is separate
from Client Core and bootstrapper releases so large native games do not inflate routine updates.

Nothing in this directory is public merely because it exists in the repository. Promotion to the CDN is a
separate, explicit operation after license review, build verification, and an end-to-end Canary install test.

Unpublished release descriptors live under `staging/`. A descriptor with `published: false` must never be
copied into the public catalog or served as a canonical release.

Packages must never include ROMs or extracted copyrighted assets. Every downloadable archive requires a
byte size and SHA-256 in `catalog-v1.json`; clients reject entries without both values.

Expected public path:

```text
/downloads/pc-packages/v1/catalog.json
/downloads/pc-packages/<package-id>/<version>/<platform>/<archive>
```
