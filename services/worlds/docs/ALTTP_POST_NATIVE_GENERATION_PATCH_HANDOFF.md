# ALTTP Post-Native-Generation Patch Handoff

Date: 2026-05-11

This handoff prepares the patch/runtime step that follows
`can_native_generate=true`. It does not open ALTTP native generation gates and
does not add ALTTP-specific behavior to the generic Worlds generator.

## Current Probe State

Command used:

```bash
/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_probe \
  <local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp
```

Observed result on 2026-05-11:

- `can_native_generate=false`
- `patch_ready=true`
- `package_consistent=true`
- `patch_mode=artifact`
- `patch_artifact_extension=.aplttp`
- `generation_item_pool_count=153`
- `effective_fillable_location_count=153`
- `item_location_count_match=true`
- `can_emit_patch=true`
- `can_emit_room_contract=true`
- remaining blockers are solver/generation blockers, not patcher blockers:
  `can_solve_logic`, `can_place_items`, `unsupported_options`,
  `logic_rules_not_authorizing`, `region_graph_not_authorizing`,
  `region_graph_edge_blockers_not_consumed`,
  `location_refinements_not_authorizing`

Patch/runtime can stand by until native generation flips true.

## Package Generation Entry Points

Available local validation binaries:

- `/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_probe`
- `/tmp/sekailink-worlds-generic-build/sekailink_generic_generation_smoke`
- `/tmp/sekailink-worlds-generic-build/sekailink_generation_server_smoke`

Validated smoke:

```bash
/tmp/sekailink-worlds-generic-build/sekailink_generation_server_smoke
```

Result:

```text
generation_server_ok=1
```

The server path already exposes `submit_seed_package`. The command payload shape
is generic:

```json
{
  "auth_token": "{generation_server_auth_token}",
  "cmd": "submit_seed_package",
  "job_id": "package-job-alttp-1",
  "room_id": "room-alttp-test",
  "seed_id": "seed-alttp-test",
  "rng_seed": "deterministic-seed-alttp-test",
  "output_root": "/tmp/sekailink-alttp-packages",
  "linkedworld_roots": [
    "<local-home>/DevSSD/sekailink-beta-3-final/clean-room/repos/sekailink-linkedworld-alttp"
  ],
  "slots": [
    {
      "slot_id": 1,
      "user_id": 1001,
      "display_name": "Jade",
      "game_key": "alttp",
      "linkedworld_id": "alttp",
      "config_version_id": 501
    }
  ]
}
```

When native generation gates open, this should produce one package under:

```text
{output_root}/{seed_id}/
```

## Expected Seed Package Artifacts

For an ALTTP artifact slot, the package must include:

- `seed_manifest.json`
- `room_manifest.json`
- `slot_manifest.1.json`
- `checks.json`
- `items.json`
- `placements.json`
- `tracker_seed_state.json`
- `sklmi_seed_contract.json`
- `link_room_seed_contract.json`
- `audit.json`
- `patch_contracts/slot-1.patch.contract.json`
- `patches/slot-1.aplttp` once the patch artifact is materialized

Required slot fields:

```json
{
  "patch_mode": "artifact",
  "patch_artifact": "patches/slot-1.aplttp",
  "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json"
}
```

Required patch contract fields:

```json
{
  "schema_version": "sekailink-patch-contract-v1",
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
}
```

The generic generator may emit a placeholder artifact contract during package
generation, but a headless launch test requires `artifact.state=materialized`,
real `.aplttp` bytes, and a raw-byte hash entry in
`seed_manifest.artifact_hashes`.

## Contract, Placeholder, And Materialized Boundaries

There are three distinct states. They must not be collapsed into one generic
"patch exists" flag.

| State | Package shape | Bytes present | Verifier status | Sekaiemu launch |
| --- | --- | --- | --- | --- |
| `contract_only_reference` | `patch_contract_ref` exists and is hashed | no `.aplttp` bytes | valid only for modes that do not require an artifact | forbidden for ALTTP artifact mode |
| `artifact_placeholder` | `patch_artifact` path exists in `slot_manifest`; contract artifact has `state=placeholder` | no `.aplttp` bytes | valid generation package | forbidden; fail before launch |
| `artifact_materialized` | `patch_artifact` path exists; contract artifact has `state=materialized`; raw-byte hash exists | `.aplttp` bytes exist | valid runtime package | allowed after base ROM validation |

ALTTP uses `patch.mode=artifact`, so its minimum generated package state is
`artifact_placeholder`, not `contract_only_reference`.

### Generated Package Contract

The generated package contract is the JSON truth emitted by Worlds:

- `slot_manifest.1.json`
- `patch_contracts/slot-1.patch.contract.json`
- `seed_manifest.json`

It is enough to prove the package shape and future artifact path:

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
      "extension": ".aplttp",
      "state": "placeholder"
    }
  }
}
```

This state is not enough to apply a ROM. It should never be treated as a real
patch artifact by Sekaiemu or by a runtime launcher.

### Patch Artifact Placeholder

A placeholder means the package has reserved a deterministic artifact path and
has emitted all contract metadata needed by the later materializer.

Allowed:

- package verifier accepts the package as generation output
- room/Nexus can reference the future artifact path
- tests can assert that the artifact path and contract are deterministic

Forbidden:

- invoking the `.aplttp` patcher
- invoking Sekaiemu with `--patch`
- hashing the placeholder as JSON
- creating an empty file at `patches/slot-1.aplttp`
- silently falling back to server dispatch

Required runtime failure if launch is attempted:

```json
{
  "error": "patch_artifact_missing",
  "path": "patches/slot-1.aplttp",
  "reason": "artifact placeholder has no .aplttp bytes"
}
```

### Materialized Patch Artifact

A materialized artifact means the package contains real `.aplttp` bytes at the
declared path and both the contract and seed manifest agree on its raw-byte
hash.

Required package state:

```json
{
  "seed_manifest": {
    "artifact_hashes": {
      "patches/slot-1.aplttp": {
        "hash_algorithm": "sha256",
        "content_type": "application/octet-stream",
        "sha256": "{aplttp_raw_bytes_sha256}"
      }
    }
  },
  "patch_contract": {
    "artifact": {
      "path": "patches/slot-1.aplttp",
      "kind": "apdelta",
      "extension": ".aplttp",
      "state": "materialized",
      "hash_algorithm": "sha256",
      "sha256": "{aplttp_raw_bytes_sha256}"
    }
  }
}
```

Only this state may proceed to `.aplttp` container validation and Sekaiemu
patch-first launch.

### Transition To Real `.aplttp`

The transition from placeholder to materialized must be explicit:

1. Native generation produces placements, items, checks, and a patch plan.
2. The ALTTP-owned patch materializer consumes the patch plan and basepatch
   rules declared by the LinkedWorld patch manifest.
3. It emits `patches/slot-1.aplttp`.
4. It computes SHA-256 over raw `.aplttp` bytes.
5. It updates the patch contract artifact from `state=placeholder` to
   `state=materialized`.
6. It records the same raw-byte hash in `seed_manifest.artifact_hashes`.
7. The package verifier re-runs before runtime launch.

The generic generator should not hardcode step 2. It should only route to a
materializer declared by the patch contract or accept a materialized artifact
that satisfies the generic hash and contract checks.

## What Is Still Missing For A Launchable ALTTP Patch

Current generic package emission can create the package structure and per-slot
patch contract, but not a launchable ALTTP patch artifact yet.

Missing patch/runtime pieces after `can_native_generate=true`:

- Native `.aplttp` artifact materialization from the generated ALTTP patch plan.
- Raw-byte SHA-256 entry for `patches/slot-1.aplttp` in
  `seed_manifest.artifact_hashes`.
- Package verifier support for binary package files, not only JSON file maps.
- Runtime gate that refuses placeholder artifacts before invoking Sekaiemu.
- Native `.aplttp` acceptance path that validates `archipelago.json`,
  `base_checksum`, `patch_file_ending=.aplttp`, and `delta.bsdiff4`.

Missing external test input:

- User-provided ALTTP Japan 1.0 base ROM with MD5
  `03a63945398191337e896e5771f77173`.

Legal boundary:

- The package must never include, fetch, or infer a copyrighted base ROM.

## Headless Apply And Launch Target

After the package contains `patches/slot-1.aplttp`, the patch-first Sekaiemu
launch contract is:

```bash
<local-home>/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/build/sekaiemu_libretro_spike \
  --probe-only \
  --profile <local-home>/Sekaiemu-Libretro-Spike-Codex/workspace/sekaiemu-libretro-spike/profiles/alttp-starter.profile \
  --core /usr/lib64/libretro/bsnes_mercury_performance_libretro.so \
  --patch {package_dir}/patches/slot-1.aplttp \
  --base-rom {user_supplied_alttp_japan_1_0_rom} \
  --save-dir /tmp/sekaiemu-alttp-package-patch-probe \
  --memory-socket /tmp/sekaiemu-alttp-package-patch-probe/memory.sock
```

Expected materialized ROM:

```text
/tmp/sekaiemu-alttp-package-patch-probe/patched/slot-1.sfc
```

Expected smoke result:

- exit code `0`
- materialized `.sfc` exists
- materialized ROM hash is recorded in runtime output/logs
- libretro loads the ROM
- memory domains are exposed

## No Hardcode Rule

Worlds should remain generic:

- It reads `patch.mode=artifact`.
- It emits `patch_artifact` and `patch_contract_ref`.
- It verifies package refs and artifact hashes generically.
- It dispatches artifact materialization based on contract fields.

ALTTP-specific details stay in the LinkedWorld-owned patch manifest and patch
contract:

- `.aplttp`
- `apdelta`
- ALTTP Japan 1.0 base ROM MD5
- `archipelago.json`
- `delta.bsdiff4`
- Sekaiemu output naming

Server-port/no-patch games remain valid through `none`, `contract_only`, and
`server_dispatch` without invoking the `.aplttp` patcher.
