/* Copyright (C) 2026 Tencent.
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
// Standalone C-compatible ABI. Dependencies are one-way:
// bq_log_api.h -> bq_log_c_api.h -> bq_log_c_types.h -> basic_types.h.
#include "bq_log/misc/bq_log_c_types.h"

#if defined(__cplusplus)
namespace bq {
    namespace api {
#define BQ_LOG_API_TYPE(cpp_type, c_type) cpp_type
#else
#define BQ_LOG_API_TYPE(cpp_type, c_type) c_type
#endif
#include "bq_log/misc/bq_log_c_api.inc"
#undef BQ_LOG_API_TYPE
#if defined(__cplusplus)
    }
}
#endif
