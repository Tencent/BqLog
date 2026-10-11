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
// The same library as #include "bq_log/bq_log.h", reached through `import bq.log;`. The
// headers say with BQ_MODULE_EXPORT_BEGIN/END which declarations make up the interface,
// and the system headers come first so that the includes inside those regions are no-ops
// and the C library is not attached to the module. Macros are never exported, so the
// fast log here is the macro-free log.fast_info.
module;

// macros.h supplies BQ_WIN below. assert.h has no include guard, so it has to be
// pre-included here, otherwise the purview includes it again (MSVC reports C5244).
#include "bq_common/platform/macros.h"
#include "bq_common/misc/assert.h"

// Every system header the public headers pull in, so that the includes inside the
// marked regions are no-ops and the C library is not attached to this module.
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
#if defined(BQ_WIN)
#include <intrin.h>
#else
#include <dlfcn.h>
#endif

// <assert.h> has no include guard: it redefines assert from the NDEBUG of the point of
// inclusion. Pre-including the system header does not help, because the purview would
// still include it again; pre-include the wrapper below instead, whose own #pragma once
// makes that second inclusion empty.
#include "bq_common/misc/assert.h"

export module bq.log;

#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }

#include "bq_log/bq_log.h"

#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
