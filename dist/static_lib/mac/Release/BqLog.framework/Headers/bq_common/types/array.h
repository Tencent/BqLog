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
/*!
 * \file array.h
 * substitute of STL vector, we exclude STL and libc++ to reduce the final executable and library file size
 * IMPORTANT: It is not thread-safe!!!
 *
 * Two types of array is supported:
 * 1. bq::array
 * 2. bq::array_inline   (every function is force inlined, best performance but more program size).
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */

#define BQ_ARRAY_INLINE inline
#define BQ_ARRAY_INLINE_MACRO(X) X

#define BQ_ARRAY_CLS_NAME BQ_ARRAY_INLINE_MACRO(array)
#define BQ_ARRAY_ITER_CLS_NAME BQ_ARRAY_INLINE_MACRO(array_iterator)

#include "bq_common/types/array_impl.h"

#undef BQ_ARRAY_INLINE
#undef BQ_ARRAY_INLINE_MACRO
#define BQ_ARRAY_INLINE bq_forceinline
#define BQ_ARRAY_INLINE_MACRO(X) X##_inline

#include "bq_common/types/array_impl.h"

#undef BQ_ARRAY_INLINE_MACRO
#undef BQ_ARRAY_CLS_NAME
#undef BQ_ARRAY_ITER_CLS_NAME
#undef BQ_ARRAY_INLINE
