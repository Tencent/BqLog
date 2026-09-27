#!/usr/bin/env bash
# Peak memory (RSS) runner, Linux & macOS.
# Usage: ./measure_memory.sh <bqlog|spdlog|glog|fmtlog|quill> [threads...]   (default: 1 4 10)
# Results are appended to run/memory.csv .
set -e

LIB=${1:?usage: measure_memory.sh <lib> [threads...]}
shift || true
THREADS=${@:-"1 4 10"}
DIR=$(cd "$(dirname "$0")" && pwd)
BIN_DIR=${BIN_DIR:-"$DIR/build"}
RUN_DIR="$DIR/run"
CSV="$RUN_DIR/memory.csv"

mkdir -p "$RUN_DIR/output"
[ -f "$CSV" ] || echo "lib,threads,peak_mb" > "$CSV"

peak_rss_kb() {
    # Linux: /proc VmHWM (high-water mark); macOS: poll ps rss and keep the max
    local pid=$1 cur=0
    if [ -r "/proc/$pid/status" ]; then
        cur=$(awk '/VmHWM/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0)
    else
        cur=$(ps -o rss= -p "$pid" 2>/dev/null | tr -d ' ' || echo 0)
    fi
    echo "${cur:-0}"
}

run_once() {
    local args="$1"
    rm -rf "$RUN_DIR/output"; mkdir -p "$RUN_DIR/output"
    (cd "$RUN_DIR" && "$BIN_DIR/bench_$LIB" $args) > /dev/null &
    local pid=$!
    local peak=0 cur
    while kill -0 "$pid" 2>/dev/null; do
        cur=$(peak_rss_kb "$pid")
        [ "$cur" -gt "$peak" ] && peak=$cur
        sleep 0.05
    done
    wait "$pid" || true
    echo "$peak"
}

for n in $THREADS; do
    if [ "$LIB" = "fmtlog" ]; then
        p1=$(run_once "$n mp")
        p2=$(run_once "$n np")
        [ "$p2" -gt "$p1" ] && p1=$p2
        peak=$p1
    else
        peak=$(run_once "$n")
    fi
    mb=$(awk "BEGIN{printf \"%.1f\", $peak/1024}")
    echo "$LIB,$n,$mb" >> "$CSV"
    echo "$LIB t=$n peak $mb MB"
done
