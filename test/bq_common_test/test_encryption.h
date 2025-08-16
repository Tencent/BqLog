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
 */
#include <iostream>
#include <string>
#include <locale>
#include <stdarg.h>
#include "test_base.h"
#include "bq_common/encryption/aes.h"

namespace bq {
    namespace test {

        class test_encryption : public test_base {
            
        private:
            void test_rsa(test_result& result)
            {
                result.add_result(true, "");
            }

            void test_aes(test_result& result, aes::enum_key_bits key_bits)
            {
                bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));

                bq::array<uint8_t> plaintext;
                bq::array<uint8_t> ciphertext;
                bq::array<uint8_t> decrypted_text;
                test_output_param(bq::log_level::info, "begin AES_%" PRId32 " test", static_cast<int32_t>(key_bits));
                for (int32_t i = 0; i < 64; ++i) {
                    uint32_t size = bq::util::rand() % (64 * 1024) * 16;
                    plaintext.clear();
                    plaintext.fill_uninitialized(size);
                    for (uint32_t j = 0; j < size / sizeof(uint32_t); ++j) {
                        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(plaintext.begin())) = bq::util::rand();
                    }
                    bq::aes aes(bq::aes::enum_cipher_mode::AES_CBC, key_bits);
                    auto key = aes.generate_key();
                    auto iv = aes.generate_iv();
                    bool enc_result = aes.encrypt(key, iv, plaintext, ciphertext);
                    result.add_result(enc_result
                        && (memcmp((const uint8_t*)ciphertext.begin(), (const uint8_t*)plaintext.begin(), bq::min_value(plaintext.size(), ciphertext.size())) != 0)
                        , "AES_%" PRId32 " encryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);

                    if (enc_result) {
                        bool dec_result = aes.decrypt(key, iv, ciphertext, decrypted_text);
                        result.add_result(dec_result, "AES_%" PRId32 " decryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);
                        if (dec_result) {
                            if (decrypted_text.size() != plaintext.size()) {
                                result.add_result(false, "AES_%" PRId32 " decryption size mismatch at iteration %" PRId32 "", static_cast<int32_t>(key_bits), i);
                            }
                            else {
                                bool content_match = (memcmp((const uint8_t*)decrypted_text.begin(), (const uint8_t*)plaintext.begin(), plaintext.size()) == 0);
                                result.add_result(content_match, "AES_%" PRId32 " decryption content mismatch at iteration %" PRId32 "", static_cast<int32_t>(key_bits), i);
                            }
                        }
                    }
                }
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                
                test_rsa(result);
                test_aes(result, bq::aes::enum_key_bits::AES_128);
                test_aes(result, bq::aes::enum_key_bits::AES_192);
                test_aes(result, bq::aes::enum_key_bits::AES_256);
                return result;
            }
        };
    }
}
