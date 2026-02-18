#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

default_python="$HOME/.config/sekailink-client/runtime/tools/python/venv/bin/python"
python_bin="${SEKAILINK_HEADLESS_PYTHON:-$default_python}"

if [[ ! -x "$python_bin" ]]; then
  python_bin="${PYTHON:-python3}"
fi

exec "$python_bin" "$repo_root/client/runtime/tests/headless_ap_smoke.py" "$@"
