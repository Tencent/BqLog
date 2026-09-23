# BqLog Go 2.4.1

Generated from Tencent/BqLog commit 58bb289358536ecdf6bb24872c034c256792aed1. Do not edit generated sources.

Install: `go get github.com/Tencent/BqLog/go/v2@v2.4.1`

Import: `import bq "github.com/Tencent/BqLog/go/v2"`

Requires Go 1.21+, CGO_ENABLED=1, and a C/C++17 compiler. Native source compiles automatically; no prebuilt BqLog DLL/so or CMake step is needed. The native objects link into the Go program. Desktop Windows, Linux, macOS and FreeBSD are supported.

Create a logger with `bq.Create_log(name, config, nil)`, call `log.Info(format, bq.Str(value))`, and flush before exit with `log.Force_flush()`.
