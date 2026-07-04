#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
sklmi_root="$(cd -- "${script_dir}/.." && pwd)"
clean_room_repos="$(cd -- "${sklmi_root}/.." && pwd)"

source_manifest="${clean_room_repos}/sekailink-linkedworld-alttp/bridge/sklmi.phase1.json"
target_manifest="${sklmi_root}/manifests/alttp.phase1.json"
tmp_manifest="${target_manifest}.tmp"

if [[ ! -f "${source_manifest}" ]]; then
  echo "missing ALTTP LinkedWorld runtime contract: ${source_manifest}" >&2
  exit 1
fi

jq '
  .version = "0.2.0-beta3-runtime" |
  .sklmi.bridge_id = "alttp-phase1" |
  .sklmi.driver_instance_id = "alttp-sekaiemu-runtime" |
  .sklmi.module = "sekaiemu" |
  .sklmi.state_file = "alttp.phase1.bridge.state" |
  .sklmi.poll_interval_ms = 16
' "${source_manifest}" > "${tmp_manifest}"

mv "${tmp_manifest}" "${target_manifest}"

checks="$(jq '.sklmi.checks | length' "${target_manifest}")"
actions="$(jq '.sklmi.actions | length' "${target_manifest}")"
echo "synced ${target_manifest}"
echo "checks=${checks} actions=${actions}"
