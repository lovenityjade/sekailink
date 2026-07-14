# SekaiLink

> [!IMPORTANT]
> **Project archived on July 14, 2026.** SekaiLink is no longer under active
> development or operated as a supported service by its original maintainer.
> This repository preserves the final source state for study, auditing, and a
> possible community-led continuation. It is provided as-is, without support or
> a security-maintenance commitment.

<p align="center">
  <img src="apps/client-core/public/assets/img/sekailink-logo-image.png" alt="SekaiLink logo mark" width="96" />
  <br />
  <img src="apps/client-core/public/assets/img/sekailink-logo-text.png" alt="SekaiLink" width="360" />
</p>

SekaiLink is a desktop-first accessibility layer for the
[Archipelago](https://archipelago.gg/) multiworld randomizer ecosystem. It
brings together a player-friendly client, an integrated emulator, runtime sync,
trackers, lobby tools, notifications, generation helpers, and server services so
players can spend less time wiring tools together and more time playing.

At a high level, SekaiLink helps players:

- choose supported games and understand which ROM/version is needed;
- import and validate base ROMs locally;
- create or join lobbies;
- generate or launch Archipelago-compatible seeds;
- start Sekaiemu, tracker runtimes, and sync services together;
- receive item/check/activity notifications in a readable way;
- debug launcher, emulator, room, and generation issues with clearer logs.

## Project Status

This repository is the final canonical BETA-3 source tree. Some parts were
production-facing, some remained experimental, and some are integration or
reference code used to package the desktop experience. See
[`docs/PROJECT_CLOSURE_2026-07-14.md`](docs/PROJECT_CLOSURE_2026-07-14.md) before
attempting to build, deploy, or continue the project.

## Core Integrations

SekaiLink is built around and alongside several major community projects:

- [Archipelago](https://archipelago.gg/) provides the multiworld randomizer
  ecosystem, generation model, room protocol, item/check semantics, and slot
  state that SekaiLink integrates with.
- [libretro](https://www.libretro.com/) provides the emulator core API used by
  Sekaiemu for supported emulation workflows.
- [PopTracker](https://github.com/black-sliver/PopTracker) provides tracker
  runtime behavior and pack-driven map/item layouts. SekaiLink uses a
  compatibility runtime path for launching and coordinating tracker packs.

PopTracker packs bundled or referenced by SekaiLink belong to their respective
community authors. Keep upstream pack credits, manifests, and licenses intact
when updating tracker support.

## AI Assistance Notice

SekaiLink is developed with AI assistance in specific areas such as codebase
navigation, prototype scaffolding, documentation drafting, debug-log analysis,
test planning, and repetitive refactoring support. Human maintainers remain
responsible for architecture, product decisions, code review, security review,
testing, release approval, and the final behavior shipped to players.

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

## Continuation

A future maintainer should begin with a security and architecture audit, review
the source-of-truth and technical handoff documents under `docs`, rotate every
deployment credential, and rebuild all release artifacts from source. Do not
assume that historical live-service configuration or credentials remain valid.
