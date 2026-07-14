#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-}"
build_root="${2:-}"

if [[ -z "$source_root" || -z "$build_root" ]]; then
  echo "Usage: $0 /path/to/harkipelago-source /path/to/build" >&2
  exit 2
fi

mkdir -p "$build_root"

podman run --rm \
  --name sekailink-soh-portable-build \
  -v "$source_root:/src:Z" \
  -v "$build_root:/build:Z" \
  localhost/sekailink-harkipelago-linux-x64:22.04 \
  bash -lc '
    set -euo pipefail
    CC=gcc-12 CXX=g++-12 cmake -S /src -B /build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS=-m64 \
      -DCMAKE_CXX_FLAGS=-m64 \
      -DBUILD_REMOTE_CONTROL=ON
    cmake --build /build --target GenerateSohOtr --parallel "$(nproc)"
    cmake --build /build --parallel "$(nproc)"
  '
