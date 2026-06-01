#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-probe}"

ROOM_HOST="${SKLMI_ROOM_HOST:-link.sekailink.com}"
SHARED_PORT="${SKLMI_ROOM_PORT:-}"
CONTROL_PORT="${SKLMI_ROOM_CONTROL_PORT:-${SHARED_PORT:-28080}}"
RUNTIME_PORT="${SKLMI_ROOM_RUNTIME_PORT:-${SHARED_PORT:-28081}}"
CONTROL_CHANNEL="${SKLMI_ROOM_CONTROL_CHANNEL:-core}"

probe_port() {
  local host="$1"
  local port="$2"
  if command -v nc >/dev/null 2>&1; then
    nc -z -w 2 "$host" "$port" >/dev/null 2>&1
    return $?
  fi
  timeout 2 bash -lc "exec 3<>/dev/tcp/${host}/${port}" >/dev/null 2>&1
}

print_env() {
  cat <<EOF
SKLMI_ROOM_HOST=${ROOM_HOST}
SKLMI_ROOM_CONTROL_PORT=${CONTROL_PORT}
SKLMI_ROOM_RUNTIME_PORT=${RUNTIME_PORT}
SKLMI_ROOM_CONTROL_CHANNEL=${CONTROL_CHANNEL}
EOF
}

print_cmd() {
  cat <<'EOF'
SKLMI_RUNTIME_BIN="${SKLMI_RUNTIME_BIN:?set SKLMI_RUNTIME_BIN}"
SKLMI_MEMORY_SOCKET="${SKLMI_MEMORY_SOCKET:?set SKLMI_MEMORY_SOCKET}"
SKLMI_BRIDGE_MANIFEST="${SKLMI_BRIDGE_MANIFEST:?set SKLMI_BRIDGE_MANIFEST}"
SKLMI_ROOM_STATE="${SKLMI_ROOM_STATE:?set SKLMI_ROOM_STATE}"
SKLMI_RUNTIME_STATE="${SKLMI_RUNTIME_STATE:?set SKLMI_RUNTIME_STATE}"
SKLMI_TRACE_LOG="${SKLMI_TRACE_LOG:?set SKLMI_TRACE_LOG}"
SKLMI_ROOM_SESSION_NAME="${SKLMI_ROOM_SESSION_NAME:?set SKLMI_ROOM_SESSION_NAME}"
SKLMI_ROOM_SLOT_ID="${SKLMI_ROOM_SLOT_ID:?set SKLMI_ROOM_SLOT_ID}"

"${SKLMI_RUNTIME_BIN}" \
  --memory-socket "${SKLMI_MEMORY_SOCKET}" \
  --bridge-manifest "${SKLMI_BRIDGE_MANIFEST}" \
  --room-state "${SKLMI_ROOM_STATE}" \
  --runtime-state "${SKLMI_RUNTIME_STATE}" \
  --trace-log "${SKLMI_TRACE_LOG}" \
  --mode sekailink_game_server \
  --room-host "${SKLMI_ROOM_HOST:-link.sekailink.com}" \
  --room-control-port "${SKLMI_ROOM_CONTROL_PORT:-28080}" \
  --room-runtime-port "${SKLMI_ROOM_RUNTIME_PORT:-28081}" \
  --room-session-name "${SKLMI_ROOM_SESSION_NAME}" \
  --room-slot-id "${SKLMI_ROOM_SLOT_ID}" \
  --room-control-channel "${SKLMI_ROOM_CONTROL_CHANNEL:-core}" \
  --room-runtime-auth-token "${SKLMI_ROOM_RUNTIME_AUTH_TOKEN:-}" \
  ${SKLMI_ROOM_CONTROL_AUTH_TOKEN:+--room-control-auth-token "${SKLMI_ROOM_CONTROL_AUTH_TOKEN}"} \
  ${SKLMI_ROOM_RUNTIME_SESSION_TOKEN:+--room-runtime-session-token "${SKLMI_ROOM_RUNTIME_SESSION_TOKEN}"} \
  --tick-ms "${SKLMI_TICK_MS:-16}" \
  --max-ticks "${SKLMI_MAX_TICKS:-5}"
EOF
}

case "${MODE}" in
  probe)
    print_env
    if probe_port "${ROOM_HOST}" "${CONTROL_PORT}"; then
      echo "control_port_reachable=${CONTROL_PORT}"
    else
      echo "control_port_unreachable=${CONTROL_PORT}"
    fi
    if probe_port "${ROOM_HOST}" "${RUNTIME_PORT}"; then
      echo "runtime_port_reachable=${RUNTIME_PORT}"
    else
      echo "runtime_port_unreachable=${RUNTIME_PORT}"
    fi
    ;;
  print-env)
    print_env
    ;;
  print-cmd)
    print_cmd
    ;;
  *)
    echo "usage: $0 [probe|print-env|print-cmd]" >&2
    exit 1
    ;;
esac
