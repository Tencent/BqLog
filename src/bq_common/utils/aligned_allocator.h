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
 * \file aligned_allocator.h
 *
 * Aligned allocator
 *
 * \author pippocao
 * \date 2025/07/10
 *
 */

#include "bq_common/bq_common_public_include.h"

namespace bq {
    void* aligned_alloc(size_t alignment, size_t size);
    void aligned_free(void* ptr);

    template <typename T, size_t Alignment>
    class aligned_allocator : public default_allocator<T> {
    public:
        using value_type = T;
        aligned_allocator() = default;

        template <typename T1, size_t Alignment1>
        aligned_allocator(const aligned_allocator<T1, Alignment1>&) { }

        value_type* allocate(size_t n)
        {
            return reinterpret_cast<value_type*>(aligned_alloc(Alignment, n));
        }
        void deallocate(value_type* p, size_t)
        {
            aligned_free(p);
        }

        bool operator==(const aligned_allocator&) const noexcept { return true; }
        bool operator!=(const aligned_allocator&) const noexcept { return false; }
    };
}