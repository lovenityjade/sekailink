# Current Patch Contract Emission Audit

Date: 2026-05-11

Scope: audit of the Worlds generator patch-contract emission path.

## Current Code Behavior

Current implementation notes from
`clean-room/repos/sekailink-worlds/server/native/sekailink_server_core/src/generic_generation.cpp`:

- `load_linkedworld_generation_surface()` loads `surface.patch` directly from
  `generation/generation-ir.json`.
- It dereferences `patch.manifest.json` from `surface.patch.manifest_ref` when
  declared, stores the resolved manifest under `surface.patch.manifest`, and
  derives `patch.mode` / artifact metadata from the manifest when needed.
- `generate_seed_package_from_linkedworlds()` writes
  `slot_manifest.<slot>.json`.
- The slot manifest sets `patch_artifact` to the mode-dependent artifact path
  for `artifact` mode, or `null` for non-artifact modes.
- The slot manifest always sets `patch_contract_ref`.
- The generator writes the explicit patch contract under
  `patch_contracts/slot-<slot>.patch.contract.json`.
- The patch contract includes the patch mode, resolved manifest, artifact ref,
  base asset, apply host, server dispatch policy, and nested `patch:
  surface.patch`.
- It hashes that JSON contract through `seed_manifest.artifact_hashes`.

This satisfies the contract/artifact separation target. Actual `.aplttp`
materialization is still a later patcher responsibility.

## Priority Behavioral References

Archipelago and MultiworldGG are now priority behavioral references for this
surface. The relevant behavior to preserve is:

- patch artifacts are per-player containers with game/player/server metadata
- artifact manifests declare patch extension, output extension, compatible
  version, base checksum, and procedure
- patch application validates source/base assets before materializing output
- client/runtime code treats the patch artifact and materialized ROM/output as
  separate things
- unsupported procedure steps or incompatible versions fail closed

SekaiLink must translate those expectations into native contracts and tests. It
must not copy upstream implementation code, bypass license boundaries, or make
Python a final native generation requirement.

## Gaps

- The current generator emits the expected artifact path for `artifact` mode,
  but does not yet materialize the `.aplttp` payload.
- `server_dispatch` is passed through from the patch manifest, but this repo
  still needs a documented minimum dispatch schema for server-port/no-patch
  LinkedWorlds.
- Runtime consumers still need end-to-end tests proving they use
  `patch_contract_ref`, nullable `patch_artifact`, and `server_dispatch`
  correctly.

## Expected ALTTP Artifact Slot Manifest

```json
{
  "slot_id": 1,
  "user_id": 1001,
  "display_name": "Jade",
  "game_key": "alttp",
  "linkedworld_id": "alttp",
  "config_version_id": 501,
  "launch_artifact": null,
  "patch_artifact": "patches/slot-1.aplttp",
  "patch_contract_ref": "patch_contracts/slot-1.patch.contract.json",
  "tracker_seed_state": "tracker_seed_state.json",
  "sklmi_contract_ref": "sklmi_seed_contract.json",
  "link_room_contract_ref": "link_room_seed_contract.json",
  "player_options_source": "nexus.config_version_id"
}
```

## Expected ALTTP Artifact Patch Contract

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "seed_id": "seed-example",
  "slot_id": 1,
  "linkedworld_id": "alttp",
  "mode": "artifact",
  "artifact": {
    "path": "patches/slot-1.aplttp",
    "kind": "apdelta",
    "extension": ".aplttp",
    "sha256": "{artifact_sha256}",
    "behavioral_reference": "APDeltaPatch-compatible container semantics"
  },
  "runtime_metadata": {
    "game": "A Link to the Past",
    "server": "",
    "player": 1,
    "player_name": "Jade"
  },
  "procedure": {
    "kind": "native-equivalent",
    "reference_semantics": "apply_bsdiff4(delta.bsdiff4)",
    "python_required_for_final_native_generation": false
  },
  "base_asset": {
    "required": true,
    "setting_key": "lttp_options.rom_file",
    "copy_to": "Zelda no Densetsu - Kamigami no Triforce (Japan).sfc",
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
    "expected_output_extension": ".sfc",
    "output_filename_template": "{patch_stem}.sfc"
  },
  "server_dispatch": {
    "enabled": false
  }
}
```

## Expected No-Patch Slot Manifest

```json
{
  "slot_id": 2,
  "user_id": 1002,
  "display_name": "Port Player",
  "game_key": "server-port-example",
  "linkedworld_id": "server-port-example",
  "config_version_id": 777,
  "launch_artifact": null,
  "patch_artifact": null,
  "patch_contract_ref": "patch_contracts/slot-2.patch.contract.json",
  "tracker_seed_state": "tracker_seed_state.json",
  "sklmi_contract_ref": "sklmi_seed_contract.json",
  "link_room_contract_ref": "link_room_seed_contract.json",
  "player_options_source": "nexus.config_version_id"
}
```

## Expected No-Patch Contract

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "seed_id": "seed-example",
  "slot_id": 2,
  "linkedworld_id": "server-port-example",
  "mode": "none",
  "artifact": null,
  "base_asset": {
    "required": false
  },
  "server_dispatch": {
    "enabled": false
  },
  "reason": "selected runtime does not consume a generated binary patch"
}
```

## Expected Server-Dispatch Contract

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "seed_id": "seed-example",
  "slot_id": 3,
  "linkedworld_id": "server-dispatch-example",
  "mode": "server_dispatch",
  "artifact": null,
  "base_asset": {
    "required": false
  },
  "server_dispatch": {
    "enabled": true,
    "target": "link_room",
    "requires": [
      "room_id",
      "seed_id",
      "slot_id",
      "config_version_id",
      "link_room_seed_contract_ref"
    ]
  }
}
```

## Generator Status

- Done in visible code: `patch.manifest_ref` resolution.
- Done in visible code: `patch_artifact` is nullable and mode-dependent.
- Done in visible code: `patch_contract_ref` is emitted in each slot manifest.
- Done in visible code: contracts are written to
  `patch_contracts/slot-<slot>.patch.contract.json`.
- Done in visible code: non-artifact modes do not require a `patches/` file.

Remaining later work:

- For `patch.mode=artifact`, write or import the declared artifact under
  `patches/` and hash both the artifact and its contract.
- Validate `server_dispatch` manifests against the minimum schema documented in
  `SERVER_PORT_PATCH_CONTRACT_REQUIREMENTS.md`.
- Add runtime consumer tests proving server-port clients reject missing dispatch
  fields instead of guessing.
