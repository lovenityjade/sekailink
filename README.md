# SekaiLink BETA-3 Canonical Repository

This repository is the single local source of truth for SekaiLink BETA-3 work.

## Layout

- `apps/client-core`: SekaiLink Client Core Electron app.
- `apps/sekaiemu`: Sekaiemu visual emulator/tracker client.
- `services/sklmi`: SKLMI tracker/runtime companion.
- `services/nexus`: identity, user, database, and profile service.
- `services/link-social`: lobby/chat/API orchestration service.
- `services/link-room`: local room runtime service used by Link.
- `services/worlds`: generation worker service.
- `services/evolution`: asset/update/distribution service.
- `linkedworlds/alttp`: A Link to the Past showcase LinkedWorld assets.
- `runtime`: packaged local runtime tree used by the client.
- `docs`: source-of-truth notes and migration guardrails.

## Current Rule

Use this repository for new work. The previous `Projects/...` and `sekailink-beta-3-final/...`
trees are backup/reference only until this repo is validated and pushed.

Do not delete old trees until manual validation is complete.
