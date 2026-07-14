#!/usr/bin/env bash
set -euo pipefail

package_id="${1:-}"
version="${2:-}"
appimage="${3:-}"
output_dir="${4:-}"

if [[ -z "$package_id" || -z "$version" || -z "$appimage" || -z "$output_dir" ]]; then
  echo "Usage: $0 PACKAGE_ID VERSION APPIMAGE OUTPUT_DIR" >&2
  exit 2
fi
if [[ ! "$package_id" =~ ^[a-z0-9][a-z0-9._-]{0,127}$ ]]; then
  echo "Invalid package id: $package_id" >&2
  exit 2
fi
if [[ ! -f "$appimage" ]] || ! file "$appimage" | grep -q 'ELF 64-bit.*x86-64'; then
  echo "The release input must be an x86-64 AppImage." >&2
  exit 2
fi

mkdir -p "$output_dir"
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT

install -m755 "$appimage" "$staging/game.appimage"
archive="$output_dir/${package_id}-${version}-linux-x64.tar.gz"
tar -C "$staging" -czf "$archive" game.appimage

bytes="$(stat -c '%s' "$archive")"
sha256="$(sha256sum "$archive" | awk '{print $1}')"

printf 'archive=%s\nbytes=%s\nsha256=%s\nexecutable=game.appimage\n' \
  "$archive" "$bytes" "$sha256"
