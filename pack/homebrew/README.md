# Homebrew formula for BqLog

[简体中文](./README_CHS.md)

The source of the `bqlog` formula in homebrew-core.

```bash
brew install bqlog
```

Installs the static and shared libraries, the headers (including the C++20 module `bq_log/bq.log.cppm`), `find_package` / pkg-config files, and the `BqLog_CategoryLogGenerator` and `BqLog_LogDecoder` tools.

```cmake
find_package(BqLog REQUIRED)
target_link_libraries(app PRIVATE bqlog::BqLogShared)   # or bqlog::BqLogStatic
```

## Releasing

The release workflow attaches `bqlog.rb`, filled in with the url and sha256 of the new tag, to the GitHub release. Submit it to homebrew-core as `Formula/b/bqlog.rb`:

```bash
brew install --build-from-source ./bqlog.rb
brew test bqlog
brew audit --new --strict --online bqlog
```

`bump.sh <version> [tarball]` fills in the same values locally.
