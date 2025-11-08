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
 */
#include <iostream>
#include <thread>
#include <utility>
#include "test_base.h"
#include "bq_common/encryption/rsa.h"
#include "bq_common/encryption/aes.h"

namespace bq {
    namespace test {

        class test_encryption : public test_base {

        private:
            void test_rsa(test_result& result, int32_t key_bits, int32_t test_count)
            {
                bq::string base_dir = bq::file_manager::get_base_dir(1);
                char key_bits_str[32];
                snprintf(key_bits_str, sizeof(key_bits_str), "%" PRId32 "", key_bits);
                char thread_id_str[64];
                snprintf(thread_id_str, sizeof(thread_id_str), "%" PRIu64 "", bq::platform::thread::get_current_thread_id());
                bq::string parent_dir = bq::file_manager::combine_path(base_dir, bq::string("enc_output"));
                bq::string output_dir = bq::file_manager::combine_path(parent_dir, bq::string("rsa_") + key_bits_str + "_tid_" + thread_id_str);
                bq::file_manager::create_directory(output_dir);
                bq::string std_out_file = bq::file_manager::combine_path(output_dir, "stdout.txt");
                bq::string std_err_file = bq::file_manager::combine_path(output_dir, "stderr.txt");
                for (int32_t i = 0; i < test_count; ++i) {
                    bq::file_manager::remove_file_or_dir(output_dir);
                    bq::file_manager::create_directory(output_dir);
                    bq::string cmd = "ssh-keygen -t rsa -b " + bq::string(key_bits_str) + " -m PEM -N \"\" -f \"" + bq::file_manager::combine_path(output_dir, "id_rsa") + "\" >\"" + std_out_file + "\" 2>\"" + std_err_file + "\"";
                    int32_t ret = system(cmd.c_str());
                    if (ret != 0) {
                        bq::string err_str = bq::file_manager::read_all_text(std_err_file);
                        bq::string out_str = bq::file_manager::read_all_text(std_out_file);
                        result.add_result(false, "RSA_%" PRId32 " ssh-keygen failed at iteration %" PRId32 ", cmd: %s, error:%s", key_bits, i, cmd.c_str(), err_str.c_str());
                        bq::util::log_device_console(bq::log_level::error, "ssh-key-gen out:%s", out_str.c_str());
                        continue;
                    }
                    bq::string pub_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa.pub"));
                    bq::string pri_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa"));
                    bq::rsa::public_key pub;
                    bq::rsa::private_key pri;
                    bool parse_pub_result = bq::rsa::parse_public_key_ssh(pub_key_text, pub);
                    result.add_result(parse_pub_result, "RSA_%" PRId32 " parse public key failed at iteration %" PRId32 "", key_bits, i);
                    bool parse_pri_result = bq::rsa::parse_private_key_pem(pri_key_text, pri);
                    result.add_result(parse_pri_result, "RSA_%" PRId32 " parse private key failed at iteration %" PRId32 "", key_bits, i);
                    if (!parse_pub_result || !parse_pri_result) {
                        continue;
                    }
                    bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));
                    bq::array<uint8_t> plaintext;
                    uint32_t size = bq::max_value(static_cast<uint32_t>(sizeof(uint32_t)), bq::util::rand() % bq::min_value(static_cast<uint32_t>(pub.n_.size()), 1024U));
                    size = static_cast<uint32_t>((size / sizeof(uint32_t)) * sizeof(uint32_t));
                    plaintext.clear();
                    plaintext.fill_uninitialized(size);
                    for (uint32_t j = 0; j < size / sizeof(uint32_t); ++j) {
                        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(plaintext.begin()) + j * sizeof(uint32_t)) = bq::util::rand();
                    }
                    // Raw RSA (no padding) returns the minimal big-endian form on decrypt; leading 0x00 bytes are not preserved.
                    // To avoid false negatives in tests, we force the first byte to be non-zero here.
                    if (plaintext[0] == 0) {
                        plaintext[0] = bq::max_value(static_cast<uint8_t>(1), static_cast<uint8_t>(bq::util::rand() & static_cast<uint32_t>(0xff)));
                    }
                    bq::array<uint8_t> ciphertext;
                    bool enc_result = bq::rsa::encrypt(pub, plaintext, ciphertext);
                    result.add_result(enc_result, "RSA_%" PRId32 " encryption test failed at iteration %" PRId32 "", key_bits, i);
                    if (enc_result) {
                        bq::array<uint8_t> decrypted_text;
                        bool dec_result = bq::rsa::decrypt(pri, ciphertext, decrypted_text);
                        result.add_result(dec_result, "RSA_%" PRId32 " decryption test failed at iteration %" PRId32 "", key_bits, i);
                        if (dec_result) {
                            if (decrypted_text.size() != plaintext.size()) {
                                result.add_result(false, "RSA_%" PRId32 " decryption size mismatch at iteration %" PRId32 "", key_bits, i);
                            } else {
                                bool content_match = (memcmp((const uint8_t*)decrypted_text.begin(), (const uint8_t*)plaintext.begin(), plaintext.size()) == 0);
                                result.add_result(content_match, "RSA_%" PRId32 " decryption content mismatch at iteration %" PRId32 "", key_bits, i);
                            }
                        }
                    }
                }
            }

            void test_aes(test_result& result, aes::enum_key_bits key_bits)
            {
                bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));

                bq::array<uint8_t> plaintext;
                bq::array<uint8_t> ciphertext;
                bq::array<uint8_t> decrypted_text;
                for (int32_t i = 0; i < 8; ++i) {
                    uint32_t size = bq::max_value(1u, bq::util::rand() % (64 * 1024)) * 16;
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
                            && (memcmp((const uint8_t*)ciphertext.begin(), (const uint8_t*)plaintext.begin(), bq::min_value(plaintext.size(), ciphertext.size())) != 0),
                        "AES_%" PRId32 " encryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);

                    if (enc_result) {
                        bool dec_result = aes.decrypt(key, iv, ciphertext, decrypted_text);
                        result.add_result(dec_result, "AES_%" PRId32 " decryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);
                        if (dec_result) {
                            if (decrypted_text.size() != plaintext.size()) {
                                result.add_result(false, "AES_%" PRId32 " decryption size mismatch at iteration %" PRId32 "", static_cast<int32_t>(key_bits), i);
                            } else {
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
                constexpr uint32_t thread_count = 2;
                test_output(bq::log_level::info, "RSA test begin...");
                bq::array<std::thread*> rsa_threads;
                for (uint32_t i = 0; i < thread_count; ++i) {
                    rsa_threads.push_back(new std::thread([&result, this]() {
                        test_rsa(result, 1024, 2);
                        test_rsa(result, 2048, 2);
                        test_rsa(result, 3072, 1);
                        //test_rsa(result, 7680, 1);
                    }));
                }
                for (uint32_t i = 0; i < thread_count; ++i) {
                    rsa_threads[i]->join();
                    delete rsa_threads[i];
                }

                test_output(bq::log_level::info, "AES test begin...");
                bq::array<std::thread*> aes_threads;
                for (uint32_t i = 0; i < thread_count; ++i) {
                    aes_threads.push_back(new std::thread([&result, this]() {
                        test_aes(result, bq::aes::enum_key_bits::AES_128);
                        test_aes(result, bq::aes::enum_key_bits::AES_192);
                        test_aes(result, bq::aes::enum_key_bits::AES_256);
                    }));
                }
                for (uint32_t i = 0; i < thread_count; ++i) {
                    aes_threads[i]->join();
                    delete aes_threads[i];
                }
                return result;
            }
        };
    }
}
