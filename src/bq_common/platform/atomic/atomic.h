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
            acq_rel,
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
                store_seq_cst(value);
                return *this;
            }

            bq_forceinline atomic<T>& operator=(const atomic<T>& rhs)
            {
                store_seq_cst(rhs.load_seq_cst());
                return *this;
            }

            // load
            bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept
            {
                return base_type::load(order);
            }

            bq_forceinline value_type load_raw() const noexcept
            {
                return base_type::load_raw();
            }

            bq_forceinline value_type load_acquire() const noexcept
            {
                return base_type::load_acquire();
            }

            bq_forceinline value_type load_relaxed() const noexcept
            {
                return base_type::load_relaxed();
            }

            bq_forceinline value_type load_acq_rel() const noexcept
            {
                return base_type::load_acq_rel();
            }

            bq_forceinline value_type load_seq_cst() const noexcept
            {
                return base_type::load_seq_cst();
            }

            // store
            bq_forceinline void store(value_type value, memory_order order) noexcept
            {
                base_type::store(value, order);
            }

            bq_forceinline void store_raw(value_type value) noexcept
            {
                base_type::store_raw(value);
            }

            bq_forceinline void store_release(value_type value) noexcept
            {
                base_type::store_release(value);
            }

            bq_forceinline void store_relaxed(value_type value) noexcept
            {
                base_type::store_relaxed(value);
            }
            
            bq_forceinline void store_acq_rel(value_type value) noexcept
            {
                base_type::store_acq_rel(value);
            }

            bq_forceinline void store_seq_cst(value_type value) noexcept
            {
                base_type::store_seq_cst(value);
            }

            //exchange
            bq_forceinline value_type exchange(value_type value, memory_order order) noexcept
            {
                return base_type::exchange(value, order);
            }

            bq_forceinline value_type exchange_raw(value_type value) noexcept
            {
                return base_type::exchange_raw(value);
            }

            bq_forceinline value_type exchange_acquire(value_type value) noexcept
            {
                return base_type::exchange_acquire(value);
            }

            bq_forceinline value_type exchange_release(value_type value) noexcept
            {
                return base_type::exchange_release(value);
            }

            bq_forceinline value_type exchange_relaxed(value_type value) noexcept
            {
                return base_type::exchange_relaxed(value);
            }

            bq_forceinline value_type exchange_acq_rel(value_type value) noexcept
            {
                return base_type::exchange_acq_rel(value);
            }

            bq_forceinline value_type exchange_seq_cst(value_type value) noexcept
            {
                return base_type::exchange_seq_cst(value);
            }

            bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                return base_type::compare_exchange_weak(expected, desired, success_order, fail_order);
            }

            bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                return base_type::compare_exchange_strong(expected, desired, success_order, fail_order);
            }

            // add_fetch
            bq_forceinline value_type add_fetch(value_type val, memory_order order) noexcept
            {
                return base_type::add_fetch(val, order);
            }

            bq_forceinline value_type add_fetch_raw(value_type val) noexcept
            {
                return base_type::add_fetch_raw(val);
            }

            bq_forceinline value_type add_fetch_acquire(value_type val) noexcept
            {
                return base_type::add_fetch_acquire(val);
            }

            bq_forceinline value_type add_fetch_release(value_type val) noexcept
            {
                return base_type::add_fetch_release(val);
            }

            bq_forceinline value_type add_fetch_relaxed(value_type val) noexcept
            {
                return base_type::add_fetch_relaxed(val);
            }

            bq_forceinline value_type add_fetch_acq_rel(value_type val) noexcept
            {
                return base_type::add_fetch_acq_rel(val);
            }

            bq_forceinline value_type add_fetch_seq_cst(value_type val) noexcept
            {
                return base_type::add_fetch_seq_cst(val);
            }
            
            // fetch_add
            bq_forceinline value_type fetch_add(value_type val, memory_order order) noexcept
            {
                return base_type::fetch_add(val, order);
            }

            bq_forceinline value_type fetch_add_raw(value_type val) noexcept
            {
                return base_type::fetch_add_raw(val);
            }

            bq_forceinline value_type fetch_add_acquire(value_type val) noexcept
            {
                return base_type::fetch_add_acquire(val);
            }

            bq_forceinline value_type fetch_add_release(value_type val) noexcept
            {
                return base_type::fetch_add_release(val);
            }

            bq_forceinline value_type fetch_add_relaxed(value_type val) noexcept
            {
                return base_type::fetch_add_relaxed(val);
            }

            bq_forceinline value_type fetch_add_acq_rel(value_type val) noexcept
            {
                return base_type::fetch_add_acq_rel(val);
            }
            
            bq_forceinline value_type fetch_add_seq_cst(value_type val) noexcept
            {
                return base_type::fetch_add_seq_cst(val);
            }

            // sub_fetch
            bq_forceinline value_type sub_fetch(value_type val, memory_order order) noexcept
            {
                return base_type::sub_fetch(val, order);
            }

            bq_forceinline value_type sub_fetch_raw(value_type val) noexcept
            {
                return base_type::sub_fetch_raw(val);
            }

            bq_forceinline value_type sub_fetch_acquire(value_type val) noexcept
            {
                return base_type::sub_fetch_acquire(val);
            }

            bq_forceinline value_type sub_fetch_release(value_type val) noexcept
            {
                return base_type::sub_fetch_release(val);
            }

            bq_forceinline value_type sub_fetch_relaxed(value_type val) noexcept
            {
                return base_type::sub_fetch_relaxed(val);
            }
            
            bq_forceinline value_type sub_fetch_acq_rel(value_type val) noexcept
            {
                return base_type::sub_fetch_acq_rel(val);
            }

            bq_forceinline value_type sub_fetch_seq_cst(value_type val) noexcept
            {
                return base_type::sub_fetch_seq_cst(val);
            }

            // fetch_sub
            bq_forceinline value_type fetch_sub(value_type val, memory_order order) noexcept
            {
                return base_type::fetch_sub(val, order);
            }

            bq_forceinline value_type fetch_sub_raw(value_type val) noexcept
            {
                return base_type::fetch_sub_raw(val);
            }

            bq_forceinline value_type fetch_sub_acquire(value_type val) noexcept
            {
                return base_type::fetch_sub_acquire(val);
            }

            bq_forceinline value_type fetch_sub_release(value_type val) noexcept
            {
                return base_type::fetch_sub_release(val);
            }
            
            bq_forceinline value_type fetch_sub_relaxed(value_type val) noexcept
            {
                return base_type::fetch_sub_relaxed(val);
            }

            bq_forceinline value_type fetch_sub_acq_rel(value_type val) noexcept
            {
                return base_type::fetch_sub_acq_rel(val);
            }

            bq_forceinline value_type fetch_sub_seq_cst(value_type val) noexcept
            {
                return base_type::fetch_sub_seq_cst(val);
            }

            // xor_fetch
            bq_forceinline value_type xor_fetch(value_type val, memory_order order) noexcept
            {
                return base_type::xor_fetch(val, order);
            }

            bq_forceinline value_type xor_fetch_raw(value_type val) noexcept
            {
                return base_type::xor_fetch_raw(val);
            }

            bq_forceinline value_type xor_fetch_acquire(value_type val) noexcept
            {
                return base_type::xor_fetch_acquire(val);
            }

            bq_forceinline value_type xor_fetch_release(value_type val) noexcept
            {
                return base_type::xor_fetch_release(val);
            }

            bq_forceinline value_type xor_fetch_relaxed(value_type val) noexcept
            {
                return base_type::xor_fetch_relaxed(val);
            }
            
            bq_forceinline value_type xor_fetch_acq_rel(value_type val) noexcept
            {
                return base_type::xor_fetch_acq_rel(val);
            }

            bq_forceinline value_type xor_fetch_seq_cst(value_type val) noexcept
            {
                return base_type::xor_fetch_seq_cst(val);
            }

            // fetch_xor
            bq_forceinline value_type fetch_xor(value_type val, memory_order order) noexcept
            {
                return base_type::fetch_xor(val, order);
            }

            bq_forceinline value_type fetch_xor_raw(value_type val) noexcept
            {
                return base_type::fetch_xor_raw(val);
            }

            bq_forceinline value_type fetch_xor_acquire(value_type val) noexcept
            {
                return base_type::fetch_xor_acquire(val);
            }

            bq_forceinline value_type fetch_xor_release(value_type val) noexcept
            {
                return base_type::fetch_xor_release(val);
            }
            
            bq_forceinline value_type fetch_xor_relaxed(value_type val) noexcept
            {
                return base_type::fetch_xor_relaxed(val);
            }

            bq_forceinline value_type fetch_xor_acq_rel(value_type val) noexcept
            {
                return base_type::fetch_xor_acq_rel(val);
            }

            bq_forceinline value_type fetch_xor_seq_cst(value_type val) noexcept
            {
                return base_type::fetch_xor_seq_cst(val);
            }

            // or_fetch
            bq_forceinline value_type or_fetch(value_type val, memory_order order) noexcept
            {
                return base_type::or_fetch(val, order);
            }

            bq_forceinline value_type or_fetch_raw(value_type val) noexcept
            {
                return base_type::or_fetch_raw(val);
            }

            bq_forceinline value_type or_fetch_acquire(value_type val) noexcept
            {
                return base_type::or_fetch_acquire(val);
            }

            bq_forceinline value_type or_fetch_release(value_type val) noexcept
            {
                return base_type::or_fetch_release(val);
            }
            
            bq_forceinline value_type or_fetch_relaxed(value_type val) noexcept
            {
                return base_type::or_fetch_relaxed(val);
            }

            bq_forceinline value_type or_fetch_acq_rel(value_type val) noexcept
            {
                return base_type::or_fetch_acq_rel(val);
            }

            bq_forceinline value_type or_fetch_seq_cst(value_type val) noexcept
            {
                return base_type::or_fetch_seq_cst(val);
            }

            // fetch_or
            bq_forceinline value_type fetch_or(value_type val, memory_order order) noexcept
            {
                return base_type::fetch_or(val, order);
            }

            bq_forceinline value_type fetch_or_raw(value_type val) noexcept
            {
                return base_type::fetch_or_raw(val);
            }

            bq_forceinline value_type fetch_or_acquire(value_type val) noexcept
            {
                return base_type::fetch_or_acquire(val);
            }

            bq_forceinline value_type fetch_or_release(value_type val) noexcept
            {
                return base_type::fetch_or_release(val);
            }
            
            bq_forceinline value_type fetch_or_relaxed(value_type val) noexcept
            {
                return base_type::fetch_or_relaxed(val);
            }

            bq_forceinline value_type fetch_or_acq_rel(value_type val) noexcept
            {
                return base_type::fetch_or_acq_rel(val);
            }

            bq_forceinline value_type fetch_or_seq_cst(value_type val) noexcept
            {
                return base_type::fetch_or_seq_cst(val);
            }

            // and_fetch
            bq_forceinline value_type and_fetch(value_type val, memory_order order) noexcept
            {
                return base_type::and_fetch(val, order);
            }

            bq_forceinline value_type and_fetch_raw(value_type val) noexcept
            {
                return base_type::and_fetch_raw(val);
            }

            bq_forceinline value_type and_fetch_acquire(value_type val) noexcept
            {
                return base_type::and_fetch_acquire(val);
            }

            bq_forceinline value_type and_fetch_release(value_type val) noexcept
            {
                return base_type::and_fetch_release(val);
            }
            
            bq_forceinline value_type and_fetch_relaxed(value_type val) noexcept
            {
                return base_type::and_fetch_relaxed(val);
            }

            bq_forceinline value_type and_fetch_acq_rel(value_type val) noexcept
            {
                return base_type::and_fetch_acq_rel(val);
            }

            bq_forceinline value_type and_fetch_seq_cst(value_type val) noexcept
            {
                return base_type::and_fetch_seq_cst(val);
            }

            // fetch_and
            bq_forceinline value_type fetch_and(value_type val, memory_order order) noexcept
            {
                return base_type::fetch_and(val, order);
            }

            bq_forceinline value_type fetch_and_raw(value_type val) noexcept
            {
                return base_type::fetch_and_raw(val);
            }

            bq_forceinline value_type fetch_and_acquire(value_type val) noexcept
            {
                return base_type::fetch_and_acquire(val);
            }

            bq_forceinline value_type fetch_and_release(value_type val) noexcept
            {
                return base_type::fetch_and_release(val);
            }
            
            bq_forceinline value_type fetch_and_relaxed(value_type val) noexcept
            {
                return base_type::fetch_and_relaxed(val);
            }

            bq_forceinline value_type fetch_and_acq_rel(value_type val) noexcept
            {
                return base_type::fetch_and_acq_rel(val);
            }

            bq_forceinline value_type fetch_and_seq_cst(value_type val) noexcept
            {
                return base_type::fetch_and_seq_cst(val);
            }

            bq_forceinline atomic<T>& operator++()
            {
                fetch_add_seq_cst(1);
                return *this;
            }

            bq_forceinline atomic<T> operator++(int32_t)
            {
                atomic<T> tmp = *this;
                fetch_add_seq_cst(1);
                return tmp;
            }

            bq_forceinline atomic<T>& operator--()
            {
                fetch_sub_seq_cst(1);
                return *this;
            }

            bq_forceinline atomic<T> operator--(int32_t)
            {
                atomic<T> tmp = *this;
                fetch_sub_seq_cst(1);
                return tmp;
            }

            bq_forceinline atomic<T>& operator+=(value_type value)
            {
                fetch_add_seq_cst(value);
                return *this;
            }

            bq_forceinline atomic<T> operator+(value_type value)
            {
                atomic<T> tmp = *this;
                tmp.add_fetch_seq_cst(value);
                return tmp;
            }

            bq_forceinline atomic<T>& operator-=(value_type value)
            {
                fetch_sub_seq_cst(value);
                return *this;
            }

            bq_forceinline atomic<T> operator-(value_type value)
            {
                atomic<T> tmp = *this;
                tmp.sub_fetch_seq_cst(value);
                return tmp;
            }
        };
    }
}
