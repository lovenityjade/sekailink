#!/usr/bin/env bash
set -euo pipefail

channel="${SEKAILINK_CHANNEL:-canonical}"
build="${SEKAILINK_BUILD:-release}"
platform="${SEKAILINK_PLATFORM:-linux-x64}"
api_base_url="${SEKAILINK_API_BASE_URL:-https://sekailink.com}"
install_dir="${SEKAILINK_INSTALL_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/sekailink-client}"
force=0
no_launch=0
no_shortcut=0
dry_run=0

usage() {
  cat <<'EOF'
Usage: sekailink-bootstrapper.sh [options]

Options:
  --channel VALUE      Release channel, default: test
  --build VALUE        Release build, default: release
  --platform VALUE     Release platform, default: linux-x64
  --api-base-url URL   API base URL, default: https://sekailink.com
  --install-dir PATH   Install directory, default: ~/.local/share/sekailink-client
  --force              Reinstall even when install-state version matches latest
  --no-launch          Update/install only
  --no-shortcut        Do not write a desktop launcher
  --dry-run            Check release state without downloading
  -h, --help           Show this help
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --channel) channel="${2:?missing value}"; shift 2 ;;
    --build) build="${2:?missing value}"; shift 2 ;;
    --platform) platform="${2:?missing value}"; shift 2 ;;
    --api-base-url) api_base_url="${2:?missing value}"; shift 2 ;;
    --install-dir) install_dir="${2:?missing value}"; shift 2 ;;
    --force) force=1; shift ;;
    --no-launch) no_launch=1; shift ;;
    --no-shortcut) no_shortcut=1; shift ;;
    --dry-run) dry_run=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
  esac
done

log() {
  printf '[SekaiLink bootstrapper] %s\n' "$*"
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 1
  }
}

need_cmd curl
need_cmd sha256sum
need_cmd python3

json_field() {
  python3 - "$1" "$2" <<'PY'
import json, sys
path, key = sys.argv[1], sys.argv[2]
try:
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    value = data.get(key, "")
    if value is None:
        value = ""
    if isinstance(value, bool):
        print("true" if value else "false")
    else:
        print(str(value))
except Exception:
    print("")
PY
}

json_available() {
  python3 - "$1" <<'PY'
import json, sys
try:
    with open(sys.argv[1], "r", encoding="utf-8") as fh:
        data = json.load(fh)
    print("false" if data.get("available") is False else "true")
except Exception:
    print("false")
PY
}

write_install_state() {
  local state_dir="$1"
  local state_path="$state_dir/install-state.json"
  mkdir -p "$state_dir"
  python3 - "$state_path" "$target_version" "$channel" "$build" "$install_dir" <<'PY'
import json, os, sys
from datetime import datetime, timezone
state_path, version, channel, build, install_dir = sys.argv[1:6]
state = {
    "version": version,
    "manifestVersion": version,
    "channel": channel,
    "build": build,
    "platform": "linux",
    "arch": "x64",
    "artifactType": "client-bundle",
    "installDir": install_dir,
    "updatedAt": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
}
with open(state_path, "w", encoding="utf-8") as fh:
    json.dump(state, fh, indent=2)
    fh.write("\n")
PY
}

ensure_launcher_secret() {
  local state_dir="$1"
  local secret_path="$state_dir/launcher-secret.key"
  mkdir -p "$state_dir"
  if [ -f "$secret_path" ] && [ "$(wc -c < "$secret_path" | tr -d ' ')" -ge 32 ]; then
    tr -d '\r\n' < "$secret_path"
    return
  fi
  python3 - "$secret_path" <<'PY'
import base64, os, sys
secret = base64.b64encode(os.urandom(32)).decode("ascii")
with open(sys.argv[1], "w", encoding="ascii") as fh:
    fh.write(secret + "\n")
print(secret)
PY
}

new_launch_token() {
  local secret="$1"
  python3 - "$secret" <<'PY'
import base64, hashlib, hmac, json, sys, time
secret = sys.argv[1].encode("utf-8")
now = int(time.time() * 1000)
payload = {
    "iss": "sekailink-bootstrapper",
    "aud": "sekailink-client",
    "iat": now,
    "exp": now + 5 * 60 * 1000,
}
body = base64.urlsafe_b64encode(json.dumps(payload, separators=(",", ":")).encode("utf-8")).decode("ascii").rstrip("=")
sig = hmac.new(secret, body.encode("utf-8"), hashlib.sha256).hexdigest()
print(body + "." + sig)
PY
}

extract_zip() {
  local zip_path="$1"
  local extract_dir="$2"
  python3 - "$zip_path" "$extract_dir" <<'PY'
import os, shutil, sys, zipfile
zip_path, extract_dir = sys.argv[1], sys.argv[2]
if os.path.exists(extract_dir):
    shutil.rmtree(extract_dir)
os.makedirs(extract_dir, exist_ok=True)
with zipfile.ZipFile(zip_path, "r") as zf:
    zf.extractall(extract_dir)
PY
}

resolve_bundle_root() {
  local extract_dir="$1"
  if [ -f "$extract_dir/resources/app.asar" ]; then
    printf '%s\n' "$extract_dir"
    return
  fi
  local only=""
  local count=0
  while IFS= read -r entry; do
    only="$entry"
    count=$((count + 1))
  done < <(find "$extract_dir" -mindepth 1 -maxdepth 1 ! -name '__MACOSX' -print)
  if [ "$count" -eq 1 ] && [ -d "$only" ]; then
    printf '%s\n' "$only"
    return
  fi
  printf '%s\n' "$extract_dir"
}

find_client_executable() {
  local root="$1"
  for name in "sekailink-client" "SekaiLink Client" "SekaiLink-client"; do
    if [ -f "$root/$name" ]; then
      printf '%s\n' "$name"
      return
    fi
  done
  local found
  found="$(find "$root" -mindepth 1 -maxdepth 1 -type f -iname '*sekailink*' -printf '%f\n' | head -n 1 || true)"
  printf '%s\n' "$found"
}

install_self_bootstrapper() {
  local target_dir="$1"
  local source_path="${BASH_SOURCE[0]}"
  local target_path="$target_dir/sekailink-bootstrapper.sh"
  if [ -f "$source_path" ]; then
    local abs_source abs_target
    abs_source="$(python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$source_path")"
    abs_target="$(python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$target_path")"
    if [ "$abs_source" != "$abs_target" ]; then
      cp -f "$source_path" "$target_path"
    fi
    chmod +x "$target_path" || true
  fi
}

write_desktop_launcher() {
  local target_dir="$1"
  local exe_rel="$2"
  [ "$no_shortcut" -eq 0 ] || return 0
  local app_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
  mkdir -p "$app_dir"
  cat > "$app_dir/sekailink.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=SekaiLink
Exec=$target_dir/sekailink-bootstrapper.sh
Path=$target_dir
Icon=$target_dir/resources/app.asar
Terminal=false
Categories=Game;
StartupNotify=true
EOF
  chmod +x "$app_dir/sekailink.desktop" || true
  log "Desktop launcher written: $app_dir/sekailink.desktop"
}

launch_client() {
  local target_dir="$1"
  local state_dir="$2"
  local exe_rel="$3"
  local exe="$target_dir/$exe_rel"
  if [ ! -f "$exe" ]; then
    echo "Client executable missing: $exe" >&2
    exit 1
  fi
  chmod +x "$exe" || true
  local secret token
  secret="$(ensure_launcher_secret "$state_dir")"
  token="$(new_launch_token "$secret")"
  log "Launching $exe"
  SEKAILINK_BOOTSTRAP_INSTALL_DIR="$target_dir" \
    SEKAILINK_BOOTSTRAP_STATE_DIR="$state_dir" \
    SEKAILINK_BOOTSTRAP_TOKEN="$token" \
    nohup "$exe" >/dev/null 2>&1 &
}

api_base_url="${api_base_url%/}"
release_url="$api_base_url/api/client/release-latest?channel=$(python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$channel")&platform=$(python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$platform")"
if [ -n "$build" ]; then
  release_url="$release_url&build=$(python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1]))' "$build")"
fi

install_dir="$(python3 -c 'import os,sys;print(os.path.abspath(os.path.expanduser(sys.argv[1])))' "$install_dir")"
state_dir="$install_dir/.sekailink"
state_path="$state_dir/install-state.json"
work_root="$(mktemp -d "${TMPDIR:-/tmp}/sekailink-bootstrapper.XXXXXX")"
release_json="$work_root/release.json"

cleanup() {
  rm -rf "$work_root"
}
trap cleanup EXIT

log "Checking $release_url"
if ! curl -fsSL -A "SekaiLink-Bootstrapper/0.1 Linux" -H "Cache-Control: no-cache" "$release_url" -o "$release_json"; then
  log "Update check failed"
  exe_rel="$(find_client_executable "$install_dir")"
  if [ "$no_launch" -eq 0 ] && [ -n "$exe_rel" ]; then
    launch_client "$install_dir" "$state_dir" "$exe_rel"
    exit 0
  fi
  exit 1
fi

target_version="$(json_field "$release_json" version)"
download_url="$(json_field "$release_json" download_url)"
sha256="$(json_field "$release_json" sha256 | tr '[:upper:]' '[:lower:]')"
artifact_type="$(json_field "$release_json" artifact_type)"
available="$(json_available "$release_json")"
[ -n "$artifact_type" ] || artifact_type="client-bundle"

if [ "$available" != "true" ]; then
  echo "Release is not available" >&2
  exit 1
fi
if [ -z "$target_version" ] || [ -z "$download_url" ] || [ -z "$sha256" ]; then
  echo "Release manifest is incomplete" >&2
  exit 1
fi
if [ "$artifact_type" != "client-bundle" ]; then
  echo "Unsupported artifact type: $artifact_type" >&2
  exit 1
fi

installed_version=""
if [ -f "$state_path" ]; then
  installed_version="$(json_field "$state_path" version)"
fi

needs_update=0
if [ "$force" -eq 1 ] || [ -z "$installed_version" ] || [ "$installed_version" != "$target_version" ]; then
  needs_update=1
fi

if [ "$dry_run" -eq 1 ]; then
  log "dry-run installed=$installed_version latest=$target_version needsUpdate=$needs_update url=$download_url"
  exit 0
fi

exe_rel="sekailink-client"
if [ "$needs_update" -eq 1 ]; then
  log "Installing version $target_version into $install_dir"
  mkdir -p "$work_root/downloads" "$work_root/extract"
  bundle_name="$(basename "${download_url%%\?*}")"
  [ -n "$bundle_name" ] || bundle_name="sekailink-client-bundle.zip"
  zip_path="$work_root/downloads/$bundle_name"
  log "Downloading $download_url"
  curl -fL -A "SekaiLink-Bootstrapper/0.1 Linux" -H "Accept: application/zip,application/octet-stream,*/*" "$download_url" -o "$zip_path"
  actual_sha="$(sha256sum "$zip_path" | awk '{print tolower($1)}')"
  if [ "$actual_sha" != "$sha256" ]; then
    echo "Checksum mismatch expected=$sha256 actual=$actual_sha" >&2
    exit 1
  fi
  log "Extracting bundle"
  extract_zip "$zip_path" "$work_root/extract"
  bundle_root="$(resolve_bundle_root "$work_root/extract")"
  if [ ! -f "$bundle_root/resources/app.asar" ]; then
    echo "Bundle app.asar missing" >&2
    exit 1
  fi
  exe_rel="$(find_client_executable "$bundle_root")"
  if [ -z "$exe_rel" ]; then
    echo "Bundle executable missing" >&2
    exit 1
  fi
  mkdir -p "$install_dir"
  find "$install_dir" -mindepth 1 -maxdepth 1 \
    ! -name ".sekailink" \
    ! -name "sekailink-bootstrapper.sh" \
    ! -name "SekaiLink-bootstrapper.ps1" \
    ! -name "SekaiLink-bootstrapper.cmd" \
    -exec rm -rf {} +
  cp -a "$bundle_root"/. "$install_dir"/
  chmod +x "$install_dir/$exe_rel" 2>/dev/null || true
  install_self_bootstrapper "$install_dir"
  write_install_state "$state_dir"
  write_desktop_launcher "$install_dir" "$exe_rel"
  log "Installed $target_version"
else
  log "Already up to date: $installed_version"
fi

if [ "$no_launch" -eq 0 ]; then
  launch_client "$install_dir" "$state_dir" "$exe_rel"
fi
