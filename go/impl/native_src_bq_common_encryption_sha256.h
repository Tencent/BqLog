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
/*!
 * \file sha256.h
 * \date 2026/11/03
 *
 * \author pippocao
 *
 * \brief
 * SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), both incremental.
 *
 */
#pragma once

#include "native_src_bq_common_bq_common.h"

namespace bq {
    class sha256 {
    public:
        static constexpr size_t digest_size = 32;
        static constexpr size_t block_size = 64;

        sha256();

        void reset();
        void update(const void* data, size_t size);

        /// <summary>
        /// Writes the digest and leaves the object reset, ready for a new message.
        /// </summary>
        void finish(uint8_t* out_digest);

        static void hash(const void* data, size_t size, uint8_t* out_digest);

    private:
        void process_block(const uint8_t* block);

        uint32_t state_[8];
        uint64_t total_size_;
        uint8_t buffer_[block_size];
        size_t buffer_size_;
    };

    /// <summary>
    /// Incremental HMAC-SHA256.
    /// </summary>
    class hmac_sha256 {
    public:
        hmac_sha256();

        /// <summary>
        /// Keys longer than one block are hashed first, as RFC 2104 requires.
        /// </summary>
        void reset(const uint8_t* key, size_t key_size);
        void update(const void* data, size_t size);
        void finish(uint8_t* out_digest);

    private:
        sha256 inner_;
        uint8_t outer_key_[sha256::block_size];
    };
}
