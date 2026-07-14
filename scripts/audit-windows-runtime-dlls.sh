#!/usr/bin/env bash
set -uo pipefail

root="${1:?Usage: audit-windows-runtime-dlls.sh BIN_DIR}"
objdump="${OBJDUMP:-/ucrt64/bin/objdump.exe}"
missing=0

for file in "$root"/*.exe "$root"/*.dll; do
  [[ -f "$file" ]] || continue
  while IFS= read -r dependency; do
    normalized="$(printf '%s' "$dependency" | tr '[:upper:]' '[:lower:]')"
    case "$normalized" in
      api-ms-win-*|kernel32.dll|user32.dll|gdi32.dll|advapi32.dll|shell32.dll|ole32.dll|oleaut32.dll|ws2_32.dll|winmm.dll|imm32.dll|version.dll|setupapi.dll|bcrypt.dll|crypt32.dll|comdlg32.dll|shlwapi.dll|rpcrt4.dll|iphlpapi.dll|ntdll.dll|msvcrt.dll|opengl32.dll|secur32.dll|wldap32.dll)
        continue
        ;;
    esac
    if [[ ! -f "$root/$dependency" ]]; then
      printf 'MISSING %s -> %s\n' "$(basename "$file")" "$dependency"
      missing=1
    fi
  done < <({ "$objdump" -p "$file" || true; } | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p')
done

printf 'AUDIT_MISSING=%s\n' "$missing"
exit "$missing"
