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
 * \file atomic.h
 *
 * \author pippocao
 * \date 2022/09/15
 *
 * simple atomic template class as substitute of std::atomic.
 * we exclude STL and libc++ to reduce the final executable and library file size
 */
#include "bq_common/misc/assert.h"
#include "bq_common/platform/macros.h"
#include "bq_common/types/type_traits.h"

namespace bq {
    namespace platform {
        enum class memory_order : uint8_t {
            relaxed,
            acquire,
            release,
            seq_cst
        };
        void atomic_thread_fence(memory_order order = memory_order::seq_cst);
    }
}

#ifdef BQ_WIN
#include "bq_common/platform/atomic/_inner_atomic_windows.h"
#elif defined(__GNUC__)
#include "bq_common/platform/atomic/_inner_atomic_GNU_C.h"
#else
static_assert(false, "bq::platform::atomic is not supported in your compiler");
#endif

namespace bq {
    namespace platform {
        template <typename T>
        class atomic : public _atomic_base<T, sizeof(T)> {
            typedef _atomic_base<T, sizeof(T)> base_type;
            static_assert(base_type::supported, "atomic type is not supported on this platform!");

        public:
            typedef typename bq::decay<T>::type value_type;
            atomic()
                : base_type()
            {
            }
            atomic(const value_type& value)
                : base_type(value)
            {
            }
            atomic(const atomic<T>& rhs)
                : base_type(rhs)
            {
            }

            bq_forceinline atomic<T>& operator=(const value_type& value)
            {
                store(value, memory_order::seq_cst);
                return *this;
            }

            bq_forceinline atomic<T>& operator=(const atomic<T>& rhs)
            {
                store(rhs.load(memory_order::seq_cst), memory_order::seq_cst);
                return *this;
            }

            bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept
            {
                return base_type::load(order);
            }

            bq_forceinline void store(value_type value, memory_order order = memory_order::seq_cst) noexcept
            {
                base_type::store(value, order);
            }

            bq_forceinline value_type exchange(value_type value, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::exchange(value, order);
            }

            bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                return base_type::compare_exchange_weak(expected, desired, success_order, fail_order);
            }

            bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                return base_type::compare_exchange_strong(expected, desired, success_order, fail_order);
            }

            bq_forceinline value_type add_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::add_fetch(val, order);
            }

            bq_forceinline value_type fetch_add(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::fetch_add(val, order);
            }

            bq_forceinline value_type sub_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::sub_fetch(val, order);
            }

            bq_forceinline value_type fetch_sub(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::fetch_sub(val, order);
            }

            bq_forceinline value_type xor_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::xor_fetch(val, order);
            }

            bq_forceinline value_type fetch_xor(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::fetch_xor(val, order);
            }

            bq_forceinline value_type or_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::or_fetch(val, order);
            }

            bq_forceinline value_type fetch_or(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::fetch_or(val, order);
            }

            bq_forceinline value_type and_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::and_fetch(val, order);
            }

            bq_forceinline value_type fetch_and(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                return base_type::fetch_and(val, order);
            }

            bq_forceinline atomic<T>& operator++()
            {
                fetch_add(1);
                return *this;
            }

            bq_forceinline atomic<T> operator++(int32_t)
            {
                atomic<T> tmp = *this;
                fetch_add(1);
                return tmp;
            }

            bq_forceinline atomic<T>& operator--()
            {
                fetch_sub(1);
                return *this;
            }

            bq_forceinline atomic<T> operator--(int32_t)
            {
                atomic<T> tmp = *this;
                fetch_sub(1);
                return tmp;
            }

            bq_forceinline atomic<T>& operator+=(value_type value)
            {
                fetch_add(value);
                return *this;
            }

            bq_forceinline atomic<T> operator+(value_type value)
            {
                atomic<T> tmp = *this;
                tmp.add_fetch(value);
                return tmp;
            }

            bq_forceinline atomic<T>& operator-=(value_type value)
            {
                fetch_sub(value);
                return *this;
            }

            bq_forceinline atomic<T> operator-(value_type value)
            {
                atomic<T> tmp = *this;
                tmp.sub_fetch(value);
                return tmp;
            }
        };
    }
}
