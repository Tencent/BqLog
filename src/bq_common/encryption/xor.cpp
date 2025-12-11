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
 * \file xor.cpp
 * \date 2025/12/11
 *
 * \author pippocao
 *
 * \brief
 *
 * Simple Encryption Algorithm
 *
 */

#include "bq_common/encryption/xor.h"

namespace bq {
    
    void xor::xor_encrypt_32bytes_aligned(uint8_t* buf, size_t len, const uint8_t* key, size_t key_size_pow2, size_t key_stream_offset)
    {
        if (len == 0) {
            return;
        }
        const size_t key_mask = key_size_pow2 - 1;
        constexpr size_t align_mask = DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1);

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & 7u) == 0 && "key_size_pow2 should be multiple of 8 for u64 path");
        assert(((static_cast<size_t>(reinterpret_cast<uintptr_t>(buf)) & align_mask) == (key_stream_offset & align_mask)) && "xor_encrypt_32bytes_aligned: relative alignment mismatch");
#endif
        uint8_t* p = buf;
        size_t remaining = len;
        size_t current_key_pos = key_stream_offset & key_mask;

        // 1. Align 'p' (buffer) to 32-byte boundary (AVX2 preferred alignment)
        // Aligning 'p' also aligns 'key' to 32-byte
        size_t align_diff = (DEFAULT_BUFFER_ALIGNMENT - (reinterpret_cast<uintptr_t>(p) & align_mask)) & align_mask;
        size_t head_len = (align_diff < remaining) ? align_diff : remaining;

        for (size_t j = 0; j < head_len; ++j) {
            p[j] ^= key[current_key_pos];
            // Optimization: key_size > 64 (power of 2) guaranteed by user.
            // head_len < 32 (max align_diff).
            // So current_key_pos will NOT wrap around here.
            current_key_pos++;
        }
        p += head_len;
        remaining -= head_len;

        if (remaining == 0) return;

        uint64_t* p64 = reinterpret_cast<uint64_t*>(p);

        // High-performance loop
        while (remaining >= 32) {
            size_t contiguous_key_len = key_size_pow2 - current_key_pos;
            size_t chunk_len = (contiguous_key_len < remaining) ? contiguous_key_len : remaining;

            size_t loop_len = chunk_len & ~align_mask;
            size_t u64_count = loop_len >> 3;

            const uint64_t* k64 = reinterpret_cast<const uint64_t*>(key + current_key_pos);

            constexpr size_t unroll_num_per_loop = (DEFAULT_BUFFER_ALIGNMENT >> 3);
            // Manually unroll 4x uint64_t to encourage generation of 256-bit SIMD instructions (AVX2)
            size_t j = 0;
            for (; j + (unroll_num_per_loop - static_cast<size_t>(1)) < u64_count; j += unroll_num_per_loop) {
                p64[j] ^= k64[j];
                p64[j + 1] ^= k64[j + 1];
                p64[j + 2] ^= k64[j + 2];
                p64[j + 3] ^= k64[j + 3];
            }

            // Advance pointers
            p += loop_len;
            p64 += u64_count;
            remaining -= loop_len;
            current_key_pos += loop_len;

            if (current_key_pos == key_size_pow2) {
                current_key_pos = 0; // Wrap around
            }
        }

        current_key_pos &= key_mask; //may be wrap in phase 1, and skip phase 2, which will lead to overflow.
        // 3. Tail
        for (size_t j = 0; j < remaining; ++j) {
            p[j] ^= key[current_key_pos];
            current_key_pos = (current_key_pos + 1) & key_mask;
        }
    }

}
