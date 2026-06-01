#!/usr/bin/env bash
set -euo pipefail

SOURCE_LOG="${1:-/var/log/mail.log}"
TARGET_LOG="${2:-/opt/sekailink/evolution/admin-agent/data/postfix_tail.log}"

mkdir -p "$(dirname "$TARGET_LOG")"

if [[ -f "$SOURCE_LOG" ]]; then
  tail -n 200 "$SOURCE_LOG" > "$TARGET_LOG"
else
  : > "$TARGET_LOG"
fi
