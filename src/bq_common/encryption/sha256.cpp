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
 * \file sha256.cpp
 * \date 2026/11/03
 *
 * \author pippocao
 *
 * \brief
 * SHA-256 and HMAC-SHA256.
 *
 */

#include "bq_common/encryption/sha256.h"

namespace bq {
    namespace {
        constexpr uint32_t sha256_round_constants[64] = {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
        };

        bq_forceinline uint32_t rotr32(uint32_t v, uint32_t c)
        {
            return (v >> c) | (v << (32U - c));
        }

        bq_forceinline uint32_t load_be32(const uint8_t* p)
        {
            return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
                | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
        }

        bq_forceinline void store_be32(uint8_t* p, uint32_t v)
        {
            p[0] = static_cast<uint8_t>(v >> 24);
            p[1] = static_cast<uint8_t>(v >> 16);
            p[2] = static_cast<uint8_t>(v >> 8);
            p[3] = static_cast<uint8_t>(v);
        }
    }

    sha256::sha256()
    {
        reset();
    }

    void sha256::reset()
    {
        state_[0] = 0x6a09e667U;
        state_[1] = 0xbb67ae85U;
        state_[2] = 0x3c6ef372U;
        state_[3] = 0xa54ff53aU;
        state_[4] = 0x510e527fU;
        state_[5] = 0x9b05688cU;
        state_[6] = 0x1f83d9abU;
        state_[7] = 0x5be0cd19U;
        total_size_ = 0;
        buffer_size_ = 0;
    }

    void sha256::process_block(const uint8_t* block)
    {
        uint32_t w[64];
        for (size_t i = 0; i < 16; ++i) {
            w[i] = load_be32(block + i * 4);
        }
        for (size_t i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = state_[0];
        uint32_t b = state_[1];
        uint32_t c = state_[2];
        uint32_t d = state_[3];
        uint32_t e = state_[4];
        uint32_t f = state_[5];
        uint32_t g = state_[6];
        uint32_t h = state_[7];
        for (size_t i = 0; i < 64; ++i) {
            const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            const uint32_t choose = (e & f) ^ (~e & g);
            const uint32_t temp1 = h + s1 + choose + sha256_round_constants[i] + w[i];
            const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    void sha256::update(const void* data, size_t size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        total_size_ += size;
        while (size > 0) {
            const size_t room = block_size - buffer_size_;
            const size_t take = (size < room) ? size : room;
            memcpy(buffer_ + buffer_size_, bytes, take);
            buffer_size_ += take;
            bytes += take;
            size -= take;
            if (buffer_size_ == block_size) {
                process_block(buffer_);
                buffer_size_ = 0;
            }
        }
    }

    void sha256::finish(uint8_t* out_digest)
    {
        // Length in bits, captured before the padding is fed in as more message bytes.
        const uint64_t bit_size = total_size_ * 8U;
        uint8_t pad[block_size + 8];
        memset(pad, 0, sizeof(pad));
        pad[0] = 0x80;
        const size_t pad_size = (buffer_size_ < 56U) ? (56U - buffer_size_) : (120U - buffer_size_);
        update(pad, pad_size);
        uint8_t length_bytes[8];
        for (size_t i = 0; i < 8; ++i) {
            length_bytes[7U - i] = static_cast<uint8_t>(bit_size >> (8U * i));
        }
        update(length_bytes, sizeof(length_bytes));
        for (size_t i = 0; i < 8; ++i) {
            store_be32(out_digest + i * 4, state_[i]);
        }
        reset();
    }

    void sha256::hash(const void* data, size_t size, uint8_t* out_digest)
    {
        sha256 hasher;
        hasher.update(data, size);
        hasher.finish(out_digest);
    }

    hmac_sha256::hmac_sha256()
    {
        memset(outer_key_, 0, sizeof(outer_key_));
    }

    void hmac_sha256::reset(const uint8_t* key, size_t key_size)
    {
        uint8_t block_key[sha256::block_size];
        memset(block_key, 0, sizeof(block_key));
        if (key_size > sha256::block_size) {
            sha256::hash(key, key_size, block_key);
        } else {
            memcpy(block_key, key, key_size);
        }
        uint8_t inner_key[sha256::block_size];
        for (size_t i = 0; i < sha256::block_size; ++i) {
            inner_key[i] = static_cast<uint8_t>(block_key[i] ^ 0x36U);
            outer_key_[i] = static_cast<uint8_t>(block_key[i] ^ 0x5cU);
        }
        memset(block_key, 0, sizeof(block_key));
        inner_.reset();
        inner_.update(inner_key, sizeof(inner_key));
        memset(inner_key, 0, sizeof(inner_key));
    }

    void hmac_sha256::update(const void* data, size_t size)
    {
        inner_.update(data, size);
    }

    void hmac_sha256::finish(uint8_t* out_digest)
    {
        uint8_t inner_digest[sha256::digest_size];
        inner_.finish(inner_digest);
        sha256 outer;
        outer.update(outer_key_, sizeof(outer_key_));
        outer.update(inner_digest, sizeof(inner_digest));
        outer.finish(out_digest);
        memset(inner_digest, 0, sizeof(inner_digest));
        memset(outer_key_, 0, sizeof(outer_key_));
    }
}
