# ALTTP Native Patcher Acceptance

Date: 2026-05-11

This document defines the acceptance criteria for a SekaiLink-native `.aplttp`
patcher. Archipelago and MultiworldGG are priority behavioral references, but
the final native path must not copy their implementation and must not require
Python.

## Scope

The patcher consumes one generated seed package slot in `patch.mode=artifact`
and materializes a runtime-loadable ALTTP ROM for Sekaiemu.

It does not generate placements, solve logic, or mutate Link Room state. Those
remain seed package responsibilities.

The package verifier defined in `PACKAGE_VERIFIER_REQUIREMENTS.md` must pass
before this patcher is invoked.

## Inputs From Seed Package

Required package files:

- `seed_manifest.json`
- `slot_manifest.<slot>.json`
- `patch_contracts/slot-<slot>.patch.contract.json`
- `patches/slot-<slot>.aplttp`

Required `slot_manifest.<slot>.json` fields:

```json
{
  "slot_id": 1,
  "linkedworld_id": "alttp",
  "patch_mode": "artifact",
  "patch_artifact": "patches/slot-1.aplttp",
  "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json",
  "sklmi_contract_ref": "sklmi_seed_contract.json",
  "link_room_contract_ref": "link_room_seed_contract.json"
}
```

Required patch contract fields:

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "mode": "artifact",
  "artifact": {
    "path": "patches/slot-1.aplttp",
    "kind": "apdelta",
    "extension": ".aplttp"
  },
  "base_asset": {
    "required": true,
    "validation": {
      "hash_algorithm": "md5",
      "accepted_hashes": [
        "03a63945398191337e896e5771f77173"
      ],
      "required_release": "Japan 1.0"
    }
  },
  "apply_host": {
    "host": "sekaiemu",
    "expected_output_directory": "patched/",
    "expected_output_extension": ".sfc",
    "output_filename_template": "{patch_stem}.sfc"
  },
  "server_dispatch": {
    "enabled": false
  }
}
```

Required external runtime input:

- A user-provided ALTTP Japan 1.0 base ROM path.
- The patcher must never fetch, vendor, or infer a copyrighted base ROM.

## Artifact Placeholder Vs Materialized Artifact

During early package generation, `patch_artifact` may point to the expected
artifact path before the `.aplttp` bytes are materialized. That state is only a
packaging placeholder.

A native patcher may run only when:

- `patch_mode` is `artifact`.
- `patch_artifact` is non-null.
- The artifact file exists in the seed package.
- The artifact hash is present in `seed_manifest.artifact_hashes` when artifact
  bytes are materialized, and matches the artifact bytes.
- The patch contract exists and its hash matches `seed_manifest.artifact_hashes`
  when listed.

If the package has only a placeholder path and no `.aplttp` bytes, the patcher
must fail with `patch_artifact_missing`, not attempt fallback generation.

## `.aplttp` Container Acceptance

The native patcher should accept `.aplttp` containers that preserve the
AP/MultiworldGG behavior:

- The container has an `archipelago.json` manifest.
- The manifest `game` is `A Link to the Past`.
- The manifest `patch_file_ending` is `.aplttp`.
- The manifest `base_checksum` matches the accepted ALTTP base ROM MD5.
- The manifest `result_file_ending` is `.sfc`.
- The manifest includes `server`, `player`, and `player_name` metadata.
- The manifest declares a supported `procedure`.
- For the APDeltaPatch-compatible path, the procedure is equivalent to
  `apply_bsdiff4` with payload `delta.bsdiff4`.
- The container contains `delta.bsdiff4` for that procedure.

The native implementation may support additional APProcedurePatch-compatible
procedures later, but unsupported procedure steps must fail closed.

## Base ROM Validation

Before applying any artifact:

- Read the user-provided base ROM bytes.
- Strip a copier header only if the SNES/SFC validation layer explicitly
  recognizes that header shape.
- Compute MD5 over the canonical base ROM bytes.
- Require `03a63945398191337e896e5771f77173` for ALTTP Japan 1.0.
- Refuse all other releases, regions, randomizer bases, and already-patched ROMs
  unless a future contract explicitly adds accepted hashes.

Failure mode: `base_rom_checksum_mismatch`.

## Materialization

For a valid artifact and base ROM:

1. Read `delta.bsdiff4` from the `.aplttp` container.
2. Apply the bsdiff4 delta to the validated base ROM bytes.
3. Write the result to the path declared by `apply_host`, normally
   `patched/{patch_stem}.sfc`.
4. Hash the materialized ROM and record it in runtime logs or a generated
   runtime status file.
5. Pass only the materialized `.sfc` to Sekaiemu/libretro.

The materialized ROM is a runtime output, not the canonical seed truth.

## Failure Modes

The native patcher must fail closed with explicit errors:

- `patch_contract_missing`
- `patch_contract_hash_mismatch`
- `patch_mode_not_artifact`
- `patch_artifact_null`
- `patch_artifact_missing`
- `patch_artifact_hash_mismatch`
- `patch_container_invalid`
- `patch_manifest_missing`
- `patch_manifest_game_mismatch`
- `patch_manifest_extension_mismatch`
- `patch_manifest_version_unsupported`
- `patch_manifest_base_checksum_missing`
- `base_rom_missing`
- `base_rom_checksum_mismatch`
- `patch_procedure_missing`
- `patch_procedure_unsupported`
- `patch_payload_missing`
- `patch_apply_failed`
- `materialized_rom_write_failed`
- `materialized_rom_hash_mismatch`

No failure mode may silently fall back to a different ROM, different patch, or
server-dispatch behavior.

## Smoke And Oracle Checks

Minimum smoke coverage:

- Package smoke: `slot_manifest.patch_artifact` points to `.aplttp` and
  `patch_contract_ref` points to `patch_contracts/`.
- Placeholder smoke: missing `.aplttp` at the declared artifact path fails with
  `patch_artifact_missing`.
- Base ROM smoke: valid Japan 1.0 base ROM passes; a different ALTTP release
  fails with `base_rom_checksum_mismatch`.
- Container smoke: missing `archipelago.json` fails with
  `patch_manifest_missing`.
- Procedure smoke: unsupported procedure fails with
  `patch_procedure_unsupported`.
- Payload smoke: missing `delta.bsdiff4` fails with `patch_payload_missing`.
- Materialization smoke: valid `.aplttp` plus valid base ROM writes a `.sfc`
  under `patched/`.

Oracle checks:

- For a known reference `.aplttp`, native materialization must match the ROM
  hash produced by the AP/MultiworldGG-compatible patch path.
- Sekaiemu must boot the materialized `.sfc` headlessly and expose the expected
  memory surface.
- The oracle path is behavioral reference only; the final native patcher must
  not invoke Python.

## Non-Artifact Modes

The native `.aplttp` patcher must ignore or reject non-artifact modes:

- `patch.mode=none`: no patcher is invoked; `patch_artifact` must be null.
- `patch.mode=server_dispatch`: dispatch is handled by Link Room/runtime
  consumers; no base ROM is required by the patcher.
- `patch.mode=contract_only`: runtime contracts are consumed directly; no patch
  artifact is required.

If the patcher is accidentally called for these modes, it must fail with
`patch_mode_not_artifact`.
