# Go wrapper

This directory is the development module
`github.com/Tencent/BqLog/wrapper/go`. Its layout and import paths are unchanged.
Release CI generates the source distribution `github.com/Tencent/BqLog/go/v2`
on the `go_dist` branch, with the same version as BqLog. See
`docs/GO_MODULE_PUBLISHING_CHS.md`. This path becomes installable after the
release workflow publishes its first version; the development module stays here.

## Native library

Build BqLog from the same source revision using `GO_SUPPORT=ON` and
`BUILD_LIB_TYPE=dynamic_lib`. CMake defines `BQ_GO` and `BQ_DYNAMIC_LIB`
for the native library. Its installed headers replace `build_type.h` with the
dynamic import variant, following the existing C++ distribution.

The current development wrapper uses cgo to link the library via `-lBqLog`.
Set `CGO_LDFLAGS=-L<library directory>` and the platform library search path.
The scripts under `build/test/go` build the native library and run Go tests.
Go 1.21 or newer and a cgo-compatible C compiler are required.

The native library must export the new `__api_go_log_write_1/2/4` symbols.
Rebuild it when upgrading this wrapper; an older BqLog DLL/so cannot satisfy
these imports even if it reports the same development version.

## Writing logs

```go
import bq "github.com/Tencent/BqLog/wrapper/go/src/bq"

log := bq.Create_log("app", config, nil)
if !log.Is_valid() {
    // Handle invalid configuration.
}
log.Info("request={} elapsed={}", bq.I64(requestID), bq.F64(elapsed))
log.Force_flush()
```

Zero arguments use the original fused write call. One to four arguments pass
their values and string pointers directly to native code, which calculates the
layout and writes the final log buffer. All paths complete begin/write/finish
within one cgo call. Native code copies strings before returning.
The shared `do_log` method is package-private (Go uses lowercase names for
unexported identifiers); it is not a public Write API. Generated category
wrappers call the exported category-level methods and work in user packages.

Larger lists use exclusively borrowed buffers from bounded size-class pools.
There is no per-logger serialization lock. Padding is initialized without
clearing bytes that the payload will overwrite. Pool cold starts, GC eviction,
and payloads above the retained size limit can allocate; stack traces also
allocate when their level is enabled. There is no unconditional zero-allocation
promise.

`log.print_stack_levels` captures Go caller frames at configured levels.
Invalid levels and out-of-range category indices return false before writing.
The zero `Arg{}` is treated as null.

Console callbacks must follow the native library's reentrancy restrictions.
Console-buffer fetching must stay on one goroutine pinned with
`runtime.LockOSThread`, as documented by `Fetch_and_remove_console_buffer`.

## Validation and benchmarks

With the matching native library available:

```sh
go test ./...
go vet ./...
go test -race ./...
GOEXPERIMENT=cgocheck2 go test ./...
```

The last command requires a Go version supporting that experiment.
Tests cover mixed direct/pooled concurrent writes and decoded output,
argument types, callback GC/stack growth, filtering, and Go stack traces.
The matching C++ workloads live in `benchmark/go`; see its README for
measurement conditions and the three UTF-16 cases the Go API cannot express.
