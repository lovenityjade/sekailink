#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

build_dir="${SEKAIEMU_E2E_BUILD_DIR:-/tmp/sekaiemu-libretro-spike-beta3-build}"
sekaiemu_bin="${SEKAIEMU_E2E_BIN:-$build_dir/sekaiemu_libretro_spike}"
core_path="${SEKAIEMU_E2E_CORE:-$canonical_root/runtime/cores/bsnes_mercury_performance_libretro.so}"
bundle_root="${SEKAIEMU_E2E_EARTHBOUND_BUNDLE:-/tmp/sekailink-earthbound-manual-bundle}"
game_rom="${SEKAIEMU_E2E_GAME_ROM:-$bundle_root/work/run-20260504-191806/patched/AP_11164262353628825690_P1_Jade-Earthbound.sfc}"
server_bin="${SEKAIEMU_E2E_MULTISERVER:-<local-home>/Games/The Legend of Jade/clients/squashfs-root/opt/MultiworldGG/MultiworldGGServer}"
seed_zip="${SEKAIEMU_E2E_SEED_ZIP:-$bundle_root/work/run-20260504-191806/generated/output/AP_11164262353628825690.zip}"
sklmi_runtime="${SEKAIEMU_E2E_SKLMI_RUNTIME:-$canonical_root/runtime/bin/sekailink_sklmi_runtime}"
sklmi_manifest_dir="${SEKAIEMU_E2E_SKLMI_MANIFEST_DIR:-$canonical_root/services/sklmi/manifests}"
profile_path="${SEKAIEMU_E2E_EARTHBOUND_PROFILE:-$repo_root/profiles/earthbound-starter.profile}"
run_root="${SEKAIEMU_E2E_RUN_ROOT:-/tmp/sekaiemu-earthbound-manual-session}"
port="${SEKAIEMU_E2E_AP_PORT:-38291}"
server_password="${SEKAIEMU_E2E_SERVER_PASSWORD:-sekailink-admin}"

require_file() {
  local path="$1"
  local label="$2"
  if [[ ! -f "$path" ]]; then
    echo "[earthbound-manual][missing] $label: $path" >&2
    exit 2
  fi
}

wait_for_port() {
  local listen_port="$1"
  for _ in $(seq 1 120); do
    if ! kill -0 "$server_pid" 2>/dev/null; then
      echo "[earthbound-manual][fail] MultiServer exited before listening." >&2
      return 1
    fi
    if ss -ltn 2>/dev/null | grep -Eq "(127\\.0\\.0\\.1:${listen_port}|0\\.0\\.0\\.0:${listen_port}|\\[::1\\]:${listen_port}|:${listen_port})"; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

require_file "$sekaiemu_bin" "Sekaiemu binary"
require_file "$core_path" "SNES libretro core"
require_file "$game_rom" "patched EarthBound ROM"
require_file "$server_bin" "MultiServer binary"
require_file "$seed_zip" "AP seed zip"
require_file "$sklmi_runtime" "SKLMI runtime"
require_file "$sklmi_manifest_dir/earthbound.phase1.json" "EarthBound SKLMI manifest"
require_file "$profile_path" "EarthBound profile"

rm -rf "$run_root"
mkdir -p "$run_root/live/logs" "$run_root/live/system" "$run_root/live/saves/runtime"

server_log="$run_root/live/multiserver.log"
echo "[earthbound-manual] starting MultiServer on 127.0.0.1:$port"
server_args=("$seed_zip" --host 127.0.0.1 --port "$port" --disable_save --loglevel debug)
if [[ -n "$server_password" ]]; then
  server_args+=(--server_password "$server_password")
fi
"$server_bin" "${server_args[@]}" \
  < <(tail -f /dev/null) \
  > "$server_log" 2>&1 &
server_pid=$!

cleanup() {
  kill "$server_pid" 2>/dev/null || true
  wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT

if ! wait_for_port "$port"; then
  echo "[earthbound-manual][fail] MultiServer did not listen on port $port" >&2
  echo "[earthbound-manual][log] $server_log" >&2
  exit 4
fi

echo "[earthbound-manual] server ready. Use AP admin TUI for Toothbrush injection, then close Sekaiemu."
echo "[earthbound-manual] run root: $run_root"

"$sekaiemu_bin" \
  --core "$core_path" \
  --game "$game_rom" \
  --profile "$profile_path" \
  --system-dir "$run_root/live/system" \
  --save-dir "$run_root/live/saves" \
  --log-dir "$run_root/live/logs" \
  --sklmi-runtime "$sklmi_runtime" \
  --sklmi-manifest-dir "$sklmi_manifest_dir" \
  --ap-host 127.0.0.1 \
  --ap-port "$port" \
  --ap-path / \
  --ap-game "EarthBound" \
  --ap-slot-name "Jade-Earthbound" \
  --ap-tags "AP,SekaiLink,EarthBoundManual" \
  > "$run_root/live/run.out" 2>&1

echo "[earthbound-manual] Sekaiemu closed. Proof files:"
echo "[earthbound-manual] trace: $run_root/live/saves/sklmi/earthbound-phase1/trace.jsonl"
echo "[earthbound-manual] server: $server_log"
