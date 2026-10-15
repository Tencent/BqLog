# vcpkg port for BqLog

[简体中文](./README_CHS.md)

The source of the `bqlog` port in microsoft/vcpkg.

```bash
vcpkg install bqlog
vcpkg install bqlog[tools]   # with the command line tools
```

The triplet decides static or shared. Installs the headers (including the C++20 module `bq_log/bq.log.cppm`), the library and `find_package` / pkg-config files; the `tools` feature adds `BqLog_CategoryLogGenerator` and `BqLog_LogDecoder` under `tools/bqlog/`.

```cmake
find_package(BqLog CONFIG REQUIRED)
target_link_libraries(main PRIVATE bqlog::BqLog)
```

## Releasing

The release workflow attaches `vcpkg_port_bqlog.tar.gz`, with the version and SHA512 of the new tag filled in, to the GitHub release. Copy it to `ports/bqlog/` in a fork of microsoft/vcpkg, run `vcpkg x-add-version bqlog` and open a pull request.

`bump.sh <version> [tarball]` fills in the same values locally.
