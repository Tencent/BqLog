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
// The C++20 module entry of BqLog:
//
//     import bq.log;
//
// is the same library as
//
//     #include "bq_log/bq_log.h"
//
// The headers are compiled inside this module, so their contents have to say which
// declarations are part of the module interface: BQ_MODULE_EXPORT_BEGIN/END mark
// them, and are empty unless a module is being built. Everything the headers do not
// mark -- file local helpers, the system headers they pull in -- stays out of the
// interface, which is what makes a header set like ours compilable at all: a
// declaration with internal linkage cannot be exported, and neither the C library
// nor the platform SDK headers are free of them.
//
// System headers come first, in the global module fragment, so that the includes
// inside the marked regions are no-ops and their declarations are not attached to
// this module.
//
// What the module cannot hand out is macros: BQ_LOG_FAST_* belongs to the
// preprocessor and macros are not exported. Importers use the macro-free fast mode
// instead, log.fast_info<"...">(args...), which needs C++20 and is therefore always
// available here.
module;

// Every system header the public headers pull in, so that the includes inside the
// marked regions are no-ops and the C library is not attached to this module.
#include <assert.h>
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
#if defined(_WIN32)
#include <intrin.h>
#else
#include <dlfcn.h>
#endif

export module bq.log;

#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }

#include "bq_log/bq_log.h"

#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
