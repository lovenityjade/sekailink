# SKLMI Private Link Room Staging

Status: staging guide
Date: 2026-05-05

## Goal

Prepare a safe `SKLMI` runtime connection toward the already validated private
modern Link Room host at `link.sekailink.com`, using the host-side private ports
`28080` and `28081` when needed.

This guide stays inside `sklmi`. It does not change the room service itself.

## What The Runtime Accepts

`sekailink_sklmi_runtime` in `sekailink_game_server` mode now accepts either:

- one shared room TCP port
  - `--room-port <port>`
- or split control/runtime ports
  - `--room-control-port <port>`
  - `--room-runtime-port <port>`

The room host can now be either:

- a literal IP address
- or a hostname such as `link.sekailink.com`

## Token Modes

There are two safe ways to connect:

### 1. Ticket Issuance Mode

Use this when `SKLMI` should request its own runtime session ticket.

Required:

- `--room-host`
- `--room-session-name`
- `--room-slot-id`
- runtime endpoint:
  - `--room-port`, or
  - `--room-runtime-port`
- control endpoint:
  - `--room-port`, or
  - `--room-control-port`
- `--room-control-channel core|admin`
- `--room-control-auth-token`

Optional:

- `--room-runtime-auth-token`

Protocol shape:

- `issue_ticket` goes to the selected control port/channel
- `runtime_event`, `pending_items`, and `acknowledge_delivery` go to the
  selected runtime port on the `runtime` channel

Expected `pending_items[]` delivery shape for room-controlled injections:

- required:
  - `delivery_id`
- recommended stable identity:
  - `item_id`
  - or `event_key`
  - or `tracker_semantic_id`
- recommended display/mapping aid:
  - `mapped_value`
- optional human label:
  - `item_name`

### 2. Pre-Issued Runtime Session Mode

Use this when the private room already gave you a runtime session token.

Required:

- `--room-host`
- `--room-session-name`
- `--room-slot-id`
- runtime endpoint:
  - `--room-port`, or
  - `--room-runtime-port`
- `--room-runtime-session-token`

Optional:

- `--room-runtime-auth-token`
- `--room-control-channel`
- `--room-control-auth-token`
- `--room-control-port`

In this mode `SKLMI` skips `issue_ticket`, which is useful if the private room
separates ticket issuance from runtime traffic across `28080` and `28081`.

## Mapping The Private Host Ports

`SKLMI` now supports both patterns:

- one-port room:
  - point `--room-port` to the single working endpoint
- two-port room:
  - set `--room-control-port 28080`
  - set `--room-runtime-port 28081`

If the real deployment uses the reverse mapping, swap them. `SKLMI` does not
hardcode semantics to those numbers.

If only the runtime port is available to you, use the pre-issued runtime session
mode and omit the control side entirely.

## Safe Staging Helper

Use the helper script:

```bash
scripts/sklmi_link_room_stage.sh probe
scripts/sklmi_link_room_stage.sh print-env
scripts/sklmi_link_room_stage.sh print-cmd
```

Defaults:

- host: `link.sekailink.com`
- control port: `28080`
- runtime port: `28081`

The script never launches the runtime by default. It only:

- probes TCP reachability
- prints the effective staging environment
- prints a runtime command template

Example env seed:

- [scripts/sklmi_link_room_private.env.example](../scripts/sklmi_link_room_private.env.example)

## Example Runtime Invocation

Shared-port room:

```bash
build/sekailink_sklmi_runtime \
  --memory-socket /tmp/sekaiemu.sock \
  --bridge-manifest /tmp/linkedworld-bridge.json \
  --room-state /tmp/linkedworld-room.state \
  --runtime-state /tmp/linkedworld-runtime-state \
  --trace-log /tmp/linkedworld-trace.jsonl \
  --mode sekailink_game_server \
  --room-host link.sekailink.com \
  --room-port 28080 \
  --room-session-name your-session \
  --room-slot-id 1 \
  --room-control-channel core \
  --room-control-auth-token your-control-token \
  --room-runtime-auth-token your-runtime-token \
  --max-ticks 5
```

Split-port room with pre-issued runtime token:

```bash
build/sekailink_sklmi_runtime \
  --memory-socket /tmp/sekaiemu.sock \
  --bridge-manifest /tmp/linkedworld-bridge.json \
  --room-state /tmp/linkedworld-room.state \
  --runtime-state /tmp/linkedworld-runtime-state \
  --trace-log /tmp/linkedworld-trace.jsonl \
  --mode sekailink_game_server \
  --room-host link.sekailink.com \
  --room-runtime-port 28081 \
  --room-session-name your-session \
  --room-slot-id 1 \
  --room-runtime-session-token your-runtime-session-token \
  --room-runtime-auth-token your-runtime-token \
  --max-ticks 5
```

## What To Check First

For a safe first live attempt, keep `--max-ticks 5` and inspect:

1. `trace.jsonl`
2. `runtime-state/*.room-sync.state`

The startup trace now includes:

- `host=...`
- `control_port=...`
- `runtime_port=...`
- whether the runtime session token was provided or issued at connect time

That makes endpoint/token mistakes visible before deeper LinkedWorld-specific
debugging starts.

## Matching Rules In SKLMI

For one room delivery, `SKLMI` currently tries to match one `room_controlled`
manifest action in this order:

1. `item_id` from the room delivery against manifest `item_id`
2. `item_name` against manifest `item_name`
3. `event_key` or `tracker_semantic_id` against manifest `event_key`
4. `mapped_value` against manifest `mapped_value`

For live LinkedWorld runs, the most stable room payload is usually:

- `delivery_id`
- `tracker_semantic_id`
- `mapped_value`
- `item_id` when the room can expose the Archipelago item id directly

If no rule matches, `apply_room_item` now logs the incoming room delivery
identity plus all local candidate rules, so mismatches are visible in one place.

Useful live trace sequence for one supported room-controlled item delivery:

- `room_item_pending`
- `room_item_applied`
- `room_item_acknowledged`

If the sequence stops at `room_item_pending`, inspect:

- `item_id`
- `tracker_semantic_id`
- `mapped_value`
- `item_name`

and compare them to the manifest action identity.

ALTTP was the first fully exercised private-room fixture here, but the payload
shape and matching order above are generic `SKLMI` behavior, not ALTTP-only
rules.
