#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOM_ROOT="/opt/sekailink/link/room-server"
PROFILE="nexus-live"
STAGE_NAME="private-live-$(date -u +%Y%m%dT%H%M%SZ)"

usage() {
  cat <<'EOF'
usage: stage-private-live-host.sh [--room-root <dir>] [--profile <loopback|nexus-live>] [--stage-name <name>]

Creates a non-destructive staging tree under <room-root>/staging/<name> with
room-server config, env templates, helper scripts and payloads. It does not
modify the active config, service unit, or runtime state.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --room-root)
      ROOM_ROOT="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --stage-name)
      STAGE_NAME="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[stage-private-live-host][error] unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

STAGE_DIR="${ROOM_ROOT}/staging/${STAGE_NAME}"
ACTIVE_CONFIG="${ROOM_ROOT}/config/room_server.json"
ACTIVE_ENV_UNDERSCORE="${ROOM_ROOT}/config/room_server.env"
ACTIVE_ENV_HYPHEN="${ROOM_ROOT}/config/room-server.env"

bash "${SELF_DIR}/prepare-live-test-layout.sh" --profile "${PROFILE}" "${STAGE_DIR}"

echo
echo "[stage-private-live-host] stage_dir=${STAGE_DIR}"
echo "[stage-private-live-host] profile=${PROFILE}"
echo "[stage-private-live-host] active_config=${ACTIVE_CONFIG}"
if [[ -f "${ACTIVE_ENV_UNDERSCORE}" ]]; then
  echo "[stage-private-live-host] active_env=${ACTIVE_ENV_UNDERSCORE}"
elif [[ -f "${ACTIVE_ENV_HYPHEN}" ]]; then
  echo "[stage-private-live-host] active_env=${ACTIVE_ENV_HYPHEN}"
else
  echo "[stage-private-live-host] active_env=missing"
fi
echo
echo "[stage-private-live-host] suggested non-destructive checks:"
printf '  %q\n' "diff -u \"${ACTIVE_CONFIG}\" \"${STAGE_DIR}/config/room_server.json\""
printf '  %q\n' "diff -u \"${ACTIVE_ENV_UNDERSCORE}\" \"${STAGE_DIR}/config/room_server.env\""
printf '  %q\n' "bash \"${SELF_DIR}/check-private-live-readiness.sh\" --room-env \"${ACTIVE_ENV_UNDERSCORE}\""
printf '  %q\n' "bash \"${SELF_DIR}/validate-private-runtime-commands.sh\" --room-env \"${ACTIVE_ENV_UNDERSCORE}\""
printf '  %q\n' "bash \"${SELF_DIR}/tail-private-live-logs.sh\" --room-env \"${ACTIVE_ENV_UNDERSCORE}\" --filter \"\${SEKAILINK_ALTTP_TEST_ROOM_ID:-alttp-live-1}\""
