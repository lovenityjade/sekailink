# Server-Port Patch Contract Requirements

Date: 2026-05-11

This document defines the remaining no-patch and `server_dispatch` requirements
for server-port games such as Twilight Princess style integrations and Ship of
Harkinian style ports.

The goal is for `Worlds` to emit a correct per-slot patch contract without a ROM
patch artifact. The behavior remains generic: the generator must not branch on
`twilight_princess`, `soh`, executable names, console ids, or emulator families.

## Mode Selection

Use `patch.mode=server_dispatch` when seed behavior is delivered to a running
server-port/runtime through a room service, local bridge, HTTP/WebSocket API, or
native plugin interface.

Use `patch.mode=none` when the slot truly has no patch, no dispatch payload, and
no launch-time mutation. The slot still receives a patch contract so consumers
can distinguish "intentionally no patch" from "generator forgot the patch".

Use `patch.mode=contract_only` when the slot only needs packaged metadata and
runtime contracts that another installed runtime consumes directly.

## Required `server_dispatch` Patch Manifest Shape

The LinkedWorld patch manifest should expose this minimum shape:

```json
{
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "server_dispatch",
    "artifact_kind": "none",
    "requires_base_asset": false,
    "emission": {
      "patch_contract_directory": "patch_contracts/",
      "patch_contract_filename_template": "slot-{slot_id}.patch.contract.json"
    },
    "server_dispatch": {
      "enabled": true,
      "target": "link_room",
      "transport": "room_contract",
      "dispatch_timing": "before_runtime_join",
      "idempotency_key_template": "{seed_id}:{slot_id}:{config_version_id}",
      "requires": [
        "room_id",
        "seed_id",
        "slot_id",
        "linkedworld_id",
        "config_version_id",
        "link_room_seed_contract_ref"
      ],
      "payload_ref": "link_room_seed_contract.json",
      "ack": {
        "required": true,
        "success_field": "accepted",
        "failure_field": "error"
      }
    }
  }
}
```

The emitted `slot_manifest.<slot>.json` should then contain:

```json
{
  "slot_id": 3,
  "linkedworld_id": "server-port-example",
  "patch_mode": "server_dispatch",
  "launch_artifact": null,
  "patch_artifact": null,
  "patch_contract_ref": "patch_contracts/slot-3.patch.contract.json",
  "tracker_seed_state": "tracker_seed_state.json",
  "sklmi_contract_ref": "sklmi_seed_contract.json",
  "link_room_contract_ref": "link_room_seed_contract.json"
}
```

The emitted `patch_contracts/slot-3.patch.contract.json` should contain:

```json
{
  "schema_version": "sekailink-patch-contract-v1",
  "seed_id": "seed-example",
  "slot_id": 3,
  "linkedworld_id": "server-port-example",
  "game_key": "server-port-example",
  "patch_mode": "server_dispatch",
  "mode": "server_dispatch",
  "artifact": null,
  "base_asset": {
    "required": false
  },
  "apply_host": null,
  "server_dispatch": {
    "enabled": true,
    "target": "link_room",
    "transport": "room_contract",
    "dispatch_timing": "before_runtime_join",
    "idempotency_key_template": "{seed_id}:{slot_id}:{config_version_id}",
    "requires": [
      "room_id",
      "seed_id",
      "slot_id",
      "linkedworld_id",
      "config_version_id",
      "link_room_seed_contract_ref"
    ],
    "payload_ref": "link_room_seed_contract.json",
    "ack": {
      "required": true,
      "success_field": "accepted",
      "failure_field": "error"
    }
  }
}
```

## Dispatch Field Requirements

- `enabled`: must be `true` for `patch.mode=server_dispatch`.
- `target`: names the generic consumer, for example `link_room`,
  `local_runtime_bridge`, or `native_plugin_host`; it must not be a hardcoded
  game id.
- `transport`: names the delivery mechanism, for example `room_contract`,
  `http`, `websocket`, `ipc`, or `plugin_call`.
- `dispatch_timing`: declares when the payload is sent. Known values are
  `before_runtime_join`, `on_runtime_ready`, and `on_room_start`.
- `requires`: lists package fields that must be present before dispatch.
- `payload_ref`: points to a seed package file, usually
  `link_room_seed_contract.json`, never an opaque external side channel.
- `idempotency_key_template`: prevents duplicate dispatch side effects when a
  client reconnects or a room server retries.
- `ack`: declares whether dispatch must be acknowledged and which fields carry
  success/failure.

## No-Patch Requirements

For `patch.mode=none`:

```json
{
  "patch": {
    "schema_version": "sekailink-patch-contract-v1",
    "mode": "none",
    "artifact_kind": "none",
    "requires_base_asset": false,
    "emission": {
      "patch_contract_directory": "patch_contracts/",
      "patch_contract_filename_template": "slot-{slot_id}.patch.contract.json"
    },
    "server_dispatch": {
      "enabled": false
    },
    "reason": "runtime consumes the seed package without patch or dispatch"
  }
}
```

The slot manifest must set `patch_artifact` to `null` and still set
`patch_contract_ref`.

## Server-Port Safety Rules

- Do not emit a placeholder patch file for no-patch/server-dispatch slots.
- Do not infer dispatch target from game name.
- Do not hide dispatch payloads in launcher command strings.
- Do not require a base ROM or ISO when `requires_base_asset=false`.
- Do not make server dispatch depend on Python-only tooling.
- Do not allow runtime consumers to proceed when required dispatch fields are
  missing.
- Do hash the patch contract in `seed_manifest.artifact_hashes`.
- Do treat the seed package as the source of truth for room/session state.

## Examples

Twilight Princess style server-port:

- `patch.mode=server_dispatch`
- `server_dispatch.target=link_room`
- `server_dispatch.transport=room_contract`
- `payload_ref=link_room_seed_contract.json`
- `patch_artifact=null`

Ship of Harkinian style installed runtime:

- Use `patch.mode=server_dispatch` if Link Room injects seed state through a
  runtime bridge or plugin.
- Use `patch.mode=contract_only` if the installed runtime reads the packaged
  contracts directly and no dispatch action is required.
- Use `patch.mode=none` only if no patch, dispatch, or contract mutation is
  required beyond the standard seed package.

These are examples of transport semantics, not game-specific branches.
