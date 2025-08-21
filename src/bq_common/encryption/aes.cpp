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
 * \file aes.cpp
 * \date 2025/08/15 22:05
 *
 * \author pippocao
 *
 * \brief
 *
 * Simple AES implementation supporting ECB, CBC, CFB, OFB, CTR modes with 128, 192, and 256-bit keys.
 *
 */

#include <string.h>
#include "bq_common/encryption/aes.h"

namespace bq {
    class aes_core {
    private:
        aes::enum_cipher_mode mode_;
        size_t nb_;
        size_t key_size_;
        size_t nk_;
        size_t nr_;
        size_t round_key_words_;
        size_t round_key_bytes_;
        bq::array<uint8_t> round_key_;

    public:
        aes_core(aes::enum_cipher_mode mode, aes::enum_key_bits key_bits)
            : mode_(mode)
            , nb_(4)
            , key_size_(static_cast<size_t>(key_bits) / 8)
            , nk_(key_size_ / 4)
            , nr_(nk_ + 6)
            , round_key_words_(nb_ * (nr_ + 1))
            , round_key_bytes_(round_key_words_ * 4)
        {
            (void)mode_;
            round_key_.fill_uninitialized(round_key_bytes_);
        }

        static constexpr uint8_t xtime(uint8_t x)
        {
            return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
        }

        static uint8_t gf_mul(uint8_t a, uint8_t b)
        {
            uint8_t p = 0;
            for (int32_t i = 0; i < 8; ++i) {
                if (b & 1)
                    p ^= a;
                uint8_t hi = a & 0x80;
                a <<= 1;
                if (hi)
                    a ^= 0x1b;
                b >>= 1;
            }
            return p;
        }

        static inline void xor_block(uint8_t* dst, const uint8_t* a, const uint8_t* b)
        {
            for (int32_t i = 0; i < 16; ++i)
                dst[i] = static_cast<uint8_t>(a[i] ^ b[i]);
        }

        static inline void add_round_key(uint8_t* state, const uint8_t* round_key_)
        {
            for (int32_t i = 0; i < 16; ++i)
                state[i] ^= round_key_[i];
        }

        static inline uint8_t rcon(uint8_t i)
        {
            uint8_t c = 1;
            if (i == 0)
                return 0;
            while (i > 1) {
                c = xtime(c);
                --i;
            }
            return c;
        }

        static uint8_t sbox_[256];
        static uint8_t rsbox_[256];

        void key_expansion(const aes::key_type& key)
        {
            // first nk_ words (key)
            for (size_t i = 0; i < nk_; ++i) {
                round_key_[4 * i + 0] = key[4 * i + 0];
                round_key_[4 * i + 1] = key[4 * i + 1];
                round_key_[4 * i + 2] = key[4 * i + 2];
                round_key_[4 * i + 3] = key[4 * i + 3];
            }
            uint8_t temp[4];
            for (size_t i = nk_; i < round_key_words_; ++i) {
                temp[0] = round_key_[4 * (i - 1) + 0];
                temp[1] = round_key_[4 * (i - 1) + 1];
                temp[2] = round_key_[4 * (i - 1) + 2];
                temp[3] = round_key_[4 * (i - 1) + 3];

                if (i % nk_ == 0) {
                    // RotWord
                    uint8_t t = temp[0];
                    temp[0] = temp[1];
                    temp[1] = temp[2];
                    temp[2] = temp[3];
                    temp[3] = t;
                    // SubWord
                    temp[0] = sbox_[temp[0]];
                    temp[1] = sbox_[temp[1]];
                    temp[2] = sbox_[temp[2]];
                    temp[3] = sbox_[temp[3]];
                    // Rcon
                    temp[0] ^= rcon(static_cast<uint8_t>(i / nk_));
                } else if (nk_ > 6 && i % nk_ == 4) {
                    // ext subword for AES_256
                    temp[0] = sbox_[temp[0]];
                    temp[1] = sbox_[temp[1]];
                    temp[2] = sbox_[temp[2]];
                    temp[3] = sbox_[temp[3]];
                }

                round_key_[4 * i + 0] = static_cast<uint8_t>(round_key_[4 * (i - nk_) + 0] ^ temp[0]);
                round_key_[4 * i + 1] = static_cast<uint8_t>(round_key_[4 * (i - nk_) + 1] ^ temp[1]);
                round_key_[4 * i + 2] = static_cast<uint8_t>(round_key_[4 * (i - nk_) + 2] ^ temp[2]);
                round_key_[4 * i + 3] = static_cast<uint8_t>(round_key_[4 * (i - nk_) + 3] ^ temp[3]);
            }
        }

        static inline void sub_bytes(uint8_t* s)
        {
            for (int32_t i = 0; i < 16; ++i) {
                s[i] = sbox_[s[i]];
            }
        }
        static inline void inv_sub_bytes(uint8_t* s)
        {
            for (int32_t i = 0; i < 16; ++i) {
                s[i] = rsbox_[s[i]];
            }
        }

        static inline void shift_rows(uint8_t* s)
        {
            uint8_t t;
            // row 1
            t = s[1];
            s[1] = s[5];
            s[5] = s[9];
            s[9] = s[13];
            s[13] = t;
            // row 2
            t = s[2];
            s[2] = s[10];
            s[10] = t;
            t = s[6];
            s[6] = s[14];
            s[14] = t;
            // row 3
            t = s[15];
            s[15] = s[11];
            s[11] = s[7];
            s[7] = s[3];
            s[3] = t;
        }
        static inline void inv_shift_rows(uint8_t* s)
        {
            uint8_t t;
            // row 1
            t = s[13];
            s[13] = s[9];
            s[9] = s[5];
            s[5] = s[1];
            s[1] = t;
            // row 2
            t = s[2];
            s[2] = s[10];
            s[10] = t;
            t = s[6];
            s[6] = s[14];
            s[14] = t;
            // row 3
            t = s[3];
            s[3] = s[7];
            s[7] = s[11];
            s[11] = s[15];
            s[15] = t;
        }

        static inline void mix_columns(uint8_t* s)
        {
            for (int32_t c = 0; c < 4; ++c) {
                const int32_t i = 4 * c;
                const uint8_t a0 = s[i + 0];
                const uint8_t a1 = s[i + 1];
                const uint8_t a2 = s[i + 2];
                const uint8_t a3 = s[i + 3];
                uint8_t t = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
                uint8_t u = a0;
                s[i + 0] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a0 ^ a1)));
                s[i + 1] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a1 ^ a2)));
                s[i + 2] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a2 ^ a3)));
                s[i + 3] ^= static_cast<uint8_t>(t ^ xtime(static_cast<uint8_t>(a3 ^ u)));
            }
        }
        static inline void inv_mix_columns(uint8_t* s)
        {
            for (int32_t c = 0; c < 4; ++c) {
                int32_t i = 4 * c;
                uint8_t a0 = s[i + 0], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
                s[i + 0] = static_cast<uint8_t>(gf_mul(a0, 0x0e) ^ gf_mul(a1, 0x0b) ^ gf_mul(a2, 0x0d) ^ gf_mul(a3, 0x09));
                s[i + 1] = static_cast<uint8_t>(gf_mul(a0, 0x09) ^ gf_mul(a1, 0x0e) ^ gf_mul(a2, 0x0b) ^ gf_mul(a3, 0x0d));
                s[i + 2] = static_cast<uint8_t>(gf_mul(a0, 0x0d) ^ gf_mul(a1, 0x09) ^ gf_mul(a2, 0x0e) ^ gf_mul(a3, 0x0b));
                s[i + 3] = static_cast<uint8_t>(gf_mul(a0, 0x0b) ^ gf_mul(a1, 0x0d) ^ gf_mul(a2, 0x09) ^ gf_mul(a3, 0x0e));
            }
        }

        void encrypt_block(uint8_t* block) const
        {
            uint8_t state[16];
            memcpy(state, block, 16);

            add_round_key(state, round_key_.begin());

            for (size_t round = 1; round < nr_; ++round) {
                sub_bytes(state);
                shift_rows(state);
                mix_columns(state);
                add_round_key(state, static_cast<const uint8_t*>(round_key_.begin()) + (16 * round));
            }

            sub_bytes(state);
            shift_rows(state);
            add_round_key(state, static_cast<const uint8_t*>(round_key_.begin()) + (16 * nr_));
            memcpy(block, state, 16);
        }

        void decrypt_block(uint8_t* block) const
        {
            uint8_t state[16];
            memcpy(state, block, 16);

            add_round_key(state, static_cast<const uint8_t*>(round_key_.begin()) + (16 * nr_));

            for (size_t round = nr_; round-- > 1;) {
                inv_shift_rows(state);
                inv_sub_bytes(state);
                add_round_key(state, static_cast<const uint8_t*>(round_key_.begin()) + (16 * round));
                inv_mix_columns(state);
            }

            inv_shift_rows(state);
            inv_sub_bytes(state);
            add_round_key(state, static_cast<const uint8_t*>(round_key_.begin()));

            memcpy(block, state, 16);
        }
    };

    uint8_t aes_core::sbox_[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };
    uint8_t aes_core::rsbox_[256] = {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
    };

    aes::aes(aes::enum_cipher_mode mode, aes::enum_key_bits key_bits)
        : mode_(mode)
        , key_bits_(key_bits)
        , key_size_(static_cast<size_t>(key_bits) / 8)
        , iv_size_(16)
    {
        assert(mode == enum_cipher_mode::AES_CBC && "Only CBC mode is supported in current AES implementation!");
    }

    void aes::fill_random(bq::array<uint8_t>& data)
    {
        uint32_t seed = static_cast<uint32_t>(bq::platform::high_performance_epoch_ms() % UINT32_MAX);
        bq::util::srand(seed);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(bq::util::rand() & static_cast<uint32_t>(0xff));
        }
    }

    aes::key_type aes::generate_key() const
    {
        key_type key;
        key.fill_uninitialized(key_size_);
        fill_random(key);
        return key;
    }

    aes::iv_type aes::generate_iv() const
    {
        key_type iv;
        iv.fill_uninitialized(iv_size_);
        fill_random(iv);
        return iv;
    }

    bool aes::encrypt(const aes::key_type& key, const aes::iv_type& iv, const uint8_t* plaintext, size_t plaintext_size, uint8_t* out_ciphertext, size_t out_ciphertext_size) const
    {
        if (key.size() != key_size_) {
            bq::util::log_device_console(bq::log_level::error, "aes::encrypt: key size mismatch");
            return false;
        }
        if (iv.size() != iv_size_) {
            bq::util::log_device_console(bq::log_level::error, "aes::encrypt: iv size mismatch");
            return false;
        }
        if (out_ciphertext_size < plaintext_size) {
            bq::util::log_device_console(bq::log_level::error, "aes::decrypt: output buffer too small");
            return false;
        }
        const size_t n = plaintext_size;
        if (!is_block_aligned(n)) {
            bq::util::log_device_console(bq::log_level::error, "aes::encrypt: plaintext size mismatch");
            return false; // no padding; caller must align length to 16
        }
        if (n == 0) {
            return true;
        }
        // in-place over output
        memcpy(out_ciphertext, plaintext, plaintext_size);

        // prepare key schedule
        aes_core core(mode_, key_bits_);
        core.key_expansion(key);

        // working buffers
        bq::array<uint8_t> chain;
        chain.fill_uninitialized(iv_size_);
        memcpy(static_cast<uint8_t*>(chain.begin()), iv.begin(), iv_size_);

        auto buf = out_ciphertext;

        for (size_t i = 0; i < n; i += 16) {
            aes_core::xor_block(buf + i, buf + i, static_cast<const uint8_t*>(chain.begin()));
            core.encrypt_block(buf + i);
            memcpy(static_cast<uint8_t*>(chain.begin()), buf + i, 16);
        }
        return true;
    }

    bool aes::decrypt(const aes::key_type& key, const aes::iv_type& iv, const uint8_t* ciphertext, size_t ciphertext_size, uint8_t* out_plaintext, size_t out_plaintext_size) const
    {
        if (key.size() != key_size_) {
            bq::util::log_device_console(bq::log_level::error, "aes::decrypt: key size mismatch");
            return false;
        }
        if (iv.size() != iv_size_) {
            bq::util::log_device_console(bq::log_level::error, "aes::decrypt: iv size mismatch");
            return false;
        }
        if (out_plaintext_size < ciphertext_size) {
            bq::util::log_device_console(bq::log_level::error, "aes::decrypt: output buffer too small");
            return false;
        }
        const size_t n = ciphertext_size;
        if (!is_block_aligned(n)) {
            bq::util::log_device_console(bq::log_level::error, "aes::decrypt: ciphertext size mismatch");
            return false; // no padding; caller must align length to 16
        }

        if (n == 0) {
            return true;
        }
        // in-place over output
        memcpy(out_plaintext, ciphertext, ciphertext_size);

        // prepare key schedule
        aes_core core(mode_, key_bits_);
        core.key_expansion(key);

        // working buffers
        uint8_t prev[16];
        uint8_t cur[16];
        memcpy(prev, iv.begin(), 16);

        auto buf = out_plaintext;
        for (size_t i = 0; i < n; i += 16) {
            memcpy(cur, buf + i, 16);
            core.decrypt_block(buf + i);
            aes_core::xor_block(buf + i, buf + i, prev);
            memcpy(prev, cur, 16);
        }
        return true;
    }

}
