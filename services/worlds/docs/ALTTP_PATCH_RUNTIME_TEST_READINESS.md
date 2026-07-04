# ALTTP Patch Runtime Test Readiness

Date: 2026-05-11

This document defines the final readiness vector for the ALTTP path:

1. Worlds emits one seed package.
2. The package verifier accepts one ALTTP artifact slot.
3. The native `.aplttp` materializer writes a `.sfc`.
4. Sekaiemu launches from the declared contract.

The generic generator must stay game-agnostic. ALTTP-specific knowledge belongs
in the LinkedWorld patch manifest and the per-slot patch contract.

The operational handoff for the step immediately after `can_native_generate=true`
is `ALTTP_POST_NATIVE_GENERATION_PATCH_HANDOFF.md`.

## Required End-To-End Vector

This vector is intentionally data-only. It describes the expected package shape
after `.aplttp` bytes exist.

```json
{
  "name": "alttp_seed_package_to_sekaiemu_launch_ready",
  "seed_manifest": {
    "seed_id": "seed-alttp-runtime-ready",
    "slot_count": 1,
    "artifact_hashes": {
      "slot_manifest.1.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{slot_manifest_json_sha256}"
      },
      "patch_contracts/slot-1.patch.contract.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{patch_contract_json_sha256}"
      },
      "tracker_seed_state.slot-1.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{tracker_seed_state_json_sha256}"
      },
      "sklmi_seed_contract.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{sklmi_contract_json_sha256}"
      },
      "link_room_seed_contract.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{link_room_contract_json_sha256}"
      },
      "patches/slot-1.aplttp": {
        "hash_algorithm": "sha256",
        "content_type": "application/octet-stream",
        "sha256": "{aplttp_raw_bytes_sha256}"
      }
    }
  },
  "slot_manifest": {
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "game_key": "alttp",
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json",
    "tracker_seed_state": "tracker_seed_state.slot-1.json",
    "sklmi_contract_ref": "sklmi_seed_contract.json",
    "link_room_contract_ref": "link_room_seed_contract.json"
  },
  "patch_contract": {
    "schema_version": "sekailink-patch-contract-v1",
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "patch_mode": "artifact",
    "mode": "artifact",
    "artifact": {
      "path": "patches/slot-1.aplttp",
      "kind": "apdelta",
      "extension": ".aplttp",
      "state": "materialized",
      "hash_algorithm": "sha256",
      "sha256": "{aplttp_raw_bytes_sha256}"
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
      "patch_argument": "--patch",
      "base_rom_argument": "--base-rom",
      "expected_output_directory": "patched/",
      "output_filename_template": "{patch_stem}.sfc",
      "expected_output_extension": ".sfc"
    },
    "server_dispatch": {
      "enabled": false
    }
  },
  "runtime_input": {
    "base_rom_file": "{user_supplied_alttp_japan_1_0_rom}",
    "base_rom_md5": "03a63945398191337e896e5771f77173"
  },
  "expected": {
    "package_verifier": "pass",
    "native_patcher_allowed": true,
    "materialized_rom": "patched/slot-1.sfc",
    "sekaiemu_patch_first_args": [
      "--patch",
      "patches/slot-1.aplttp",
      "--base-rom",
      "{user_supplied_alttp_japan_1_0_rom}"
    ],
    "server_dispatch_allowed": false
  }
}
```

## Package Verifier Gate

The verifier must pass before the `.aplttp` patcher or Sekaiemu launch path is
allowed to run.

Required checks:

- `slot_manifest.1.json` exists and is hashed.
- `patch_contract_ref` resolves under `patch_contracts/` and is hashed.
- `patch_artifact` is non-null because `patch_mode=artifact`.
- `patch_contract.artifact.path` equals `slot_manifest.patch_artifact`.
- `patch_contract.artifact.state=materialized`.
- `seed_manifest.artifact_hashes["patches/slot-1.aplttp"]` exists.
- The artifact hash is computed from raw `.aplttp` bytes.
- `server_dispatch.enabled=false`.

The placeholder package vector remains valid for generation smoke tests, but it
is not launch-ready. A placeholder package must stop before patcher invocation
with `patch_artifact_missing`.

## Native `.aplttp` Materialization Gate

The native materializer may run only after package verification.

It must then enforce ALTTP-owned knowledge from the patch contract:

- The user supplies the base ROM; SekaiLink does not fetch or vendor it.
- The base ROM MD5 must be `03a63945398191337e896e5771f77173`.
- The `.aplttp` container must include `archipelago.json`.
- The container manifest must declare `game=A Link to the Past`.
- The container manifest must declare `patch_file_ending=.aplttp`.
- The container manifest `base_checksum` must match the accepted base ROM MD5.
- The procedure must be supported by the native patcher.
- The APDelta-compatible path requires `delta.bsdiff4`.

Expected materialized output for `patches/slot-1.aplttp` is
`patched/slot-1.sfc`, following `{patch_stem}.sfc`.

## Sekaiemu Launch Gate

Sekaiemu may launch in either patch-first or materialized-ROM form, but both
forms must derive from the verified package contract.

Patch-first form:

```json
{
  "argv": [
    "--patch",
    "patches/slot-1.aplttp",
    "--base-rom",
    "{user_supplied_alttp_japan_1_0_rom}"
  ],
  "expected_materialized_rom": "patched/slot-1.sfc"
}
```

Materialized-ROM form:

```json
{
  "argv": [
    "--game",
    "patched/slot-1.sfc"
  ],
  "precondition": "patched/slot-1.sfc was produced from the verified patch contract"
}
```

The runtime must not infer ALTTP behavior from `game_key` alone. It should read
the patch contract and the LinkedWorld patch manifest.

## Server Dispatch And No-Patch Coherence

This ALTTP vector must not weaken generic no-patch support:

- `patch_mode=server_dispatch`, `none`, and `contract_only` keep
  `patch_artifact=null`.
- Non-artifact modes must not create placeholder files under `patches/`.
- Non-artifact modes must not invoke the `.aplttp` patcher.
- Server-port games declare their dispatch contract in their LinkedWorld-owned
  patch manifest.

## Current Verifier Audit

Visible `generic_generation.cpp` behavior is close to the docs:

- `patch_contract_ref` is mandatory and must point under `patch_contracts/`.
- Slot refs for tracker, SKLMI, and Link Room are required and hashed.
- `artifact` mode requires `patch_artifact`.
- Placeholder artifact state is accepted when artifact bytes are not yet
  present.
- Non-artifact modes reject any `patch_artifact`.
- `server_dispatch` requires an enabled dispatch object.

Precise implementation gaps to close before final ALTTP runtime readiness:

- Raw-byte artifact hash verification: `verify_generated_seed_package_contracts`
  currently verifies JSON package files from a JSON file map. A materialized
  `.aplttp` must be represented as package bytes or package-file metadata so
  `seed_manifest.artifact_hashes["patches/slot-1.aplttp"].sha256` can be
  compared against raw bytes.
- Server-dispatch schema validation: `server_dispatch` currently checks
  `enabled=true`, but the generic verifier should also require `target`,
  `transport`, `dispatch_timing`, `idempotency_key_template`, `requires`,
  `payload_ref`, and `ack` as documented in
  `SERVER_PORT_PATCH_CONTRACT_REQUIREMENTS.md`.
- Launch readiness split: artifact placeholder packages are generation-valid
  but launch-invalid. Runtime code should preserve that split and never treat a
  placeholder package as Sekaiemu-ready.

No ALTTP-specific logic is required in `generic_generation.cpp`; the code change
needed there is generic package-byte verification and generic server-dispatch
schema validation.
