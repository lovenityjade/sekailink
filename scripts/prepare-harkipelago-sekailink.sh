#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_root="${1:-}"

if [[ -z "$source_root" || ! -f "$source_root/soh/soh/OTRGlobals.cpp" ]]; then
  echo "Usage: $0 /path/to/harkipelago-source" >&2
  exit 2
fi
if [[ ! -f "$source_root/libultraship/cmake/dependencies/common.cmake" ]]; then
  echo "Harkipelago submodules are not initialized: libultraship is missing." >&2
  exit 2
fi

patch_dir="$repo_root/third_party/patches/harkipelago-sekailink"
apclient_source="$repo_root/third_party/upstream/poptracker-sekailink/lib/apclientpp/apclient.hpp"

if ! grep -q 'SekaiLinkLaunchOptions' "$source_root/soh/soh/OTRGlobals.cpp"; then
  patch --batch -d "$source_root" -p1 < "$patch_dir/0001-sekailink-managed-launch.patch"
fi
if ! grep -q 'GIT_SUBMODULES ""' "$source_root/soh/CMakeLists.txt"; then
  patch --batch -d "$source_root" -p1 < "$patch_dir/0002-shallow-imgui-fetch.patch"
fi

mouse_header="$source_root/libultraship/include/ship/window/MouseStateManager.h"
if ! grep -q '^#include <cstdint>$' "$mouse_header"; then
  sed -i '/^#pragma once$/a\\\n#include <cstdint>' "$mouse_header"
fi

install -Dm644 "$apclient_source" "$source_root/subprojects/apclientpp/apclient.hpp"

echo "Harkipelago source prepared for SekaiLink."
