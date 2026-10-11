/* Copyright (C) 2026 Tencent.
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
// Small integer intrinsics, included by bq_common_public_include.h. Each falls back to its _portable version.
#include <stdint.h>
#include "bq_common/platform/macros.h"
#if defined(BQ_MSVC)
#include <intrin.h>
#endif

namespace bq {
    bq_forceinline uint32_t bq_ctz64_portable(uint64_t value)
    {
        uint32_t index = 0;
        while ((value & 1) == 0) {
            value >>= 1;
            ++index;
        }
        return index;
    }

    // index of the lowest set bit, value must not be 0
    bq_forceinline uint32_t bq_ctz64(uint64_t value)
    {
#if defined(BQ_MSVC)
        unsigned long index;
#if defined(BQ_X86) && !defined(BQ_X86_64)
        if (_BitScanForward(&index, static_cast<unsigned long>(value))) {
            return static_cast<uint32_t>(index);
        }
        _BitScanForward(&index, static_cast<unsigned long>(value >> 32));
        return static_cast<uint32_t>(index) + 32;
#else
        _BitScanForward64(&index, value);
        return static_cast<uint32_t>(index);
#endif
#elif defined(BQ_GCC) || defined(BQ_CLANG)
        return static_cast<uint32_t>(__builtin_ctzll(value));
#else
        return bq_ctz64_portable(value);
#endif
    }

    bq_forceinline uint64_t bq_umul128_portable(uint64_t a, uint64_t b, uint64_t& high)
    {
        const uint64_t a_lo = a & 0xFFFFFFFFULL, a_hi = a >> 32;
        const uint64_t b_lo = b & 0xFFFFFFFFULL, b_hi = b >> 32;
        const uint64_t p0 = a_lo * b_lo, p1 = a_lo * b_hi, p2 = a_hi * b_lo, p3 = a_hi * b_hi;
        const uint64_t mid = (p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
        high = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
        return (mid << 32) | (p0 & 0xFFFFFFFFULL);
    }

    // full 128 bit product of a * b, returns the low 64 bits
    bq_forceinline uint64_t bq_umul128(uint64_t a, uint64_t b, uint64_t& high)
    {
#if defined(BQ_MSVC) && defined(BQ_X86_64)
        return _umul128(a, b, &high);
#elif defined(BQ_MSVC) && defined(BQ_ARM_64)
        high = __umulh(a, b);
        return a * b;
#elif defined(__SIZEOF_INT128__)
        __extension__ typedef unsigned __int128 uint128_type;
        const uint128_type r = static_cast<uint128_type>(a) * b;
        high = static_cast<uint64_t>(r >> 64);
        return static_cast<uint64_t>(r);
#else
        return bq_umul128_portable(a, b, high);
#endif
    }
}
