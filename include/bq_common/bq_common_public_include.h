#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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
//  bq_common_public_include.h
//  include this file when dynamic link bq_common library

//  Created by Yu Cao on 2022/8/31.
//
#ifdef __clang__
#pragma message("###################Message __clang")
#elif defined(_MSC_VER)
#pragma message("###################Message _MSC_VER")
#elif defined(__GNUC__)
#pragma message("###################Message GCC")
#endif

#if defined(BQ_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow" // GCC warning check
#pragma GCC diagnostic ignored "-Warray-bounds" // GCC warning check
#pragma GCC diagnostic ignored "-Wrestrict" // GCC error check
#pragma GCC diagnostic ignored "-Wuse-after-free" // GCC warning check
#endif

#include "bq_common/misc/assert.h"
#include "bq_common/platform/macros.h"
#include "bq_common/types/type_traits.h"
#include "bq_common/types/type_tools.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"
#include "bq_common/types/hash_map.h"
#include "bq_common/types/optional.h"
#include "bq_common/types/basic_types.h"

#if defined(BQ_GCC)
#pragma GCC diagnostic pop
#endif