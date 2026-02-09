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
/*!
 * \file hash_map.h
 * substitute of STL unordered_map, we exclude STL and libc++ to reduce the final executable and library file size
 * IMPORTANT: It is not thread-safe!!!
 *
 * Two types of array is supported:
 * 1. bq::hash_map
 * 2. bq::hash_map_inline   (every function is force inlined, best performance but more program size).
 *
 * \author pippocao
 * \date 2022/09/10
 *
 */

#define BQ_HASH_MAP_INLINE inline
#define BQ_HASH_MAP_INLINE_MACRO(X) X

#define BQ_HASH_MAP_CLS_NAME BQ_HASH_MAP_INLINE_MACRO(hash_map)
#define BQ_HASH_MAP_KV_CLS_NAME BQ_HASH_MAP_INLINE_MACRO(kv_pair)
#define BQ_HASH_MAP_ITER_CLS_NAME BQ_HASH_MAP_INLINE_MACRO(hash_map_iterator)

#include "bq_common/types/hash_map_impl.h"

#undef BQ_HASH_MAP_INLINE
#undef BQ_HASH_MAP_INLINE_MACRO
#define BQ_HASH_MAP_INLINE bq_forceinline
#define BQ_HASH_MAP_INLINE_MACRO(X) X##_inline

#include "bq_common/types/hash_map_impl.h"
#undef BQ_HASH_MAP_CLS_NAME
#undef BQ_HASH_MAP_KV_CLS_NAME
#undef BQ_HASH_MAP_ITER_CLS_NAME
#undef BQ_HASH_MAP_INLINE_MACRO
#undef BQ_HASH_MAP_INLINE
