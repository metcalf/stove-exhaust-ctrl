#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

mkdir -p "$SCRIPT_DIR/data"
rsync -avz --progress \
  "root@home-sbc-server-local.itsshedtime.com:/var/log.hdd/remote/stove-exhaust-ctrl.log-*.gz" \
  "root@home-sbc-server-local.itsshedtime.com:/var/log/remote/stove-exhaust-ctrl.log" \
  "$SCRIPT_DIR/data/synced"

pipenv run python "$SCRIPT_DIR/parse_synced.py" "$SCRIPT_DIR/data/synced"
