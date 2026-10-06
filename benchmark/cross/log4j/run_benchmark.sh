#!/usr/bin/env bash
# Log4j2 benchmark runner, Linux & macOS. Needs java + javac (JDK 17+).
# Usage: ./run_benchmark.sh [from_thread] [to_thread]
set -e

FROM=${1:-1}
TO=${2:-10}
DIR=$(cd "$(dirname "$0")" && pwd)
RUN_DIR=${RUN_DIR:-"$DIR/../run"}
CSV="$RUN_DIR/results.csv"

JAVAC=javac
JAVA=java
if [ -n "$JAVA_HOME" ]; then
    JAVAC="$JAVA_HOME/bin/javac"
    JAVA="$JAVA_HOME/bin/java"
fi

mkdir -p "$RUN_DIR/output" "$DIR/classes" "$DIR/output"
[ -f "$CSV" ] || echo "lib,test,threads,ms,cpu_ms,peak_mb" > "$CSV"

(cd "$DIR" && "$JAVAC" -cp "lib/*" -d classes src/bq/benchmark/log4j/main.java)

for n in ${THREADS:-$(seq "$FROM" "$TO")}; do
    rm -rf "$DIR/output"
    mkdir -p "$DIR/output"
    OUT="$RUN_DIR/stdout_log4j2_${n}.txt"
    sync
    (cd "$DIR" && "$JAVA" -cp "classes:lib/*:." bq.benchmark.log4j.main "$n" mp) > "$OUT"
    grep '^RESULT|' "$OUT" | while IFS='|' read -r _ lib test threads ms cpu_ms peak_mb; do
        echo "$lib,$test,$threads,$ms,$cpu_ms,$peak_mb" >> "$CSV"
        echo "log4j2 t=$n $test : $ms ms, cpu $cpu_ms ms"
    done
    if [ "$n" -eq 1 ]; then
        size=$(find "$DIR/output" -type f -exec stat -c %s {} \; 2>/dev/null | awk '{s+=$1}END{print s}')
        # macOS stat fallback
        [ -z "$size" ] && size=$(find "$DIR/output" -type f -exec stat -f %z {} \; | awk '{s+=$1}END{print s}')
        echo "FILE|log4j2|log4j2.log|$size bytes" >> "$RUN_DIR/filesizes.txt"
    fi
done
