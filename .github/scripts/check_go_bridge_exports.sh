#!/usr/bin/env bash
# Asserts that every desktop prebuilt dynamic library exports the Go bridge entry points.
#
# The bridge is only compiled when the build drivers receive go=YES, and a parameter in the wrong
# position drops it from the release silently: assert it in every shipped dynamic library, the
# Node addon included.
#
# Usage: bash check_go_bridge_exports.sh [dir]     (default: dist)
set -eu

dir="${1:-dist}"
symbols="__api_go_log_write __api_go_log_write_1 __api_go_log_write_2 __api_go_log_write_4"

os="$(uname -s)"
dumpbin_path="$(command -v dumpbin || true)"
case "$os" in
    MINGW*|MSYS*|CYGWIN*)
        vswhere='/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
        if [ -z "$dumpbin_path" ] && [ -x "$vswhere" ]; then
            dumpbin_path="$("$vswhere" -latest -prerelease -products '*' -find 'VC/Tools/MSVC/*/bin/**/dumpbin.exe' | tr -d '\r' | head -n1)"
            if [ -n "$dumpbin_path" ]; then
                dumpbin_path="$(cygpath -u "$dumpbin_path")"
            fi
        fi
        ;;
esac

has_symbol() {
    lib="$1"
    sym="$2"
    case "$os" in
        Darwin)
            # Both slices of each universal library must define the symbol.
            for arch in arm64 x86_64; do
                nm -arch "$arch" -gU "$lib" 2>/dev/null | grep -Eq "[[:space:]]_${sym}$" || return 1
            done
            return 0
            ;;
        MINGW*|MSYS*|CYGWIN*)
            if [ -n "$dumpbin_path" ]; then
                MSYS_NO_PATHCONV=1 "$dumpbin_path" /nologo /exports "$lib" 2>/dev/null | grep -Eq "[[:space:]]${sym}([[:space:]]|$)" && return 0
            fi
            if command -v llvm-readobj >/dev/null 2>&1; then
                llvm-readobj --coff-exports "$lib" 2>/dev/null | grep -Eq "[[:space:]]Name: ${sym}$" && return 0
            fi
            if command -v objdump >/dev/null 2>&1; then
                objdump -p "$lib" 2>/dev/null | sed -n '/\[Ordinal\/Name Pointer\] Table/,/^$/p' | grep -qw "$sym" && return 0
            fi
            ;;
        *)
            if command -v nm >/dev/null 2>&1; then
                nm -D --defined-only "$lib" 2>/dev/null | grep -Eq "[[:space:]]${sym}$" && return 0
            fi
            if command -v objdump >/dev/null 2>&1; then
                objdump -T "$lib" 2>/dev/null | sed '/\*UND\*/d' | grep -Eq "[[:space:]]${sym}$" && return 0
            fi
            ;;
    esac
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
$(find "$dir" -type f \( -name '*BqLog*.dll' -o -name '*BqLog*.so' -o -name '*BqLog*.so.*' -o -name '*BqLog*.dylib' -o -name '*BqLog*.node' \) 2>/dev/null | sort)
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
