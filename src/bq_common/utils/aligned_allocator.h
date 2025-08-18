﻿#pragma once
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
 * \file aligned_allocator.h
 *
 * Aligned allocator
 *
 * \author pippocao
 * \date 2025/07/10
 *
 */

#include "bq_common/bq_common.h"

namespace bq {
    template <typename T, size_t Alignment>
    class aligned_allocator : public default_allocator<T> {
    public:
        using value_type = T;
        aligned_allocator() = default;

        template <typename T1, size_t Alignment1>
        aligned_allocator(const aligned_allocator<T1, Alignment1>&) { }

        value_type* allocate(size_t n)
        {
            return reinterpret_cast<value_type*>(bq::platform::aligned_alloc(Alignment, n));
        }
        void deallocate(value_type* p, size_t)
        {
            bq::platform::aligned_free(p);
        }

        bool operator==(const aligned_allocator&) const noexcept { return true; }
        bool operator!=(const aligned_allocator&) const noexcept { return false; }
    };
}