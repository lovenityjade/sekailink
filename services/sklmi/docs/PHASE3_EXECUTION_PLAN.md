# SKLMI Phase 3 Execution Plan

Status: completed on 2026-04-23

## Goal

Prove that SKLMI can talk to a real Sekaiemu runtime without Lua and without collapsing back into frame-coupled bridge logic.

## Scope

Phase 3 is the runtime integration proof, not the full room/server bridge.

It must prove:

- a real Sekaiemu runtime process can be launched
- SKLMI can connect over the runtime socket
- memory domains are exposed and usable
- a manifest-backed bridge can detect one check and perform one injection
- the first SNES path stays aligned with SNI-style separation of concerns

## What Was Implemented

In `sekailink-sklmi`:

- `RuntimeSocketMemoryProvider`
- runtime socket smoke test
- real integration proof executable:
  - `sekailink_sklmi_sekaiemu_runtime_proof`

In `sekailink-emu`:

- `sekailink_runtime_snes`
- `DOMAINS` response support in the runtime protocol
- SNES domain metadata exposure through `SnesRuntime::memory_domains()`
- GBA domain metadata exposure through `GbaRuntime::memory_domains()`

## Real Proof Target

Chosen first target:

- `EarthBound`

Reason:

- simple SNES-backed proof
- matches the roadmap preference
- avoids overfitting Phase 3 to SoH or Wind Waker

## Validation Command

Example local validation:

```bash
/tmp/sekailink-sklmi-build/sekailink_sklmi_sekaiemu_runtime_proof \
  /tmp/sekailink-runtime-build/sekailink_runtime_snes \
  "<local-home>/DevSSD/SekaiLinkDev/archive/legacy/sekailink-monorepo/roms/snes/EarthBound (USA).sfc" \
  /tmp/sklmi-earthbound-proof.srm
```

Expected result:

```text
sklmi_sekaiemu_runtime_proof_ok
```

## Output Meaning

This validates:

- real runtime process
- real memory provider over socket
- manifest-driven SKLMI bridge session
- check detection and item injection over a real Sekaiemu runtime path

This does not yet validate:

- AP/SekaiLink room authority
- production tracker sync
- offline room generation handoff
- packaged `.linkedworld` runtime flow

Those belong to Phase 4 and beyond.
