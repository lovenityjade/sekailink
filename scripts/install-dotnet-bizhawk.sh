#!/usr/bin/env bash
set -euo pipefail

DOTNET_CHANNEL="${1:-9.0}"
DOTNET_DIR="${DOTNET_DIR:-$HOME/.dotnet}"
BIZHAWK_DIR="${BIZHAWK_DIR:-$HOME/Projects/sekailink/third_party/emulators/BizHawk}"

log() { printf "\n[sekailink] %s\n" "$*"; }

log "Installing .NET SDK channel ${DOTNET_CHANNEL} in ${DOTNET_DIR}"
mkdir -p "$DOTNET_DIR"
curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
bash /tmp/dotnet-install.sh --channel "$DOTNET_CHANNEL" --install-dir "$DOTNET_DIR"

export DOTNET_ROOT="$DOTNET_DIR"
export PATH="$DOTNET_ROOT:$PATH"

log "Verifying dotnet"
dotnet --info | sed -n '1,30p'
dotnet --list-sdks || true

if [[ ! -d "$BIZHAWK_DIR" ]]; then
  log "BizHawk directory not found: $BIZHAWK_DIR"
  log "Set BIZHAWK_DIR and rerun."
  exit 1
fi

log "Preparing BizHawk build scripts"
cd "$BIZHAWK_DIR"
chmod +x Dist/*.sh Dist/.*.sh || true

log "Building BizHawk Release"
bash Dist/BuildRelease.sh

log "Done. If build succeeded, artifacts are in BizHawk output folders."
log "Tip: add dotnet to PATH permanently by appending to ~/.bashrc:"
echo 'export DOTNET_ROOT="$HOME/.dotnet"'
echo 'export PATH="$DOTNET_ROOT:$PATH"'
