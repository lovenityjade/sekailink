#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

build_dir="${SEKAIEMU_E2E_BUILD_DIR:-/tmp/sekaiemu-libretro-spike-beta3-build}"
sekaiemu_bin="${SEKAIEMU_E2E_BIN:-$build_dir/sekaiemu_libretro_spike}"
core_path="${SEKAIEMU_E2E_CORE:-$canonical_root/runtime/cores/bsnes_mercury_performance_libretro.so}"
game_rom="${SEKAIEMU_E2E_GAME_ROM:-/tmp/sekaiemu-real-alttp-full/saves/patched/AP_78856210104802680998_P2_Jade-ALTTP.sfc}"
server_bin="${SEKAIEMU_E2E_MULTISERVER:-/home/thelovenityjade/Games/The Legend of Jade/clients/squashfs-root/opt/MultiworldGG/MultiworldGGServer}"
seed_zip="${SEKAIEMU_E2E_SEED_ZIP:-/home/thelovenityjade/Downloads/AP_78856210104802680998.zip}"
sklmi_runtime="${SEKAIEMU_E2E_SKLMI_RUNTIME:-$canonical_root/runtime/bin/sekailink_sklmi_runtime}"
sklmi_manifest_dir="${SEKAIEMU_E2E_SKLMI_MANIFEST_DIR:-$canonical_root/services/sklmi/manifests}"
profile_path="${SEKAIEMU_E2E_ALTTP_PROFILE:-$repo_root/profiles/alttp-starter.profile}"
tracker_bundle="${SEKAIEMU_E2E_TRACKER_BUNDLE:-$canonical_root/linkedworlds/alttp/tracker/default.bundle}"
start_state_dir="${SEKAIEMU_E2E_START_STATE_DIR:-/tmp/sekaiemu-links-house-state}"
run_root="${SEKAIEMU_E2E_RUN_ROOT:-/tmp/sekaiemu-alttp-chest-e2e}"
port="${SEKAIEMU_E2E_AP_PORT:-38290}"

enter_script="$script_dir/alttp-enter-links-house.input"
chest_script="$script_dir/alttp-open-links-house-chest.input"

require_file() {
  local path="$1"
  local label="$2"
  if [[ ! -f "$path" ]]; then
    echo "[e2e][missing] $label: $path" >&2
    exit 2
  fi
}

require_dir() {
  local path="$1"
  local label="$2"
  if [[ ! -d "$path" ]]; then
    echo "[e2e][missing] $label: $path" >&2
    exit 2
  fi
}

wait_for_port() {
  local listen_port="$1"
  for _ in $(seq 1 80); do
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
require_file "$enter_script" "Link's House input script"
require_file "$chest_script" "chest input script"

rm -rf "$run_root"
if [[ -d "$start_state_dir/system" && -d "$start_state_dir/saves" ]]; then
  echo "[e2e] phase 1/2: using calibrated Link's House savestate from $start_state_dir"
  mkdir -p "$run_root/state"
  cp -a "$start_state_dir/system" "$run_root/state/system"
  cp -a "$start_state_dir/saves" "$run_root/state/saves"
  if [[ -f "$start_state_dir/links-house-state.ppm" ]]; then
    cp "$start_state_dir/links-house-state.ppm" "$run_root/links-house-state.ppm"
  fi
else
  mkdir -p "$run_root/state/system" "$run_root/state/saves" "$run_root/state/logs"

  echo "[e2e] phase 1/2: creating Link's House savestate"
  SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
  SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" \
  "$sekaiemu_bin" \
    --core "$core_path" \
    --game "$game_rom" \
    --system-dir "$run_root/state/system" \
    --save-dir "$run_root/state/saves" \
    --log-dir "$run_root/state/logs" \
    --input-script "$enter_script" \
    --save-state-at-frame 2200 \
    --dump-frame-at-frame 2200 \
    --dump-frame-path "$run_root/links-house-state.ppm" \
    --quit-after-frame 2205 \
    > "$run_root/state/run.out" 2>&1

  state_path="$run_root/state/saves/states/$(basename "${game_rom%.*}").state"
  if [[ ! -f "$state_path" ]]; then
    echo "[e2e][fail] savestate was not created: $state_path" >&2
    exit 3
  fi
fi

mkdir -p "$run_root/live"
cp -a "$run_root/state/system" "$run_root/live/system"
cp -a "$run_root/state/saves" "$run_root/live/saves"
mkdir -p "$run_root/live/logs"
rm -rf "$run_root/live/saves/sklmi" "$run_root/live/saves/tracker"

server_log="$run_root/live/multiserver.log"
echo "[e2e] phase 2/2: running chest heartbeat on AP port $port"
"$server_bin" "$seed_zip" --host 127.0.0.1 --port "$port" --disable_save --loglevel debug \
  > "$server_log" 2>&1 &
server_pid=$!
cleanup() {
  kill "$server_pid" 2>/dev/null || true
  wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT

if ! wait_for_port "$port"; then
  echo "[e2e][fail] MultiServer did not listen on port $port" >&2
  exit 4
fi

SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" \
"$sekaiemu_bin" \
  --core "$core_path" \
  --game "$game_rom" \
  --profile "$profile_path" \
  --system-dir "$run_root/live/system" \
  --save-dir "$run_root/live/saves" \
  --log-dir "$run_root/live/logs" \
  --load-state-on-start \
  --sklmi-runtime "$sklmi_runtime" \
  --sklmi-manifest-dir "$sklmi_manifest_dir" \
  --ap-host 127.0.0.1 \
  --ap-port "$port" \
  --ap-path / \
  --ap-game "A Link to the Past" \
  --ap-slot-name "Jade-ALTTP" \
  --ap-tags "AP,SekaiLink,ChestE2E" \
  --tracker-bundle "$tracker_bundle" \
  --input-script "$chest_script" \
  --dump-frame-at-frame 560 \
  --dump-frame-path "$run_root/live/chest-result.ppm" \
  --quit-after-frame 1500 \
  > "$run_root/live/run.out" 2>&1

cleanup
trap - EXIT

trace_log="$run_root/live/saves/sklmi/alttp-phase1/trace.jsonl"
room_sync_state="$run_root/live/saves/sklmi/alttp-phase1/runtime/alttp_phase1.room-sync.state"
tracker_state="$run_root/live/saves/tracker/$(basename "$tracker_bundle")/state.json"
sekaiemu_log="$run_root/live/logs/sekaiemu.log"

grep -Fq '"event_type":"location_checked"' "$trace_log"
grep -Fq '"canonical_id":59836' "$trace_log"
grep -Fq 'LocationChecks","locations":[59836]' "$server_log"
grep -Fq "reported|59836|Link's House" "$room_sync_state"
grep -Fq 'local_checked=1' "$run_root/live/run.out"
grep -Fq 'recent=1' "$run_root/live/run.out"
grep -Fq 'Sekaiemu runtime exited cleanly' "$sekaiemu_log"

if command -v magick >/dev/null 2>&1; then
  magick "$run_root/links-house-state.ppm" "$run_root/links-house-state.png" || true
  magick "$run_root/live/chest-result.ppm" "$run_root/live/chest-result.png" || true
fi

echo "[e2e][pass] ALTTP chest heartbeat complete"
echo "[e2e][proof] trace: $trace_log"
echo "[e2e][proof] server: $server_log"
echo "[e2e][proof] tracker: $tracker_state"
echo "[e2e][proof] frame: $run_root/live/chest-result.ppm"
