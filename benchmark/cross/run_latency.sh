#!/usr/bin/env bash
# Logging thread cost runner, Linux & macOS.
# Usage: ./run_latency.sh [rounds] [threads...]   (default: 7 rounds, threads 1 4)
# Runs every library, rounds interleaved so all libraries see the same machine state.
# Results are appended to $RUN_DIR/latency.csv ; take the median of the rounds.
set -e

ROUNDS=${1:-7}
shift || true
THREADS=${@:-"1 4"}
LIBS=${LIBS:-"bqlog_fast bqlog_normal quill fmtlog spdlog"}
DIR=$(cd "$(dirname "$0")" && pwd)
BIN_DIR=${BIN_DIR:-"$DIR/build"}
RUN_DIR=${RUN_DIR:-"$DIR/run"}
CSV="$RUN_DIR/latency.csv"

mkdir -p "$RUN_DIR"
[ -f "$CSV" ] || echo "round,lib,threads,mean_ns,p50_ns,p75_ns,p90_ns,p95_ns,p99_ns,p999_ns,worst_ns,consumer_cpu_pct,peak_mb" > "$CSV"

for r in $(seq 1 "$ROUNDS"); do
    for n in $THREADS; do
        for lib in $LIBS; do
            rm -rf "$RUN_DIR/output"
            mkdir -p "$RUN_DIR/output"
            # finish writing back the previous test's files first, so it does not slow this one down
            sync
            line=$(cd "$RUN_DIR" && "$BIN_DIR/bench_latency_$lib" "$n" | grep '^RESULT_LAT|')
            echo "$line" | awk -F'|' -v r="$r" 'BEGIN{OFS=","} {print r,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13}' >> "$CSV"
            echo "round $r: $line"
        done
    done
done
