/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
// The system headers a C++20 module entry of bq_common preincludes, and the switch that tells the
// library headers those includes are already done. bq_log has its own copy of this file, which
// forwards to this one.
//
// Include this only from the global module fragment of a module entry: after `module;` and before
// `export module`. The directives then belong to the global module and stay outside the module
// purview (clang -Winclude-angled-in-module-purview, MSVC C5244).

#define BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
// <assert.h> has no include guard, so it is preincluded here once instead of from inside the
// purview every time bq_common/misc/assert.h is reached.
#include <assert.h>

// macros.h probes the platform inside this fragment and defines BQ_WIN/BQ_APPLE/BQ_MSVC; the
// headers below need those macros, so it comes first. assert.h is listed because <assert.h> has no
// include guard and would otherwise be included again inside the purview.
#include "native_include_bq_common_platform_macros.h"
#include "native_include_bq_common_misc_assert.h"

// The system headers the platform specific parts of the library need.
#if defined(BQ_WIN)
#include <intrin.h>
#else
#include <dlfcn.h>
#endif
#if defined(BQ_APPLE)
#include <mach/mach_time.h>
#endif
