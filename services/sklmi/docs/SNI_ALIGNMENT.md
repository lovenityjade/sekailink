# SKLMI SNES Alignment

Status: guiding note
Date: 2026-04-23

## Rule

For SNES-backed integrations, SKLMI must align with the stability philosophy of `SNI`.

It must not invent a fragile frame-loop-heavy substitute when a cleaner memory-bridge model already exists.

## What This Means

SKLMI should model the good properties of SNI:

- clear memory-provider boundary
- stable read/write contract
- reconnect separated from render timing
- bridge state outside the emulator frame loop
- protocol/client behavior isolated from raw memory access

## What This Rejects

SKLMI should not reintroduce the worst properties of Lua bridges:

- network logic inside frame-sensitive polling
- game-specific socket code mixed with memory code
- per-game copy-paste bridge implementations
- brittle reconnect logic tied to emulator update cadence

## Practical Consequence

When building the first SNES-facing path:

- use SNI as the behavioral reference
- keep SKLMI generic
- move game-specific behavior into rules or optional adapters
- prefer simple, inspectable, deterministic bridge state
