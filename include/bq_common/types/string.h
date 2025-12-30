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
 * \file string.h
 * substitute of STL string_base, we exclude STL and libc++ to reduce the final executable and library file size
 * IMPORTANT: It is not thread-safe!!!
 *
 * \author pippocao
 * \date 2022/08/10
 *
 */


#define BQ_STRING_INLINE inline
#define BQ_STRING_INLINE_MACRO(X) X

#define BQ_STRING_CLS_NAME BQ_STRING_INLINE_MACRO(string_base)

#include "bq_common/types/string_impl.h"

#undef BQ_STRING_INLINE
#undef BQ_STRING_INLINE_MACRO
#define BQ_STRING_INLINE bq_forceinline
#define BQ_STRING_INLINE_MACRO(X) X##_inline

#include "bq_common/types/string_impl.h"

#undef BQ_STRING_INLINE_MACRO
#undef BQ_STRING_CLS_NAME
#undef BQ_STRING_INLINE

