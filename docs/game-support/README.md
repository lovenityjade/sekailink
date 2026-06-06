# SekaiLink Game Support

This directory tracks game compatibility for SekaiLink. It is not a raw copy of
the Archipelago ecosystem. It is the operational layer SekaiLink needs to decide
which games can be installed, tested, certified, shipped, and promoted.

## Source Inputs

- Local Archipelago runtime worlds under `runtime/ap/worlds`.
- Local Sekaiemu profiles under `runtime/profiles` and `apps/sekaiemu/profiles`.
- Local SKLMI manifests under `runtime/sklmi/manifests` and
  `services/sklmi/manifests`.
- Deep Research report at `/home/thelovenityjade/deep-research-report.md`.
- Deep Research CSV at
  `/home/thelovenityjade/Archipelago Games Sheet - Welcome Page.csv`.

The CSV found in the home directory is the welcome/FAQ page of the community
sheet, not the dense game table. The Markdown report is therefore the primary
external research artifact for the first matrix import.

## Core Idea

Archipelago APWorlds remain the source of truth for generation, item/location
IDs, options, patchers, and game logic. SekaiLink compatibility adds runtime
evidence:

- APWorld is present or installable.
- Tracker pack or compatible web tracker exists.
- Sekaiemu can launch the game/core profile.
- SKLMI can observe checks and inject received items, ideally through existing
  AP clients, SNI/Lua, BizHawk connectors, or generated manifests.
- Room server and client packaging can ship the pieces together.

The SKLMI manifest should be treated as a runtime adapter or generated artifact,
not as a second copy of APWorld logic.

## Files

- [tiers.md](tiers.md): SekaiLink certification tiers.
- [intake-schema.md](intake-schema.md): normalized fields for importing games.
- [compatibility-matrix.md](compatibility-matrix.md): first Nintendo matrix from
  Deep Research plus local SekaiLink evidence.
- [games/](games): per-game notes for certification work.

Machine-readable registry:

- `runtime/game-registry/games.json`
- `runtime/game-registry/platforms/*.json`

