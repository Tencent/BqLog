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
//  bq_common.h
//  include this file when static link bq_common library
//  Created by pippocao on 2022/8/31.
//

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif
#define BQ_SRC

#include "native_include_bq_common_bq_common_public_include.h"
#include "native_src_bq_common_platform_simd.h"
#include "native_src_bq_common_platform_atomic_atomic.h"
#include "native_src_bq_common_platform_no_lib_cpp_impl.h"
#include "native_src_bq_common_utils_aligned_allocator.h"
#include "native_src_bq_common_utils_utility_types.h"
#include "native_src_bq_common_platform_platform_misc.h"
#include "native_src_bq_common_platform_thread_thread.h"
#include "native_src_bq_common_platform_thread_spin_lock.h"
#include "native_src_bq_common_platform_thread_mutex.h"
#include "native_src_bq_common_platform_thread_condition_variable.h"
#include "native_src_bq_common_utils_util.h"
#include "native_src_bq_common_utils_property.h"
#include "native_src_bq_common_utils_property_ex.h"
#include "native_src_bq_common_utils_file_manager.h"
#include "native_src_bq_common_platform_io_memory_map.h"
#include "native_src_bq_common_encryption_rsa.h"
#include "native_src_bq_common_encryption_aes.h"
#include "native_src_bq_common_encryption_vernam.h"
#include "native_src_bq_common_global_common_vars.h"
