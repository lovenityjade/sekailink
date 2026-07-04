#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/dist/pdf"
mkdir -p "$OUT"

if ! command -v pandoc >/dev/null 2>&1; then
  echo "pandoc not found; using xelatex fallback with raw Markdown pages." >&2
fi

if ! command -v xelatex >/dev/null 2>&1; then
  echo "xelatex is required to build Core Access PDFs." >&2
  exit 127
fi

COMMON=(
  --pdf-engine=xelatex
  --toc
  --number-sections
  -V geometry:margin=0.75in
  -V colorlinks=true
  -V linkcolor=blue
  -V urlcolor=blue
)

build_pdf() {
  local output="$1"
  shift
  if command -v pandoc >/dev/null 2>&1; then
    pandoc "${COMMON[@]}" -o "$OUT/$output" "$@"
    return
  fi

  local name="${output%.pdf}"
  local tex="$OUT/$name.tex"
  {
    printf '%s\n' '\documentclass[10pt]{article}'
    printf '%s\n' '\usepackage{fontspec}'
    printf '%s\n' '\usepackage[margin=0.65in]{geometry}'
    printf '%s\n' '\usepackage{hyperref}'
    printf '%s\n' '\usepackage{fancyhdr}'
    printf '%s\n' '\usepackage{fancyvrb}'
    printf '%s\n' '\pagestyle{fancy}'
    printf '%s\n' '\fancyhead[L]{SekaiLink Core Access}'
    printf '%s\n' '\fancyhead[R]{\today}'
    printf '%s\n' '\begin{document}'
    printf '%s\n' '\tableofcontents'
    for file in "$@"; do
      local title
      title="$(basename "$file" .md | sed 's/[#%&_{}]/\\&/g')"
      printf '\\section{%s}\n' "$title"
      printf '\\VerbatimInput[fontsize=\\small]{%s}\n' "$file"
      printf '%s\n' '\clearpage'
    done
    printf '%s\n' '\end{document}'
  } > "$tex"
  (cd "$OUT" && xelatex -interaction=nonstopmode -halt-on-error "$(basename "$tex")" >/dev/null)
  (cd "$OUT" && xelatex -interaction=nonstopmode -halt-on-error "$(basename "$tex")" >/dev/null)
}

FULL_DOCS=(
  "$ROOT/README.md"
  "$ROOT/toc.md"
  "$ROOT/01-concepts.md"
  "$ROOT/02-first-login.md"
  "$ROOT/03-tui-navigation.md"
  "$ROOT/04-roles-and-permissions.md"
  "$ROOT/05-logs-and-debugging.md"
  "$ROOT/06-users-and-configs.md"
  "$ROOT/07-lobbies-and-rooms.md"
  "$ROOT/08-client-signals.md"
  "$ROOT/09-maintenance-mode.md"
  "$ROOT/10-releases-and-cdn.md"
  "$ROOT/11-scheduler.md"
  "$ROOT/12-bots-discord-twitch.md"
  "$ROOT/13-cleanup-and-backups.md"
  "$ROOT/14-incident-playbooks.md"
  "$ROOT/15-command-reference.md"
  "$ROOT/16-training-moderators.md"
  "$ROOT/17-admin-runbook.md"
  "$ROOT/glossary.md"
  "$ROOT/change-control/README.md"
  "$ROOT/change-control/emergency-change-policy.md"
  "$ROOT/change-control/platform-sync-policy.md"
)

build_pdf sekailink-core-access-full.pdf "${FULL_DOCS[@]}"
build_pdf sekailink-core-access-service-training.pdf \
  "$ROOT/README.md" \
  "$ROOT/02-first-login.md" \
  "$ROOT/03-tui-navigation.md" \
  "$ROOT/04-roles-and-permissions.md" \
  "$ROOT/05-logs-and-debugging.md" \
  "$ROOT/07-lobbies-and-rooms.md" \
  "$ROOT/08-client-signals.md" \
  "$ROOT/14-incident-playbooks.md" \
  "$ROOT/16-training-moderators.md" \
  "$ROOT/glossary.md"
build_pdf sekailink-core-access-command-reference.pdf \
  "$ROOT/15-command-reference.md" \
  "$ROOT/glossary.md"
build_pdf sekailink-core-access-incident-playbooks.pdf \
  "$ROOT/14-incident-playbooks.md" \
  "$ROOT/quick-reference.md"
build_pdf sekailink-core-access-quick-reference.pdf \
  "$ROOT/quick-reference.md" \
  "$ROOT/toc.md"

echo "PDFs written to $OUT"
