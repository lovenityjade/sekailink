#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 --start | --restart | --stop"
}

if [[ $# -ne 1 ]]; then
  usage
  exit 1
fi

case "$1" in
  --start)
    systemctl start multiworldgg-webhost.service
    systemctl start multiworldgg-workers.service
    ;;
  --restart)
    systemctl restart multiworldgg-webhost.service
    systemctl restart multiworldgg-workers.service
    ;;
  --stop)
    systemctl stop multiworldgg-workers.service
    systemctl stop multiworldgg-webhost.service
    ;;
  *)
    usage
    exit 1
    ;;
 esac
