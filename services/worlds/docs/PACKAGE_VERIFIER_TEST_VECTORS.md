# Package Verifier Test Vectors

Date: 2026-05-11

These fixtures are non-overlapping verifier vectors for the package contract.
They are intentionally small JSON examples, not generated package outputs.

## Valid Artifact Placeholder

The package verifier accepts this as a generation-time package, but runtime
materialization must not call the `.aplttp` patcher until bytes exist.

```json
{
  "name": "valid_artifact_placeholder",
  "seed_manifest": {
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
      }
    }
  },
  "slot_manifest": {
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
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
      "state": "placeholder"
    },
    "server_dispatch": {
      "enabled": false
    }
  },
  "expected": {
    "package_verifier": "pass",
    "runtime_patcher_allowed": false,
    "runtime_patcher_error_if_invoked": "patch_artifact_missing"
  }
}
```

## Valid Artifact Materialized

The package verifier accepts this as a runtime-ready artifact package. The
artifact hash is over raw `.aplttp` bytes, not JSON serialization.

```json
{
  "name": "valid_artifact_materialized",
  "seed_manifest": {
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
      "patches/slot-1.aplttp": {
        "hash_algorithm": "sha256",
        "content_type": "application/octet-stream",
        "sha256": "{artifact_bytes_sha256}"
      }
    }
  },
  "slot_manifest": {
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
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
      "sha256": "{artifact_bytes_sha256}"
    },
    "server_dispatch": {
      "enabled": false
    }
  },
  "expected": {
    "package_verifier": "pass",
    "runtime_patcher_allowed": true
  }
}
```

## Valid Server Dispatch

Non-artifact modes keep `patch_artifact` null and never route into the `.aplttp`
patcher.

```json
{
  "name": "valid_server_dispatch",
  "seed_manifest": {
    "artifact_hashes": {
      "slot_manifest.3.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{slot_manifest_json_sha256}"
      },
      "patch_contracts/slot-3.patch.contract.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{patch_contract_json_sha256}"
      }
    }
  },
  "slot_manifest": {
    "slot_id": 3,
    "linkedworld_id": "twilight_princess",
    "patch_mode": "server_dispatch",
    "patch_artifact": null,
    "patch_contract_ref": "patch_contracts/slot-3.patch.contract.json"
  },
  "patch_contract": {
    "schema_version": "sekailink-patch-contract-v1",
    "slot_id": 3,
    "linkedworld_id": "twilight_princess",
    "patch_mode": "server_dispatch",
    "mode": "server_dispatch",
    "artifact": null,
    "server_dispatch": {
      "enabled": true,
      "target": "link_room",
      "transport": "room_contract",
      "dispatch_timing": "before_client_ready",
      "idempotency_key_template": "{seed_id}:{slot_id}:server_dispatch",
      "requires": [
        "server_connection",
        "slot_identity"
      ],
      "payload_ref": "slot_state/slot-3.server-dispatch.json",
      "ack": {
        "required": true,
        "timeout_ms": 10000,
        "failure_mode": "server_dispatch_ack_timeout"
      }
    }
  },
  "expected": {
    "package_verifier": "pass",
    "runtime_patcher_allowed": false,
    "server_dispatch_allowed": true
  }
}
```

## Invalid Missing Contract Hash

```json
{
  "name": "invalid_missing_contract_hash",
  "seed_manifest": {
    "artifact_hashes": {
      "slot_manifest.1.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{slot_manifest_json_sha256}"
      }
    }
  },
  "slot_manifest": {
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
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
      "state": "placeholder"
    }
  },
  "expected": {
    "package_verifier": "fail",
    "error": "patch_contract_ref_missing_hash"
  }
}
```

## Invalid Materialized Artifact Missing Hash

```json
{
  "name": "invalid_materialized_artifact_missing_hash",
  "seed_manifest": {
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
      }
    }
  },
  "slot_manifest": {
    "slot_id": 1,
    "linkedworld_id": "alttp",
    "patch_mode": "artifact",
    "patch_artifact": "patches/slot-1.aplttp",
    "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
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
      "sha256": "{artifact_bytes_sha256}"
    }
  },
  "expected": {
    "package_verifier": "fail",
    "error": "artifact_hash_required_when_materialized"
  }
}
```

## Invalid Non-Artifact With Artifact

```json
{
  "name": "invalid_non_artifact_with_artifact",
  "seed_manifest": {
    "artifact_hashes": {
      "slot_manifest.3.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{slot_manifest_json_sha256}"
      },
      "patch_contracts/slot-3.patch.contract.json": {
        "hash_algorithm": "sha256",
        "content_type": "application/json",
        "sha256": "{patch_contract_json_sha256}"
      }
    }
  },
  "slot_manifest": {
    "slot_id": 3,
    "linkedworld_id": "ship_of_harkinian",
    "patch_mode": "server_dispatch",
    "patch_artifact": "patches/slot-3.aplttp",
    "patch_contract_ref": "patch_contracts/slot-3.patch.contract.json"
  },
  "patch_contract": {
    "schema_version": "sekailink-patch-contract-v1",
    "slot_id": 3,
    "linkedworld_id": "ship_of_harkinian",
    "patch_mode": "server_dispatch",
    "mode": "server_dispatch",
    "artifact": {
      "path": "patches/slot-3.aplttp",
      "kind": "apdelta",
      "extension": ".aplttp"
    },
    "server_dispatch": {
      "enabled": true
    }
  },
  "expected": {
    "package_verifier": "fail",
    "error": "artifact_forbidden_for_non_artifact_mode"
  }
}
```

## Oracle Summary

- `artifact` plus `state=placeholder` is package-valid but not runtime-patcher
  valid.
- `artifact` plus `state=materialized` requires a raw-byte artifact hash entry.
- `server_dispatch`, `none`, and `contract_only` must have no artifact path.
- Every `patch_contract_ref` must resolve and be hashed, regardless of mode.
- Binary artifact hash entries must use an object form with `content_type` and
  `hash_algorithm`.
