#!/usr/bin/env bash

set -euo pipefail

HOST="127.0.0.1"
PORT="32071"
CHANNEL=""
TOKEN_ENV=""
COMMAND_FILE=""
CLI_BIN="${SEKAILINK_ROOM_SERVER_TCP_CLI_BIN:-sekailink_room_server_tcp_cli}"
PRINT_ONLY="0"
STATE_FILE=""
ENV_FILE=""

resolve_env_file_path() {
  local requested="${1:-}"
  if [[ -n "${requested}" ]]; then
    if [[ -f "${requested}" ]]; then
      echo "${requested}"
      return 0
    fi
    local alternate="${requested/room-server.env/room_server.env}"
    if [[ "${alternate}" == "${requested}" ]]; then
      alternate="${requested/room_server.env/room-server.env}"
    fi
    if [[ -f "${alternate}" ]]; then
      echo "${alternate}"
      return 0
    fi
    return 1
  fi
  return 1
}

default_token_env_for_channel() {
  case "${1:-}" in
    admin)
      echo "SEKAILINK_ROOM_SERVER_ADMIN_TOKEN"
      ;;
    runtime)
      echo "SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN"
      ;;
    client_report)
      echo "SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN"
      ;;
    *)
      return 1
      ;;
  esac
}

usage() {
  cat <<'EOF'
usage: send-room-command.sh --channel <admin|runtime|client_report> --command-file <file> [--token-env <ENV_NAME>] [--env-file <file>] [--state-file <file>] [--host <host>] [--port <port>] [--cli <binary>] [--print-only]

Builds a TCP protocol envelope from a command JSON file and sends it through
sekailink_room_server_tcp_cli.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)
      HOST="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --channel)
      CHANNEL="$2"
      shift 2
      ;;
    --token-env)
      TOKEN_ENV="$2"
      shift 2
      ;;
    --command-file)
      COMMAND_FILE="$2"
      shift 2
      ;;
    --state-file)
      STATE_FILE="$2"
      shift 2
      ;;
    --env-file)
      ENV_FILE="$2"
      shift 2
      ;;
    --cli)
      CLI_BIN="$2"
      shift 2
      ;;
    --print-only)
      PRINT_ONLY="1"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[send-room-command][error] unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "${CHANNEL}" || -z "${COMMAND_FILE}" ]]; then
  echo "[send-room-command][error] missing required arguments." >&2
  usage >&2
  exit 1
fi

if [[ -n "${ENV_FILE}" ]]; then
  if ! ENV_FILE="$(resolve_env_file_path "${ENV_FILE}")"; then
    echo "[send-room-command][error] env file not found: ${ENV_FILE}" >&2
    exit 1
  fi
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

if [[ ! -f "${COMMAND_FILE}" ]]; then
  echo "[send-room-command][error] command file not found: ${COMMAND_FILE}" >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "[send-room-command][error] jq is required to compact and wrap the command JSON." >&2
  exit 1
fi

if [[ -n "${STATE_FILE}" ]]; then
  if [[ ! -f "${STATE_FILE}" ]]; then
    echo "[send-room-command][error] state file not found: ${STATE_FILE}" >&2
    exit 1
  fi
  if [[ "${HOST}" == "127.0.0.1" ]]; then
    HOST="$(jq -r '.effective_tcp_host // "127.0.0.1"' "${STATE_FILE}")"
  fi
  if [[ "${PORT}" == "32071" ]]; then
    PORT="$(jq -r '.effective_tcp_port // 0' "${STATE_FILE}")"
  fi
  if [[ "${PORT}" == "0" || -z "${PORT}" || "${PORT}" == "null" ]]; then
    echo "[send-room-command][error] could not derive a TCP port from state file: ${STATE_FILE}" >&2
    exit 1
  fi
fi

if [[ -z "${TOKEN_ENV}" ]]; then
  TOKEN_ENV="$(default_token_env_for_channel "${CHANNEL}" || true)"
fi

if [[ -z "${TOKEN_ENV}" ]]; then
  echo "[send-room-command][error] token env is required for channel: ${CHANNEL}" >&2
  exit 1
fi

if [[ -z "${!TOKEN_ENV:-}" ]]; then
  echo "[send-room-command][error] environment variable ${TOKEN_ENV} is empty." >&2
  exit 1
fi

PAYLOAD="$(jq -cn \
  --arg channel "${CHANNEL}" \
  --arg auth_token "${!TOKEN_ENV}" \
  --slurpfile command "${COMMAND_FILE}" \
  '{channel:$channel, auth_token:$auth_token, command:$command[0]}')"

echo "[send-room-command] host=${HOST} port=${PORT} channel=${CHANNEL} command_file=${COMMAND_FILE}" >&2

if [[ "${PRINT_ONLY}" == "1" ]]; then
  echo "${PAYLOAD}"
  exit 0
fi

exec "${CLI_BIN}" --host "${HOST}" --port "${PORT}" --payload "${PAYLOAD}"
