#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../../../.." && pwd)"
BUILD_ROOT="${SEKAILINK_ROOM_SERVER_BUILD_ROOT:-/tmp/sekailink-link-room-build}"
RUNTIME_ROOT="${SEKAILINK_ROOM_SERVER_RUNTIME_ROOT:-/tmp/sekailink-link-room-loopback}"

SERVICE_BIN="${SEKAILINK_ROOM_SERVER_SERVICE_BIN:-${BUILD_ROOT}/sekailink_room_server_service}"
CLI_BIN="${SEKAILINK_ROOM_SERVER_TCP_CLI_BIN:-${BUILD_ROOT}/sekailink_room_server_tcp_cli}"
CONFIG_PATH="${RUNTIME_ROOT}/room_server.json"
STATE_PATH="${RUNTIME_ROOT}/runtime/room_server_state.json"
LOG_PATH="${RUNTIME_ROOT}/logs/service.log"
PAYLOAD_DIR="${SCRIPT_DIR}/payloads"
SEND_SCRIPT="${SCRIPT_DIR}/send-room-command.sh"

export SEKAILINK_ROOM_SERVER_ADMIN_TOKEN="${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN:-admin-loopback-token}"
export SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN="${SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN:-runtime-loopback-token}"
export SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN="${SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN:-client-loopback-token}"

if [[ ! -x "${SERVICE_BIN}" ]]; then
  echo "[run-local-alttp-loopback][error] missing service binary: ${SERVICE_BIN}" >&2
  exit 1
fi

if [[ ! -x "${CLI_BIN}" ]]; then
  echo "[run-local-alttp-loopback][error] missing tcp cli binary: ${CLI_BIN}" >&2
  exit 1
fi

rm -rf "${RUNTIME_ROOT}"
mkdir -p "${RUNTIME_ROOT}/audit" "${RUNTIME_ROOT}/projection" "${RUNTIME_ROOT}/runtime" "${RUNTIME_ROOT}/logs"

cp "${SCRIPT_DIR}/room_server.alttp-live.loopback.json.example" "${CONFIG_PATH}"
sed -i "s#/opt/sekailink/link/room-server/data/audit#${RUNTIME_ROOT}/audit#g" "${CONFIG_PATH}"
sed -i "s#/opt/sekailink/link/room-server/data/projection/room_projection.sqlite3#${RUNTIME_ROOT}/projection/room_projection.sqlite3#g" "${CONFIG_PATH}"

"${SERVICE_BIN}" --config "${CONFIG_PATH}" --state-file "${STATE_PATH}" >"${LOG_PATH}" 2>&1 &
SERVICE_PID=$!

cleanup() {
  kill "${SERVICE_PID}" >/dev/null 2>&1 || true
  wait "${SERVICE_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in $(seq 1 60); do
  if curl -fsS http://127.0.0.1:18081/health >/dev/null 2>&1; then
    break
  fi
  sleep 0.25
done

TCP_PORT="$(jq -r '.effective_tcp_port' "${STATE_PATH}")"
HTTP_PORT="$(jq -r '.effective_http_port' "${STATE_PATH}")"

SEKAILINK_ROOM_SERVER_TCP_CLI_BIN="${CLI_BIN}" \
  bash "${SEND_SCRIPT}" --host 127.0.0.1 --port "${TCP_PORT}" --channel admin \
  --token-env SEKAILINK_ROOM_SERVER_ADMIN_TOKEN \
  --command-file "${PAYLOAD_DIR}/create-room.alttp-live.command.json"

SEKAILINK_ROOM_SERVER_TCP_CLI_BIN="${CLI_BIN}" \
  bash "${SEND_SCRIPT}" --host 127.0.0.1 --port "${TCP_PORT}" --channel runtime \
  --token-env SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN \
  --command-file "${PAYLOAD_DIR}/set-slot-data.alttp-live.command.json"

SEKAILINK_ROOM_SERVER_TCP_CLI_BIN="${CLI_BIN}" \
  bash "${SEND_SCRIPT}" --host 127.0.0.1 --port "${TCP_PORT}" --channel runtime \
  --token-env SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN \
  --command-file "${PAYLOAD_DIR}/enqueue-item.alttp-live.command.json"

SEKAILINK_ROOM_SERVER_TCP_CLI_BIN="${CLI_BIN}" \
  bash "${SEND_SCRIPT}" --host 127.0.0.1 --port "${TCP_PORT}" --channel runtime \
  --token-env SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN \
  --command-file "${PAYLOAD_DIR}/record-check.alttp-live.command.json"

sleep 0.5

echo "--- HEALTH ---"
curl -fsS "http://127.0.0.1:${HTTP_PORT}/health"
echo
echo "--- ROOMS ---"
curl -fsS -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${HTTP_PORT}/rooms"
echo
echo "--- SUMMARY ---"
curl -fsS -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${HTTP_PORT}/rooms/alttp-live-1/summary"
echo
echo "--- SNAPSHOT ---"
curl -fsS -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${HTTP_PORT}/rooms/alttp-live-1/snapshot"
echo
echo "--- EVENTS ---"
curl -fsS -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://127.0.0.1:${HTTP_PORT}/rooms/alttp-live-1/events"
echo
echo "--- STATE FILE ---"
cat "${STATE_PATH}"
echo
echo "--- LOG TAIL ---"
tail -n 80 "${LOG_PATH}"
