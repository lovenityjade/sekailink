#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$ROOT/15-command-reference.md"
REGISTRY="$ROOT/commands/registry.txt"
PDF_DIR="$ROOT/dist/pdf"
SECRET_RE='(BEGIN (RSA|OPENSSH|EC|DSA|PRIVATE) KEY|ghp_[A-Za-z0-9_]{20,}|xox[baprs]-[A-Za-z0-9-]{20,}|admin_token[[:space:]]*[:=][[:space:]]*[^<[:space:]]+|password[[:space:]]*[:=][[:space:]]*[^<[:space:]]+)'

required_files=(
  README.md
  toc.md
  01-concepts.md
  02-first-login.md
  03-tui-navigation.md
  04-roles-and-permissions.md
  05-logs-and-debugging.md
  06-users-and-configs.md
  07-lobbies-and-rooms.md
  08-client-signals.md
  09-maintenance-mode.md
  10-releases-and-cdn.md
  11-scheduler.md
  12-bots-discord-twitch.md
  13-cleanup-and-backups.md
  14-incident-playbooks.md
  15-command-reference.md
  16-training-moderators.md
  17-admin-runbook.md
  glossary.md
  quick-reference.md
  change-control/README.md
  change-control/template-change-record.md
  change-control/template-connectivity-contract.md
  change-control/template-sklmi-approval-request.md
  change-control/emergency-change-policy.md
  change-control/platform-sync-policy.md
)

for file in "${required_files[@]}"; do
  test -f "$ROOT/$file" || {
    echo "missing required doc: $file" >&2
    exit 1
  }
done

while IFS= read -r command; do
  [[ -z "$command" ]] && continue
  if ! grep -Fq "$command" "$REF"; then
    echo "command missing from reference: $command" >&2
    exit 1
  fi
done < "$REGISTRY"

while IFS= read -r entry; do
  file="${entry%%:*}"
  line_rest="${entry#*:}"
  line="${line_rest%%:*}"
  text="${entry#*:*:}"
  while IFS= read -r link; do
    target="${link#*(}"
    target="${target%)}"
    target="${target%%#*}"
    [[ -z "$target" ]] && continue
    [[ "$target" =~ ^https?:// ]] && continue
    [[ "$target" =~ ^mailto: ]] && continue
    [[ "$target" =~ ^# ]] && continue
    if [[ "$target" == /* ]]; then
      test -e "$target" || {
        echo "broken absolute link $target in $file:$line" >&2
        exit 1
      }
    else
      base="$(dirname "$file")"
      test -e "$base/$target" || {
        echo "broken relative link $target in $file:$line" >&2
        exit 1
      }
    fi
  done < <(printf '%s\n' "$text" | grep -oE '\[[^]]+\]\([^)]+\)' || true)
done < <(grep -RInE '\[[^]]+\]\([^)]+\)' "$ROOT" --include='*.md')

if grep -RInE "$SECRET_RE" "$ROOT" --include='*.md' --include='*.txt'; then
  echo "possible secret pattern found in docs" >&2
  exit 1
fi

required_pdfs=(
  sekailink-core-access-full.pdf
  sekailink-core-access-service-training.pdf
  sekailink-core-access-command-reference.pdf
  sekailink-core-access-incident-playbooks.pdf
  sekailink-core-access-quick-reference.pdf
)

for pdf in "${required_pdfs[@]}"; do
  test -s "$PDF_DIR/$pdf" || {
    echo "missing or empty PDF: $pdf" >&2
    exit 1
  }
  if command -v pdfinfo >/dev/null 2>&1; then
    pdfinfo "$PDF_DIR/$pdf" >/dev/null
  fi
done

if command -v pdftotext >/dev/null 2>&1; then
  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT
  for pdf in "${required_pdfs[@]}"; do
    pdftotext "$PDF_DIR/$pdf" "$tmpdir/$pdf.txt"
  done
  if grep -RInE "$SECRET_RE" "$tmpdir"; then
    echo "possible secret pattern found in generated PDFs" >&2
    exit 1
  fi
fi

echo "Core Access docs validation ok"
