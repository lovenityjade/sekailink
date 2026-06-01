#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
canonical_root="$(cd "$repo_root/../.." && pwd)"

build_dir="${SEKAIEMU_TRACKER_PREVIEW_BUILD_DIR:-$repo_root/build-codex-tracker}"
renderer="${SEKAIEMU_TRACKER_PREVIEW_BIN:-$build_dir/tracker_preview_render}"
bundle="${SEKAIEMU_TRACKER_PREVIEW_BUNDLE:-$canonical_root/linkedworlds/alttp/tracker/default.bundle}"
out_dir="${SEKAIEMU_TRACKER_PREVIEW_OUT_DIR:-/tmp/sekaiemu-tracker-preview}"
width="${SEKAIEMU_TRACKER_PREVIEW_WIDTH:-1280}"
height="${SEKAIEMU_TRACKER_PREVIEW_HEIGHT:-720}"
mode="${SEKAIEMU_TRACKER_PREVIEW_MODE:-quick}"
state_path="${SEKAIEMU_TRACKER_PREVIEW_STATE:-}"
contact_sheet="${SEKAIEMU_TRACKER_PREVIEW_CONTACT_SHEET:-1}"
contact_tile="${SEKAIEMU_TRACKER_PREVIEW_CONTACT_TILE:-4x}"

if [[ ! -x "$renderer" ]]; then
  cmake --build "$build_dir" --target tracker_preview_render -j2
fi

mkdir -p "$out_dir"
rendered_pngs=()
rendered_ppms=()

render_one() {
  local name="$1"
  local tab="$2"
  local map="$3"
  local ppm="$out_dir/$name.ppm"
  local png="$out_dir/$name.png"

  if [[ -n "$state_path" ]]; then
    "$renderer" "$bundle" "$ppm" "$width" "$height" "$tab" "$map" "$state_path"
  else
    "$renderer" "$bundle" "$ppm" "$width" "$height" "$tab" "$map"
  fi
  rendered_ppms+=("$ppm")
  if command -v magick >/dev/null 2>&1; then
    magick "$ppm" "$png"
    rendered_pngs+=("$png")
    echo "[tracker-preview] png: $png"
  else
    echo "[tracker-preview] ppm: $ppm"
  fi
}

render_one "alttp-light-world" "light-world" "light_world"
render_one "alttp-dark-world" "dark-world" "dark_world"
render_one "alttp-items" "items" "light_world"

if [[ "$mode" == "full" ]]; then
  render_one "alttp-mountain" "mountain" "death_mountain"
  render_one "alttp-escape" "escape" "hc_escape"
  render_one "alttp-eastern" "eastern" "eastern_palace"
  render_one "alttp-desert" "desert" "desert_palace"
  render_one "alttp-hera" "hera" "tower_of_hera"
  render_one "alttp-pod" "pod" ""
  render_one "alttp-swamp" "swamp" ""
  render_one "alttp-skull" "skull" ""
  render_one "alttp-thieves" "thieves" ""
  render_one "alttp-ice" "ice" ""
  render_one "alttp-mire" "mire" ""
  render_one "alttp-turtle" "turtle" ""
  render_one "alttp-gt" "gt" ""
fi

if [[ "$contact_sheet" != "0" ]] && command -v magick >/dev/null 2>&1 && (( ${#rendered_pngs[@]} > 1 )); then
  contact_path="$out_dir/alttp-tracker-contact-sheet.png"
  magick montage "${rendered_pngs[@]}" \
    -tile "$contact_tile" \
    -geometry 420x236+10+10 \
    -background '#0b1420' \
    -bordercolor '#24384f' \
    "$contact_path"
  echo "[tracker-preview] contact-sheet: $contact_path"
fi

echo "[tracker-preview] output: $out_dir"
