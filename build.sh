#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$ROOT_DIR/client/app"
RELEASE_DIR="$APP_DIR/release"

install_mode="auto" # auto|install|skip|ci
clean=0
skip_ui=0
skip_pack=0
copy_to=""
extra_pack_args=()

usage() {
  cat <<'EOF'
Usage: ./build.sh [options] [-- <electron-builder args...>]

Builds the SekaiLink Linux AppImage via Electron Builder.

Options:
  --clean               Remove previous build outputs (dist/, release/linux-unpacked/)
  --install             Force npm install before building
  --skip-install        Do not run npm install/ci
  --ci                  Use npm ci (requires package-lock.json)
  --skip-ui             Skip "npm run build" (UI + TS)
  --skip-pack           Skip AppImage packaging step
  --copy-to-webhost     Copy the resulting AppImage into WebHostLib/static/downloads/
  -h, --help            Show this help

Examples:
  ./build.sh
  ./build.sh --clean --install
  ./build.sh --ci --copy-to-webhost
  ./build.sh -- --publish never
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing dependency: $1"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --clean)
      clean=1
      ;;
    --install)
      install_mode="install"
      ;;
    --skip-install)
      install_mode="skip"
      ;;
    --ci)
      install_mode="ci"
      ;;
    --skip-ui)
      skip_ui=1
      ;;
    --skip-pack)
      skip_pack=1
      ;;
    --copy-to-webhost)
      copy_to="$ROOT_DIR/WebHostLib/static/downloads"
      ;;
    --)
      shift
      extra_pack_args+=("$@")
      break
      ;;
    *)
      extra_pack_args+=("$1")
      ;;
  esac
  shift
done

[[ -d "$APP_DIR" ]] || die "expected directory not found: $APP_DIR"

if [[ "$(uname -s)" != "Linux" ]]; then
  die "AppImage packaging is only supported on Linux (uname -s: $(uname -s))"
fi

need_cmd node
need_cmd npm

echo "Root:    $ROOT_DIR"
echo "App dir: $APP_DIR"
echo "Node:    $(node --version)"
echo "npm:     $(npm --version)"

cd "$APP_DIR"

if [[ $clean -eq 1 ]]; then
  echo "Cleaning build outputs..."
  rm -rf dist release/linux-unpacked
fi

case "$install_mode" in
  skip)
    ;;
  ci)
    [[ -f package-lock.json ]] || die "package-lock.json not found; --ci requires it"
    echo "Installing dependencies (npm ci)..."
    npm ci
    ;;
  install)
    echo "Installing dependencies (npm install)..."
    npm install
    ;;
  auto)
    if [[ ! -d node_modules ]]; then
      echo "Installing dependencies (npm install)..."
      npm install
    fi
    ;;
  *)
    die "unknown install mode: $install_mode"
    ;;
esac

if [[ $skip_ui -eq 0 ]]; then
  echo "Building UI (tsc + vite)..."
  npm run build
fi

if [[ $skip_pack -eq 0 ]]; then
  echo "Packaging AppImage (electron-builder --linux)..."
  if [[ ${#extra_pack_args[@]} -gt 0 ]]; then
    npm run electron:pack -- "${extra_pack_args[@]}"
  else
    npm run electron:pack
  fi
fi

latest_appimage=""
if compgen -G "$RELEASE_DIR"/*.AppImage >/dev/null; then
  latest_appimage="$(ls -1t "$RELEASE_DIR"/*.AppImage | head -n 1)"
fi

[[ -n "$latest_appimage" ]] || die "no AppImage found in: $RELEASE_DIR"

echo "Built: $latest_appimage"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$latest_appimage" | sed -n '1p'
fi

if [[ -n "$copy_to" ]]; then
  mkdir -p "$copy_to"
  echo "Copying to: $copy_to/"
  cp -f "$latest_appimage" "$copy_to/"
  echo "Copied: $copy_to/$(basename -- "$latest_appimage")"
fi

