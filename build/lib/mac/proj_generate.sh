#!/bin/zsh
set -e -u -o pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" >/dev/null 2>&1 && pwd)"
exec "${SCRIPT_DIR}/dont_execute_this.sh" gen-vsproj "$@"