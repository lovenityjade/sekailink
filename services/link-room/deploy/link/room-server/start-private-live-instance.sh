#!/usr/bin/env bash

set -euo pipefail

STAGE_ROOT="${1:-/home/debian/sekailink-room-live-staging/private-instance}"
TCP_PORT="${SEKAILINK_PRIVATE_ROOM_TCP_PORT:-28080}"
HTTP_PORT="${SEKAILINK_PRIVATE_ROOM_HTTP_PORT:-28081}"
ACTIVE_ENV="${SEKAILINK_ACTIVE_ROOM_ENV:-/opt/sekailink/link/room-server/config/room_server.env}"
SERVICE_BIN="${SEKAILINK_PRIVATE_ROOM_SERVICE_BIN:-/home/debian/sekailink-room-live-staging/bin/sekailink_room_server_service}"

mkdir -p \
  "${STAGE_ROOT}/config" \
  "${STAGE_ROOT}/runtime" \
  "${STAGE_ROOT}/audit" \
  "${STAGE_ROOT}/logs"

if [[ -r "${ACTIVE_ENV}" ]]; then
  cp "${ACTIVE_ENV}" "${STAGE_ROOT}/config/room_server.env"
elif command -v sudo >/dev/null 2>&1; then
  sudo cp "${ACTIVE_ENV}" "${STAGE_ROOT}/config/room_server.env"
  sudo chown "$(id -un)":"$(id -gn)" "${STAGE_ROOT}/config/room_server.env"
else
  echo "[start-private-live-instance][error] cannot read ${ACTIVE_ENV} and sudo is unavailable." >&2
  exit 1
fi

cat > "${STAGE_ROOT}/config/room_server.json" <<EOF
{
  "tcp_port": ${TCP_PORT},
  "http_port": ${HTTP_PORT},
  "audit_root": "${STAGE_ROOT}/audit",
  "projection_root": "sekailink_room_projection",
  "projection_backend": "mysql",
  "restore_from_audit": false,
  "restore_from_projection": false,
  "purge_expired_periodically": true,
  "purge_interval_ms": 1000,
  "auth_policy": {
    "admin_token": null,
    "runtime_token": null,
    "client_report_token": null
  }
}
EOF

pkill -f "${SERVICE_BIN} --config ${STAGE_ROOT}/config/room_server.json" || true

nohup bash -lc \
  "set -a; source '${STAGE_ROOT}/config/room_server.env'; set +a; '${SERVICE_BIN}' --config '${STAGE_ROOT}/config/room_server.json' --state-file '${STAGE_ROOT}/runtime/room_server_state.json'" \
  > "${STAGE_ROOT}/logs/service.log" 2>&1 &

sleep 2

echo "stage_root=${STAGE_ROOT}"
echo "tcp_port=${TCP_PORT}"
echo "http_port=${HTTP_PORT}"
cat "${STAGE_ROOT}/runtime/room_server_state.json"
