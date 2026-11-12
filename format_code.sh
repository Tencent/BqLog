#!/bin/bash

ROOT_DIR="$(dirname "$0")"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

# Unknown keys are downgraded to a warning on purpose: the config names both SpaceInEmptyBraces
# (clang-format 22 and later) and SpaceInEmptyBlock (18 to 21), and every version knows only one of
# them. Refusing to run on the other one would make the script depend on which version is installed.
"$CLANG_FORMAT" --version >/dev/null 2>&1 || {
    echo "cannot run $CLANG_FORMAT; set CLANG_FORMAT to a clang-format binary" >&2
    exit 1
}

DIRS=("src" "include" "demo" "benchmark" "test" "tools")

export CLANG_FORMAT

for dir in "${DIRS[@]}"; do
    find "$ROOT_DIR/$dir" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.cppm" \) -exec sh -c '
        for file; do
            echo "Formatting $file"
            "$CLANG_FORMAT" -Wno-error=unknown -i "$file"
        done
    ' sh {} +
done

echo "Done!"
