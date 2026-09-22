#!/bin/bash
# Build the Go wrapper: native library (GO_SUPPORT=ON) staged into
# wrapper/go/lib, then go vet + go build + run tests.
# Go packages are distributed as source via go modules, so there is no
# binary packing step here.
# Usage: build_all_and_pack.sh [gcc|clang] [CONFIG]
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."

if [[ "$(uname)" == "Darwin" ]]; then
    exec "$PROJECT_ROOT/build/test/go/run_mac.sh" "${2:-RelWithDebInfo}"
else
    exec "$PROJECT_ROOT/build/test/go/run_linux.sh" "${1:-clang}" "${2:-RelWithDebInfo}"
fi
