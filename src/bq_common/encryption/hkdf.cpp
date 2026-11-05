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
 * \file hkdf.cpp
 * \date 2026/11/03
 *
 * \author pippocao
 *
 * \brief
 * HKDF-SHA256 (RFC 5869).
 *
 */

#include "bq_common/encryption/hkdf.h"

namespace bq {
    static_assert(hkdf_sha256::hash_size == sha256::digest_size, "HKDF is defined over SHA-256 here");
    static_assert(hkdf_sha256::max_output_size == 255 * sha256::digest_size, "RFC 5869 caps the output at 255 blocks");

    bool hkdf_sha256::derive(const void* salt, size_t salt_size,
        const void* ikm, size_t ikm_size,
        const void* info, size_t info_size,
        void* out, size_t out_size)
    {
        // Extract: PRK = HMAC(salt, IKM), with an absent salt standing in as hash_size zero bytes.
        uint8_t prk[hash_size];
        {
            hmac_sha256 extractor;
            if (salt == nullptr || salt_size == 0) {
                static const uint8_t empty_salt[hash_size] = { 0 };
                extractor.reset(empty_salt, sizeof(empty_salt));
            } else {
                extractor.reset(static_cast<const uint8_t*>(salt), salt_size);
            }
            extractor.update(ikm, ikm_size);
            extractor.finish(prk);
        }
        const bool ok = expand(prk, info, info_size, out, out_size);
        memset(prk, 0, sizeof(prk));
        return ok;
    }

    bool hkdf_sha256::expand(const uint8_t prk[hash_size],
        const void* info, size_t info_size,
        void* out, size_t out_size)
    {
        if (out_size > max_output_size) {
            return false;
        }
        uint8_t* dst = static_cast<uint8_t*>(out);
        uint8_t previous[hash_size];
        size_t previous_size = 0;
        size_t produced = 0;
        uint8_t counter = 1;
        while (produced < out_size) {
            // T(i) = HMAC(PRK, T(i-1) | info | i), and the stream is a prefix of the concatenation.
            hmac_sha256 expander;
            expander.reset(prk, hash_size);
            if (previous_size != 0) {
                expander.update(previous, previous_size);
            }
            if (info_size != 0) {
                expander.update(info, info_size);
            }
            expander.update(&counter, 1);
            expander.finish(previous);
            previous_size = hash_size;
            const size_t take = (out_size - produced < hash_size) ? (out_size - produced) : hash_size;
            memcpy(dst + produced, previous, take);
            produced += take;
            counter = static_cast<uint8_t>(counter + 1U);
        }
        memset(previous, 0, sizeof(previous));
        return true;
    }
}
