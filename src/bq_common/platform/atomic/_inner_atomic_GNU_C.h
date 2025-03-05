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
 * \file _inner_atomic_posix.h
 *
 * \author pippocao
 * \date 2022/09/17
 *
 * Bq atomic implementation for posix platform
 * both arm and Intel architecture is supported for all memory order semantic.
 */
#ifdef __GNUC__
#include "bq_common/types/type_traits.h"
namespace bq {
    namespace platform {
        template <size_t N>
        struct _atomic_gnu_c_standard_type {
        };

        template <>
        struct _atomic_gnu_c_standard_type<1> {
            typedef uint8_t* ptr_type;
            typedef uint8_t value_type;
        };

        template <>
        struct _atomic_gnu_c_standard_type<2> {
            typedef uint16_t* ptr_type; // int16_t is not compatible to VC++ type short in the intrinsic functions
            typedef uint16_t value_type;
        };

        template <>
        struct _atomic_gnu_c_standard_type<4> {
            typedef uint32_t* ptr_type; // int32_t is not compatible to VC++ type short in the intrinsic functions
            typedef uint32_t value_type;
        };

        template <>
        struct _atomic_gnu_c_standard_type<8> {
            typedef uint64_t* ptr_type; // int64_t is not compatible to VC++ type short in the intrinsic functions
            typedef uint64_t value_type;
        };

        template <typename T>
        bq_forceinline typename _atomic_gnu_c_standard_type<sizeof(T)>::value_type get_atomic_value(const T& value)
        {
            return (typename _atomic_gnu_c_standard_type<sizeof(T)>::value_type)(value);
        }

        template <typename T>
        bq_forceinline typename _atomic_gnu_c_standard_type<sizeof(T)>::ptr_type get_atomic_ptr(const T& value)
        {
            return reinterpret_cast<typename _atomic_gnu_c_standard_type<sizeof(T)>::ptr_type>(const_cast<T*>(&value));
        }

        template <typename T, size_t N>
        class _atomic_base {
        public:
            static constexpr bool supported = true;

        protected:
            static_assert(sizeof(T) == N, "please don't use bq::platform::_atomic_base, use bq::platform::atomic instead.");
            static_assert(N == 1 || N == 2 || N == 4 || N == 8, "please don't use bq::platform::_atomic_base, use bq::platform::atomic instead.");
            typedef typename bq::decay<T>::type value_type;
            typedef typename _atomic_gnu_c_standard_type<sizeof(T)>::value_type api_type;
            union {
                alignas(8) value_type value_;
                alignas(8) api_type value_standard;
            };
            _atomic_base()
                : value_()
            {
            }
            _atomic_base(const value_type& value)
                : value_(value)
            {
            }

            bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_ACQUIRE));
                default:
                    return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type load_raw() const noexcept
            {
                return value_;
            }

            bq_forceinline value_type load_acquire() const noexcept
            {
                return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type load_relaxed() const noexcept
            {
                return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type load_seq_cst() const noexcept
            {
                return (value_type)(__atomic_load_n(&value_standard, __ATOMIC_SEQ_CST));
            }

            bq_forceinline void store(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_RELAXED);
                case memory_order::release:
                    return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_RELEASE);
                default:
                    return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_SEQ_CST);
                }
            }
            bq_forceinline void store_raw(value_type val) noexcept
            {
                value_ = val;
            }

            bq_forceinline void store_release(value_type val) noexcept
            {
                return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_RELEASE);
            }

            bq_forceinline void store_relaxed(value_type val) noexcept
            {
                return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_RELAXED);
            }

            bq_forceinline void store_seq_cst(value_type val) noexcept
            {
                return __atomic_store(&value_standard, get_atomic_ptr(val), __ATOMIC_SEQ_CST);
            }

            bq_forceinline value_type exchange(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type exchange_raw(value_type value) noexcept
            {
                value_type old = value_;
                value_ = value;
                return old;
            }

            bq_forceinline value_type exchange_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type exchange_release(value_type val) noexcept
            {
                return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type exchange_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type exchange_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_exchange_n(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                switch (success_order) {
                case memory_order::relaxed:
                    return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                case memory_order::acquire:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
                    }
                case memory_order::release:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
                    }
                default:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
                    case memory_order::acquire:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
                    }
                }
            }

            bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept
            {
                switch (success_order) {
                case memory_order::relaxed:
                    return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                case memory_order::acquire:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
                    }
                case memory_order::release:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
                    }
                default:
                    switch (fail_order) {
                    case memory_order::relaxed:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
                    case memory_order::acquire:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
                    default:
                        return __atomic_compare_exchange(&value_standard, get_atomic_ptr(expected), get_atomic_ptr(desired), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
                    }
                }
            }

            bq_forceinline value_type add_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type add_fetch_raw(value_type val) noexcept
            {
                value_ += val;
                return value_;
            }

            bq_forceinline value_type add_fetch_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type add_fetch_release(value_type val) noexcept
            {
                return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type add_fetch_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type add_fetch_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_add_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type fetch_add(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type fetch_add_raw(value_type val) noexcept
            {
                value_type old_value = value_;
                value_ += val;
                return old_value; 
            }

            bq_forceinline value_type fetch_add_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type fetch_add_release(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type fetch_add_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type fetch_add_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_add(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type sub_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type sub_fetch_raw(value_type val) noexcept
            {
                value_ -= val;
                return value_;
            }

            bq_forceinline value_type sub_fetch_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type sub_fetch_release(value_type val) noexcept
            {
                return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type sub_fetch_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type sub_fetch_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_sub_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type fetch_sub(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type fetch_sub_raw(value_type val) noexcept
            {
                value_type old_value = value_;
                value_ -= val;
                return old_value;
            }

            bq_forceinline value_type fetch_sub_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type fetch_sub_release(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type fetch_sub_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type fetch_sub_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_sub(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type xor_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type xor_fetch_raw(value_type val) noexcept
            {
                value_ ^= val;
                return value_;
            }

            bq_forceinline value_type xor_fetch_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type xor_fetch_release(value_type val) noexcept
            {
                return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type xor_fetch_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type xor_fetch_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_xor_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type fetch_xor(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type fetch_xor_raw(value_type val) noexcept
            {
                value_type old_value = value_;
                value_ ^= val;
                return old_value;
            }

            bq_forceinline value_type fetch_xor_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type fetch_xor_release(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type fetch_xor_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type fetch_xor_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_xor(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type or_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type or_fetch_raw(value_type val) noexcept
            {
                value_ |= val;
                return value_; 
            }

            bq_forceinline value_type or_fetch_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type or_fetch_release(value_type val) noexcept
            {
                return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type or_fetch_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type or_fetch_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_or_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type fetch_or(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type fetch_or_raw(value_type val) noexcept
            {
                value_type old_value = value_;
                value_ |= val;
                return old_value;
            }

            bq_forceinline value_type fetch_or_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type fetch_or_release(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type fetch_or_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type fetch_or_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_or(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type and_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type and_fetch_raw(value_type val) noexcept
            {
                value_ &= val;
                return value_;
            }

            bq_forceinline value_type and_fetch_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type and_fetch_release(value_type val) noexcept
            {
                return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type and_fetch_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type and_fetch_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_and_fetch(&value_standard, val, __ATOMIC_SEQ_CST));
            }

            bq_forceinline value_type fetch_and(value_type val, memory_order order = memory_order::seq_cst) noexcept
            {
                switch (order) {
                case memory_order::relaxed:
                    return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_RELAXED));
                case memory_order::acquire:
                    return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_ACQUIRE));
                case memory_order::release:
                    return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_RELEASE));
                default:
                    return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_SEQ_CST));
                }
            }

            bq_forceinline value_type fetch_and_raw(value_type val) noexcept
            {
                value_type old_value = value_;
                value_ &= val;
                return old_value;
            }

            bq_forceinline value_type fetch_and_acquire(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_ACQUIRE));
            }

            bq_forceinline value_type fetch_and_release(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_RELEASE));
            }

            bq_forceinline value_type fetch_and_relaxed(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_RELAXED));
            }

            bq_forceinline value_type fetch_and_seq_cst(value_type val) noexcept
            {
                return (value_type)(__atomic_fetch_and(&value_standard, val, __ATOMIC_SEQ_CST));
            }
        };

        bq_forceinline void atomic_thread_fence(memory_order order)
        {
            switch (order) {
            case memory_order::relaxed:
                return __atomic_thread_fence(__ATOMIC_RELAXED);
            case memory_order::acquire:
                return __atomic_thread_fence(__ATOMIC_ACQUIRE);
            case memory_order::release:
                return __atomic_thread_fence(__ATOMIC_RELEASE);
            default:
                return __atomic_thread_fence(__ATOMIC_SEQ_CST);
            }
        }
    }
}
#endif
