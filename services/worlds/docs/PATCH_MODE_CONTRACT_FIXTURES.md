# Patch Mode Contract Fixtures

Date: 2026-05-11

These fixtures define the minimum cases the generic generator should accept
without game-id branches.

## Artifact Slot

Used by patch-producing games such as ALTTP.

```json
{
  "slot_id": 1,
  "linkedworld_id": "alttp",
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "artifact",
    "artifact_kind": "apdelta",
    "artifact_extension": ".aplttp",
    "emission": {
      "package_path": "patches/slot-1.aplttp",
      "per_slot": true
    }
  },
  "expected": {
    "patch_artifact_required": true,
    "patch_contract_ref_required": true,
    "patch_contract_hash_required": true,
    "binary_artifact_hash_required_when_materialized": true,
    "native_patcher_invoked": true
  }
}
```

## No-Patch Slot

Used by games where the selected runtime does not need a generated binary patch.

```json
{
  "slot_id": 2,
  "linkedworld_id": "server-port-example",
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "none",
    "emission": {
      "package_path": null,
      "per_slot": true
    }
  },
  "expected": {
    "patch_artifact_required": false,
    "patch_contract_ref_required": true,
    "patch_contract_hash_required": true,
    "binary_artifact_hash_required_when_materialized": false,
    "native_patcher_invoked": false
  }
}
```

## Server Dispatch Slot

Used by server-port games such as Twilight Princess style integrations or Ship
of Harkinian style ports when seed state is delivered through a server/runtime
API instead of a patch file.

```json
{
  "slot_id": 3,
  "linkedworld_id": "server-dispatch-example",
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "server_dispatch",
    "artifact_kind": "none",
    "server_dispatch": {
      "target": "link_room",
      "transport": "room_contract",
      "dispatch_timing": "before_runtime_join",
      "payload_ref": "link_room_seed_contract.json",
      "requires": [
        "room_id",
        "seed_id",
        "slot_id",
        "linkedworld_id",
        "config_version_id",
        "link_room_seed_contract_ref"
      ],
      "ack": {
        "required": true,
        "success_field": "accepted",
        "failure_field": "error"
      }
    }
  },
  "expected": {
    "patch_artifact_required": false,
    "patch_contract_ref_required": true,
    "patch_contract_hash_required": true,
    "binary_artifact_hash_required_when_materialized": false,
    "link_room_contract_required": true,
    "native_patcher_invoked": false
  }
}
```

## Contract-Only Slot

Used when Worlds only needs to package runtime metadata for an installed
server-port/runtime.

```json
{
  "slot_id": 4,
  "linkedworld_id": "contract-only-example",
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "contract_only",
    "contracts": [
      "sklmi_seed_contract.json",
      "link_room_seed_contract.json"
    ]
  },
  "expected": {
    "patch_artifact_required": false,
    "patch_contract_ref_required": true,
    "patch_contract_hash_required": true,
    "binary_artifact_hash_required_when_materialized": false,
    "native_patcher_invoked": false
  }
}
```

## Acceptance Rules

- All modes produce one `patch_contracts/slot-<slot>.json` entry.
- Every `patch_contract_ref` must exist and be present in
  `seed_manifest.artifact_hashes`.
- Only `artifact` requires a file under `patches/`.
- Binary artifact hashes are required when artifact bytes are materialized.
- `none`, `server_dispatch`, and `contract_only` are valid package states, not
  missing patch errors.
- The native `.aplttp` patcher is invoked only for `artifact` slots whose
  contract declares `.aplttp`/`apdelta` semantics.
- A mixed room may contain all of these slots in one seed package.
