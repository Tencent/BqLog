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
/*!
 * \file basic_types.h
 *
 * \author pippocao
 * \date 2022/07/14
 *
 */
#include <stdint.h>

#if defined(__cplusplus)
namespace bq {
    enum class log_level : int32_t {
        verbose,
        debug,
        info,
        warning,
        error,
        fatal,

        log_level_max = 32,
    };
}
#else
// Keep the ABI fixed at 32 bits, even with -fshort-enums.
typedef int32_t bq_log_level;
enum bq_log_level_value {
    bq_log_level_verbose = 0,
    bq_log_level_debug,
    bq_log_level_info,
    bq_log_level_warning,
    bq_log_level_error,
    bq_log_level_fatal,
    bq_log_level_max = 32
};
#endif
