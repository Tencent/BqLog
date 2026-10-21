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

// Declarations between these two markers make up the interface of the C++20 module
// (see include/bq_log/bq.log.cppm). They are empty unless a module is being built,
// which defines them as `export {` and `}` before including any header; the include
// entry sees exactly the same declarations with the same linkage.
// An export block takes namespace scoped declarations only, so internal linkage helpers and
// the bodies in *_impl.h stay outside it: a class carries its own members when it is exported.
#ifndef BQ_MODULE_EXPORT_BEGIN
#define BQ_MODULE_EXPORT_BEGIN
#define BQ_MODULE_EXPORT_END
#endif

#include "native_include_bq_common_misc_assert.h"
#include "native_include_bq_common_platform_macros.h"
#include "native_include_bq_common_platform_inline_intrinsics.h"
#if defined(BQ_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // some pragma is not valid for all GCC versions
#pragma GCC diagnostic ignored "-Wstringop-overflow" // GCC warning check
#pragma GCC diagnostic ignored "-Warray-bounds" // GCC warning check
#pragma GCC diagnostic ignored "-Wrestrict" // GCC error check
#pragma GCC diagnostic ignored "-Wuse-after-free" // GCC warning check
#endif
#include "native_include_bq_common_types_type_traits.h"
#include "native_include_bq_common_types_type_tools.h"
#include "native_include_bq_common_types_array.h"
#include "native_include_bq_common_types_string.h"
#include "native_include_bq_common_types_hash_map.h"
#include "native_include_bq_common_types_basic_types.h"
// atomic.h must stay before fast_clock.h: fast_clock.h is written in terms of the atomic templates.
#include "native_include_bq_common_platform_atomic_atomic.h"
#include "native_include_bq_common_platform_fast_clock.h"
#if defined(BQ_GCC)
#pragma GCC diagnostic pop
#endif