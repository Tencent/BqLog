#!/bin/bash
# Build the Go wrapper: native library (GO_SUPPORT=ON), then go vet +
# go build + run tests against the freshly built artifacts.
# Release CI runs module/ to package sources, then publish.sh to update
# go_dist. This development build/test command never publishes.
# Usage: build_all_and_pack.sh [gcc|clang] [CONFIG]
set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$DIR/../../.."

if [[ "$(uname)" == "Darwin" ]]; then
    exec "$PROJECT_ROOT/build/test/go/run_mac.sh" "${2:-RelWithDebInfo}"
else
    exec "$PROJECT_ROOT/build/test/go/run_linux.sh" "${1:-clang}" "${2:-RelWithDebInfo}"
fi
