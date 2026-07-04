#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_ROOT="${1:-/home/debian/sekailink-room-live-staging/private-instance}"
CLI_BIN="${SEKAILINK_PRIVATE_ROOM_TCP_CLI_BIN:-/home/debian/sekailink-room-live-staging/bin/sekailink_room_server_tcp_cli}"
ENV_FILE="${STAGE_ROOT}/config/room_server.env"
STATE_FILE="${STAGE_ROOT}/runtime/room_server_state.json"
PAYLOAD_ROOT="${SEKAILINK_PRIVATE_ROOM_PAYLOAD_ROOT:-${SELF_DIR}/payloads}"

if [[ ! -d "${PAYLOAD_ROOT}" && -d "${SELF_DIR}/../payloads" ]]; then
  PAYLOAD_ROOT="${SELF_DIR}/../payloads"
fi

export SEKAILINK_ROOM_SERVER_TCP_CLI_BIN="${CLI_BIN}"

bash "${SELF_DIR}/send-room-command.sh" \
  --env-file "${ENV_FILE}" \
  --state-file "${STATE_FILE}" \
  --channel admin \
  --command-file "${PAYLOAD_ROOT}/create-room.alttp-live.command.json"

bash "${SELF_DIR}/send-room-command.sh" \
  --env-file "${ENV_FILE}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${PAYLOAD_ROOT}/set-slot-data.alttp-live.command.json"

bash "${SELF_DIR}/send-room-command.sh" \
  --env-file "${ENV_FILE}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${PAYLOAD_ROOT}/enqueue-item.alttp-live.command.json"

bash "${SELF_DIR}/send-room-command.sh" \
  --env-file "${ENV_FILE}" \
  --state-file "${STATE_FILE}" \
  --channel runtime \
  --command-file "${PAYLOAD_ROOT}/record-check.alttp-live.command.json"

set -a
source "${ENV_FILE}"
set +a

curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:$(jq -r '.effective_http_port // 0' "${STATE_FILE}")/rooms/alttp-live-1/snapshot"
printf '\n'
curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:$(jq -r '.effective_http_port // 0' "${STATE_FILE}")/rooms/alttp-live-1/events"
printf '\n---LOG---\n'
tail -n 60 "${STAGE_ROOT}/logs/service.log"
