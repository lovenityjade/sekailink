#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE_ENV="${SELF_DIR}/room-server.alttp-live.profile.env.example"
ROOM_ENV=""
SERVICE_LOG=""
SYSTEMD_UNIT=""
LINES="80"
FILTER=""

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
usage: tail-private-live-logs.sh [--profile-env <file>] [--room-env <file>] [--service-log <file>] [--systemd-unit <unit>] [--lines <n>] [--filter <text>]

Reads either the configured room-server log file or falls back to journalctl for
the private link room instance. If --filter is provided, only matching lines
are shown.
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
    --service-log)
      SERVICE_LOG="$2"
      shift 2
      ;;
    --systemd-unit)
      SYSTEMD_UNIT="$2"
      shift 2
      ;;
    --lines)
      LINES="$2"
      shift 2
      ;;
    --filter)
      FILTER="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[tail-private-live-logs][error] unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "${PROFILE_ENV}" ]]; then
  echo "[tail-private-live-logs][error] profile env not found: ${PROFILE_ENV}" >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${PROFILE_ENV}"
set +a

ROOM_ROOT="${SEKAILINK_LINK_ROOM_ROOT:-/opt/sekailink/link/room-server}"
ROOM_ENV="${ROOM_ENV:-${SEKAILINK_LINK_ROOM_ENV:-${ROOM_ROOT}/config/room_server.env}}"
SERVICE_LOG="${SERVICE_LOG:-${SEKAILINK_LINK_ROOM_LOG:-/opt/sekailink/link/logs/room-server/service.log}}"
SYSTEMD_UNIT="${SYSTEMD_UNIT:-${SEKAILINK_LINK_ROOM_SERVICE:-sekailink-room-server.service}}"

if ! ROOM_ENV="$(resolve_room_env_path "${ROOM_ENV}")"; then
  echo "[tail-private-live-logs][error] room env not found near: ${ROOM_ENV}" >&2
  exit 1
fi

set -a
# shellcheck disable=SC1090
source "${ROOM_ENV}"
set +a

print_stream() {
  if [[ -n "${FILTER}" ]]; then
    grep -F "${FILTER}" || true
  else
    cat
  fi
}

if [[ -f "${SERVICE_LOG}" ]]; then
  echo "log_source=file:${SERVICE_LOG}"
  tail -n "${LINES}" "${SERVICE_LOG}" | print_stream
  exit 0
fi

if ! command -v journalctl >/dev/null 2>&1; then
  echo "[tail-private-live-logs][error] no file log found and journalctl is unavailable." >&2
  exit 1
fi

echo "log_source=journalctl:${SYSTEMD_UNIT}"
journalctl -u "${SYSTEMD_UNIT}" -n "${LINES}" --no-pager | print_stream
