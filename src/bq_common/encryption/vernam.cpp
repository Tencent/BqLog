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
 *
 * \file vernam.cpp
 * \date 2025/12/11
 *
 * \author pippocao
 *
 * \brief
 *
 * Simple Encryption Algorithm
 *
 */

#include "bq_common/encryption/vernam.h"
#include "bq_common/platform/macros.h"
namespace bq {

    // =================================================================================================
    // Implementations
    // =================================================================================================
#ifdef BQ_UNIT_TEST
    vernam::mode vernam::hardware_acceleration_mode_ = vernam::mode::auto_detect;

    void vernam::set_hardware_acceleration_mode(mode m)
    {
        hardware_acceleration_mode_ = m;
    }
#endif

// Define scalar if we are in unit test (to allow fallback) OR if we don't have any HW acceleration available
#if defined(BQ_UNIT_TEST) || (!defined(BQ_X86) && !defined(BQ_ARM_NEON))
    // -------------------------------------------------------------------------------------------------
    static void vernam_encrypt_scalar(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset)
    {
        if (len == 0)
            return;

        const size_t key_mask = key_size_pow2 - 1;
        constexpr size_t align_mask = 7;

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & align_mask) == 0 && "vernam_encrypt_scalar key_size_pow2 should be multiple of 8 for u64 path");
        assert(((static_cast<size_t>(reinterpret_cast<uintptr_t>(buf)) & align_mask) == (key_stream_offset & align_mask)) && "vernam_encrypt_32bytes_aligned: relative alignment mismatch");
#endif

        uint8_t* p = buf;
        size_t remaining = len;
        size_t current_key_pos = key_stream_offset & key_mask;

        // Head alignment loop (align to 8 bytes for uint64_t optimization)
        size_t align_diff = (8 - (reinterpret_cast<uintptr_t>(p) & align_mask)) & align_mask;
        size_t head_len = (align_diff < remaining) ? align_diff : remaining;

        for (size_t j = 0; j < head_len; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }

        p += head_len;
        remaining -= head_len;

        if (remaining == 0)
            return;

        // 64-bit Scalar Loop
        uint64_t* p64 = reinterpret_cast<uint64_t*>(p);
        while (remaining >= sizeof(uint64_t)) {
            size_t contiguous_key_len = key_size_pow2 - current_key_pos;
            size_t chunk_len = (contiguous_key_len < remaining) ? contiguous_key_len : remaining;

            // Align chunk_len down to 8 bytes
            size_t loop_len = chunk_len & ~align_mask;
            size_t u64_count = loop_len >> 3;

            const uint64_t* k64 = reinterpret_cast<const uint64_t*>(key + current_key_pos);

            for (size_t i = 0; i < u64_count; ++i) {
                p64[i] ^= k64[i];
            }

            p += loop_len;
            p64 += u64_count;
            remaining -= loop_len;
            current_key_pos += loop_len;

            if (current_key_pos == key_size_pow2) {
                current_key_pos = 0;
            }
        }

        // Tail
        current_key_pos &= key_mask;
        for (size_t j = 0; j < remaining; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
    }
#endif

#if defined(BQ_X86)

    // 2. SSE Implementation (x86/x64)
    // -------------------------------------------------------------------------------------------------
    static BQ_HW_SIMD_SSE_TARGET void vernam_encrypt_sse(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset)
    {
        const size_t key_mask = key_size_pow2 - 1;
        constexpr size_t align_mask = 15; // 16-byte alignment

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & align_mask) == 0 && "vernam_encrypt_sse key_size_pow2 should be multiple of 16");
#endif

        uint8_t* p = buf;
        size_t remaining = len;
        size_t current_key_pos = key_stream_offset & key_mask;

        // Alignment to 16 bytes
        size_t align_diff = (16 - (reinterpret_cast<uintptr_t>(p) & align_mask)) & align_mask;
        size_t head_len = (align_diff < remaining) ? align_diff : remaining;

        for (size_t j = 0; j < head_len; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
        p += head_len;
        remaining -= head_len;

        if (remaining < 16) {
            current_key_pos &= key_mask;
            for (size_t j = 0; j < remaining; ++j) {
                p[j] ^= key[current_key_pos];
                current_key_pos = (current_key_pos + 1) & key_mask;
            }
            return;
        }

        // SSE Loop
        while (remaining >= 16) {
            size_t contiguous_key_len = key_size_pow2 - current_key_pos;
            size_t chunk_len = (contiguous_key_len < remaining) ? contiguous_key_len : remaining;
            size_t loop_len = chunk_len & ~align_mask;

            const uint8_t* k_ptr = key + current_key_pos;
            size_t num_blocks = loop_len >> 4;

            for (size_t i = 0; i < num_blocks; ++i) {
                __m128i v_buf = _mm_load_si128(reinterpret_cast<__m128i*>(p));
                __m128i v_key = _mm_loadu_si128(reinterpret_cast<const __m128i*>(k_ptr));

                __m128i v_res = _mm_xor_si128(v_buf, v_key);

                _mm_store_si128(reinterpret_cast<__m128i*>(p), v_res);

                p += 16;
                k_ptr += 16;
            }

            remaining -= loop_len;
            current_key_pos += loop_len;

            if (current_key_pos == key_size_pow2) {
                current_key_pos = 0;
            }
        }

        current_key_pos &= key_mask;
        for (size_t j = 0; j < remaining; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
    }

    // 2. AVX2 Implementation (x86/x64 Only)
    // -------------------------------------------------------------------------------------------------
    // We use intrinsics to FORCE AVX2 instructions regardless of compiler flags.
    // This function is only called if g_has_avx2 is true.
    static BQ_HW_SIMD_TARGET void vernam_encrypt_avx2(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset)
    {
        // ... (Header alignment same as scalar, omitted for brevity, usually caller handles large chunks) ...
        // Re-implementing logic with AVX2 intrinsics

        const size_t key_mask = key_size_pow2 - 1;
        constexpr size_t align_mask = 31; // 32-byte alignment for AVX2

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & align_mask) == 0 && "vernam_encrypt_avx2 key_size_pow2 should be multiple of 32");
        assert(((static_cast<size_t>(reinterpret_cast<uintptr_t>(buf)) & align_mask) == (key_stream_offset & align_mask)) && "vernam_encrypt_avx2: relative alignment mismatch");
#endif

        uint8_t* p = buf;
        size_t remaining = len;
        size_t current_key_pos = key_stream_offset & key_mask;

        // Alignment to 32 bytes
        size_t align_diff = (32 - (reinterpret_cast<uintptr_t>(p) & align_mask)) & align_mask;
        size_t head_len = (align_diff < remaining) ? align_diff : remaining;

        for (size_t j = 0; j < head_len; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos++;
        }
        p += head_len;
        remaining -= head_len;

        if (remaining < 32) {
            // Fallback to scalar tail handling if remaining is small
            current_key_pos &= key_mask;
            for (size_t j = 0; j < remaining; ++j) {
                p[j] ^= key[current_key_pos];
                current_key_pos = (current_key_pos + 1) & key_mask;
            }
            return;
        }

        // AVX2 Loop
        while (remaining >= 32) {
            size_t contiguous_key_len = key_size_pow2 - current_key_pos;
            size_t chunk_len = (contiguous_key_len < remaining) ? contiguous_key_len : remaining;

            size_t loop_len = chunk_len & ~align_mask; // multiple of 32

            const uint8_t* k_ptr = key + current_key_pos;

            // Using intrinsics
            size_t num_blocks = loop_len >> 5; // divide by 32
            for (size_t i = 0; i < num_blocks; ++i) {
                // Load 32 bytes from buffer (aligned)
                __m256i v_buf = _mm256_load_si256(reinterpret_cast<__m256i*>(p));
                // Load 32 bytes from key (unaligned potentially, loadu handles it)
                __m256i v_key = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(k_ptr));

                // VERNAM
                __m256i v_res = _mm256_xor_si256(v_buf, v_key);

                // Store back (aligned)
                _mm256_store_si256(reinterpret_cast<__m256i*>(p), v_res);

                p += 32;
                k_ptr += 32;
            }

            remaining -= loop_len;
            current_key_pos += loop_len;

            if (current_key_pos == key_size_pow2) {
                current_key_pos = 0;
            }
        }

        current_key_pos &= key_mask;
        for (size_t j = 0; j < remaining; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
    }
#endif

#if defined(BQ_ARM) && defined(BQ_ARM_NEON)
    // 3. NEON Implementation (ARMv7 / ARMv8)
    // -------------------------------------------------------------------------------------------------
    // ARMv8 (AArch64) guarantees NEON support.
    // ARMv7 usually supports it (Android requires it for most ABIs now).
    static void vernam_encrypt_neon(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset)
    {
        const size_t key_mask = key_size_pow2 - 1;
        constexpr size_t align_mask = 15; // 16-byte alignment for NEON

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & align_mask) == 0 && "vernam_encrypt_neon key_size_pow2 should be multiple of 16");
        assert(((static_cast<size_t>(reinterpret_cast<uintptr_t>(buf)) & align_mask) == (key_stream_offset & align_mask)) && "vernam_encrypt_neon: relative alignment mismatch");
#endif

        uint8_t* p = buf;
        size_t remaining = len;
        size_t current_key_pos = key_stream_offset & key_mask;

        // Alignment to 16 bytes
        size_t align_diff = (16 - (reinterpret_cast<uintptr_t>(p) & align_mask)) & align_mask;
        size_t head_len = (align_diff < remaining) ? align_diff : remaining;

        for (size_t j = 0; j < head_len; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos++;
        }
        p += head_len;
        remaining -= head_len;

        // NEON Loop (128-bit)
        while (remaining >= 16) {
            size_t contiguous_key_len = key_size_pow2 - current_key_pos;
            size_t chunk_len = (contiguous_key_len < remaining) ? contiguous_key_len : remaining;
            size_t loop_len = chunk_len & ~align_mask;

            const uint8_t* k_ptr = key + current_key_pos;
            size_t num_blocks = loop_len >> 4; // divide by 16

            for (size_t i = 0; i < num_blocks; ++i) {
                // vld1q_u8 loads 128-bit (16 bytes)
                uint8x16_t v_buf = vld1q_u8(p);
                uint8x16_t v_key = vld1q_u8(k_ptr);

                // veorq_u8 does VERNAM
                uint8x16_t v_res = veorq_u8(v_buf, v_key);

                // vst1q_u8 stores 128-bit
                vst1q_u8(p, v_res);

                p += 16;
                k_ptr += 16;
            }

            remaining -= loop_len;
            current_key_pos += loop_len;

            if (current_key_pos == key_size_pow2) {
                current_key_pos = 0;
            }
        }

        current_key_pos &= key_mask;
        for (size_t j = 0; j < remaining; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
    }
#endif

    // =================================================================================================
    // Public Entry Point (Dispatcher)
    // =================================================================================================

    void vernam::vernam_encrypt_32bytes_aligned(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset)
    {
#if defined(BQ_X86)
#ifdef BQ_UNIT_TEST
        if (hardware_acceleration_mode_ == mode::scalar) {
            vernam_encrypt_scalar(buf, len, key, key_size_pow2, key_stream_offset);
            return;
        }
        if (hardware_acceleration_mode_ == mode::sse) {
            vernam_encrypt_sse(buf, len, key, key_size_pow2, key_stream_offset);
            return;
        }
#endif
        if (common_global_vars::get().avx2_support_) {
            vernam_encrypt_avx2(buf, len, key, key_size_pow2, key_stream_offset);
            return;
        }
        // Fallback for non-AVX2 x86 (Use SSE)
        vernam_encrypt_sse(buf, len, key, key_size_pow2, key_stream_offset);
#elif defined(BQ_ARM)
#ifdef BQ_UNIT_TEST
        if (hardware_acceleration_mode_ == mode::scalar) {
            vernam_encrypt_scalar(buf, len, key, key_size_pow2, key_stream_offset);
            return;
        }
#endif
#if defined(BQ_ARM_NEON)
        // For Android/iOS ARM64/ARMv7, NEON is effectively standard.
        // We can use it directly without complex runtime checks for most modern contexts.
        vernam_encrypt_neon(buf, len, key, key_size_pow2, key_stream_offset);
#else
        vernam_encrypt_scalar(buf, len, key, key_size_pow2, key_stream_offset);
#endif
#else
        // Fallback for non-AVX2 x86 or other architectures
        vernam_encrypt_scalar(buf, len, key, key_size_pow2, key_stream_offset);
#endif
    }

}
