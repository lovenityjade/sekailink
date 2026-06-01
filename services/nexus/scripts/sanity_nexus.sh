#!/usr/bin/env bash
set -euo pipefail

required_paths=(
  "README.md"
  "docs/SOURCE_OF_TRUTH.md"
  "server/native/sekailink_server_core/CMakeLists.txt"
  "deploy/nexus/auth/identity/identity_service.json.example"
  "deploy/nexus/api-gateway/room-query/nexus_room_query.json.example"
  "deploy/nexus/lobby-admin/lobby_admin.json.example"
  "deploy/nexus/admin-agent/admin_agent.json.example"
  "third_party/sekailink_runtime/third_party/nlohmann/json.hpp"
)

for path in "${required_paths[@]}"; do
  if [[ ! -e "$path" ]]; then
    echo "Missing required path: $path" >&2
    exit 1
  fi
done

if rg -n "client/runtime/native/sekailink_runtime/third_party" server/native/sekailink_server_core/CMakeLists.txt >/dev/null; then
  echo "CMake still references monorepo path for runtime third_party." >&2
  exit 1
fi

echo "sekailink-nexus sanity check passed."
