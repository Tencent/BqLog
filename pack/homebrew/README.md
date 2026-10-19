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

The `url` and `sha256` in `Formula/bqlog.rb` are placeholders in the repository: they belong to a tag that does not exist yet, so an unfilled formula fails to fetch instead of installing the wrong source. The release workflow fills the file in with the url and sha256 of the new tag and publishes it, together with the vcpkg port, as the `package_manager_files_<version>` artifact of the release workflow run on the Actions tab. Download that one and submit it to homebrew-core as `Formula/b/bqlog.rb`.

Homebrew refuses to install a formula by path (`brew install ./bqlog.rb` fails with "Homebrew requires formulae to be in a tap"), so the file has to sit inside a local checkout of homebrew-core — the one `brew tap --force homebrew/core` writes to — before it can be checked:

```bash
export HOMEBREW_NO_INSTALL_FROM_API=1
brew install --build-from-source bqlog
brew test bqlog
brew audit --new --strict --online bqlog
brew style bqlog
```

`bump.sh <version> [tarball]` fills in the same values locally.
