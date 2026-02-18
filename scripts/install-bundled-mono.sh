#!/usr/bin/env bash
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
arch="$(uname -m)"
case "$arch" in
  x86_64|amd64) arch="x86_64" ;;
  aarch64|arm64) arch="arm64" ;;
esac

dest_root="$repo_root/third_party/mono/sekailink-mono-linux-${arch}"
tmp_dir="$(mktemp -d -t sekailink-mono-linux-${arch}.XXXXXX)"
tmp_zip="$tmp_dir/sekailink-mono-linux-${arch}.zip"
trap 'rm -rf "$tmp_dir"' EXIT

echo "[mono-bundle] building portable mono zip from system mono..."
"$repo_root/scripts/make-mono-portable-zip.sh" "$tmp_zip"

echo "[mono-bundle] extracting to: $dest_root"
rm -rf "$dest_root"
mkdir -p "$dest_root"

if command -v unzip >/dev/null 2>&1; then
  unzip -q "$tmp_zip" -d "$tmp_dir"
else
  python3 - <<'PY'
import os, sys, zipfile
zip_path = sys.argv[1]
out_dir = sys.argv[2]
with zipfile.ZipFile(zip_path) as z:
  z.extractall(out_dir)
PY
  "$tmp_zip" "$tmp_dir"
fi

# The zip contains a top-level folder named sekailink-mono-linux-${arch}.
src="$tmp_dir/sekailink-mono-linux-${arch}"
[[ -d "$src" ]] || die "expected extracted folder missing: $src"
cp -a "$src"/. "$dest_root"/

mono_bin="$dest_root/bin/mono"
[[ -f "$mono_bin" ]] || die "mono binary missing after install: $mono_bin"
chmod +x "$mono_bin" || true

echo "[mono-bundle] done: $mono_bin"
