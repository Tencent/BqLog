# fast path instruction count

Guards the instruction count of the fast-path log call sites. A probe function
per public macro (`BQ_LOG_FAST_INFO` via a global `bq::log` and via a `const
bq::log&`) is compiled to assembly at `-O2`, and the instructions from the
function label to the first `ret` are counted. The number must not grow between
commits without an explicit decision: the fast path is the reason the macro
exists.

## Usage

```
python count.py --std=c++20 --extra="-mcmodel=small"
```

- `--cxx` compiler (default `g++`; MSVC `cl` support is sketched in the
  docstring but the GCC parsing path is the verified one)
- `--std` standard flag, passed through verbatim (`c++11` ... `c++23`)
- `--extra` extra compiler flags, space separated inside one quoted argument
  (must use `=` form)

Baseline on GCC 15.2.0 (MinGW-w64, x86-64, `-mcmodel=small`, `-O2`,
`-std=c++20`) with 64-bit layout enabled:

```
_Z15probe_macro_refRKN2bq3logEif 73
_Z18probe_macro_globalif 70
```

The same numbers were confirmed unchanged with the T01 site race fix
(`site_init_guard` + acquire/release publication of `level_word`): the extra
work only lives in the cold init path (`init_fast_log_site` and the slow paths),
not in the inline fast path.

T08 adds `probe_fast_global` / `probe_fast_ref` under `BQ_CPP_20` in `probe.cpp`
for the macro-free `log.fast_info<"...">` interface. Baseline on the same
compiler and flags:

```
_Z15probe_macro_refRKN2bq3logEif 73
_Z18probe_macro_globalif 70
_Z14probe_fast_refRKN2bq3logEif  74
_Z17probe_fast_globalif          71
```

The macro numbers must not change (they are the reason the macro exists). The
`fast_info` entries are allowed to be one instruction above the matching macro:
`fast_info` shares one site across every log, so at the call site it must read
the registration flag from the site *and* the per-log level word, while the
macro's site already caches the level word pointer of its one bound log. The
gate is therefore `probe_fast_* <= probe_macro_* + 1`, with the benchmark
latency no worse than the macro.
