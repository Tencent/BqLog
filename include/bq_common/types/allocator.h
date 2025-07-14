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
 * \file allocator.h
 * substitute of STL vector, we exclude STL and libc++ to reduce the final executable and library file size
 *
 * Default allocator
 *
 * \author pippocao
 * \date 2025/07/10
 *
 */

#include "bq_common/bq_common_public_include.h"
#include <stdlib.h>

namespace bq {
    template <typename Allocator>
    class allocator_traits {
    private:
        template <typename A, typename = void>
        struct extract_allocator_template;

        template <template <typename> class Wrapper, typename Inner>
        struct extract_allocator_template<Wrapper<Inner>, void> {
            template <typename U>
            using rebind = Wrapper<U>;
        };

    public:
        template <typename NewType>
        using rebind_alloc = typename extract_allocator_template<Allocator>::template rebind<NewType>;
    };

    template <typename T>
    class default_allocator {
    public:
        using value_type = T;
        default_allocator() = default;

        template <typename U>
        default_allocator(const default_allocator<U>&) { }

        value_type* allocate(size_t n)
        {
            return static_cast<value_type*>(malloc(n * sizeof(value_type)));
        }
        void deallocate(value_type* p, size_t)
        {
            free(p);
        }
        template <typename U, typename... Args>
        void construct(U* p, Args&&... args)
        {
            bq::object_constructor<U>::construct(p, bq::forward<Args>(args)...);
        }
        template <typename U>
        void destroy(U* p)
        {
            bq::object_destructor<U>::destruct(p);
        }
        template <typename U>
        void destroy(U* p, size_t n)
        {
            bq::object_array_destructor<U>::destruct(p, n);
        }

        bool operator==(const default_allocator&) const noexcept { return true; }
        bool operator!=(const default_allocator&) const noexcept { return false; }
    };
}