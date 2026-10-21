/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
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
#include <intrin.h>
namespace bq {
    namespace platform {
        bq_forceinline void atomic_thread_fence_acquire() noexcept
        {
#if defined(BQ_ARM)
            __dmb(_ARM64_BARRIER_ISHLD);
#else
            _ReadWriteBarrier();
#endif
        }

        bq_forceinline void atomic_thread_fence_release() noexcept
        {
#if defined(BQ_ARM)
            __dmb(_ARM64_BARRIER_ISH);
#else
            _ReadWriteBarrier();
#endif
        }

        template <typename T, size_t N>
        class alignas(8) _atomic_base {
        public:
            static constexpr bool supported = false;

        protected:
            typedef typename bq::decay<T>::type value_type;
            alignas(8) value_type value_;

            bq_forceinline void explicit_copy(const value_type& val)
            {
                value_ = val;
            }
        };

#define BQ_ATOMIC_CONCATX(x, y) x##y

#if defined(BQ_X86)
#define BQ_ATOMIC_INTRIN_RELAXED(x) x
#define BQ_ATOMIC_INTRIN_ACQUIRE(x) x
#define BQ_ATOMIC_INTRIN_RELEASE(x) x
#elif defined(BQ_ARM)
#define BQ_ATOMIC_INTRIN_RELAXED(x) BQ_ATOMIC_CONCATX(x, _nf)
#define BQ_ATOMIC_INTRIN_ACQUIRE(x) BQ_ATOMIC_CONCATX(x, _acq)
#define BQ_ATOMIC_INTRIN_RELEASE(x) BQ_ATOMIC_CONCATX(x, _rel)
#endif

#define BQ_ATOMIC_INTRINSIC_RELAXED(_Result, _Intrinsic, ...) \
    _Result = BQ_ATOMIC_INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_ACQUIRE(_Result, _Intrinsic, ...) \
    _Result = BQ_ATOMIC_INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_RELEASE(_Result, _Intrinsic, ...) \
    _Result = BQ_ATOMIC_INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_ACQ_REL(_Result, _Intrinsic, ...) \
    _Result = _Intrinsic(__VA_ARGS__);

#define BQ_ATOMIC_INTRINSIC_SEQ_CST(_Result, _Intrinsic, ...) \
    _Result = _Intrinsic(__VA_ARGS__);

#define BQ_ATOMIC_CHOOSE_INTRINSIC(_Order, _Result, _Intrinsic, ...) \
    switch (_Order) {                                                \
    case memory_order::relaxed:                                      \
        _Result = BQ_ATOMIC_INTRIN_RELAXED(_Intrinsic)(__VA_ARGS__); \
        break;                                                       \
    case memory_order::acquire:                                      \
        _Result = BQ_ATOMIC_INTRIN_ACQUIRE(_Intrinsic)(__VA_ARGS__); \
        break;                                                       \
    case memory_order::release:                                      \
        _Result = BQ_ATOMIC_INTRIN_RELEASE(_Intrinsic)(__VA_ARGS__); \
        break;                                                       \
    case memory_order::acq_rel:                                      \
        _Result = _Intrinsic(__VA_ARGS__);                           \
        break;                                                       \
    case memory_order::seq_cst:                                      \
        _Result = _Intrinsic(__VA_ARGS__);                           \
        break;                                                       \
    }

#define BQ_ATOMIC_INTRIN_BIT_1(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 8)
#define BQ_ATOMIC_INTRIN_BIT_2(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 16)
#define BQ_ATOMIC_INTRIN_BIT_4(_Intrinsic) _Intrinsic
#define BQ_ATOMIC_INTRIN_BIT_8(_Intrinsic) BQ_ATOMIC_CONCATX(_Intrinsic, 64)

#define BQ_ATOMIC_INTRIN_BIT_SUFFIX(_Intrinsic, SIZE_BYTE) BQ_ATOMIC_CONCATX(BQ_ATOMIC_INTRIN_BIT_, SIZE_BYTE)(_Intrinsic)

#define BQ_ATOMIC_ISO_VOLATILE_BITS_1 8
#define BQ_ATOMIC_ISO_VOLATILE_BITS_2 16
#define BQ_ATOMIC_ISO_VOLATILE_BITS_4 32
#define BQ_ATOMIC_ISO_VOLATILE_BITS_8 64
#define BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE) BQ_ATOMIC_CONCATX(BQ_ATOMIC_ISO_VOLATILE_BITS_, SIZE_BYTE)

// seq_cst stores and read-modify-writes are full barriers, so a seq_cst load only needs acquire
#if defined(BQ_ARM)
// __iso_volatile_load/store has been available since VS2013 and avoids the dmb that the
// /volatile:ms model adds to an ordinary volatile access.
#define BQ_ATOMIC_LOAD_RELAXED(_Bits, _Ptr) BQ_ATOMIC_CONCATX(__iso_volatile_load, _Bits)(reinterpret_cast<const volatile BQ_ATOMIC_CONCATX(__int, _Bits)*>(_Ptr))
#define BQ_ATOMIC_STORE_RELAXED(_Bits, _Ptr, _Value) BQ_ATOMIC_CONCATX(__iso_volatile_store, _Bits)(reinterpret_cast<volatile BQ_ATOMIC_CONCATX(__int, _Bits)*>(_Ptr), static_cast<BQ_ATOMIC_CONCATX(__int, _Bits)>(_Value))
#else
        // x86/x64: an aligned access is atomic and TSO already provides acquire/release, so a volatile
        // access is a bare mov; __iso_volatile_load/store is missing from the toolsets that UE 4.x uses.
        template <typename T>
        bq_forceinline T _atomic_load_relaxed(const T* ptr) noexcept
        {
            return *reinterpret_cast<const volatile T*>(ptr);
        }

        template <typename T>
        bq_forceinline void _atomic_store_relaxed(T* ptr, T value) noexcept
        {
            *reinterpret_cast<volatile T*>(ptr) = value;
        }

#if defined(BQ_X86_32)
        // 32bit x86: the 8 byte access is the only one that is not atomic, it keeps the lock prefix.
        bq_forceinline __int64 _atomic_load_relaxed(const __int64* ptr) noexcept
        {
            return _InterlockedExchangeAdd64(const_cast<__int64 volatile*>(ptr), 0);
        }

        bq_forceinline void _atomic_store_relaxed(__int64* ptr, __int64 value) noexcept
        {
            _InterlockedExchange64(reinterpret_cast<__int64 volatile*>(ptr), value);
        }
#endif

#define BQ_ATOMIC_LOAD_RELAXED(_Bits, _Ptr) _atomic_load_relaxed(_Ptr)
#define BQ_ATOMIC_STORE_RELAXED(_Bits, _Ptr, _Value) _atomic_store_relaxed(_Ptr, _Value)
#endif

#if _MSC_FULL_VER >= 193632407
#define BQ_ATOMIC_HAS_ACQUIRE_LOAD 1
#endif

#if defined(BQ_ARM_64)
#if defined(BQ_ATOMIC_HAS_ACQUIRE_LOAD)
#define BQ_ATOMIC_LOAD_ACQUIRE(_Bits, _Ptr) BQ_ATOMIC_CONCATX(__load_acquire, _Bits)(reinterpret_cast<const volatile unsigned BQ_ATOMIC_CONCATX(__int, _Bits)*>(_Ptr))
#else
#define BQ_ATOMIC_LOAD_ACQUIRE(_Bits, _Ptr) BQ_ATOMIC_CONCATX(__ldar, _Bits)(reinterpret_cast<const volatile unsigned BQ_ATOMIC_CONCATX(__int, _Bits)*>(_Ptr))
#endif
#define BQ_ATOMIC_STORE_RELEASE(_Bits, _Ptr, _Value) BQ_ATOMIC_CONCATX(__stlr, _Bits)(reinterpret_cast<volatile unsigned BQ_ATOMIC_CONCATX(__int, _Bits)*>(_Ptr), static_cast<unsigned BQ_ATOMIC_CONCATX(__int, _Bits)>(_Value))
#else
#if defined(BQ_ARM_32)
#define BQ_ATOMIC_ACQUIRE_RELEASE_BARRIER() __dmb(_ARM_BARRIER_ISH)
#else
#define BQ_ATOMIC_ACQUIRE_RELEASE_BARRIER() _ReadWriteBarrier()
#endif
        template <typename T>
        bq_forceinline T _barrier_after_load(T value) noexcept
        {
            BQ_ATOMIC_ACQUIRE_RELEASE_BARRIER();
            return value;
        }
#define BQ_ATOMIC_LOAD_ACQUIRE(_Bits, _Ptr) _barrier_after_load(BQ_ATOMIC_LOAD_RELAXED(_Bits, _Ptr))
#define BQ_ATOMIC_STORE_RELEASE(_Bits, _Ptr, _Value) (BQ_ATOMIC_ACQUIRE_RELEASE_BARRIER(), BQ_ATOMIC_STORE_RELAXED(_Bits, _Ptr, _Value))
#endif

#define BQ_ATOMIC_SPECIALIZED_BASE(SIZE_BYTE)                                                                                                                                                                                               \
    template <typename T>                                                                                                                                                                                                                   \
    class _atomic_base<T, SIZE_BYTE> {                                                                                                                                                                                                      \
    public:                                                                                                                                                                                                                                 \
        static constexpr bool supported = true;                                                                                                                                                                                             \
                                                                                                                                                                                                                                            \
    protected:                                                                                                                                                                                                                              \
        static_assert(sizeof(T) == SIZE_BYTE, "please don't use bq::platform::_atomic_base, use bq::platform::atomic instead.");                                                                                                            \
        typedef typename bq::decay<T>::type value_type;                                                                                                                                                                                     \
        typedef typename _atomic_standard_windows_type<sizeof(T)>::value_type api_type;                                                                                                                                                     \
        alignas(8) value_type value_;                                                                                                                                                                                                       \
        bq_forceinline void explicit_copy(const value_type& val)                                                                                                                                                                            \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                                   \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load(memory_order order = memory_order::seq_cst) const noexcept                                                                                                                                           \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            if (order == memory_order::relaxed) {                                                                                                                                                                                           \
                result = BQ_ATOMIC_LOAD_RELAXED(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                           \
            } else {                                                                                                                                                                                                                        \
                result = BQ_ATOMIC_LOAD_ACQUIRE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                           \
            }                                                                                                                                                                                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load_raw() const noexcept                                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            return value_;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load_acquire() const noexcept                                                                                                                                                                             \
        {                                                                                                                                                                                                                                   \
            api_type result = BQ_ATOMIC_LOAD_ACQUIRE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                      \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load_relaxed() const noexcept                                                                                                                                                                             \
        {                                                                                                                                                                                                                                   \
            api_type result = BQ_ATOMIC_LOAD_RELAXED(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                      \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load_acq_rel() const noexcept                                                                                                                                                                             \
        {                                                                                                                                                                                                                                   \
            api_type result = BQ_ATOMIC_LOAD_ACQUIRE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                      \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type load_seq_cst() const noexcept                                                                                                                                                                             \
        {                                                                                                                                                                                                                                   \
            api_type result = BQ_ATOMIC_LOAD_ACQUIRE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_));                                                                                                                      \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store(value_type val, memory_order order) noexcept                                                                                                                                                              \
        {                                                                                                                                                                                                                                   \
            if (order == memory_order::relaxed) {                                                                                                                                                                                           \
                BQ_ATOMIC_STORE_RELAXED(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                                                \
            } else if (order == memory_order::release) {                                                                                                                                                                                    \
                BQ_ATOMIC_STORE_RELEASE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                                                \
            } else {                                                                                                                                                                                                                        \
                api_type result;                                                                                                                                                                                                            \
                BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                              \
                (void)result;                                                                                                                                                                                                               \
            }                                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store_raw(value_type val) noexcept                                                                                                                                                                              \
        {                                                                                                                                                                                                                                   \
            value_ = val;                                                                                                                                                                                                                   \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store_release(value_type val) noexcept                                                                                                                                                                          \
        {                                                                                                                                                                                                                                   \
            BQ_ATOMIC_STORE_RELEASE(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                                                    \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store_relaxed(value_type val) noexcept                                                                                                                                                                          \
        {                                                                                                                                                                                                                                   \
            BQ_ATOMIC_STORE_RELAXED(BQ_ATOMIC_ISO_VOLATILE_BITS(SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                                                    \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store_acq_rel(value_type val) noexcept                                                                                                                                                                          \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                                   \
        }                                                                                                                                                                                                                                   \
        bq_forceinline void store_seq_cst(value_type val) noexcept                                                                                                                                                                          \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            (void)result;                                                                                                                                                                                                                   \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange(value_type val, memory_order order) noexcept                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                            \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_raw(value_type val) noexcept                                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            value_type old = value_;                                                                                                                                                                                                        \
            value_ = val;                                                                                                                                                                                                                   \
            return old;                                                                                                                                                                                                                     \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_acquire(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_release(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_relaxed(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_acq_rel(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type exchange_seq_cst(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline bool compare_exchange_weak(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept                       \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(success_order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedCompareExchange, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(desired), get_atomic_value<value_type>(expected)); \
            bool is_suc = ((value_type)result == expected);                                                                                                                                                                                 \
            if (!is_suc) {                                                                                                                                                                                                                  \
                expected = load(fail_order);                                                                                                                                                                                                \
            }                                                                                                                                                                                                                               \
            return is_suc;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline bool compare_exchange_strong(value_type& expected, value_type desired, const memory_order success_order = memory_order::seq_cst, const memory_order fail_order = memory_order::seq_cst) noexcept                     \
        {                                                                                                                                                                                                                                   \
            return compare_exchange_weak(expected, desired, success_order, fail_order);                                                                                                                                                     \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_ += val;                                                                                                                                                                                                                  \
            return value_;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type add_fetch_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result + val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                         \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_type old_value = value_;                                                                                                                                                                                                  \
            value_ += val;                                                                                                                                                                                                                  \
            return old_value;                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_add_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedExchangeAdd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                               \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            return add_fetch(-val, order);                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            return add_fetch_raw(-val);                                                                                                                                                                                                     \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return add_fetch_acquire(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return add_fetch_release(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return add_fetch_relaxed(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return add_fetch_acq_rel(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type sub_fetch_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return add_fetch_seq_cst(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            return fetch_add(-val, order);                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            return fetch_add_raw(-val);                                                                                                                                                                                                     \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return fetch_add_acquire(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return fetch_add_release(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return fetch_add_relaxed(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return fetch_add_acq_rel(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_sub_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            return fetch_add_seq_cst(-val);                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_ ^= val;                                                                                                                                                                                                                  \
            return value_;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type xor_fetch_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result ^ val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_type old_value = value_;                                                                                                                                                                                                  \
            value_ ^= val;                                                                                                                                                                                                                  \
            return old_value;                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_xor_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedXor, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch(value_type val, memory_order order) noexcept                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_raw(value_type val) noexcept                                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            value_ |= val;                                                                                                                                                                                                                  \
            return value_;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_acquire(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_release(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_relaxed(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_acq_rel(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type or_fetch_seq_cst(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result | val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or(value_type val, memory_order order) noexcept                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                  \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_raw(value_type val) noexcept                                                                                                                                                                     \
        {                                                                                                                                                                                                                                   \
            value_type old_value = value_;                                                                                                                                                                                                  \
            value_ |= val;                                                                                                                                                                                                                  \
            return old_value;                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_acquire(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_release(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_relaxed(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_acq_rel(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_or_seq_cst(value_type val) noexcept                                                                                                                                                                 \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedOr, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                        \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_ &= val;                                                                                                                                                                                                                  \
            return value_;                                                                                                                                                                                                                  \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type and_fetch_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result & val;                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and(value_type val, memory_order order) noexcept                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_CHOOSE_INTRINSIC(order, result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                 \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_raw(value_type val) noexcept                                                                                                                                                                    \
        {                                                                                                                                                                                                                                   \
            value_type old_value = value_;                                                                                                                                                                                                  \
            value_ &= val;                                                                                                                                                                                                                  \
            return old_value;                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_acquire(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQUIRE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_release(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELEASE(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_relaxed(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_RELAXED(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_acq_rel(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_ACQ_REL(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
        bq_forceinline value_type fetch_and_seq_cst(value_type val) noexcept                                                                                                                                                                \
        {                                                                                                                                                                                                                                   \
            api_type result;                                                                                                                                                                                                                \
            BQ_ATOMIC_INTRINSIC_SEQ_CST(result, BQ_ATOMIC_INTRIN_BIT_SUFFIX(_InterlockedAnd, SIZE_BYTE), get_atomic_ptr(&value_), get_atomic_value<value_type>(val));                                                                       \
            return (value_type)result;                                                                                                                                                                                                      \
        }                                                                                                                                                                                                                                   \
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
        bq_forceinline typename _atomic_standard_windows_type<sizeof(T)>::value_type get_atomic_value(const T& value)
        {
            return (typename _atomic_standard_windows_type<sizeof(T)>::value_type)(value);
        }

        template <typename T>
        bq_forceinline typename _atomic_standard_windows_type<sizeof(T)>::ptr_type get_atomic_ptr(const T* value)
        {
            return reinterpret_cast<typename _atomic_standard_windows_type<sizeof(T)>::ptr_type>(const_cast<T*>(value));
        }

#pragma warning(push)
#pragma warning(disable : 6001)
#pragma warning(disable : 4701)
        BQ_ATOMIC_SPECIALIZED_BASE(1);
        BQ_ATOMIC_SPECIALIZED_BASE(2);
        BQ_ATOMIC_SPECIALIZED_BASE(4);
        BQ_ATOMIC_SPECIALIZED_BASE(8);
#pragma warning(pop)
    }
}
#endif // BQ_WIN
