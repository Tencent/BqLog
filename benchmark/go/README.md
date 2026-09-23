# Go benchmark matching the C++ workloads

This benchmark mirrors the ten workloads in `benchmark/cpp/main.cpp` that
the current Go public API can express. It uses the actual Go wrapper.
It does not replace the wrapper with the experimental native fast path.

Each writer produces **2,000,000 entries**. Timing includes writer creation,
writing, joining, and flushing all logs. The appenders remain enabled and write
real compressed, encrypted compressed, and text files. Configurations, the RSA
public key, template warmups, hot-window sequence, parameter widths, and UTF-8
character positions match C++. `go test` checks the configs and all 2,048
multi-format strings against the C++ source.

The three UTF-16 input workloads are omitted because the Go API only accepts
UTF-8 strings. The Go writers are goroutines; C++ writers are OS threads.
C++ literal formats and Go string formats also retain their language-specific
implementation paths. These are not identical machine-code workloads.

## Build and run

Build the native library with the existing CMake configuration:
`BUILD_LIB_TYPE=dynamic_lib` and `GO_SUPPORT=ON`. Use the matching library
for the C++ comparison too. The existing C++ benchmark CMake project defaults
to static linking, so do not silently compare different native builds.

From this directory, on Windows (with Go and GCC on PATH):

```powershell
$env:CGO_ENABLED = '1'
$env:CGO_LDFLAGS = '-LE:/BqLog/artifacts/dynamic_lib/lib/Release'
$env:PATH = 'E:\BqLog\artifacts\dynamic_lib\lib\Release;' + $env:PATH
go test ./...
go build -o E:\BqLog\artifacts\go_benchmark.exe .
```

Run the executable in a new output directory:

```powershell
E:\BqLog\artifacts\go_benchmark.exe -threads 8
```

The default is one writer. `-case compressed_4` selects a single workload.
`BENCH_POOL_SIZE` has the same meaning and default (50,000) as in C++.
Normal Go GC and scheduler settings are preserved unless the caller sets them.
Each run reports the runtime version, GOMAXPROCS, native version, and failures.

On Linux/macOS, use the corresponding `CGO_LDFLAGS=-L...` and dynamic library
search path for the native library, then the same `go test` / `go build` steps.

Output goes into `benchmark_output/` relative to the working directory.
Use a fresh directory per measured run: the inherited `capacity_limit=1`
configuration can remove previous files on appender initialization. A full
eight-writer run produces several GB. Keep benchmark output outside the source
tree.

The Windows comparison runner is
`build/benchmark/go/run_comparison.ps1`. It accepts built C++/Go executables,
a library directory, and a new output directory. It runs the two programs
sequentially, alternates their order, repeats each writer count three times,
and saves every sample to CSV. Use the paired C++ entry point from the result
artifacts when comparing the same ten cases.
Pass `-BaselineGoExecutable <previous Go benchmark>` to include a third,
unchanged Go binary in the same run when measuring wrapper optimizations.
