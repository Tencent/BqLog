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

#include "bq_common/bq_common.h"

namespace bq {
    // ChaCha20 as a seekable keystream (RFC 8439 layout: 32-bit block counter, 96-bit nonce).
    // The counter is 32 bits, so one (key, nonce) pair covers 256 GiB; a log segment is far below that.
    class chacha20 {
    public:
        enum class round_count : uint32_t {
            rounds_8 = 8,
            rounds_12 = 12,
            rounds_20 = 20
        };

        static constexpr size_t key_size = 32;
        static constexpr size_t nonce_size = 12;
        static constexpr size_t block_size = 64;

        /// <summary>
        /// XOR the keystream covering [byte_offset, byte_offset + len) into buf, in place.
        /// No alignment requirement on buf; byte_offset is an absolute stream position.
        /// </summary>
        static void keystream_xor(uint8_t* buf, size_t len, const uint8_t* key, const uint8_t* nonce,
            uint64_t byte_offset, round_count rounds = round_count::rounds_20);

        /// <summary>
        /// One 64-byte block at block_index, for per-block derivation (mask rotation, pool conditioning).
        /// </summary>
        static void keystream_block(uint8_t* out64, const uint8_t* key, const uint8_t* nonce,
            uint64_t block_index, round_count rounds = round_count::rounds_20);

#ifdef BQ_UNIT_TEST
        enum class mode : int32_t {
            auto_detect = 0,
            scalar = 1,
            sse2 = 2,
            avx2 = 3,
            neon = 4
        };
        static mode hardware_acceleration_mode_;
        static void set_hardware_acceleration_mode(mode m);
#endif
    };
}
