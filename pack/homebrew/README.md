# Homebrew formulas for BqLog

[简体中文](./README_CHS.md)

The formulas and the bump script for the `pippocao/homebrew-bqlog` tap. The tap repository
holds copies; this directory is the reviewable source of truth, so a formula change is
reviewed together with the code it packages.

## What the formula installs

One formula, so `brew install bqlog` covers every use:

| | Contents |
|---|---|
| Libraries | `libBqLog.a` and `libBqLog.dylib` (both forms), exported as `bqlog::BqLogShared` and `bqlog::BqLogStatic` |
| Headers | `include/bq_log/` and `include/bq_common/`, including the C++20 module interface unit `bq_log/bq.log.cppm` |
| Package discovery | `BqLogConfig.cmake` and `bqlog.pc` |
| Command line tools | `BqLog_CategoryLogGenerator` and `BqLog_LogDecoder`, in `bin/` |

The tools compile the library sources into their executables, which costs a few seconds inside
this formula — far less than a second formula that would rebuild the whole library for two small
commands.

It builds from source; no bottle is published, so `brew install` compiles the library.

## Platforms

macOS (Apple silicon and Intel) and Linux (glibc, which includes the Ubuntu used by WSL2).

iOS, Android, HarmonyOS, native Windows and the BSD/Solaris family are not covered. Those
keep using the prebuilt archives on the release page or their own package manager.

## Creating the tap

```bash
brew tap-new pippocao/bqlog
cp -r pack/homebrew/Formula "$(brew --repository)/Library/Taps/pippocao/homebrew-bqlog/"
```

Then commit and push the tap repository.

## Installing

```bash
brew tap pippocao/bqlog
brew install bqlog
```

## Updating the formulas for a new release

1. Publish the release as usual; it creates the tag `Release_<version>`.
2. Point the formulas at it and refresh the checksum:

```bash
bash pack/homebrew/bump.sh 2.7.0
```

3. Run the acceptance checks below, copy `Formula/` into the tap and push.

`bump.sh` downloads the tag archive to compute its checksum, so run it after the release is
published.

## Release order

The tag anchors everything: the tarball checksum and the `brew test-bot` run are both taken
from it, so the order matters.

1. Tag the commit that will reach the main branch, and push the tag. **Do not merge main yet.**
2. Point the formulas at that tag with `bump.sh`, then verify the tap as described below.
3. Merge the branch into main. Prefer a fast-forward: a merge commit would leave the release
   tag on a commit that is not the head of main.
4. Run `brew test-bot` to rehearse the official CI, bottle build included.

Two rules follow from this:

- A pushed tag is fixed. If verification turns up a code change, ship the next patch release
  rather than moving the tag.
- `release.yml` silently skips creating a tag that already exists, so never run the release
  workflow against a tag you intend to abandon.

## Acceptance checks

```bash
brew install --build-from-source ./Formula/bqlog.rb
brew audit --strict --online --new bqlog
brew test bqlog
```

`brew install --build-from-source` is the path a user takes. `brew audit` is what
`homebrew-core` runs, so passing it here is what makes the formula portable to the main
repository later.

## Towards homebrew-core

`brew audit --strict --online --new bqlog` applies the same checks as `homebrew-core`, and
`brew test-bot` rehearses their full CI including the bottle builds. Together they answer
"would this be accepted" without opening a pull request.

Before proposing the formula upstream:

- the release tag has to exist in this repository; their CI builds from it and never from a
  fork;
- `livecheck` has to resolve, which is why the formula overrides the default: the tag is
  `Release_<version>` rather than the `v<version>` Homebrew assumes. Without the override the
  audit fails and the autobump bot never notices a new release;
- the formula carries the command line tools as well as the library, so one install serves both
  kinds of user. They add a few seconds of compilation rather than a second formula, which would
  rebuild the whole library for two small commands.

A merged formula cannot be withdrawn the way a tap can — removal means deprecating it and then
disabling it. Verify with `brew audit` and `brew test-bot`, never with a pull request.

## Consuming the installed library

```cmake
find_package(BqLog REQUIRED)
target_link_libraries(app PRIVATE bqlog::BqLogShared)   # or bqlog::BqLogStatic
```

```bash
pkg-config --cflags --libs bqlog
```

## C++20 modules

The module interface unit ships with the headers and there is no prebuilt BMI: it is tied to
the compiler, the standard and the macros, so the consumer compiles it once.

```cpp
import bq.log;
```

```cmake
find_package(BqLog REQUIRED)

target_sources(app PRIVATE FILE_SET CXX_MODULES
    BASE_DIRS ${BQLOG_INCLUDE_DIR}
    FILES ${BQLOG_MODULE_FILE})
target_link_libraries(app PRIVATE bqlog::BqLogShared)
```

This needs CMake 3.28 or newer and a toolchain that implements C++20 modules and whose module
dependency scanning works (`clang-scan-deps`). That is a hard requirement: a compiler that
accepts `-std=c++20` but cannot parse `export module` fails on the interface unit instead of
skipping it. The include entry has no such requirement — the rest of the library builds with any
supported compiler.

Two constraints, from [the integration guide](../../docs/INTEGRATION_GUIDE.md):

- `BQ_LOG_FAST_*` are macros and cannot be imported; in module mode the fast log is written
  as `log.fast_info<"...">(args...)`.
- The module must be compiled with the same compiler, standard and macros as the rest of the
  project.
