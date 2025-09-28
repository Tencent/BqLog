#pragma once
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
#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/atomic/atomic.h"
#include "bq_common/platform/no_lib_cpp_impl.h"
#include "bq_common/platform/platform_misc.h"
#include "bq_common/platform/thread/thread.h"
#include "bq_common/platform/thread/mutex.h"
#include "bq_common/platform/thread/spin_lock.h"
#include "bq_common/platform/thread/condition_variable.h"
#include "bq_common/platform/io/memory_map.h"
#include "bq_common/utils/aligned_allocator.h"
#include "bq_common/utils/util.h"
#include "bq_common/utils/property.h"
#include "bq_common/utils/property_ex.h"
#include "bq_common/utils/file_manager.h"
#include "bq_common/utils/utility_types.h"
#include "bq_common/encryption/rsa.h"
#include "bq_common/encryption/aes.h"
#include "bq_common/global/vars.h"
