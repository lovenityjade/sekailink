#!/usr/bin/env bash
set -euo pipefail

root="${1:?project root required}"

log_file="$root/../saves/alttp-test/bridge/outbox.log"
[[ -f "$log_file" ]] || { echo "missing fixture: $log_file" >&2; exit 1; }

grep -Fq "inject|item|0xC|player=0|progress=1|ALTTP live Blue Boomerang" "$log_file"
grep -Fq "inject|item|0xA|player=0|progress=14|ALTTP hot Hookshot test" "$log_file"

echo "ALTTP golden injection fixtures verified."
