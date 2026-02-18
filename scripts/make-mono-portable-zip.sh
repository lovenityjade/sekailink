#!/usr/bin/env bash
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

if [[ "$(uname -s)" != "Linux" ]]; then
  die "Linux only (uname -s=$(uname -s))"
fi

mono_bin="$(command -v mono || true)"
[[ -n "$mono_bin" ]] || die "mono not found in PATH. Install mono first (e.g. mono-complete) then rerun."
mono_bin="$(realpath "$mono_bin")"

arch="$(uname -m)"
case "$arch" in
  x86_64|amd64) arch="x86_64" ;;
  aarch64|arm64) arch="arm64" ;;
esac

out_zip="${1:-dist/sekailink-mono-linux-${arch}.zip}"
work="$(mktemp -d -t sekailink-mono-portable.XXXXXX)"
trap 'rm -rf "$work"' EXIT

root_dir="$work/sekailink-mono-linux-${arch}"
mkdir -p "$root_dir/bin" "$root_dir/lib" "$root_dir/etc"

echo "[mono-portable] mono: $mono_bin"
echo "[mono-portable] out:  $out_zip"

cp -a "$mono_bin" "$root_dir/bin/mono"

# Copy Mono's managed assemblies (best-effort). Location differs by distro (/usr/lib vs /usr/lib64).
prefix="$(dirname "$(dirname "$mono_bin")")"
mono_lib_src=""
for d in "$prefix/lib/mono" "$prefix/lib64/mono" "/usr/lib/mono" "/usr/lib64/mono"; do
  if [[ -d "$d" ]]; then
    mono_lib_src="$d"
    break
  fi
done
if [[ -n "$mono_lib_src" ]]; then
  echo "[mono-portable] copying managed assemblies from: $mono_lib_src"
  mkdir -p "$root_dir/lib/mono"
  # Copy the whole mono tree (contains gac/, 4.5/, etc).
  cp -a "$mono_lib_src"/. "$root_dir/lib/mono/"
else
  echo "[mono-portable] warn: could not find /usr/lib*/mono; portable mono may not run on systems without mono."
fi

# Copy mono config (best-effort).
if [[ -d "/etc/mono" ]]; then
  echo "[mono-portable] copying config from: /etc/mono"
  mkdir -p "$root_dir/etc/mono"
  cp -a /etc/mono/. "$root_dir/etc/mono/"
fi

# Copy mono-native shared libs that the mono binary typically needs.
# We intentionally do NOT try to bundle glibc/ld-linux etc.
copy_lib() {
  local src="$1"
  local dest_name="${2:-}"
  [[ -f "$src" ]] || return 0
  local base="${dest_name:-$(basename "$src")}"
  # Always copy the real file contents (not symlink metadata), so zip/unzip keeps a usable payload.
  local real
  real="$(realpath "$src" 2>/dev/null || printf "%s" "$src")"
  cp -f "$real" "$root_dir/lib/$base"
  chmod 755 "$root_dir/lib/$base" || true
}

copy_first_existing_named() {
  local name="$1"
  for d in /usr/lib64 /usr/lib "$prefix/lib64" "$prefix/lib"; do
    if [[ -f "$d/$name" ]]; then
      copy_lib "$d/$name" "$name"
      return 0
    fi
  done
  return 0
}

echo "[mono-portable] bundling mono native libs (best-effort)..."
while IFS= read -r line; do
  # ldd lines: "libX.so => /path/to/libX.so (0x...)" OR "/lib64/ld-linux... (0x...)"
  p="$(echo "$line" | awk '{print $3}' | tr -d '()' || true)"
  [[ -n "$p" && -f "$p" ]] || continue
  b="$(basename "$p")"
  case "$b" in
    libmono*|libmonosgen*|libMonoPosixHelper*|libmono-native*|libmono-profiler*|libmono-*)
      copy_lib "$p"
      ;;
  esac
done < <(ldd "$mono_bin" 2>/dev/null || true)

# Some mono helpers are dlopen()'d and won't appear in ldd output.
copy_first_existing_named "libMonoPosixHelper.so"
copy_first_existing_named "libmono-native.so"
copy_first_existing_named "libmono-btls-shared.so"

mkdir -p "$(dirname "$out_zip")"

if command -v zip >/dev/null 2>&1; then
  (cd "$work" && zip -r -9 "$(realpath "$out_zip")" "$(basename "$root_dir")" >/dev/null)
else
  # Fallback: python zipfile (may not preserve symlinks, but ok for our use).
  python3 -m zipfile -c "$out_zip" -C "$work" "$(basename "$root_dir")"
fi

echo "[mono-portable] done: $out_zip"
