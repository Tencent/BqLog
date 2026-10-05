#!/usr/bin/env bash
# Throughput runner, Linux & macOS.
# Usage: ./run_benchmark.sh <bqlog|spdlog|glog|fmtlog|quill> [from_thread] [to_thread]
# Run it several times to get several rounds; make_tables.py takes the median.
# Results are appended to run/results.csv ; log files go to run/output/.
set -e

LIB=${1:?usage: run_benchmark.sh <lib> [from] [to]}
FROM=${2:-1}
TO=${3:-10}
# THREADS="1 2 4 6 8 10" overrides the from..to range
DIR=$(cd "$(dirname "$0")" && pwd)
BIN_DIR=${BIN_DIR:-"$DIR/build"}
RUN_DIR=${RUN_DIR:-"$DIR/run"}
CSV="$RUN_DIR/results.csv"
SIZES="$RUN_DIR/filesizes.txt"

mkdir -p "$RUN_DIR/output"
[ -f "$CSV" ] || echo "lib,test,threads,ms,cpu_ms,peak_mb" > "$CSV"

for n in ${THREADS:-$(seq "$FROM" "$TO")}; do
    rm -rf "$RUN_DIR/output"
    mkdir -p "$RUN_DIR/output"

    # fmtlog's setLogFile races with its polling thread: run each test in its own process
    if [ "$LIB" = "fmtlog" ]; then
        ARG_SETS="$n mp
$n np"
    # BqLog: one logger per process, so the memory column belongs to that logger alone;
    # BqLog and quill run with fixed size blocking buffers, then with buffers that grow when full
    elif [ "$LIB" = "bqlog" ]; then
        ARG_SETS=""
        for mode in block expand; do
            for cfg in bqlog_fast_compress bqlog_fast_compress_enc bqlog_fast_text bqlog_compress bqlog_compress_enc bqlog_text; do
                ARG_SETS="$ARG_SETS$n $mode $cfg
"
            done
        done
    elif [ "$LIB" = "quill" ]; then
        ARG_SETS="$n block
$n expand"
    else
        ARG_SETS="$n"
    fi

    idx=0
    printf '%s\n' "$ARG_SETS" | while read -r args; do
        [ -n "$args" ] || continue
        idx=$((idx + 1))
        OUT="$RUN_DIR/stdout_${LIB}_${n}_${idx}.txt"
        (cd "$RUN_DIR" && "$BIN_DIR/bench_$LIB" $args) > "$OUT"
        grep '^RESULT|' "$OUT" | while IFS='|' read -r _ lib test threads ms cpu_ms peak_mb; do
            echo "$lib,$test,$threads,$ms,$cpu_ms,$peak_mb" >> "$CSV"
            echo "$LIB t=$n $test : $ms ms, cpu $cpu_ms ms"
        done
    done

    # keep the output file listing of the 1-thread run for the size comparison
    if [ "$n" -eq 1 ]; then
        find "$RUN_DIR/output" -type f -exec ls -l {} \; | awk '{print $5, $9}' | while read -r size name; do
            echo "FILE|$LIB|$(basename "$name")|$size bytes" >> "$SIZES"
        done
    fi
done
