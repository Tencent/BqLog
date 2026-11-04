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
 * \file hkdf.h
 * \date 2026/11/03
 *
 * \author pippocao
 *
 * \brief
 * HKDF-SHA256 (RFC 5869), the extract-then-expand construction the segment key derivation uses.
 *
 */
#pragma once

#include "bq_common/encryption/sha256.h"

namespace bq {
    class hkdf_sha256 {
    public:
        // 32, the SHA-256 digest size. Spelled out rather than taken from sha256::digest_size:
        // bq_common.h pulls in both headers, so this one can be parsed before that class exists.
        // hkdf.cpp asserts the two agree.
        static constexpr size_t hash_size = 32;
        static constexpr size_t max_output_size = 255 * hash_size;

        /// <summary>
        /// Extract then expand. An empty salt is allowed and is treated as hash_size zero bytes, as
        /// the RFC requires. Returns false only when the requested output length is out of range.
        /// </summary>
        static bool derive(const void* salt, size_t salt_size,
            const void* ikm, size_t ikm_size,
            const void* info, size_t info_size,
            void* out, size_t out_size);

        /// <summary>
        /// The expand half alone, for a caller that already holds a pseudorandom key.
        /// </summary>
        static bool expand(const uint8_t prk[hash_size],
            const void* info, size_t info_size,
            void* out, size_t out_size);
    };
}
