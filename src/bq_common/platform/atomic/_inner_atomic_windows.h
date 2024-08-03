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
 * \file _inner_atomic_windows.h
 *
 * \author pippocao
 * \date 2022/09/16
 *
 * Bq atomic implementation for windows platform
 *  64bit atomic type is not supported on 32bit windows
 *  both arm and Intel architecture is supported for all memory order semantic.
 */
#ifdef BQ_WIN
// although <winsock2.h> does nothing help to atomic, but we must include it before <windows.h> to avoid compiling error when develop net socket functionalities.
#include <winsock2.h>
#include <windows.h>
#include <intrin.h>
namespace bq {
    namespace platform {
        template <typename T, size_t N>
        class _atomic_base {
        public:
            static constexpr bool supported = false;

        protected:
            typedef typename bq::decay<T>::type value_type;
            value_type value_;
            _atomic_base()
                : value_()
            {
            }
            _atomic_base(const value_type& value)
                : value_(value)
            {
            }
        };

#define BQ_ATOMIC_CONCATX(x, y) x##y

#if defined(_M_IX86) || (defined(_M_X64) && !defined(_M_ARM64EC))
#define _INTRIN_RELAXED(x) x
#define _INTRIN_ACQUIRE(x) x
#define _INTRIN_RELEASE(x) x
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _INTRIN_RELAXED(x) BQ_ATOMIC_CONCATX(x, _nf)
#define _INTRIN_ACQUIRE(x) BQ_ATOMIC_CONCATX(x, _acq)
#define _INTRIN_RELEASE(x) BQ_ATOMIC_CONCATX(x, _rel)
#endif

#define BQ_ATOMIC_CHOOSE_INTRINSIC(_Order, _Result, _Intrinsic, ...) \
    switch (_Order) {                                                \
    case memory_order::relaxed:                                      \
        _Result = _INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__);          \
        break;                                                       \
    case memory_order::acquire:                                      \
        _Result = _INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__);          \
        break;                                                       \
    case memory_order::release:                                      \
        _Result = _INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__);          \
        break;                                                       \
    case memory_order::seq_cst:                                      \
        _Result = _Intrinsic(__VA_ARGS__);                           \
        break;                                                       \
    }

#define _INTRIN_BIT_1(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 8)
#define _INTRIN_BIT_2(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 16)
#define _INTRIN_BIT_4(_Intrinsic) _Intrinsic
#define _INTRIN_BIT_8(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 64)

#define _INTRIN_BIT_SUFFIX(_Intrinsic, SIZE_BYTE) BQ_ATOMIC_CONCATX(_INTRIN_BIT_, SIZE_BYTE)(_Intrinsic)

#define SPECIALIZED_ATOMIC_BASE(SIZE_BYTE)                                                                                                                                                                                         \
    template <typename T>                                                                                                                                                                                                          \
    class _atomic_base<T, SIZE_BYTE> {                                                                                                                                                                                             \
    public:                                                                                                                                                                                                                        \
        static constexpr bool supported = true;                                                                                                                                                                                    \
                                                                                                                                                                                                                                   \
    protected:                                                                                                                                                                                                                     \
        static_assert(sizeof(T) == SIZE_BYTE, "please don't use bq::platform::_atomic_base, use bq::platform::atomic instead.");                                                                                                   \
        typedef typename bq::decay<T>::type value_type;                                                                                                                                                                            \
        typedef typename _atomic_standard_windows_type<sizeof(T)>::value_type api_type;                                                                                                                                            \
        volatile value_type value_;                                                                                                                                                                                                \
        _atomic_base()                                                                                                                                                                                                             \
            : value_()                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        _atomic_base(const value_type& value)                                                                                                                                                                                      \
            : value_(value)                                                                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
                                                                                                                                                                                                                                   \
        bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept                                                                                                                                  \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                             \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                            \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                            \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept              \
        {                                                                                                                                                                                                                          \
            (void)fail_order;                                                                                                                                                                                                      \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(success_order, result, _INTRIN_BIT_SUFFIX(_InterlockedCompareExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(desired), get_atomic_value<value_type>(expected)); \
            bool is_suc = ((value_type)result == expected);                                                                                                                                                                        \
            expected = (value_type)result;                                                                                                                                                                                         \
            return is_suc;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept            \
        {                                                                                                                                                                                                                          \
            (void)fail_order;                                                                                                                                                                                                      \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(success_order, result, _INTRIN_BIT_SUFFIX(_InterlockedCompareExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(desired), get_atomic_value<value_type>(expected)); \
            bool is_suc = ((value_type)result == expected);                                                                                                                                                                        \
            expected = (value_type)result;                                                                                                                                                                                         \
            return is_suc;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            return add_fetch(-val, order);                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            return fetch_add(-val, order);                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and(value_type val, memory_order order = memory_order::seq_cst) noexcept                                                                                                                   \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
    };

        template <size_t N>
        struct _atomic_standard_windows_type {
        };

        template <>
        struct _atomic_standard_windows_type<1> {
            typedef volatile char* ptr_type;
            typedef char value_type;
        };

        template <>
        struct _atomic_standard_windows_type<2> {
            typedef volatile short* ptr_type; // int16_t is not compatible to VC++ type short in the intrinsic functions
            typedef short value_type;
        };

        template <>
        struct _atomic_standard_windows_type<4> {
            typedef volatile long* ptr_type; // int32_t is not compatible to VC++ type short in the intrinsic functions
            typedef long value_type;
        };

        template <>
        struct _atomic_standard_windows_type<8> {
            typedef volatile __int64* ptr_type; // int64_t is not compatible to VC++ type short in the intrinsic functions
            typedef __int64 value_type;
        };

        template <typename T>
        typename _atomic_standard_windows_type<sizeof(T)>::value_type get_atomic_value(const T& value)
        {
            return (typename _atomic_standard_windows_type<sizeof(T)>::value_type)(value);
        }

        template <typename T>
        typename _atomic_standard_windows_type<sizeof(T)>::ptr_type get_atomic_ptr(const T* value)
        {
            return reinterpret_cast<typename _atomic_standard_windows_type<sizeof(T)>::ptr_type>(const_cast<T*>(value));
        }

#pragma warning(push)
#pragma warning(disable : 6001)
#pragma warning(disable : 4701)
        SPECIALIZED_ATOMIC_BASE(1);
        SPECIALIZED_ATOMIC_BASE(2);
        SPECIALIZED_ATOMIC_BASE(4);
#ifdef _M_X64
        SPECIALIZED_ATOMIC_BASE(8);
#endif
#pragma warning(pop)

        inline void atomic_thread_fence(memory_order order)
        {
            if (order != memory_order::relaxed) {
                MemoryBarrier();
            }
        }
    }
}

#endif // BQ_WIN
