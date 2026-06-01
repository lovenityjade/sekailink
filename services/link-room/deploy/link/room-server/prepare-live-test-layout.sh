#!/usr/bin/env bash

set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="loopback"
TARGET_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    -h|--help)
      cat <<'EOF'
usage: prepare-live-test-layout.sh [--profile <loopback|nexus-live>] [target_dir]

Creates a local or host-side staging tree with room-server config, env, helper
scripts and payloads, without touching the active service paths.
EOF
      exit 0
      ;;
    *)
      TARGET_DIR="$1"
      shift
      ;;
  esac
done

TARGET_DIR="${TARGET_DIR:-${SELF_DIR}/../../../out/link-room-live-test}"

if [[ "${TARGET_DIR}" != /* ]]; then
  TARGET_DIR="$(pwd)/${TARGET_DIR}"
fi
TARGET_DIR_PARENT="$(dirname "${TARGET_DIR}")"
mkdir -p "${TARGET_DIR_PARENT}"
TARGET_DIR="$(cd "${TARGET_DIR_PARENT}" && pwd)/$(basename "${TARGET_DIR}")"

echo "[prepare-live-test-layout] target=${TARGET_DIR}"
echo "[prepare-live-test-layout] profile=${PROFILE}"

mkdir -p \
  "${TARGET_DIR}/bin" \
  "${TARGET_DIR}/config" \
  "${TARGET_DIR}/data/audit" \
  "${TARGET_DIR}/data/projection" \
  "${TARGET_DIR}/runtime" \
  "${TARGET_DIR}/logs/room-server" \
  "${TARGET_DIR}/ops" \
  "${TARGET_DIR}/payloads"

case "${PROFILE}" in
  loopback)
    cp "${SELF_DIR}/room_server.alttp-live.loopback.json.example" "${TARGET_DIR}/config/room_server.json"
    ;;
  nexus-live)
    cp "${SELF_DIR}/room_server.json.example" "${TARGET_DIR}/config/room_server.json"
    ;;
  *)
    echo "[prepare-live-test-layout][error] unknown profile: ${PROFILE}" >&2
    exit 1
    ;;
esac

cp "${SELF_DIR}/room_server.env.example" "${TARGET_DIR}/config/room_server.env"
cp "${SELF_DIR}/room-server.alttp-live.profile.env.example" "${TARGET_DIR}/ops/room-server.alttp-live.profile.env"
cp "${SELF_DIR}/sekailink-room-server.service" "${TARGET_DIR}/ops/sekailink-room-server.service"
cp "${SELF_DIR}/send-room-command.sh" "${TARGET_DIR}/ops/send-room-command.sh"
cp "${SELF_DIR}/check-private-live-readiness.sh" "${TARGET_DIR}/ops/check-private-live-readiness.sh"
cp "${SELF_DIR}/tail-private-live-logs.sh" "${TARGET_DIR}/ops/tail-private-live-logs.sh"
cp "${SELF_DIR}/validate-private-runtime-commands.sh" "${TARGET_DIR}/ops/validate-private-runtime-commands.sh"
cp "${SELF_DIR}/stage-private-live-host.sh" "${TARGET_DIR}/ops/stage-private-live-host.sh"
cp "${SELF_DIR}/start-private-live-instance.sh" "${TARGET_DIR}/ops/start-private-live-instance.sh"
cp "${SELF_DIR}/exercise-private-live-instance.sh" "${TARGET_DIR}/ops/exercise-private-live-instance.sh"
cp -R "${SELF_DIR}/payloads/." "${TARGET_DIR}/payloads/"

# Keep a legacy hyphenated copy for older notes or local ad-hoc tooling.
cp "${TARGET_DIR}/config/room_server.env" "${TARGET_DIR}/config/room-server.env"

chmod +x "${TARGET_DIR}/ops/send-room-command.sh"
chmod +x "${TARGET_DIR}/ops/check-private-live-readiness.sh"
chmod +x "${TARGET_DIR}/ops/tail-private-live-logs.sh"
chmod +x "${TARGET_DIR}/ops/validate-private-runtime-commands.sh"
chmod +x "${TARGET_DIR}/ops/stage-private-live-host.sh"
chmod +x "${TARGET_DIR}/ops/start-private-live-instance.sh"
chmod +x "${TARGET_DIR}/ops/exercise-private-live-instance.sh"

cat > "${TARGET_DIR}/README.txt" <<'EOF'
SekaiLink Link Room live test staging tree

This directory is a local staging target only. It does not alter the live host.

Contents:
- config/room_server.json
- config/room_server.env
- config/room-server.env
- ops/room-server.alttp-live.profile.env
- ops/sekailink-room-server.service
- ops/send-room-command.sh
- ops/check-private-live-readiness.sh
- ops/tail-private-live-logs.sh
- ops/validate-private-runtime-commands.sh
- ops/stage-private-live-host.sh
- ops/start-private-live-instance.sh
- ops/exercise-private-live-instance.sh
- payloads/*.command.json

Next steps:
1. Replace tokens in config/room_server.env
2. Fill edge values in ops/room-server.alttp-live.profile.env
3. Run ops/check-private-live-readiness.sh once the private host paths exist
4. Run ops/validate-private-runtime-commands.sh before inviting testers
5. Copy the runtime assets to the target link host
6. Place sekailink_room_server_service and sekailink_room_server_tcp_cli in bin/
EOF

echo "[prepare-live-test-layout] wrote:"
find "${TARGET_DIR}" -maxdepth 2 -type f | sort
