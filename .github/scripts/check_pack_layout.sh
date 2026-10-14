#!/usr/bin/env bash
# Asserts that a prebuilt package under dist/ keeps the layout the release page publishes.
#
# The install layout is generated from GNUInstallDirs, so this is the guard that a change
# to it did not move or drop the files users download:
#   <package>/<libtype>/include/bq_log/...
#   <package>/<libtype>/lib/<config>/<library>
#   <package>/tools/bin/<tool>
#
# Usage: bash check_pack_layout.sh [dir]     (default: dist)
set -eu

dir="${1:-dist}"
checked=0

fail() {
    echo "check_pack_layout: $1" >&2
    exit 1
}

[ -d "$dir" ] || fail "no such directory: $dir"

# A library package carries one directory per library form, each with its own headers.
while IFS= read -r libtype_dir; do
    [ -n "$libtype_dir" ] || continue
    checked=$((checked + 1))

    [ -f "${libtype_dir}/include/bq_log/bq_log.h" ] \
        || fail "missing include/bq_log/bq_log.h under ${libtype_dir}"
    [ -f "${libtype_dir}/include/bq_log/bq.log.cppm" ] \
        || fail "missing include/bq_log/bq.log.cppm under ${libtype_dir}"

    # The library keeps its configuration level below lib/ (plus an ABI level on ohos). The node
    # addon package carries its shared library under that extension instead of .so, like
    # check_go_bridge_exports.sh already accepts.
    if ! find "${libtype_dir}/lib" -mindepth 2 -maxdepth 3 \
        \( -name '*.a' -o -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \
           -o -name '*.node' -o -name '*.lib' -o -name '*.dll' \
           -o -name '*.framework' -o -name '*.xcframework' \) \
        -print -quit 2>/dev/null | grep -q .; then
        fail "no library file under ${libtype_dir}/lib/<config>"
    fi
done <<EOF
$(find "$dir" -type d \( -name static_lib -o -name dynamic_lib \) 2>/dev/null | sort)
EOF

# A tool package carries both commands in bin/.
while IFS= read -r tools_dir; do
    [ -n "$tools_dir" ] || continue

    for tool in BqLog_CategoryLogGenerator BqLog_LogDecoder; do
        find "${tools_dir}/bin" -maxdepth 1 -name "${tool}*" -print -quit 2>/dev/null | grep -q . \
            || fail "missing ${tool} under ${tools_dir}/bin"
    done
    checked=$((checked + 1))
done <<EOF
$(find "$dir" -type d -name tools 2>/dev/null | sort)
EOF

[ "$checked" -gt 0 ] || fail "nothing to check under $dir"

echo "check_pack_layout: ok (${checked} package directories)"
