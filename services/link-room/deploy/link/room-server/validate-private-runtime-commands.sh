#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_ENV="${SELF_DIR}/room-server.alttp-live.profile.env.example"
ROOM_ENV=""
STATE_FILE=""
SEND_HELPER="${SELF_DIR}/send-room-command.sh"
HTTP_HOST="127.0.0.1"
HTTP_PORT=""
ROOM_ID=""
SLOT_ID=""
SESSION_NAME=""
CANONICAL_ID="1001"
LINKEDWORLD_ROOT=""

resolve_room_env_path() {
  local preferred="${1:-}"
  if [[ -n "${preferred}" ]]; then
    if [[ -f "${preferred}" ]]; then
      echo "${preferred}"
      return 0
    fi
    local alternate="${preferred/room-server.env/room_server.env}"
    if [[ "${alternate}" == "${preferred}" ]]; then
      alternate="${preferred/room_server.env/room-server.env}"
    fi
    if [[ -f "${alternate}" ]]; then
      echo "${alternate}"
      return 0
    fi
    return 1
  fi
  return 1
}

usage() {
  cat <<'EOF'
usage: validate-private-runtime-commands.sh [--profile-env <file>] [--room-env <file>] [--state-file <file>] [--room-id <id>] [--slot-id <id>] [--session-name <name>] [--canonical-id <id>] [--linkedworld-root <dir>]

Replays the private modern Link Room test sequence for the SKLMI-facing runtime
surface:
create_room -> apply_linkedworld_surface -> set_slot_data ->
enqueue_received_item -> issue_ticket -> pending_items -> runtime_event ->
acknowledge_delivery -> pending_items
and then confirms the room summary/snapshot over HTTP.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile-env)
      PROFILE_ENV="$2"
      shift 2
      ;;
    --room-env)
      ROOM_ENV="$2"
      shift 2
      ;;
    --state-file)
      STATE_FILE="$2"
      shift 2
      ;;
    --room-id)
      ROOM_ID="$2"
      shift 2
      ;;
    --slot-id)
      SLOT_ID="$2"
      shift 2
      ;;
    --session-name)
      SESSION_NAME="$2"
      shift 2
      ;;
    --canonical-id)
      CANONICAL_ID="$2"
      shift 2
      ;;
    --linkedworld-root)
      LINKEDWORLD_ROOT="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[validate-private-runtime-commands][error] unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${PROFILE_ENV}" ]]; then
  echo "[validate-private-runtime-commands][error] profile env not found: ${PROFILE_ENV}" >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "[validate-private-runtime-commands][error] jq is required." >&2
  exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
  echo "[validate-private-runtime-commands][error] curl is required." >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${PROFILE_ENV}"
set +a

ROOM_ROOT="${SEKAILINK_LINK_ROOM_ROOT:-/opt/sekailink/link/room-server}"
ROOM_ENV="${ROOM_ENV:-${SEKAILINK_LINK_ROOM_ENV:-${ROOM_ROOT}/config/room_server.env}}"
STATE_FILE="${STATE_FILE:-${SEKAILINK_LINK_ROOM_STATE:-${ROOM_ROOT}/runtime/room_server_state.json}}"
ROOM_ID="${ROOM_ID:-${SEKAILINK_ALTTP_TEST_ROOM_ID:-alttp-live-1}}"
SLOT_ID="${SLOT_ID:-${SEKAILINK_ALTTP_TEST_SLOT_ID:-1}}"
SESSION_NAME="${SESSION_NAME:-${ROOM_ID}}"
LINKEDWORLD_ROOT="${LINKEDWORLD_ROOT:-${SEKAILINK_LINKEDWORLD_ALTTP_ROOT:-${SELF_DIR}/../../../../sekailink-linkedworld-alttp}}"

if ! ROOM_ENV="$(resolve_room_env_path "${ROOM_ENV}")"; then
  echo "[validate-private-runtime-commands][error] room env not found near: ${ROOM_ENV}" >&2
  exit 1
fi

if [[ ! -f "${STATE_FILE}" ]]; then
  echo "[validate-private-runtime-commands][error] state file not found: ${STATE_FILE}" >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${ROOM_ENV}"
set +a

HTTP_PORT="$(jq -r '.effective_http_port // 0' "${STATE_FILE}")"
if [[ "${HTTP_PORT}" == "0" ]]; then
  echo "[validate-private-runtime-commands][error] invalid http port in state file: ${STATE_FILE}" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

CREATE_ROOM_FILE="${TMP_DIR}/create-room.json"
APPLY_LINKEDWORLD_FILE="${TMP_DIR}/apply-linkedworld-surface.json"
SLOT_DATA_FILE="${TMP_DIR}/set-slot-data.json"
ENQUEUE_ITEM_FILE="${TMP_DIR}/enqueue-item.json"
ISSUE_TICKET_FILE="${TMP_DIR}/issue-ticket.json"
PENDING_ITEMS_FILE="${TMP_DIR}/pending-items.json"
RUNTIME_EVENT_FILE="${TMP_DIR}/runtime-event.json"
ACK_FILE="${TMP_DIR}/ack.json"

jq -n \
  --arg room_id "${ROOM_ID}" \
  --arg room_type "${SEKAILINK_ALTTP_TEST_ROOM_TYPE:-live}" \
  --arg game "${SEKAILINK_ALTTP_TEST_GAME:-A Link to the Past}" \
  --arg slot_name "${SEKAILINK_ALTTP_TEST_SLOT_NAME:-Link}" \
  --arg slot_alias "${SEKAILINK_ALTTP_TEST_SLOT_ALIAS:-ALTTP Link}" \
  --arg seed_name "${SEKAILINK_ALTTP_TEST_SEED_NAME:-alttp-live-seed-1}" \
  --arg seed_id "${SEKAILINK_ALTTP_TEST_SEED_ID:-seed-live-1}" \
  --arg seed_hash "${SEKAILINK_ALTTP_TEST_SEED_HASH:-BOW-MOON-MAP}" \
  --arg tracker_pack "${SEKAILINK_ALTTP_TEST_TRACKER_PACK:-alttp-pack}" \
  --arg tracker_variant "${SEKAILINK_ALTTP_TEST_TRACKER_VARIANT:-Map Tracker - AP}" \
  --argjson team_id "${SEKAILINK_ALTTP_TEST_TEAM_ID:-0}" \
  --argjson slot_id "${SLOT_ID}" \
  '{
    cmd:"create_room",
    room_id:$room_id,
    room_type:$room_type,
    game:$game,
    team_id:$team_id,
    slot_id:$slot_id,
    slot_name:$slot_name,
    slot_alias:$slot_alias,
    seed_name:$seed_name,
    seed_id:$seed_id,
    seed_hash:$seed_hash,
    tracker_pack:$tracker_pack,
    tracker_variant:$tracker_variant
  }' > "${CREATE_ROOM_FILE}"

LINKEDWORLD_MANIFEST="${LINKEDWORLD_ROOT}/manifest/manifest.json"
LINKEDWORLD_ROOM_METADATA="${LINKEDWORLD_ROOT}/metadata/room-metadata.complete.json"
LINKEDWORLD_SLOT_DATA="${LINKEDWORLD_ROOT}/metadata/slot-data.complete.json"
LINKEDWORLD_PRESET="${LINKEDWORLD_ROOT}/presets/alttp.runtime-complete.side-by-side.json"
LINKEDWORLD_BRIDGE="${LINKEDWORLD_ROOT}/bridge/sklmi.phase1.json"
if [[ -f "${LINKEDWORLD_MANIFEST}" && -f "${LINKEDWORLD_ROOM_METADATA}" && -f "${LINKEDWORLD_SLOT_DATA}" && -f "${LINKEDWORLD_PRESET}" && -f "${LINKEDWORLD_BRIDGE}" ]]; then
  jq -n \
    --arg room_id "${ROOM_ID}" \
    --slurpfile manifest "${LINKEDWORLD_MANIFEST}" \
    --slurpfile room_metadata "${LINKEDWORLD_ROOM_METADATA}" \
    --slurpfile slot_data_contract "${LINKEDWORLD_SLOT_DATA}" \
    --slurpfile preset "${LINKEDWORLD_PRESET}" \
    --slurpfile bridge "${LINKEDWORLD_BRIDGE}" \
    '{
      cmd:"apply_linkedworld_surface",
      room_id:$room_id,
      manifest:$manifest[0],
      room_metadata:$room_metadata[0],
      slot_data_contract:$slot_data_contract[0],
      preset:$preset[0],
      bridge:$bridge[0]
    }' > "${APPLY_LINKEDWORLD_FILE}"
else
  echo "[validate-private-runtime-commands][warn] LinkedWorld files not found under ${LINKEDWORLD_ROOT}; surface apply will be skipped." >&2
fi

jq -n \
  --arg room_id "${ROOM_ID}" \
  --arg goal "${SEKAILINK_ALTTP_TEST_GOAL:-ganon}" \
  --arg mode "${SEKAILINK_ALTTP_TEST_MODE:-open}" \
  --arg difficulty "${SEKAILINK_ALTTP_TEST_DIFFICULTY:-normal}" \
  --arg weapons "${SEKAILINK_ALTTP_TEST_WEAPONS:-randomized}" \
  '{
    cmd:"set_slot_data",
    room_id:$room_id,
    slot_data:{
      goal:$goal,
      mode:$mode,
      difficulty:$difficulty,
      weapons:$weapons
    }
  }' > "${SLOT_DATA_FILE}"

jq -n \
  --arg room_id "${ROOM_ID}" \
  '{
    cmd:"enqueue_received_item",
    room_id:$room_id,
    item:{
      item_id:1001,
      item_name:"Hookshot",
      location_id:1001,
      sender_slot:2,
      sender_alias:"Cloud",
      flags:0
    }
  }' > "${ENQUEUE_ITEM_FILE}"

echo "[validate-private-runtime-commands] create_room"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel admin \
  --command-file "${CREATE_ROOM_FILE}" > "${TMP_DIR}/create-room.response.json"
jq -c '.' "${TMP_DIR}/create-room.response.json"

if [[ -f "${APPLY_LINKEDWORLD_FILE}" ]]; then
  echo "[validate-private-runtime-commands] apply_linkedworld_surface"
  bash "${SEND_HELPER}" \
    --env-file "${ROOM_ENV}" \
    --state-file "${STATE_FILE}" \
    --channel admin \
    --command-file "${APPLY_LINKEDWORLD_FILE}" | jq -c '. | {ok, channel, linkedworld_id:(.linkedworld_surface.linkedworld_id // null), item_semantic_count:(.linkedworld_surface.item_semantics | length)}'
fi

echo "[validate-private-runtime-commands] set_slot_data"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${SLOT_DATA_FILE}" | jq -c '.'

echo "[validate-private-runtime-commands] enqueue_received_item"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${ENQUEUE_ITEM_FILE}" | jq -c '.'

jq -n \
  --arg session_name "${SESSION_NAME}" \
  --argjson slot_id "${SLOT_ID}" \
  '{
    cmd:"issue_ticket",
    session_name:$session_name,
    slot_id:$slot_id,
    client_kind:"runtime",
    driver_instance_id:"sklmi-driver-1",
    linkedworld_id:"alttp",
    core_profile:"snes_v1"
  }' > "${ISSUE_TICKET_FILE}"

echo "[validate-private-runtime-commands] issue_ticket"
ISSUE_RESPONSE="$(bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel admin \
  --command-file "${ISSUE_TICKET_FILE}")"
echo "${ISSUE_RESPONSE}" | jq -c '.'
SESSION_TOKEN="$(echo "${ISSUE_RESPONSE}" | jq -r '.session_token // empty')"
if [[ -z "${SESSION_TOKEN}" ]]; then
  echo "[validate-private-runtime-commands][error] issue_ticket did not return session_token." >&2
  exit 1
fi

jq -n \
  --arg session_name "${SESSION_NAME}" \
  --arg session_token "${SESSION_TOKEN}" \
  --argjson slot_id "${SLOT_ID}" \
  '{
    cmd:"pending_items",
    session_name:$session_name,
    slot_id:$slot_id,
    session_token:$session_token
  }' > "${PENDING_ITEMS_FILE}"

echo "[validate-private-runtime-commands] pending_items before ack"
PENDING_RESPONSE="$(bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${PENDING_ITEMS_FILE}")"
echo "${PENDING_RESPONSE}" | jq -c '.'
DELIVERY_ID="$(echo "${PENDING_RESPONSE}" | jq -r '.pending_items[0].delivery_id // empty')"
if [[ -z "${DELIVERY_ID}" ]]; then
  echo "[validate-private-runtime-commands][error] pending_items returned no delivery_id." >&2
  exit 1
fi
ITEM_EVENT_KEY="$(echo "${PENDING_RESPONSE}" | jq -r '.pending_items[0].event_key // .pending_items[0].tracker_semantic_id // empty')"
ITEM_MAPPED_VALUE="$(echo "${PENDING_RESPONSE}" | jq -r '.pending_items[0].mapped_value // empty')"
if [[ -z "${ITEM_EVENT_KEY}" || -z "${ITEM_MAPPED_VALUE}" ]]; then
  echo "[validate-private-runtime-commands][error] pending_items is missing converted LinkedWorld item semantics." >&2
  echo "[validate-private-runtime-commands][error] expected one of: event_key or tracker_semantic_id, plus mapped_value." >&2
  echo "[validate-private-runtime-commands][error] current first pending item: $(echo "${PENDING_RESPONSE}" | jq -c '.pending_items[0]')" >&2
  exit 1
fi

jq -n \
  --arg session_name "${SESSION_NAME}" \
  --arg session_token "${SESSION_TOKEN}" \
  --argjson slot_id "${SLOT_ID}" \
  --argjson canonical_id "${CANONICAL_ID}" \
  '{
    cmd:"runtime_event",
    session_name:$session_name,
    session_token:$session_token,
    slot_id:$slot_id,
    driver_instance_id:"sklmi-driver-1",
    linkedworld_id:"alttp",
    core_profile:"snes_v1",
    event_type:"location_checked",
    canonical_id:$canonical_id
  }' > "${RUNTIME_EVENT_FILE}"

echo "[validate-private-runtime-commands] runtime_event"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${RUNTIME_EVENT_FILE}" | jq -c '.'

jq -n \
  --arg session_name "${SESSION_NAME}" \
  --arg session_token "${SESSION_TOKEN}" \
  --argjson slot_id "${SLOT_ID}" \
  --argjson delivery_id "${DELIVERY_ID}" \
  '{
    cmd:"acknowledge_delivery",
    session_name:$session_name,
    slot_id:$slot_id,
    delivery_id:$delivery_id,
    session_token:$session_token
  }' > "${ACK_FILE}"

echo "[validate-private-runtime-commands] acknowledge_delivery"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${ACK_FILE}" | jq -c '.'

echo "[validate-private-runtime-commands] pending_items after ack"
bash "${SEND_HELPER}" \
  --env-file "${ROOM_ENV}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${PENDING_ITEMS_FILE}" | jq -c '.'

echo "[validate-private-runtime-commands] room summary"
curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://${HTTP_HOST}:${HTTP_PORT}/rooms/${ROOM_ID}/summary" | jq -c '.'

echo "[validate-private-runtime-commands] room snapshot"
curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://${HTTP_HOST}:${HTTP_PORT}/rooms/${ROOM_ID}/snapshot" | jq -c '.'
