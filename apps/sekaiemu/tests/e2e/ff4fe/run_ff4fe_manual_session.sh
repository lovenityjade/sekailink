#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

build_dir="${SEKAIEMU_E2E_BUILD_DIR:-/tmp/sekaiemu-libretro-spike-beta3-build}"
sekaiemu_bin="${SEKAIEMU_E2E_BIN:-$build_dir/sekaiemu_libretro_spike}"
core_path="${SEKAIEMU_E2E_CORE:-$canonical_root/runtime/cores/bsnes_mercury_performance_libretro.so}"
game_rom="${SEKAIEMU_E2E_GAME_ROM:-/tmp/sekailink-ff4fe-heartbeat-gen/patch/AP_50880559353072062288_P1_Jade-FF4FE.sfc}"
server_bin="${SEKAIEMU_E2E_MULTISERVER:-<local-home>/Games/The Legend of Jade/clients/squashfs-root/opt/MultiworldGG/MultiworldGGServer}"
seed_zip="${SEKAIEMU_E2E_SEED_ZIP:-/tmp/sekailink-ff4fe-heartbeat-gen/output/AP_50880559353072062288.zip}"
sklmi_runtime="${SEKAIEMU_E2E_SKLMI_RUNTIME:-$canonical_root/runtime/bin/sekailink_sklmi_runtime}"
sklmi_manifest="${SEKAIEMU_E2E_SKLMI_MANIFEST:-$canonical_root/services/sklmi/manifests/ff4fe.phase1.json}"
profile_path="${SEKAIEMU_E2E_FF4FE_PROFILE:-$repo_root/profiles/ff4fe-heartbeat.profile}"
run_root="${SEKAIEMU_E2E_RUN_ROOT:-/tmp/sekaiemu-ff4fe-manual-session}"
port="${SEKAIEMU_E2E_AP_PORT:-38292}"
server_password="${SEKAIEMU_E2E_SERVER_PASSWORD:-sekailink-admin}"

require_file() {
  local path="$1"
  local label="$2"
  if [[ ! -f "$path" ]]; then
    echo "[ff4fe-manual][missing] $label: $path" >&2
    exit 2
  fi
}

wait_for_port() {
  local listen_port="$1"
  for _ in $(seq 1 120); do
    if ! kill -0 "$server_pid" 2>/dev/null; then
      echo "[ff4fe-manual][fail] MultiServer exited before listening." >&2
      return 1
    fi
    if ss -ltn 2>/dev/null | grep -Eq "(127\\.0\\.0\\.1:${listen_port}|0\\.0\\.0\\.0:${listen_port}|\\[::1\\]:${listen_port}|:${listen_port})"; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

wait_for_socket() {
  local socket_path="$1"
  for _ in $(seq 1 120); do
    if ! kill -0 "$sekaiemu_pid" 2>/dev/null; then
      echo "[ff4fe-manual][fail] Sekaiemu exited before memory socket appeared." >&2
      return 1
    fi
    if [[ -S "$socket_path" ]]; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

require_file "$sekaiemu_bin" "Sekaiemu binary"
require_file "$core_path" "SNES libretro core"
require_file "$game_rom" "patched FF4FE ROM"
require_file "$server_bin" "MultiServer binary"
require_file "$seed_zip" "AP seed zip"
require_file "$sklmi_runtime" "SKLMI runtime"
require_file "$sklmi_manifest" "FF4FE SKLMI manifest"
require_file "$profile_path" "FF4FE profile"

rm -rf "$run_root"
mkdir -p "$run_root/live/logs" "$run_root/live/system" "$run_root/live/saves/runtime" "$run_root/live/saves/sklmi/ff4fe-phase1/runtime"

memory_socket="$run_root/live/saves/runtime/sekaiemu-memory.sock"
server_log="$run_root/live/multiserver.log"
sklmi_trace="$run_root/live/saves/sklmi/ff4fe-phase1/trace.jsonl"
sklmi_room_state="$run_root/live/saves/sklmi/ff4fe-phase1/room.state"
sklmi_runtime_state="$run_root/live/saves/sklmi/ff4fe-phase1/runtime"
sklmi_log="$run_root/live/saves/sklmi/ff4fe-phase1/companion.log"

echo "[ff4fe-manual] starting MultiServer on 127.0.0.1:$port"
server_args=("$seed_zip" --host 127.0.0.1 --port "$port" --disable_save --loglevel debug)
if [[ -n "$server_password" ]]; then
  server_args+=(--server_password "$server_password")
fi
"$server_bin" "${server_args[@]}" \
  < <(tail -f /dev/null) \
  > "$server_log" 2>&1 &
server_pid=$!

cleanup() {
  kill "${sklmi_pid:-0}" 2>/dev/null || true
  kill "${sekaiemu_pid:-0}" 2>/dev/null || true
  kill "$server_pid" 2>/dev/null || true
  wait "${sklmi_pid:-0}" 2>/dev/null || true
  wait "${sekaiemu_pid:-0}" 2>/dev/null || true
  wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT

if ! wait_for_port "$port"; then
  echo "[ff4fe-manual][fail] MultiServer did not listen on port $port" >&2
  echo "[ff4fe-manual][log] $server_log" >&2
  exit 4
fi

echo "[ff4fe-manual] starting Sekaiemu and memory socket"
"$sekaiemu_bin" \
  --core "$core_path" \
  --game "$game_rom" \
  --profile "$profile_path" \
  --system-dir "$run_root/live/system" \
  --save-dir "$run_root/live/saves" \
  --log-dir "$run_root/live/logs" \
  --memory-socket "$memory_socket" \
  --ap-host 127.0.0.1 \
  --ap-port "$port" \
  --ap-path / \
  --ap-game "Final Fantasy IV Free Enterprise" \
  --ap-slot-name "Jade-FF4FE" \
  --ap-tags "AP,SekaiLink,FF4FEManual" \
  > "$run_root/live/run.out" 2>&1 &
sekaiemu_pid=$!

if ! wait_for_socket "$memory_socket"; then
  echo "[ff4fe-manual][fail] memory socket did not appear: $memory_socket" >&2
  exit 5
fi

echo "[ff4fe-manual] starting SKLMI against Sekaiemu memory socket"
"$sklmi_runtime" \
  --memory-socket "$memory_socket" \
  --bridge-manifest "$sklmi_manifest" \
  --room-state "$sklmi_room_state" \
  --runtime-state "$sklmi_runtime_state" \
  --trace-log "$sklmi_trace" \
  --mode archipelago \
  --ap-host 127.0.0.1 \
  --ap-port "$port" \
  --ap-path / \
  --ap-game "Final Fantasy IV Free Enterprise" \
  --ap-slot-name "Jade-FF4FE" \
  --ap-password "" \
  --ap-uuid sekailink-sekaiemu \
  --ap-tags "AP,SekaiLink,FF4FEManual" \
  --tick-ms 16 \
  > "$sklmi_log" 2>&1 &
sklmi_pid=$!

echo "[ff4fe-manual] ready. Close Sekaiemu to stop the session."
echo "[ff4fe-manual] run root: $run_root"
wait "$sekaiemu_pid"

echo "[ff4fe-manual] Sekaiemu closed. Proof files:"
echo "[ff4fe-manual] trace: $sklmi_trace"
echo "[ff4fe-manual] server: $server_log"
