#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

build_dir="${SEKAIEMU_E2E_BUILD_DIR:-$repo_root/build-codex-tracker}"
sekaiemu_bin="${SEKAIEMU_E2E_BIN:-$build_dir/sekaiemu_libretro_spike}"
core_path="${SEKAIEMU_E2E_CORE:-$canonical_root/runtime/cores/bsnes_mercury_performance_libretro.so}"
game_rom="${SEKAIEMU_E2E_GAME_ROM:-<local-home>/Games/The Legend of Jade/manual-test/alttp-keysanity-20260527/patched/AP_13680877444271862070_P1_Jade-ALTTP.sfc}"
server_bin="${SEKAIEMU_E2E_MULTISERVER:-<local-home>/Games/The Legend of Jade/clients/squashfs-root/opt/MultiworldGG/MultiworldGGServer}"
seed_zip="${SEKAIEMU_E2E_SEED_ZIP:-<local-home>/Games/The Legend of Jade/manual-test/alttp-keysanity-20260527/output/AP_13680877444271862070.zip}"
sklmi_runtime="${SEKAIEMU_E2E_SKLMI_RUNTIME:-$canonical_root/runtime/bin/sekailink_sklmi_runtime}"
sklmi_manifest_dir="${SEKAIEMU_E2E_SKLMI_MANIFEST_DIR:-$canonical_root/services/sklmi/manifests}"
profile_path="${SEKAIEMU_E2E_ALTTP_PROFILE:-$repo_root/profiles/alttp-starter.profile}"
tracker_bundle="${SEKAIEMU_E2E_TRACKER_BUNDLE:-$canonical_root/linkedworlds/alttp/tracker/default.bundle}"
start_state_dir="${SEKAIEMU_E2E_START_STATE_DIR:-/tmp/sekaiemu-links-house-state}"
run_root="${SEKAIEMU_E2E_RUN_ROOT:-/tmp/sekaiemu-alttp-keysanity-manual-session}"
port="${SEKAIEMU_E2E_AP_PORT:-38290}"
server_password="${SEKAIEMU_E2E_SERVER_PASSWORD:-}"

require_file() {
  local path="$1"
  local label="$2"
  if [[ ! -f "$path" ]]; then
    echo "[manual-check][missing] $label: $path" >&2
    exit 2
  fi
}

require_dir() {
  local path="$1"
  local label="$2"
  if [[ ! -d "$path" ]]; then
    echo "[manual-check][missing] $label: $path" >&2
    exit 2
  fi
}

wait_for_port() {
  local listen_port="$1"
  for _ in $(seq 1 120); do
    if ! kill -0 "$server_pid" 2>/dev/null; then
      echo "[manual-check][fail] MultiServer exited before listening." >&2
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
require_file "$game_rom" "patched ALTTP ROM"
require_file "$server_bin" "MultiServer binary"
require_file "$seed_zip" "AP seed zip"
require_file "$sklmi_runtime" "SKLMI runtime"
require_dir "$sklmi_manifest_dir" "SKLMI manifest directory"
require_file "$profile_path" "ALTTP profile"
require_dir "$tracker_bundle" "ALTTP tracker bundle"

rm -rf "$run_root"
mkdir -p "$run_root/live/logs"
load_state_args=()
if [[ -d "$start_state_dir/saves" && -d "$start_state_dir/system" ]]; then
  cp -a "$start_state_dir/system" "$run_root/live/system"
  cp -a "$start_state_dir/saves" "$run_root/live/saves"
  load_state_args=(--load-state-on-start)
else
  echo "[manual-check] no start state found; booting from ROM start."
  mkdir -p "$run_root/live/system" "$run_root/live/saves"
fi
rm -rf "$run_root/live/saves/sklmi" "$run_root/live/saves/tracker" "$run_root/live/saves/runtime"
mkdir -p "$run_root/live/saves/runtime"

server_log="$run_root/live/multiserver.log"
echo "[manual-check] starting MultiServer on 127.0.0.1:$port"
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
  echo "[manual-check][fail] MultiServer did not listen on port $port" >&2
  echo "[manual-check][log] $server_log" >&2
  exit 4
fi

echo "[manual-check] server ready. Play normally, then close Sekaiemu when done."
echo "[manual-check] run root: $run_root"
echo "[manual-check] tracker bundle: $tracker_bundle"
echo "[manual-check] tracker snapshot: $run_root/live/saves/sklmi/alttp-phase1/runtime/tracker.snapshot.json"
echo "[manual-check] tracker commands: $run_root/live/saves/sklmi/alttp-phase1/runtime/tracker.commands.jsonl"
echo "[manual-check] tracker controls: F8 display mode, F9 show/toggle, F10 tab, F11 auto-map"

"$sekaiemu_bin" \
  --core "$core_path" \
  --game "$game_rom" \
  --profile "$profile_path" \
  --system-dir "$run_root/live/system" \
  --save-dir "$run_root/live/saves" \
  --log-dir "$run_root/live/logs" \
  "${load_state_args[@]}" \
  --sklmi-runtime "$sklmi_runtime" \
  --sklmi-manifest-dir "$sklmi_manifest_dir" \
  --ap-host 127.0.0.1 \
  --ap-port "$port" \
  --ap-path / \
  --ap-game "A Link to the Past" \
  --ap-slot-name "Jade-ALTTP" \
  --ap-tags "AP,SekaiLink,ManualCheck" \
  --tracker-pack "$tracker_bundle" \
  --tracker-assets-root "$tracker_bundle" \
  --tracker-bundle "$tracker_bundle" \
  > "$run_root/live/run.out" 2>&1

echo "[manual-check] Sekaiemu closed. Proof files:"
echo "[manual-check] trace: $run_root/live/saves/sklmi/alttp-phase1/trace.jsonl"
echo "[manual-check] server: $server_log"
echo "[manual-check] tracker: $run_root/live/saves/tracker/$(basename "$tracker_bundle")/state.json"
