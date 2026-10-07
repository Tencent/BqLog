# BqLog for Go 2.6.0

**GitHub: [https://github.com/Tencent/BqLog](https://github.com/Tencent/BqLog)**

Go bindings for BqLog, Tencent's lightweight, high-performance logging system.

[Integration Guide](https://github.com/Tencent/BqLog/blob/52d087f330881e350fc005e9501387ecb188df4e/docs/INTEGRATION_GUIDE.md#go) | [简体中文](https://github.com/Tencent/BqLog/blob/52d087f330881e350fc005e9501387ecb188df4e/docs/INTEGRATION_GUIDE_CHS.md#go)

## Quick Start

Install through Go Modules:

```sh
# Skip initialization if the project already has go.mod
go mod init example.com/myapp
go get github.com/Tencent/BqLog/go/v2@latest
```

Save the following as `main.go`:

```go
package main

import bq "github.com/Tencent/BqLog/go/v2"

func main() {
    config := `
appenders_config.console.type=console
appenders_config.console.levels=[all]
`
    log := bq.Create_log("go_log", config, nil)
    if !log.Is_valid() {
        panic("invalid log config")
    }
    defer log.Force_flush()

    log.Info("Hello from Go! params: {}, {}", "text", 123)
}
```

Run the example:

```sh
go run .
```

Logging methods accept zero or any number of arguments. Objects support Go's
String/Error methods, and nil is logged as null.

### Packages and versions

The module follows the BqLog version. Go Modules downloads the source, and
`go build` compiles the native sources automatically.

[pkg.go.dev](https://pkg.go.dev/github.com/Tencent/BqLog/go/v2) provides package
search, README and API documentation.
