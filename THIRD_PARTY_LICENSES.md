# Third-party licenses

This branch contains only BqLog's own benchmark code, scripts and measured data. **No source code or binaries of the libraries below are included**: CMake (FetchContent) downloads the C++ libraries from their official repositories at build time, and `log4j/fetch_deps.sh` / `fetch_deps.ps1` download the Java libraries from Maven Central. Each library is used unmodified, only to measure it, under its own license:

| Library | Version | License | Copyright | Source |
|---|---|---|---|---|
| quill | v13.0.0 | MIT | Copyright (c) 2020 - present, Odysseas Georgoudis | https://github.com/odygrd/quill |
| spdlog | v1.17.0 | MIT | Copyright (c) 2016 - present, Gabi Melman and spdlog contributors | https://github.com/gabime/spdlog |
| fmtlog | v2.3.0 | MIT | Copyright (c) 2021 Meng Rao | https://github.com/MengRao/fmtlog |
| {fmt} (bundled with fmtlog) | as bundled | MIT | Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors | https://github.com/fmtlib/fmt |
| glog | v0.7.1 | BSD 3-Clause | Copyright (c) 2024, Google Inc. | https://github.com/google/glog |
| Apache Log4j 2 (log4j-api, log4j-core) | 2.26.0 | Apache License 2.0 | Copyright 1999-2026 The Apache Software Foundation | https://logging.apache.org/log4j/2.x/ |
| LMAX Disruptor | 4.0.0 | Apache License 2.0 | Copyright 2011 LMAX Ltd. | https://github.com/LMAX-Exchange/disruptor |

The full license text of each library ships with its source (`LICENSE`, `LICENCE.txt` or `COPYING` in the repository root; `META-INF/LICENSE` and `META-INF/NOTICE` in the Log4j jars). Anyone who redistributes a build of this benchmark must keep those notices, as the licenses above require.

The product names above are trademarks of their respective owners. They are named here only to identify the libraries that were measured; no endorsement is implied. The results describe these specific versions and configurations on the stated hardware; see [README.md](README.md) for the exact setup.

BqLog itself, including this benchmark code, is licensed under the [Apache License 2.0](https://github.com/Tencent/BqLog/blob/main/LICENSE.txt).
