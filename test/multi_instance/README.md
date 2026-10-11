# Multi-instance regression suite

Automated regression for multiple BqLog processes sharing a log name and cwd:
instance assignment, per-instance storage isolation, recovery, cross-runtime
coexistence (2.6.0) and dual-runtime plugins. Driver is `run.py` (Python 3
standard library only); the worker is a small C++ program linking BqLog.

## Build

```bash
cmake -S test/multi_instance -B build_mi -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_mi -j
```

`CMakeLists.txt` builds `bq_multi_instance_worker` (the main worker) and, when
`-DBQ_LEGACY_SRC=<path to a 2.6.0 checkout>` is given, the legacy worker
`bq_multi_instance_worker_legacy` built from that checkout (used by case M6).
It also always builds the dual-runtime host `bq_dual_runtime_host` with two
statically-self-contained plugins `plugin_a` / `plugin_b` (case M7). On MinGW
all test binaries statically link the C++ runtime so they never load a
mismatched `libstdc++-6.dll` from PATH.

The BqLog library is compiled with `BQ_MULTI_INSTANCE_TEST` to expose the
test-only C API `__api_test_get_storage_report` (see
`include/bq_log/misc/bq_log_c_api.inc`).

## Run

```bash
python test/multi_instance/run.py --worker build_mi/bq_multi_instance_worker \
    --out build_mi/mi_out [--case M4] [--records 256]
```

- Without `--case` all cases M1..M9 run; each case gets its own directory
  under `--out/<case>/` as the cwd of every child (so `bqlog_mmap/` trees are
  isolated); `--out` is cleaned before the run.
- Per case a `result.json`, per worker stdout captures and the log outputs are
  kept; a `summary.json` lands at the top level.
- Failure messages name the worker and the missing/duplicated record.

## Cases

| case | scenario | asserts |
|---|---|---|
| M1 | 2/4/16 same-name workers in one cwd (`--report`) | instances are exactly {0..N-1}; lp and hp mmap mapped for every worker; every output file holds exactly its own records in seq order |
| M2 | one worker `--two-logs`: a second log is created and writes alternate between the two, using the **same format strings** | each log's output holds only its own records, counts and order correct |
| M3 | A: recovery on, holds; B: same name `--recovery off`, runs to completion while A holds | `bqlog_mmap/p0/` unchanged during B; no `p1/` created (B must not claim an instance) |
| M4 | A `--die-after 500` (write committed json, self-kill, no flush); C: same name joins | C takes instance 0; every committed record of A survives its death — each appears at least once across A's output and C's output (recovery is at-least-once: no loss, duplicates accepted); nothing beyond committed_seq; C writes its own records completely |
| M5 | A0, A1 concurrently on instances 0/1, both `--die-after 300`; then C, then C+D | C takes instance 0, D instance 1; `bqlog_mmap/p1/` untouched while C holds; each dead worker's committed records survive at least once (at-least-once recovery, duplicates accepted), nothing beyond committed_seq |
| M6 | legacy 2.6.0 worker running, new worker joins (needs `-DBQ_LEGACY_SRC`) | new worker lp mapped; legacy output complete; `bqlog_mmap/mmap_<name>/` survives while the legacy worker holds it; removed after all legacy workers exit. SKIP if the legacy worker is not built |
| M7 | host loads two differently named copies of a plugin, each statically linking its own BqLog runtime, both create the same recovery log | the two runtimes get different instances (0 and 1). SKIP if the host is not built |
| M8 | `--child-lock-test`: worker creates a recovery log, spawns a sleeping child, exits normally; a new worker E starts immediately | E gets instance 0 (the child must not inherit the instance lock handle) |
| M9 | D `--die-after N --die-flush` (flush everything, then self-kill); E joins the same name | every committed record of D appears **exactly once** across D's and E's outputs (the flush means recovery must not replay anything), nothing beyond committed_seq |

## Worker CLI

```
bq_multi_instance_worker <worker_id> <log_name> <records> [options]
  --recovery on|off          default on
  --die-after N              after N records: write committed_<id>.json, then self-kill
  --die-flush                with --die-after: flush before the kill, so recovery must deliver
                             each committed record exactly once (M9). Without it the records
                             stay in the buffer/appender cache and recovery is at-least-once
                             (M4/M5)
  --two-logs                 also create <log_name>_b, alternate writes between the two
  --report                   write instance/lp_mapped/hp_mapped into ready_<id>.json
  --child-lock-test <path>   hold the log, spawn a sleeping child of itself, exit (M8)
  --sleep N                  do nothing for N seconds (M8's child)
  --hold-until <file>        after warmup, pause (instance stays claimed) until
                             <file> is deleted, then continue (M1/M3/M5 barriers)
```
