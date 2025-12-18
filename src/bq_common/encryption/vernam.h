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
 *
 * \file xor.h
 * \date 2025/12/11 
 *
 * \author pippocao
 *
 * \brief
 *
 * Simple Encryption Algorithm
 *
 */

#include "bq_common/bq_common.h"

namespace bq {
    class vernam {
    public:
        static constexpr size_t DEFAULT_BUFFER_ALIGNMENT = 32; //256 bites alignment to improve SIMD performance
        static_assert((DEFAULT_BUFFER_ALIGNMENT& (DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1))) == 0, "DEFAULT_BUFFER_ALIGNMENT must be power of two");
        /// <summary>
        /// Encrypt/Decrypt buffer in-place with Vernam Cipher.
        /// Attention: buf must be aligned to DEFAULT_BUFFER_ALIGNMENT relative to key_stream_offset.
        /// </summary>
        /// <param name="buf"></param>
        /// <param name="len"></param>
        /// <param name="key"></param>
        /// <param name="key_size_pow2"></param>
        /// <param name="key_stream_offset"></param>
        static void vernam_encrypt_32bytes_aligned(uint8_t* BQ_RESTRICT buf, size_t len, const uint8_t* BQ_RESTRICT key, size_t key_size_pow2, size_t key_stream_offset);
    
#ifdef BQ_UNIT_TEST
        static bool hardware_acceleration_enabled_;
        static void set_hardware_acceleration_enabled(bool enabled);
#endif
    };
}
