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
 * \file _inner_atomic_windows.h
 *
 * \author pippocao
 * \date 2022/09/16
 *
 * Bq atomic implementation for windows platform
 *  64bit atomic type is not supported on 32bit windows
 *  both arm and Intel architecture is supported for all memory order semantic.
 */
#ifdef BQ_MSVC
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
#define _BQ_INTRIN_RELAXED(x) x
#define _BQ_INTRIN_ACQUIRE(x) x
#define _BQ_INTRIN_RELEASE(x) x
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define _BQ_INTRIN_RELAXED(x) BQ_ATOMIC_CONCATX(x, _nf)
#define _BQ_INTRIN_ACQUIRE(x) BQ_ATOMIC_CONCATX(x, _acq)
#define _BQ_INTRIN_RELEASE(x) BQ_ATOMIC_CONCATX(x, _rel)
#endif

#define BQ_ATOMIC_INTRINSIC_RELAXED(_Result, _Intrinsic, ...) \
    _Result = _BQ_INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_ACQUIRE(_Result, _Intrinsic, ...) \
    _Result = _BQ_INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_RELEASE(_Result, _Intrinsic, ...) \
    _Result = _BQ_INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_ACQ_REL(_Result, _Intrinsic, ...) \
    _Result = _Intrinsic(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_SEQ_CST(_Result, _Intrinsic, ...) \
    _Result = _Intrinsic(__VA_ARGS__);

#define BQ_ATOMIC_CHOOSE_INTRINSIC(_Order, _Result, _Intrinsic, ...) \
    switch (_Order) {                                                \
    case memory_order::relaxed:                                      \
        _Result = _BQ_INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__);       \
        break;                                                       \
    case memory_order::acquire:                                      \
        _Result = _BQ_INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__);       \
        break;                                                       \
    case memory_order::release:                                      \
        _Result = _BQ_INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__);       \
        break;                                                       \
    case memory_order::acq_rel:                                      \
        _Result = _Intrinsic(__VA_ARGS__);                           \
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
        alignas(8) value_type value_;                                                                                                                                                                                              \
        _atomic_base()                                                                                                                                                                                                             \
            : value_()                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        _atomic_base(const value_type& val)                                                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept                                                                                                                                  \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load_raw() const noexcept                                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            return value_;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load_acquire() const noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                                     \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load_relaxed() const noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                                     \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load_acq_rel() const noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                                     \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type load_seq_cst() const noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>((value_type)0));                                                     \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store(value_type val, memory_order order) noexcept                                                                                                                                                     \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                            \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store_raw(value_type val) noexcept                                                                                                                                                                     \
        {                                                                                                                                                                                                                          \
            value_ = val;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store_release(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store_relaxed(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store_acq_rel(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline void store_seq_cst(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange(value_type val, memory_order order) noexcept                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                            \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_raw(value_type val) noexcept                                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            value_type old = value_;                                                                                                                                                                                               \
            value_ = val;                                                                                                                                                                                                          \
            return old;                                                                                                                                                                                                            \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_acquire(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_release(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_relaxed(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_acq_rel(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type exchange_seq_cst(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept              \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(success_order, result, _INTRIN_BIT_SUFFIX(_InterlockedCompareExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(desired), get_atomic_value<value_type>(expected)); \
            bool is_suc = ((value_type)result == expected);                                                                                                                                                                        \
            if (!is_suc) {                                                                                                                                                                                                         \
                expected = load(fail_order);                                                                                                                                                                                       \
            }                                                                                                                                                                                                                      \
            return is_suc;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept            \
        {                                                                                                                                                                                                                          \
            return compare_exchange_weak(expected, desired, success_order, fail_order);                                                                                                                                            \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_ += val;                                                                                                                                                                                                         \
            return value_;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type add_fetch_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_type old_value = value_;                                                                                                                                                                                         \
            value_ += val;                                                                                                                                                                                                         \
            return old_value;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_add_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            return add_fetch(-val, order);                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            return add_fetch_raw(-val);                                                                                                                                                                                            \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return add_fetch_acquire(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return add_fetch_release(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return add_fetch_relaxed(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return add_fetch_acq_rel(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type sub_fetch_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return add_fetch_seq_cst(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            return fetch_add(-val, order);                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            return fetch_add_raw(-val);                                                                                                                                                                                            \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return fetch_add_acquire(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return fetch_add_release(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return fetch_add_relaxed(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return fetch_add_acq_rel(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_sub_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            return fetch_add_seq_cst(-val);                                                                                                                                                                                        \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_ ^= val;                                                                                                                                                                                                         \
            return value_;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type xor_fetch_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_type old_value = value_;                                                                                                                                                                                         \
            value_ ^= val;                                                                                                                                                                                                         \
            return old_value;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_xor_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch(value_type val, memory_order order) noexcept                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_raw(value_type val) noexcept                                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            value_ |= val;                                                                                                                                                                                                         \
            return value_;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_acquire(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_release(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_relaxed(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_acq_rel(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type or_fetch_seq_cst(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or(value_type val, memory_order order) noexcept                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_raw(value_type val) noexcept                                                                                                                                                            \
        {                                                                                                                                                                                                                          \
            value_type old_value = value_;                                                                                                                                                                                         \
            value_ |= val;                                                                                                                                                                                                         \
            return old_value;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_acquire(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_release(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_relaxed(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_acq_rel(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_or_seq_cst(value_type val) noexcept                                                                                                                                                        \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_ &= val;                                                                                                                                                                                                         \
            return value_;                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type and_fetch_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                       \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and(value_type val, memory_order order) noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_raw(value_type val) noexcept                                                                                                                                                           \
        {                                                                                                                                                                                                                          \
            value_type old_value = value_;                                                                                                                                                                                         \
            value_ &= val;                                                                                                                                                                                                         \
            return old_value;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_acquire(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_release(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_relaxed(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_acq_rel(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
        bq_forceinline value_type fetch_and_seq_cst(value_type val) noexcept                                                                                                                                                       \
        {                                                                                                                                                                                                                          \
            api_type result;                                                                                                                                                                                                       \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, _INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                             \
        }                                                                                                                                                                                                                          \
    };

        template <size_t N>
        struct _atomic_standard_windows_type {
        };

        template <>
        struct _atomic_standard_windows_type<1> {
            typedef char* ptr_type;
            typedef char value_type;
        };

        template <>
        struct _atomic_standard_windows_type<2> {
            typedef short* ptr_type; // int16_t is not compatible to VC++ type short in the intrinsic functions
            typedef short value_type;
        };

        template <>
        struct _atomic_standard_windows_type<4> {
            typedef long* ptr_type; // int32_t is not compatible to VC++ type short in the intrinsic functions
            typedef long value_type;
        };

        template <>
        struct _atomic_standard_windows_type<8> {
            typedef __int64* ptr_type; // int64_t is not compatible to VC++ type short in the intrinsic functions
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
        SPECIALIZED_ATOMIC_BASE(8);
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
