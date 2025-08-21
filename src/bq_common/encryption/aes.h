#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * \file aes.h
 * \date 2025/08/15 22:05
 *
 * \author pippocao
 *
 * \brief
 *
 * Simple AES implementation supporting ECB, CBC, CFB, OFB, CTR modes with 128, 192, and 256-bit keys.
 *
 */

#include "bq_common/bq_common.h"

namespace bq {

    class aes {
    public:
        enum class enum_cipher_mode {
            AES_ECB = 0, // Electronic Codebook
            AES_CBC = 1, // Cipher Block Chaining
            AES_CFB = 2, // Cipher Feedback
            AES_OFB = 3, // Output Feedback
            AES_CTR = 4 // Counter Mode
        };
        enum class enum_key_bits {
            AES_128 = 128,
            AES_192 = 192,
            AES_256 = 256
        };
        using key_type = bq::array<uint8_t>;
        using iv_type = bq::array<uint8_t>;

    public:
        aes(enum_cipher_mode mode, enum_key_bits key_bits);

        key_type generate_key() const;

        iv_type generate_iv() const;

        bq_forceinline size_t get_key_size() const {
            return key_size_;
        }

        bq_forceinline size_t get_iv_size() const {
            return iv_size_;
        }

        bool encrypt(const key_type& key, const iv_type& iv, const uint8_t* plaintext, size_t plaintext_size, uint8_t* out_ciphertext, size_t out_ciphertext_size) const;
        bool decrypt(const key_type& key, const iv_type& iv, const uint8_t* ciphertext, size_t ciphertext_size, uint8_t* out_plaintext, size_t out_plaintext_size) const;

        template<typename Allocator>
        bool encrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t, Allocator>& plaintext, bq::array<uint8_t>& out_ciphertext) const
        {
            out_ciphertext.clear();
            out_ciphertext.fill_uninitialized(plaintext.size());
            return encrypt(key, iv, plaintext.begin(), plaintext.size(), out_ciphertext.begin(), out_ciphertext.size());
        }
        template<typename Allocator>
        bool decrypt(const key_type& key, const iv_type& iv, const bq::array<uint8_t, Allocator>& ciphertext, bq::array<uint8_t>& out_plaintext) const
        {
            out_plaintext.clear();
            out_plaintext.fill_uninitialized(ciphertext.size());
            return decrypt(key, iv, ciphertext.begin(), ciphertext.size(), out_plaintext.begin(), out_plaintext.size());
        }

    private:
        enum_cipher_mode mode_;
        enum_key_bits key_bits_;
        size_t key_size_;
        size_t iv_size_;

    private:
        // helper to fill random bytes
        static void fill_random(bq::array<uint8_t>& data);

        static bool is_block_aligned(size_t n)
        {
            return (n % 16u) == 0u;
        }
    };
}
