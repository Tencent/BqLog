/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
// The module entry of the library: import bq.log; says what #include "bq_log/bq_log.h" says.
module;

// The system headers the headers below pull in, so that the includes inside them are no-ops.
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

// Completing the list above is what lets the headers skip their own system includes, which is
// the only way to keep those directives out of the purview (clang -Winclude-angled-in-module-purview).
#define BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED

// macros.h supplies BQ_WIN below; assert.h brings in <assert.h>, which has no include
// guard, so it would otherwise be included inside the purview (MSVC C5244).
#include "bq_common/platform/macros.h"
#include "bq_common/misc/assert.h"

// The one system header that needs the platform macro from above.
#if defined(BQ_WIN)
#include <intrin.h>
#else
#include <dlfcn.h>
#endif

export module bq.log;

// The headers mark their interface with these; the include entry sees them empty.
#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }
#include "bq_log/bq_log.h"
#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
