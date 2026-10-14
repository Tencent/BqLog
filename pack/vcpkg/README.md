# vcpkg port for BqLog

[简体中文](./README_CHS.md)

An overlay port, so vcpkg users can install BqLog from a checkout, and so the port can be
verified here before it is submitted to `microsoft/vcpkg`.

```
pack/vcpkg/ports/bqlog/
├── portfile.cmake
└── vcpkg.json
```

## Verifying with the overlay

```bash
vcpkg install bqlog --overlay-ports=pack/vcpkg/ports
```

The `SHA512` in `portfile.cmake` covers the `Release_<version>` tag archive, so it has to be
refreshed whenever `vcpkg.json` moves to a new version. A wrong value is not silent: vcpkg
stops and prints the checksum it computed, which is the value to paste in.

Verified on macOS with `arm64-osx` (static) and `arm64-osx-dynamic`: both install a plain
`lib/libBqLog.a` or `lib/libBqLog.dylib` rather than a framework, and a consumer built through
the vcpkg toolchain finds the package and runs.

Keep the `version` in `vcpkg.json` on the release this directory ships: the portfile builds
its `REF` from it.

## Static or shared

vcpkg decides the library form through the triplet (`x64-windows` is dynamic,
`x64-windows-static` is static) and the port passes the matching `BUILD_LIB_TYPE`. There is
no way to install both forms from one port.

`TARGET_PLATFORM` is not passed: the build detects it from the CMake built-ins.

On Apple the port also passes `APPLE_LIB_FORMAT` (`a` or `dylib`) to match the triplet. Without
it a single-form build produces `BqLog.framework`, which neither vcpkg nor the pkg-config file
it rewrites can consume.

## What the port installs

The headers (including the C++20 module interface unit `bq_log/bq.log.cppm`), the library, and
the package discovery files: `BqLogConfig.cmake` for `find_package(BqLog)` (vcpkg moves the
CMake files to `share/bqlog`, and the pkg-config module stays in `lib/pkgconfig`) and
`bqlog.pc`. The command line tools are not part of the port.

A vcpkg build has exactly one library form, so the exported target is `bqlog::BqLog`. The
Homebrew package, which ships both forms at once, exposes `bqlog::BqLogShared` and
`bqlog::BqLogStatic` instead.

## Submitting to vcpkg

1. Verify the overlay install above, on at least `x64-windows` and `x64-linux`.
2. Fork `microsoft/vcpkg` and add `ports/bqlog/{portfile.cmake,vcpkg.json}`.
3. Run `vcpkg x-add-version bqlog` to create the version database entry.
4. Open a PR. Their CI builds the port on every triplet and their reviewers own the merge.
