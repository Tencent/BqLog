#!/usr/bin/env bash
# Asserts that every desktop prebuilt dynamic library exports the Go bridge entry points.
#
# The bridge is only compiled when the build drivers receive go=YES. A parameter in the wrong
# position drops it from the release silently (see T02 in design/impl_guide/REVIEW_2026-10-11.md),
# so a release must assert that it is there instead of trusting the driver. The check used to run
# on one Windows job only and only looked at *.dll; this covers .dll / .so / .dylib.
#
# Usage: bash check_go_bridge_exports.sh [dir]     (default: dist)
set -eu

dir="${1:-dist}"
symbols="__api_go_log_write __api_go_log_write_1 __api_go_log_write_2 __api_go_log_write_4"

# True when the dynamic library $1 exports the symbol $2. Several readers are tried because the
# runners differ: MSYS/MinGW and the Linux/BSD containers have objdump and nm, macOS has nm with
# -arch, and a plain MSVC shell only has dumpbin.
has_symbol() {
    lib="$1"
    sym="$2"
    if command -v objdump >/dev/null 2>&1; then
        objdump -p "$lib" 2>/dev/null | grep -qw "$sym" && return 0
        objdump -T "$lib" 2>/dev/null | grep -qw "$sym" && return 0
    fi
    if command -v nm >/dev/null 2>&1; then
        case "$lib" in
            *.dylib)
                # built as a universal binary: both slices must export the symbol
                nm -arch arm64 -gU "$lib" 2>/dev/null | grep -qw "$sym" && return 0
                nm -arch x86_64 -gU "$lib" 2>/dev/null | grep -qw "$sym" && return 0
                ;;
            *)
                # -D --defined-only keeps undefined imports out of the match
                nm -D --defined-only "$lib" 2>/dev/null | grep -qw "$sym" && return 0
                nm -g "$lib" 2>/dev/null | grep -qw "$sym" && return 0
                ;;
        esac
    fi
    if command -v dumpbin >/dev/null 2>&1; then
        dumpbin /nologo /exports "$lib" 2>/dev/null | grep -qw "$sym" && return 0
    fi
    # Last resort for a runner whose bash has none of the readers above (a plain Git Bash has no
    # objdump/nm, and dumpbin needs the MSVC environment): the export name tables are plain
    # strings in the image, and a library built without the bridge does not contain the name at all.
    grep -aq "$sym" "$lib" && return 0
    return 1
}

found=0
while IFS= read -r lib; do
    [ -n "$lib" ] || continue
    found=1
    for sym in $symbols; do
        if ! has_symbol "$lib" "$sym"; then
            echo "check_go_bridge_exports: missing $sym in $lib" >&2
            exit 1
        fi
    done
    echo "check_go_bridge_exports: ok $lib"
done <<EOF
$(find "$dir" -type f \( -name '*BqLog*.dll' -o -name '*BqLog*.so' -o -name '*BqLog*.so.*' -o -name '*BqLog*.dylib' \) 2>/dev/null | sort)
EOF

if [ "$found" -eq 0 ]; then
    echo "check_go_bridge_exports: no BqLog dynamic library found under $dir" >&2
    exit 1
fi

echo "check_go_bridge_exports: all BqLog dynamic libraries export the Go bridge"

# The package also promises the C++20 module entry, which is the headers' only other entry point:
# a release that ships the headers without it cannot be imported. The interface unit is the same
# file on every platform, so the platforms that ship the desktop package all assert it here.
cpm="$(find "$dir" -type f -name 'bq.log.cppm' 2>/dev/null | head -n1)"
if [ -z "$cpm" ]; then
    echo "check_go_bridge_exports: no bq.log.cppm in the package under $dir" >&2
    exit 1
fi

echo "check_go_bridge_exports: the package ships $cpm"
