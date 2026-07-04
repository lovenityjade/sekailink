#!/usr/bin/env bash
set -euo pipefail

ROOT="${SEKAILINK_WORKBENCH_ROOT:-/d/SekaiLink}"
REPO="${SEKAILINK_REPO:-$ROOT/repos/sekailink-canonical}"
BUILD_ROOT="${SEKAILINK_BUILD_ROOT:-$ROOT/build}"
LOG_ROOT="${SEKAILINK_LOG_ROOT:-$ROOT/logs/worker}"
ARTIFACT_ROOT="${SEKAILINK_ARTIFACT_ROOT:-$ROOT/artifacts}"

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

mkdir -p "$LOG_ROOT" "$ARTIFACT_ROOT" "$BUILD_ROOT"

usage() {
  cat <<'USAGE'
SekaiLink MSYS2 Windows Worker

Usage:
  sekai-worker-msys.sh status
  sekai-worker-msys.sh doctor
  sekai-worker-msys.sh build sekaiemu [Debug|Release]
  sekai-worker-msys.sh test sekaiemu
  sekai-worker-msys.sh build client
  sekai-worker-msys.sh build bootloader
  sekai-worker-msys.sh package client
  sekai-worker-msys.sh lab sekaiemu-preview
  sekai-worker-msys.sh lab runtime-init
  sekai-worker-msys.sh lab runtime-bootstrap-python
  sekai-worker-msys.sh lab runtime-doctor
  sekai-worker-msys.sh lab runtime-list [all|nes|snes|gb_gbc|gba|n64|gamecube]
  sekai-worker-msys.sh lab runtime-yamls <family>
  sekai-worker-msys.sh lab runtime-generate <family>
  sekai-worker-msys.sh lab runtime-generate-game <game_key>
  sekai-worker-msys.sh lab runtime-roms
  sekai-worker-msys.sh lab runtime-rom-register <game_key> <path>
  sekai-worker-msys.sh lab runtime-trackers
  sekai-worker-msys.sh lab runtime-tracker-register <game_key> <path>
  sekai-worker-msys.sh lab runtime-plan <game_key> [server]
  sekai-worker-msys.sh lab runtime-launch <game_key> [--auto-server] [--run]
  sekai-worker-msys.sh lab runtime-seeds [family]
  sekai-worker-msys.sh lab runtime-server start|stop|status [family]
  sekai-worker-msys.sh lab runtime-status
  sekai-worker-msys.sh lab runtime-mark <game_key> <status> [note]
  sekai-worker-msys.sh lab runtime-logs [--tail N]
  sekai-worker-msys.sh logs

This worker is meant to be run locally on the Windows Box from MSYS2 UCRT64.
Keep SSH usage to one coarse command when remote control is needed.
USAGE
}

timestamp() {
  date +"%Y%m%d-%H%M%S"
}

write_result() {
  local command="$1"
  local status="$2"
  local log_path="$3"
  cat > "$ARTIFACT_ROOT/last-build-result.json" <<JSON
{
  "command": "$command",
  "status": "$status",
  "log": "$log_path",
  "repo": "$REPO",
  "artifacts": "$ARTIFACT_ROOT",
  "updated_at": "$(date -Iseconds)"
}
JSON
}

run_logged() {
  local name="$1"
  shift
  local log_path="$LOG_ROOT/$(timestamp)-$name.log"
  echo "sekai-worker command=$name started=$(date -Iseconds)" | tee "$log_path"
  if "$@" 2>&1 | tee -a "$log_path"; then
    echo "sekai-worker command=$name status=ok ended=$(date -Iseconds)" | tee -a "$log_path"
    write_result "$name" "ok" "$log_path"
  else
    local code=$?
    echo "sekai-worker command=$name status=failed code=$code ended=$(date -Iseconds)" | tee -a "$log_path"
    write_result "$name" "failed" "$log_path"
    return "$code"
  fi
}

require_repo() {
  if [[ ! -d "$REPO" ]]; then
    echo "missing repo: $REPO" >&2
    exit 2
  fi
}

cmd_status() {
  echo "SekaiLink Windows Workbench"
  echo "root=$ROOT"
  echo "repo=$REPO"
  echo "build_root=$BUILD_ROOT"
  echo "logs=$LOG_ROOT"
  echo "artifacts=$ARTIFACT_ROOT"
  uname -a
  whoami
  if [[ -d "$REPO" ]]; then
    git -C "$REPO" status --short --branch
    git -C "$REPO" rev-parse --short HEAD || true
  fi
}

cmd_doctor() {
  cmd_status
  local missing=0
  for tool in git cmake ninja gcc g++ node npm python zip unzip jq curl openssl rsync; do
    printf "%-10s" "$tool"
    if command -v "$tool"; then
      :
    else
      missing=1
    fi
  done
  git --version
  cmake --version | head -1
  ninja --version
  gcc --version | head -1
  node --version
  npm --version
  python --version
  return "$missing"
}

cmd_build_sekaiemu() {
  require_repo
  local config="${1:-Debug}"
  cmake -S "$REPO/apps/sekaiemu" -B "$BUILD_ROOT/sekaiemu-ucrt64-$config" -G Ninja -DCMAKE_BUILD_TYPE="$config"
  cmake --build "$BUILD_ROOT/sekaiemu-ucrt64-$config" -j"${SEKAILINK_BUILD_JOBS:-4}"
}

run_client_cmd() {
  local name="$1"
  shift
  local tmp_dir="$ROOT/tmp"
  mkdir -p "$tmp_dir"
  local script="$tmp_dir/sekai-client-$name-$$.cmd"
  local client_dir
  client_dir="$(cygpath -w "$REPO/apps/client-core")"
  {
    printf '@echo off\r\n'
    printf 'setlocal\r\n'
    printf 'set "PATH=C:\\Program Files\\nodejs;C:\\msys64\\ucrt64\\bin;C:\\msys64\\usr\\bin;%%PATH%%"\r\n'
    printf 'cd /d "%s" || exit /b 1\r\n' "$client_dir"
    for line in "$@"; do
      printf '%s\r\n' "$line"
    done
  } > "$script"
  local script_win
  script_win="$(cygpath -m "$script")"
  /c/Windows/System32/cmd.exe //c "$script_win"
  local code=$?
  rm -f "$script"
  return "$code"
}

cmd_test_sekaiemu() {
  require_repo
  local config="${1:-Debug}"
  if [[ ! -d "$BUILD_ROOT/sekaiemu-ucrt64-$config" ]]; then
    cmd_build_sekaiemu "$config"
  fi
  ctest --test-dir "$BUILD_ROOT/sekaiemu-ucrt64-$config" --output-on-failure
}

cmd_build_client() {
  require_repo
  rm -rf "$REPO/apps/client-core/node_modules"
  run_client_cmd build "call npm ci || exit /b 1" "call npm run build || exit /b 1"
}

cmd_build_bootloader() {
  require_repo
  run_client_cmd bootloader "call npm run bootstrapper:pack || exit /b 1"
}

cmd_package_client() {
  require_repo
  rm -rf "$REPO/apps/client-core/node_modules"
  run_client_cmd package "call npm ci || exit /b 1" "call npm run electron:pack:win || exit /b 1" "call npm run electron:pack:update-bundles || exit /b 1"
}

cmd_lab_sekaiemu_preview() {
  require_repo
  local exe="$REPO/runtime/platforms/win32-x64/bin/sekaiemu_libretro_spike.exe"
  local tracker="$REPO/runtime/tracker-bundles/alttp-linkedworld-default"
  if [[ ! -x "$exe" && ! -f "$exe" ]]; then
    echo "missing sekaiemu exe: $exe" >&2
    exit 2
  fi
  "$exe" --layout-preview --windowed --tracker-pack "$tracker" --tracker-variant split-screen
}

runtime_lab_py() {
  echo "$REPO/tools/runtime-lab/runtime_lab_windows.py"
}

cmd_runtime_lab() {
  require_repo
  local subcommand="${1:-doctor}"
  shift || true
  local lab_root="${SEKAILINK_RUNTIME_LAB_ROOT:-/d/RuntimeLab}"
  export SEKAILINK_RUNTIME_LAB_ROOT="$lab_root"
  export SEKAILINK_REPO="$REPO"
  python "$(runtime_lab_py)" "$subcommand" "$@"
}

cmd_logs() {
  find "$LOG_ROOT" -maxdepth 1 -type f -printf "%TY-%Tm-%Td %TH:%TM %s %p\n" 2>/dev/null | sort -r | head -30 || true
  if [[ -f "$ARTIFACT_ROOT/last-build-result.json" ]]; then
    echo
    cat "$ARTIFACT_ROOT/last-build-result.json"
  fi
}

command="${1:-status}"
target="${2:-}"
arg="${3:-}"

if [[ "$command" == "lab" ]]; then
  shift || true
  lab_command="${1:-doctor}"
  shift || true
  case "$lab_command" in
    sekaiemu-preview) cmd_lab_sekaiemu_preview ;;
    runtime-init) run_logged runtime-init cmd_runtime_lab init "$@" ;;
    runtime-bootstrap-python) run_logged runtime-bootstrap-python cmd_runtime_lab bootstrap-python "$@" ;;
    runtime-doctor) run_logged runtime-doctor cmd_runtime_lab doctor "$@" ;;
    runtime-list) run_logged "runtime-list-${1:-all}" cmd_runtime_lab list "${1:-all}" "${@:2}" ;;
    runtime-yamls) run_logged "runtime-yamls-${1:-all}" cmd_runtime_lab generate-yamls "${1:-all}" --runtime-only "${@:2}" ;;
    runtime-generate) run_logged "runtime-generate-${1:-all}" cmd_runtime_lab generate "${1:-all}" --runtime-only "${@:2}" ;;
    runtime-generate-game) run_logged "runtime-generate-game-${1:-missing}" cmd_runtime_lab generate-game "$@" --runtime-only ;;
    runtime-roms) run_logged runtime-roms cmd_runtime_lab roms "$@" ;;
    runtime-rom-register) run_logged "runtime-rom-register-${1:-missing}" cmd_runtime_lab rom-register "$@" --update-host ;;
    runtime-trackers) run_logged runtime-trackers cmd_runtime_lab trackers "$@" ;;
    runtime-tracker-register) run_logged "runtime-tracker-register-${1:-missing}" cmd_runtime_lab tracker-register "$@" ;;
    runtime-plan) run_logged "runtime-plan-${1:-missing}" cmd_runtime_lab plan-launch "$@" ;;
    runtime-launch) run_logged "runtime-launch-${1:-missing}" cmd_runtime_lab launch "$@" ;;
    runtime-seeds) run_logged "runtime-seeds-${1:-all}" cmd_runtime_lab seeds "$@" ;;
    runtime-server) run_logged "runtime-server-${1:-status}-${2:-all}" cmd_runtime_lab server "$@" ;;
    runtime-status) run_logged "runtime-status-${1:-all}" cmd_runtime_lab status "$@" ;;
    runtime-mark) run_logged "runtime-mark-${1:-missing}" cmd_runtime_lab mark "$@" ;;
    runtime-logs) run_logged runtime-logs cmd_runtime_lab logs "$@" ;;
    *) usage >&2; exit 2 ;;
  esac
  exit $?
fi

case "$command:$target" in
  status:) run_logged status cmd_status ;;
  doctor:) run_logged doctor cmd_doctor ;;
  build:sekaiemu) run_logged "build-sekaiemu-${arg:-Debug}" cmd_build_sekaiemu "${arg:-Debug}" ;;
  test:sekaiemu) run_logged "test-sekaiemu-${arg:-Debug}" cmd_test_sekaiemu "${arg:-Debug}" ;;
  build:client) run_logged build-client cmd_build_client ;;
  build:bootloader) run_logged build-bootloader cmd_build_bootloader ;;
  package:client) run_logged package-client cmd_package_client ;;
  logs:) cmd_logs ;;
  help:|--help:|-h:) usage ;;
  *) usage >&2; exit 2 ;;
esac
