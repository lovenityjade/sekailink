#!/usr/bin/env bash
set -euo pipefail

root="${1:?project root required}"

dumps_dir="$root/../saves/earthbound-test/dumps"
log_file="$root/../saves/earthbound-live-test/bridge/outbox.log"

before="$dumps_dir/snapshot_2_system_ram.bin"
after="$dumps_dir/snapshot_3_system_ram.bin"

for f in "$before" "$after" "$log_file"; do
  [[ -f "$f" ]] || { echo "missing fixture: $f" >&2; exit 1; }
done

byte_before_9c11=$(od -An -j $((0x9C11)) -N 1 -tu1 "$before" | tr -d '[:space:]')
byte_after_9c11=$(od -An -j $((0x9C11)) -N 1 -tu1 "$after" | tr -d '[:space:]')
byte_before_9c6c=$(od -An -j $((0x9C6C)) -N 1 -tu1 "$before" | tr -d '[:space:]')
byte_after_9c6c=$(od -An -j $((0x9C6C)) -N 1 -tu1 "$after" | tr -d '[:space:]')

[[ "$byte_before_9c11" == "0" ]] || { echo "unexpected pre-check value at 0x9C11: $byte_before_9c11" >&2; exit 1; }
[[ "$byte_after_9c11" == "8" ]] || { echo "unexpected post-check value at 0x9C11: $byte_after_9c11" >&2; exit 1; }
[[ "$byte_before_9c6c" == "0" ]] || { echo "unexpected pre-check value at 0x9C6C: $byte_before_9c6c" >&2; exit 1; }
[[ "$byte_after_9c6c" == "16" ]] || { echo "unexpected post-check value at 0x9C6C: $byte_after_9c6c" >&2; exit 1; }

grep -Fq "location|0xEB0000|Onett - Tracy Gift|byte=17|bit=3" "$log_file"
grep -Fq "location|0xEB0001|Onett - Tracy's Room Present|byte=108|bit=4" "$log_file"

echo "EarthBound golden fixtures verified."
