#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

blocked_patterns=(
  "<local-home>/Projects/Sekaiemu-Libretro-Spike-Codex"
  "<local-home>/DevSSD/sekailink-beta-3-final"
  "<local-home>/DevSSD/sekailink-canonical"
  "<local-home>/DevSSD/_sekailink_quarantine"
  "<local-home>/DevSSD/sekailink-legacy-quarantine-2026-05-17"
  "<local-home>/SekaiLinkDev"
)

status=0
for pattern in "${blocked_patterns[@]}"; do
  if rg -n \
    --hidden \
    --glob '!.git' \
    --glob '!**/docs/**' \
    --glob '!docs/**' \
    --glob '!**/README.md' \
    --glob '!scripts/doctor-source-roots.sh' \
    --fixed-strings "$pattern" "$repo_root"; then
    status=1
  fi
done

if [[ "$status" -ne 0 ]]; then
  echo "Blocked legacy source roots were found. Update the references before packaging." >&2
  exit "$status"
fi

echo "No blocked legacy source roots found outside migration docs."
