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
/*!
 * \file aligned_allocator.cpp
 *
 * Aligned allocator
 *
 * \author pippocao
 * \date 2026/01/30
 *
 */

#include "bq_common/utils/aligned_allocator.h"
#include "bq_common/bq_common.h"

namespace bq {
    void* aligned_alloc(size_t alignment, size_t size)
    {
        return bq::platform::aligned_alloc(alignment, size);
    }
    void aligned_free(void* ptr) {
        return bq::platform::aligned_free(ptr);
    }
    
}