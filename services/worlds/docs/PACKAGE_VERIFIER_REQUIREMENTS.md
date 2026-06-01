# Package Verifier Requirements

Date: 2026-05-11

This document defines package verifier requirements for patch/runtime contracts.
The verifier runs after seed package emission and before any runtime patcher,
server dispatch, or client launch consumes the package.

Concrete verifier vectors live in
`clean-room/repos/sekailink-worlds/docs/PACKAGE_VERIFIER_TEST_VECTORS.md`.
The ALTTP end-to-end runtime readiness vector lives in
`clean-room/repos/sekailink-worlds/docs/ALTTP_PATCH_RUNTIME_TEST_READINESS.md`.

## Goals

- Prove every manifest reference points to an emitted package file.
- Prove every per-slot patch contract exists and is hashed.
- Prove `patch_artifact` is required only for `patch.mode=artifact`.
- Prove non-artifact modes never invoke the `.aplttp` patcher.
- Make future binary artifact hashing explicit.

## Required Top-Level Checks

For every generated package:

- `seed_manifest.json` exists.
- `seed_manifest.artifact_hashes` exists and is an object.
- Every emitted JSON package file except `seed_manifest.json` has an entry in
  `seed_manifest.artifact_hashes`.
- Every hash entry uses the package path as the key.
- JSON file hashes are computed from the canonical serialized JSON used for the
  package.
- Future binary artifact hashes must be computed from raw bytes, not JSON
  serialization.

The verifier should fail closed if a file exists without a hash entry or a hash
entry references a missing file.

## Slot Reference Checks

For every `slot_manifest.<slot>.json`:

- The slot manifest path exists.
- `patch_mode` is present.
- `patch_contract_ref` is present, non-empty, and points to an existing package
  file.
- `patch_contract_ref` starts with `patch_contracts/`.
- `patch_contract_ref` is present in `seed_manifest.artifact_hashes`.
- `tracker_seed_state`, `sklmi_contract_ref`, and `link_room_contract_ref` point
  to existing package files.
- Those referenced files are present in `seed_manifest.artifact_hashes`.

## Patch Contract Checks

For every `patch_contract_ref`:

- The referenced JSON parses.
- `schema_version` is `sekailink-patch-contract-v1`.
- `slot_id` matches the slot manifest.
- `linkedworld_id` matches the slot manifest.
- `patch_mode` and `mode` match the slot manifest `patch_mode`.
- `artifact` matches the slot manifest `patch_artifact`.
- The contract hash in `seed_manifest.artifact_hashes` matches the contract
  bytes.

## Artifact Mode Checks

For `patch.mode=artifact`:

```json
{
  "slot_manifest": {
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
  },
  "patch_contract": {
    "mode": "artifact",
    "artifact": {
      "path": "patches/slot-1.aplttp",
      "kind": "apdelta",
      "extension": ".aplttp"
    }
  },
  "verifier": {
    "patch_artifact_required": true,
    "patch_artifact_hash_required_when_materialized": true,
    "native_patcher_invoked": true
  }
}
```

Verifier rules:

- `patch_artifact` must be non-null.
- `patch_contract.artifact.path` must equal `slot_manifest.patch_artifact`.
- Artifact extension must match the contract extension.
- If artifact bytes are present, `seed_manifest.artifact_hashes` must include
  `patch_artifact` and the hash must match raw bytes.
- If artifact bytes are not present yet, the package must mark the artifact as
  an explicit placeholder state before runtime materialization.
- A runtime patcher may not run while the artifact is only a placeholder.

Recommended placeholder marker:

```json
{
  "artifact": {
    "path": "patches/slot-1.aplttp",
    "kind": "apdelta",
    "extension": ".aplttp",
    "state": "placeholder"
  }
}
```

Recommended materialized marker:

```json
{
  "artifact": {
    "path": "patches/slot-1.aplttp",
    "kind": "apdelta",
    "extension": ".aplttp",
    "state": "materialized",
    "hash_algorithm": "sha256",
    "sha256": "{artifact_sha256}"
  }
}
```

## Non-Artifact Mode Checks

For `patch.mode=none`, `server_dispatch`, and `contract_only`:

```json
{
  "slot_manifest": {
    "patch_mode": "server_dispatch",
    "patch_artifact": null,
    "patch_contract_ref": "patch_contracts/slot-3.patch.contract.json"
  },
  "patch_contract": {
    "mode": "server_dispatch",
    "artifact": null
  },
  "verifier": {
    "patch_artifact_required": false,
    "native_patcher_invoked": false
  }
}
```

Verifier rules:

- `patch_artifact` must be null.
- `patch_contract.artifact` must be null.
- No path under `patches/` is required for that slot.
- The `.aplttp` patcher must not be invoked.
- `server_dispatch` mode must include `server_dispatch.enabled=true` and the
  dispatch fields required by `SERVER_PORT_PATCH_CONTRACT_REQUIREMENTS.md`.
- `none` and `contract_only` must include `server_dispatch.enabled=false` or no
  dispatch object.

## Future Binary Artifact Hashing

When binary artifacts are emitted, `seed_manifest.artifact_hashes` must support
raw-byte hashes. The preferred shape is:

```json
{
  "artifact_hashes": {
    "patch_contracts/slot-1.patch.contract.json": {
      "hash_algorithm": "sha256",
      "content_type": "application/json",
      "sha256": "{contract_json_sha256}"
    },
    "patches/slot-1.aplttp": {
      "hash_algorithm": "sha256",
      "content_type": "application/octet-stream",
      "sha256": "{artifact_bytes_sha256}"
    }
  }
}
```

Legacy string hash values may be accepted for JSON-only package files during the
transition, but binary artifacts need the object form so consumers know the hash
was computed from bytes.

## Failure Modes

The verifier should report these errors precisely:

- `seed_manifest_missing`
- `artifact_hashes_missing`
- `package_file_hash_missing`
- `package_file_hash_mismatch`
- `package_hash_entry_without_file`
- `slot_manifest_missing`
- `slot_manifest_ref_missing`
- `slot_manifest_ref_missing_hash`
- `patch_contract_ref_missing`
- `patch_contract_ref_missing_file`
- `patch_contract_ref_missing_hash`
- `patch_contract_hash_mismatch`
- `patch_contract_slot_mismatch`
- `patch_contract_mode_mismatch`
- `artifact_required_for_artifact_mode`
- `artifact_forbidden_for_non_artifact_mode`
- `artifact_hash_required_when_materialized`
- `artifact_hash_mismatch`
- `native_patcher_forbidden_for_non_artifact_mode`
- `server_dispatch_contract_incomplete`

## Required Code Changes If Not Already Present

- Verify all `slot_manifest` refs exist and are hashed before runtime launch.
- Verify `patch_contract_ref` exists and is hashed for every slot.
- Treat `patch_artifact` as required only when `patch_mode=artifact`.
- Ensure `.aplttp` patcher routing is impossible for `none`,
  `server_dispatch`, and `contract_only`.
- Add raw-byte hash support for future binary artifacts under
  `seed_manifest.artifact_hashes`.
- Validate the full generic `server_dispatch` schema, not only the
  `enabled=true` flag.
