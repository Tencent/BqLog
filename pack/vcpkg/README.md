# vcpkg port for BqLog

[简体中文](./README_CHS.md)

The source of the `bqlog` port in microsoft/vcpkg.

```bash
vcpkg install bqlog
vcpkg install "bqlog[tools]"   # with the command line tools
```

The triplet decides static or shared. Installs the headers (including the C++20 module `bq_log/bq.log.cppm`), the library and `find_package` / pkg-config files; the `tools` feature adds `BqLog_CategoryLogGenerator` and `BqLog_LogDecoder` under `tools/bqlog/`.

This port installs the C/C++ core only: the C and C++ APIs plus the tools. The Java, C#, Node.js and Python wrappers are not included — each one needs a core built with its own language option, so they are published through their own package managers instead. See the [Integration Guide](../../docs/INTEGRATION_GUIDE.md) for the channel of each language.

```cmake
find_package(BqLog CONFIG REQUIRED)
target_link_libraries(main PRIVATE bqlog::BqLog)
```

## Releasing

The `version` and `SHA512` in `ports/bqlog/` are placeholders in the repository: they belong to a tag that does not exist yet, so an unfilled port fails to fetch instead of installing the wrong source. The release workflow fills the port in with the version and SHA512 of the new tag and publishes it, together with the Homebrew formula, as the `package_manager_files_<version>` artifact of the release workflow run on the Actions tab. Copy that one to `ports/bqlog/` in a fork of microsoft/vcpkg, run `vcpkg x-add-version bqlog` and open a pull request.

`bump.sh <version> [tarball]` fills in the same values locally.
