#!/bin/bash

ROOT_DIR="$(dirname "$0")"

DIRS=("src" "include" "demo" "benchmark" "test" "tools")

for dir in "${DIRS[@]}"; do
    find "$ROOT_DIR/$dir" -type f \( -name "*.cpp" -o -name "*.h" \) -exec sh -c '
        for file; do
            echo "Formatting $file"
            '"clang-format"' -i "$file"
        done
    ' sh {} +
done

echo "Done!"