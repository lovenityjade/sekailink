# Runtime Boundary

## Client Core

Client Core is the user-facing app. It authenticates with Nexus, manages lobbies through Link,
stores game/seed configuration, coordinates generation, downloads generated packages, resolves
tracker packs, and launches Sekaiemu.

## SKLMI

SKLMI is the local tracker companion process. It loads PopTracker-compatible packs and publishes
`tracker.snapshot.json` with draw-ready tracker state. It receives commands through
`tracker.commands.jsonl`.

## Sekaiemu

Sekaiemu is the visual emulator client. It should:

- run the libretro core;
- keep audio/input responsive;
- read tracker snapshots only when `mtime` or size changes;
- cache decoded images and GPU textures;
- render tracker views passively.

Sekaiemu should not compute tracker logic in production BETA-3.

## Server Services

- Nexus: identity, users, profiles, database-backed game settings.
- Link Social: lobby/chat/API layer and generation handoff.
- Link Room: room runtime surface.
- Worlds: generation service using the background generator.
- Evolution: distribution, updates, and shared assets such as tracker packs.
