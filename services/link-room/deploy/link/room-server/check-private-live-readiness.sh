#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_ENV="${SELF_DIR}/room-server.alttp-live.profile.env.example"
ROOM_ENV=""
STATE_FILE=""
SERVICE_LOG=""
SEND_HELPER="${SELF_DIR}/send-room-command.sh"
SYSTEMD_UNIT=""
JOURNAL_LINES="20"

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
usage: check-private-live-readiness.sh [--profile-env <file>] [--room-env <file>] [--state-file <file>] [--service-log <file>] [--systemd-unit <unit>] [--journal-lines <n>]

Reads the room-server operator profile, env file, state file and logs to verify
that a private live deployment on link.sekailink.com is ready for an operator-
only ALTTP smoke.
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
    --service-log)
      SERVICE_LOG="$2"
      shift 2
      ;;
    --systemd-unit)
      SYSTEMD_UNIT="$2"
      shift 2
      ;;
    --journal-lines)
      JOURNAL_LINES="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[check-private-live-readiness][error] unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${PROFILE_ENV}" ]]; then
  echo "[check-private-live-readiness][error] profile env not found: ${PROFILE_ENV}" >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${PROFILE_ENV}"
set +a

ROOM_ROOT="${SEKAILINK_LINK_ROOM_ROOT:-/opt/sekailink/link/room-server}"
ROOM_ENV="${ROOM_ENV:-${SEKAILINK_LINK_ROOM_ENV:-${ROOM_ROOT}/config/room_server.env}}"
STATE_FILE="${STATE_FILE:-${SEKAILINK_LINK_ROOM_STATE:-${ROOM_ROOT}/runtime/room_server_state.json}}"
SERVICE_LOG="${SERVICE_LOG:-${SEKAILINK_LINK_ROOM_LOG:-/opt/sekailink/link/logs/room-server/service.log}}"
CONFIG_FILE="${ROOM_ROOT}/config/room_server.json"
BINARY_FILE="${ROOM_ROOT}/bin/sekailink_room_server_service"
CLI_FILE="${ROOM_ROOT}/bin/sekailink_room_server_tcp_cli"
SYSTEMD_UNIT="${SYSTEMD_UNIT:-${SEKAILINK_LINK_ROOM_SERVICE:-sekailink-room-server.service}}"

if ! ROOM_ENV="$(resolve_room_env_path "${ROOM_ENV}")"; then
  echo "[check-private-live-readiness][error] room env not found near: ${ROOM_ENV}" >&2
  exit 1
fi

for path in "${BINARY_FILE}" "${CONFIG_FILE}" "${ROOM_ENV}" "${STATE_FILE}"; do
  if [[ ! -f "${path}" ]]; then
    echo "[check-private-live-readiness][error] required file not found: ${path}" >&2
    exit 1
  fi
done

if ! command -v jq >/dev/null 2>&1; then
  echo "[check-private-live-readiness][error] jq is required." >&2
  exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
  echo "[check-private-live-readiness][error] curl is required." >&2
  exit 1
fi

if [[ ! -f "${SERVICE_LOG}" ]]; then
  SERVICE_LOG=""
fi

CLI_AVAILABLE="true"
if [[ ! -f "${CLI_FILE}" ]]; then
  CLI_AVAILABLE="false"
fi

set -a
# shellcheck disable=SC1090
source "${ROOM_ENV}"
set +a

PROJECTION_BACKEND="$(jq -r '.projection_backend // "jsonl"' "${CONFIG_FILE}")"
PROJECTION_TARGET="$(jq -r '.projection_root // ""' "${CONFIG_FILE}")"

for token_env in \
  SEKAILINK_ROOM_SERVER_ADMIN_TOKEN \
  SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN \
  SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN; do
  if [[ -z "${!token_env:-}" ]]; then
    echo "[check-private-live-readiness][error] missing token env: ${token_env}" >&2
    exit 1
  fi
done

TCP_HOST="$(jq -r '.effective_tcp_host // "127.0.0.1"' "${STATE_FILE}")"
TCP_PORT="$(jq -r '.effective_tcp_port // 0' "${STATE_FILE}")"
HTTP_HOST="$(jq -r '.effective_http_host // "127.0.0.1"' "${STATE_FILE}")"
HTTP_PORT="$(jq -r '.effective_http_port // 0' "${STATE_FILE}")"
STATUS="$(jq -r '.status // "unknown"' "${STATE_FILE}")"
ROOM_COUNT="$(jq -r '.room_count // 0' "${STATE_FILE}")"
LOOPBACK_ONLY="$(jq -r '.loopback_only // false' "${STATE_FILE}")"
ADMIN_AUTH_ENABLED="$(jq -r '.admin_auth_enabled // false' "${STATE_FILE}")"
RUNTIME_AUTH_ENABLED="$(jq -r '.runtime_auth_enabled // false' "${STATE_FILE}")"

if [[ "${TCP_PORT}" == "0" || "${HTTP_PORT}" == "0" ]]; then
  echo "[check-private-live-readiness][error] invalid effective ports in state file: ${STATE_FILE}" >&2
  exit 1
fi

MYSQL_HOST="${SEKAILINK_MYSQL_HOST:-}"
MYSQL_PORT="${SEKAILINK_MYSQL_PORT:-3306}"
MYSQL_USER="${SEKAILINK_MYSQL_USER:-}"
MYSQL_SOCKET="${SEKAILINK_MYSQL_SOCKET:-}"
MYSQL_PASSWORD_PRESENT="false"
if [[ -n "${SEKAILINK_MYSQL_PASSWORD:-}" ]]; then
  MYSQL_PASSWORD_PRESENT="true"
fi

if [[ "${PROJECTION_BACKEND}" == "mysql" ]]; then
  if [[ -z "${PROJECTION_TARGET}" || "${PROJECTION_TARGET}" == replace-* ]]; then
    echo "[check-private-live-readiness][error] invalid mysql projection target in config: ${CONFIG_FILE}" >&2
    exit 1
  fi
  if [[ -z "${MYSQL_USER}" ]]; then
    echo "[check-private-live-readiness][error] missing SEKAILINK_MYSQL_USER for mysql projection." >&2
    exit 1
  fi
  if [[ -z "${MYSQL_HOST}" && -z "${MYSQL_SOCKET}" ]]; then
    echo "[check-private-live-readiness][error] mysql projection needs SEKAILINK_MYSQL_HOST or SEKAILINK_MYSQL_SOCKET." >&2
    exit 1
  fi
fi

HEALTH_JSON="$(curl -fsS "http://${HTTP_HOST}:${HTTP_PORT}/health")"
ROOMS_JSON="$(curl -fsS \
  -H "Authorization: Bearer ${SEKAILINK_ROOM_SERVER_ADMIN_TOKEN}" \
  "http://${HTTP_HOST}:${HTTP_PORT}/rooms")"

echo "profile_env=${PROFILE_ENV}"
echo "room_env=${ROOM_ENV}"
echo "state_file=${STATE_FILE}"
if [[ -n "${SERVICE_LOG}" ]]; then
  echo "service_log=${SERVICE_LOG}"
else
  echo "service_log=journalctl:${SYSTEMD_UNIT}"
fi
echo "room_root=${ROOM_ROOT}"
echo "tcp_cli_available=${CLI_AVAILABLE}"
echo "link_host=${SEKAILINK_LINK_HOST:-link.sekailink.com}"
echo "projection_backend=${PROJECTION_BACKEND}"
echo "projection_target=${PROJECTION_TARGET}"
echo "state_status=${STATUS}"
echo "state_room_count=${ROOM_COUNT}"
echo "state_loopback_only=${LOOPBACK_ONLY}"
echo "state_tcp=${TCP_HOST}:${TCP_PORT}"
echo "state_http=${HTTP_HOST}:${HTTP_PORT}"
echo "state_admin_auth_enabled=${ADMIN_AUTH_ENABLED}"
echo "state_runtime_auth_enabled=${RUNTIME_AUTH_ENABLED}"
if [[ "${PROJECTION_BACKEND}" == "mysql" ]]; then
  echo "mysql_host=${MYSQL_HOST:-unset}"
  echo "mysql_port=${MYSQL_PORT}"
  echo "mysql_user=${MYSQL_USER:-unset}"
  echo "mysql_socket=${MYSQL_SOCKET:-unset}"
  echo "mysql_password_present=${MYSQL_PASSWORD_PRESENT}"
  echo "nexus_projection_chain=link-room -> mysql(room_records,room_event_records,client_report_records) -> nexus-room-query"
fi
echo "public_tcp=${SEKAILINK_LINK_ROOM_PUBLIC_TCP_HOST:-unset}:${SEKAILINK_LINK_ROOM_PUBLIC_TCP_PORT:-unset}"
echo "public_http=${SEKAILINK_LINK_ROOM_PUBLIC_HTTP_BASE_URL:-unset}"
echo "health=$(echo "${HEALTH_JSON}" | jq -c '.')"
echo "rooms=$(echo "${ROOMS_JSON}" | jq -c '.')"
echo "recent_log_tail:"
if [[ -n "${SERVICE_LOG}" ]]; then
  tail -n "${JOURNAL_LINES}" "${SERVICE_LOG}"
elif command -v journalctl >/dev/null 2>&1; then
  journalctl -u "${SYSTEMD_UNIT}" -n "${JOURNAL_LINES}" --no-pager
else
  echo "[check-private-live-readiness][warn] no file log found and journalctl is unavailable."
fi
echo
echo "next_log_tail_command:"
printf '%q ' \
  bash "${SELF_DIR}/tail-private-live-logs.sh" \
  --profile-env "${PROFILE_ENV}" \
  --room-env "${ROOM_ENV}" \
  --systemd-unit "${SYSTEMD_UNIT}" \
  --lines "${JOURNAL_LINES}" \
  --filter "${SEKAILINK_ALTTP_TEST_ROOM_ID:-alttp-live-1}"
echo
echo
echo "next_create_room_command:"
if [[ "${CLI_AVAILABLE}" == "true" ]]; then
  printf '%q ' \
    bash "${SEND_HELPER}" \
    --env-file "${ROOM_ENV}" \
    --state-file "${STATE_FILE}" \
    --channel admin \
    --command-file "${SELF_DIR}/payloads/create-room.alttp-live.command.json"
  echo
else
  echo "[warn] tcp cli not present at ${CLI_FILE}; copy sekailink_room_server_tcp_cli before replaying mutation commands."
fi
echo
echo "next_runtime_slot_data_command:"
if [[ "${CLI_AVAILABLE}" == "true" ]]; then
  printf '%q ' \
    bash "${SEND_HELPER}" \
    --env-file "${ROOM_ENV}" \
    --state-file "${STATE_FILE}" \
    --channel runtime \
    --command-file "${SELF_DIR}/payloads/set-slot-data.alttp-live.command.json"
  echo
else
  echo "[warn] tcp cli not present at ${CLI_FILE}; copy sekailink_room_server_tcp_cli before replaying mutation commands."
fi
echo
echo "next_runtime_surface_validation:"
if [[ "${CLI_AVAILABLE}" == "true" ]]; then
  printf '%q ' \
    bash "${SELF_DIR}/validate-private-runtime-commands.sh" \
    --profile-env "${PROFILE_ENV}" \
    --room-env "${ROOM_ENV}" \
    --state-file "${STATE_FILE}"
  echo
else
  echo "[warn] tcp cli not present at ${CLI_FILE}; copy sekailink_room_server_tcp_cli before validating ticket/pending_items/runtime_event/acknowledge_delivery."
fi
