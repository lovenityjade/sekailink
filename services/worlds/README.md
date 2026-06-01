# sekailink-worlds

Role:

- generation
- randomization
- patchs
- artefacts seed
- verite de seed consommee par Link Room, SKLMI, Sekaiemu et Nexus

Surface active extraite:

- `server/native/`
- `deploy/`
- `docs/`
- `scripts/`
- `third_party/`

Reference:

- `../../../docs/repo-contracts/sekailink-worlds.md`

Pivot actuel:

- `docs/RUNTIME_FREEZE_AND_GENERATOR_PIVOT.md`
- `docs/GENERIC_GENERATOR_CONTRACT.md`
- `docs/LINKEDWORLD_GENERATION_GATES.md`
- `docs/ALTTP_NATIVE_GENERATION_PLAN.md`
- `docs/ALTTP_REFERENCE_PATCH_VALIDATION.md`
- `CHANGELOG.md`

Native generator foundation:

- `server/native/sekailink_server_core/include/sekailink_server/generic_generation.hpp`
- `server/native/sekailink_server_core/src/generic_generation.cpp`
- `sekailink_generic_generation_smoke`
- `sekailink_generic_generation_probe`
- `third_party/sekailink_runtime/third_party/nlohmann/json.hpp`

Rule:

- Worlds stays generic. ALTTP is the first full validation target, not a hardcoded generator path.
- LinkedWorld modules declare their generation capabilities. Worlds refuses incomplete modules cleanly instead of inventing missing game logic.
- Existing AP-compatible ALTTP generation may be used as a validation oracle, but native Worlds support stays gated until the C++ solver, placer, and patch emitter are complete.
