/* Copyright (C) 2025 Tencent.
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
//
//  bq_common_public_include.h
//  include this file when dynamic link bq_common library

//  Created by Yu Cao on 2022/8/31.
//

// Basic includes
#if !defined(BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED)
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <time.h>
#endif

// A generated category log is one text for two entries: the header, and a module unit that turns
// the class into the module's own by reading it between these markers, which it defines as
// `export {` and `}`. The include entry sees them empty. bq.log's own module entry does not use
// them: it re-exports the names it wants from a header read in the global module fragment, so a
// definition there would be owned by the module and could not be linked with the library.
// An export block takes namespace scoped declarations only, so internal linkage helpers and
// the bodies in *_impl.h stay outside it: a class carries its own members when it is exported.
#ifndef BQ_MODULE_EXPORT_BEGIN
#define BQ_MODULE_EXPORT_BEGIN
#define BQ_MODULE_EXPORT_END
#endif

#include "bq_common/misc/assert.h"
#include "bq_common/platform/macros.h"
#include "bq_common/platform/inline_intrinsics.h"
#if defined(BQ_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // some pragma is not valid for all GCC versions
#pragma GCC diagnostic ignored "-Wstringop-overflow" // GCC warning check
#pragma GCC diagnostic ignored "-Warray-bounds" // GCC warning check
#pragma GCC diagnostic ignored "-Wrestrict" // GCC error check
#pragma GCC diagnostic ignored "-Wuse-after-free" // GCC warning check
#endif
#include "bq_common/types/type_traits.h"
#include "bq_common/types/type_tools.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"
#include "bq_common/types/hash_map.h"
#include "bq_common/types/basic_types.h"
// atomic.h must stay before fast_clock.h: fast_clock.h is written in terms of the atomic templates.
#include "bq_common/platform/atomic/atomic.h"
#include "bq_common/platform/fast_clock.h"
#if defined(BQ_GCC)
#pragma GCC diagnostic pop
#endif