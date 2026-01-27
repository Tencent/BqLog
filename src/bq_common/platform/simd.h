#pragma once
/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "bq_common/platform/macros.h"

#if defined(BQ_X86)
    #ifdef BQ_MSVC
        #include <intrin.h>
    #else
        // For MinGW/Clang, standard headers like immintrin.h often hide AVX/SSE types
        // unless -mavx/-msse4.2 is passed globally. However, for runtime dispatch (target attributes),
        // we need the types/intrinsics to be visible even if the global build is generic x86_64.
        #ifndef __AVX2__
            #define __AVX__
            #define __AVX2__
            #define __SSE4_1__
            #define __SSE4_2__
            #include <immintrin.h>
            #undef __AVX__
            #undef __AVX2__
            #undef __SSE4_1__
            #undef __SSE4_2__
        #else
            #include <immintrin.h>
        #endif
    #endif
#elif defined(BQ_ARM)
    #if defined(BQ_MSVC)
        #include <intrin.h>
        #include <arm64_neon.h>
    #else
        #include <arm_acle.h>
    #endif
    #if defined(BQ_ARM_NEON)
        #include <arm_neon.h>
    #endif
#endif

namespace bq {
    extern bool _bq_crc32_supported_;

#if defined(BQ_X86)
    extern bool _bq_avx2_supported_;
#endif

    // Internal flag to check if SIMD UTF is supported on current platform
    constexpr bool _bq_utf_simd_supported_ = 
#if defined(BQ_X86)
        // Assume basic SSE is available on modern x86 (including Android x86/Atom)
        true;
#elif defined(BQ_ARM_NEON)
        true; // Assume NEON on ARMv8/M1
#else
        false;
#endif
        

    // Helpers for NEON
#if defined(BQ_ARM_NEON)
    bq_forceinline uint16_t bq_vmaxvq_u16(uint16x8_t v) {
#if defined(BQ_ARM_64)
        return vmaxvq_u16(v);
#else
        uint16x4_t hi = vget_high_u16(v);
        uint16x4_t lo = vget_low_u16(v);
        uint16x4_t m = vmax_u16(hi, lo);
        m = vpmax_u16(m, m);
        m = vpmax_u16(m, m);
        return vget_lane_u16(m, 0);
#endif
    }

    bq_forceinline uint8_t bq_vmaxvq_u8(uint8x16_t v) {
#if defined(BQ_ARM_64)
        return vmaxvq_u8(v);
#else
        uint8x8_t hi = vget_high_u8(v);
        uint8x8_t lo = vget_low_u8(v);
        uint8x8_t m = vmax_u8(hi, lo);
        m = vpmax_u8(m, m);
        m = vpmax_u8(m, m);
        m = vpmax_u8(m, m);
        return vget_lane_u8(m, 0);
#endif
    }
#endif

}