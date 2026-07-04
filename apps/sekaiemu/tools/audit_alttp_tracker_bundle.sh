#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

bundle="${SEKAIEMU_TRACKER_AUDIT_BUNDLE:-$canonical_root/linkedworlds/alttp/tracker/default.bundle}"
out_dir="${SEKAIEMU_TRACKER_AUDIT_OUT_DIR:-/tmp/sekaiemu-tracker-audit}"
report="$out_dir/alttp-tracker-bundle-audit.md"

if [[ ! -d "$bundle" ]]; then
  echo "[tracker-audit][missing] bundle directory: $bundle" >&2
  exit 2
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "[tracker-audit][missing] jq is required for strict JSON validation." >&2
  exit 2
fi

mkdir -p "$out_dir"

count_files() {
  find "$1" -type f 2>/dev/null | wc -l | tr -d ' '
}

count_json_array_or_keys() {
  local file="$1"
  jq 'if type=="array" then length elif type=="object" then keys|length else 0 end' "$file"
}

strict_json_files=()
relaxed_json_files=()
while IFS= read -r file; do
  if jq empty "$file" >/dev/null 2>&1; then
    strict_json_files+=("$file")
  else
    relaxed_json_files+=("$file")
  fi
done < <(find "$bundle" -type f -name '*.json' | sort)

manifest_maps="$(jq '.maps | length' "$bundle/manifest.json")"
manifest_tabs="$(jq '.tabs | length' "$bundle/manifest.json")"
poptracker_maps=0
if [[ -f "$bundle/poptracker-adapted/maps/maps.json" ]] && jq empty "$bundle/poptracker-adapted/maps/maps.json" >/dev/null 2>&1; then
  poptracker_maps="$(jq 'length' "$bundle/poptracker-adapted/maps/maps.json")"
fi

{
  echo "# ALTTP Tracker Bundle Audit"
  echo
  echo "Generated: $(date -Iseconds)"
  echo
  echo "Bundle: \`$bundle\`"
  echo
  echo "## Counts"
  echo
  echo "- Total files: \`$(count_files "$bundle")\`"
  echo "- Adapted image files: \`$(count_files "$bundle/poptracker-adapted/images")\`"
  echo "- Strict JSON files: \`${#strict_json_files[@]}\`"
  echo "- Relaxed/non-strict JSON-like files: \`${#relaxed_json_files[@]}\`"
  echo "- Manifest maps: \`$manifest_maps\`"
  echo "- Manifest tabs: \`$manifest_tabs\`"
  echo "- PopTracker-adapted map entries: \`$poptracker_maps\`"
  echo
  echo "## Strict Runtime Metadata"
  echo
  for file in \
    manifest.json \
    tracker-flow.v1.json \
    item-icon-metadata.json \
    map-pin-metadata.json \
    settings-metadata.json \
    autotabbing-hints.json \
    poptracker-adapted/maps/maps.json \
    poptracker-adapted/sekailink-adaptation.json; do
    path="$bundle/$file"
    if [[ -f "$path" ]] && jq empty "$path" >/dev/null 2>&1; then
      echo "- \`$file\`: strict, top-level count \`$(count_json_array_or_keys "$path")\`"
    elif [[ -f "$path" ]]; then
      echo "- \`$file\`: non-strict"
    else
      echo "- \`$file\`: missing"
    fi
  done
  echo
  echo "## Relaxed JSON-Like Files"
  echo
  if (( ${#relaxed_json_files[@]} == 0 )); then
    echo "- None"
  else
    for file in "${relaxed_json_files[@]}"; do
      echo "- \`${file#"$bundle/"}\`"
    done
  fi
  echo
  echo "## Design Note"
  echo
  echo "Sekaiemu should consume strict LinkedWorld metadata at runtime. Relaxed"
  echo "PopTracker pack files should be normalized/exported before they become"
  echo "runtime dependencies."
} > "$report"

echo "[tracker-audit] report: $report"
