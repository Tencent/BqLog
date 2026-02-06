/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "bq_common/platform/simd.h"
#include "bq_common/bq_common.h"

namespace bq {
    bool _bq_crc32_supported_ = common_global_vars::get().crc32_supported_;

#if defined(BQ_X86)
    bool _bq_avx2_supported_ = common_global_vars::get().avx2_support_;
#endif

}