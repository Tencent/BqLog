#!/bin/sh
set -eu
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" >/dev/null 2>&1 && pwd)
exec "$SCRIPT_DIR/dont_execute_this.sh" all "$@"