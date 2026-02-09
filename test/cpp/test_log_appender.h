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
#include <random>
#include "test_base.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/appender/appender_file_binary.h"
#include "bq_log/log/decoder/appender_decoder_base.h"

namespace bq {
    namespace test {
        void clear_appender_file_base_test_folder()
        {
            if (bq::file_manager::is_dir(TO_ABSOLUTE_PATH("appender_test", 0))) {
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("appender_test", 0));
            }
        }
        class appender_file_base_for_test : public appender_file_base {
            template <typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name, bool use_decoder, const bq::string& pub_key, const bq::string& private_key);

        protected:
            virtual bool parse_exist_log_file(parse_file_context& context) override
            {
                (void)context;
                return true;
            }
            virtual bq::string get_file_ext_name() override
            {
                return ".ft";
            }
            virtual bool init_impl(const bq::property_value& config_obj) override
            {
                appender_file_base::init_impl(config_obj);
                set_flush_when_destruct(false);
                open_new_indexed_file_by_name();
                return true;
            }

        public:
            void read_mode()
            {
                seek_read_file_absolute(static_cast<size_t>(0));
            }
        };
        class appender_file_binary_for_test : public appender_file_binary {
            template <typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name, bool use_decoder, const bq::string& pub_key, const bq::string& private_key);

        protected:
            virtual bool parse_exist_log_file(parse_file_context& context) override
            {
                return appender_file_binary::parse_exist_log_file(context);
            }
            virtual bq::string get_file_ext_name() override
            {
                return ".bt";
            }
            virtual appender_file_binary::appender_format_type get_appender_format() const override
            {
                return appender_format_type::raw;
            }
            virtual uint32_t get_binary_format_version() const override
            {
                return 1;
            }
            virtual bool init_impl(const bq::property_value& config_obj) override
            {
                appender_file_binary::init_impl(config_obj);
                set_flush_when_destruct(false);
                open_new_indexed_file_by_name();
                return true;
            }

        public:
            void read_mode()
            {
                parse_file_context context("test_appender_file");
                ;
                parse_exist_log_file(context); // used to seek file to begin of data section
            }
        };
        class appender_file_binary_rolling_for_test : public appender_file_binary_for_test {
            template <typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name, bool use_decoder, const bq::string& pub_key, const bq::string& private_key);

        protected:
            virtual bool init_impl(const bq::property_value& config_obj)
            {
                const_cast<bq::property_value&>(config_obj).add_object_item("max_file_size", static_cast<bq::property_value::integral_type>(1024 * 1024 * 3));
                return appender_file_binary_for_test::init_impl(config_obj);
            }
        };
        class appender_decoder_for_test : public appender_decoder_base {
            template <typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name, bool use_decoder, const bq::string& pub_key, const bq::string& private_key);

        protected:
            virtual appender_decode_result init_private() override
            {
                return appender_decode_result::success;
            }
            virtual appender_decode_result decode_private() override
            {
                return appender_decode_result::success;
            }
            virtual uint32_t get_binary_format_version() const override
            {
                return 1;
            }
        };

        template <typename AppenderType>
        void do_appender_test(test_result& result, const bq::string test_name, bool use_decoder, const bq::string& pub_key, const bq::string& private_key)
        {
            test_output_dynamic_param(bq::log_level::info, "%s test begin, please wait...                \r", test_name.c_str());
            clear_appender_file_base_test_folder();
            log_imp log_obj;
            bq::array<bq::string> categories;
            categories.push_back("");
            categories.push_back("CategoryA");
            categories.push_back("CategoryA.ModuleB");
            categories.push_back("CategoryB");
            bq::property_value log_config = bq::property_value::create_from_string(R"(
                        log.recovery = true
                        appenders_config.appender_0.type=console
	                )");
            log_obj.init(test_name, log_config, categories);
            bq::string appender_config_str = R"(
                        type=text_file
                        levels=[all]
                        file_name=appender_test/%appender_name%
                        base_dir_type=0
                        enable_rolling_log_file=false
                        %pub_key%
                        )";
            appender_config_str = appender_config_str.replace("%appender_name%", test_name);
            appender_config_str = appender_config_str.replace("%pub_key%", pub_key.is_empty() ? "" : ("pub_key=" + pub_key));
            bq::property_value appender_config = bq::property_value::create_from_string(appender_config_str);
            size_t total_write_size = 0;
            while (true) {
                AppenderType appender_write;
                appender_write.init("test_appender", appender_config, &log_obj);
                for (int32_t i = 0; i < 32; ++i) {
                    std::random_device sd;
                    std::minstd_rand linear_ran(sd());
                    std::uniform_int_distribution<size_t> rand_seq(1, 128 * 1024);
                    size_t new_size = rand_seq(linear_ran);
                    auto handle = appender_write.alloc_write_cache(new_size);
                    result.add_result(handle.allcoated_len() == new_size, "%s alloc test", test_name.c_str());
                    for (size_t pos = 0; pos < handle.allcoated_len(); ++pos) {
                        size_t byte_pos = total_write_size + pos;
                        uint32_t value = static_cast<uint32_t>(byte_pos & (~(sizeof(uint32_t) - static_cast<size_t>(1))));
                        size_t offset = byte_pos & (sizeof(uint32_t) - static_cast<size_t>(1));
                        uint8_t value_byte = *(static_cast<uint8_t*>(static_cast<void*>(&value)) + offset);
                        handle.data()[pos] = value_byte;
                    }
                    if (new_size % 24 == 0) {
                        std::uniform_int_distribution<size_t> rand_seq2(1, new_size);
                        new_size = rand_seq2(linear_ran);
                        handle.reset_used_len(new_size);
                    }
                    appender_write.return_write_cache(handle);
                    if ((i % bq::max_value(i % 4, 1)) == 0) {
                        appender_write.mark_write_finished();
                    }
                    total_write_size += new_size;
                }
                appender_write.mark_write_finished();
                size_t total_size = private_key.is_empty() ? (128 * 1024 * 1024) : (4 * 1024 * 1024);
                if (total_write_size > total_size) {
                    appender_write.flush_write_cache();
                    appender_write.flush_write_io();
                    break;
                }
            }
            if (use_decoder) {
                size_t left_read_size = total_write_size;
                size_t byte_pos = 0;
                bool check_result = true;
                for (int32_t file_idx = 1;; ++file_idx) {
                    char idx_str[32];
                    snprintf(idx_str, sizeof(idx_str), "%" PRId32, file_idx);
                    auto file_path = TO_ABSOLUTE_PATH("appender_test/" + test_name + +idx_str + ".bt", 0);
                    bq::file_handle file;
                    if (bq::file_manager::is_file(file_path)) {
                        file = bq::file_manager::instance().open_file(file_path, bq::file_open_mode_enum::read);
                    }
                    if (!file) {
                        if (left_read_size != 0) {
                            result.add_result(check_result, "%s read size test", test_name.c_str());
                        }
                        break;
                    }
                    appender_decoder_for_test decoder_read;
                    decoder_read.init(file, private_key);
                    while (left_read_size > 0 && check_result) {
                        std::random_device sd;
                        std::minstd_rand linear_ran(sd());
                        std::uniform_int_distribution<size_t> rand_seq(1, 128 * 1024);
                        size_t read_size = rand_seq(linear_ran);
                        auto handle = decoder_read.read_with_cache(read_size);
                        if (handle.len() > left_read_size) {
                            result.add_result(false, "%s read size test", test_name.c_str());
                            check_result = false;
                            break;
                        }
                        if (handle.len() == 0) {
                            break;
                        }
                        left_read_size -= handle.len();
                        for (size_t i = 0; i < handle.len(); ++i) {
                            uint32_t expected_read_value = static_cast<uint32_t>(byte_pos & (~(sizeof(uint32_t) - static_cast<size_t>(1))));
                            size_t offset = byte_pos & (sizeof(uint32_t) - static_cast<size_t>(1));
                            uint8_t expected_value_byte = *(static_cast<uint8_t*>(static_cast<void*>(&expected_read_value)) + offset);
                            if (expected_value_byte != handle.data()[i]) {
                                check_result = false;
                                break;
                            }
                            byte_pos++;
                        }
                    }
                }
                result.add_result(check_result, "%s read content test", test_name.c_str());
            } else {
                AppenderType appender_read;
                appender_read.init("test_appender", appender_config, &log_obj);
                appender_read.read_mode();
                size_t left_read_size = total_write_size;
                size_t byte_pos = 0;
                bool check_result = true;
                while (left_read_size > 0 && check_result) {
                    std::random_device sd;
                    std::minstd_rand linear_ran(sd());
                    std::uniform_int_distribution<size_t> rand_seq(1, 128 * 1024);
                    size_t read_size = rand_seq(linear_ran);
                    auto handle = appender_read.read_with_cache(read_size);
                    if (handle.len() > left_read_size || handle.len() == 0) {
                        result.add_result(false, "%s read size test", test_name.c_str());
                        check_result = false;
                        break;
                    }
                    left_read_size -= handle.len();
                    for (size_t i = 0; i < handle.len(); ++i) {
                        uint32_t expected_read_value = static_cast<uint32_t>(byte_pos & (~(sizeof(uint32_t) - static_cast<size_t>(1))));
                        size_t offset = byte_pos & (sizeof(uint32_t) - static_cast<size_t>(1));
                        uint8_t expected_value_byte = *(static_cast<uint8_t*>(static_cast<void*>(&expected_read_value)) + offset);
                        if (expected_value_byte != handle.data()[i]) {
                            check_result = false;
                            break;
                        }
                        byte_pos++;
                    }
                }
                result.add_result(check_result, "%s read content test", test_name.c_str());
            }
            test_output_dynamic(bq::log_level::info, "                                                                              \r");
        }

        class test_log_appender : public test_base {
        private:
            void do_console_appender_test(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "appender_console test begin, please wait...                \r");
                clear_appender_file_base_test_folder();
                (void)result;
            }
            void do_file_appender_test(test_result& result)
            {
                do_appender_test<appender_file_base_for_test>(result, "appender_file_base_test", false, "", "");
            }
            void do_binary_appender_test(test_result& result)
            {
                do_appender_test<appender_file_binary_for_test>(result, "appender_file_binary_test", false, "", "");
            }
            void do_binary_appender_rolling_test(test_result& result)
            {
                do_appender_test<appender_file_binary_rolling_for_test>(result, "appender_file_binary_rolling_test", true, "", "");
            }
            void do_binary_appender_test_with_enc(test_result& result)
            {
                bq::string pub_key = bq::string("ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFc")
                    + "Nr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZE"
                    + "PwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+"
                    + "r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6";
                bq::string priv_key = bq::string("-----BEGIN RSA PRIVATE KEY-----\n")
                    + "MIIEpAIBAAKCAQEAsL90LQ1wf30DfhaJ8jh7ktrguiGXNem33eqkJOMpATrd503w\n"
                    + "RXDa/GVr73H+B9IRZK6TLUNACTtOexoVU0Agn5730rUqtmK2vjuVRqSOUOjS61Ll\n"
                    + "REYpozYMLcW17dz/DcPO4xCCYmP/JthVF4WRD8KAO7ftv/ed3DRNYrSbJCbaI8Gl\n"
                    + "JTDdHVwScbODT/DHwTz2UObzVfMYBH9UgkM0AQCbKH8tZDxx6iTQV1AbDQOOEj+R\n"
                    + "/eSTs/q9KGjZGMU/PGZOLW5OTd4ZOWl8mVaePTqoa1wj3+aMA2uC6pOFM1U5PRIp\n"
                    + "sb7UrolE86+yCYR2Mu47Bp0oKunR9MZVH8SnlwIDAQABAoIBACtaWpmuYTi0JkYo\n"
                    + "Kx/hoNXtoA+nq5pKwJHLOwXdPjKSCNnycQvnWZ9tFSN/V2r9qMyEUY9ZnnxlMqPZ\n"
                    + "Sv/Hi/j7Ghhx3Y8s+VwB62SPemT4JrwX8ipj91SULjqP80br3Re4PqfNZd3SX0Rc\n"
                    + "7co+Nc2izKdZPxTGHM9leNHMMP2VrVbZeSlQBnqlFVMqi2g9ukMGZG10vPdIJV7z\n"
                    + "5dqWaKuW/2F8dp/o6i/uUDWAH4fITLD1PLqx5/kP8ohOXup8wxYaY8jhKlvswuCh\n"
                    + "qlY773SamnrIGctQWe63Fe9q7hzs3vcCOfciFYsVX2qfHPvGORzd8DgMwe3TFbrM\n"
                    + "nmUijcECgYEA5zyarIPQfbEsmMtKgY7OSz+M1SaOE9r/I0Hl2dz7f0r7JocZ6KUj\n"
                    + "NinbX/zvJ7AMzDUefbptjf9F4vNa0eBZmSZDAiWm3byvMKS5uLboFNrKnYptIc3c\n"
                    + "0CzDzC+nMi4NrPoAZZrUyJw1Emr7gWVVG2FW2NatOVqfPq3XBbk9dncCgYEAw60H\n"
                    + "FqcvrSTAVSk9L+TB8Fn92p5YQtaV7CSZj9GQdvfs+pIkPlq+jYvSH7nYjK2Qlq+h\n"
                    + "sn+3YcVVczbGuhSLuq4bHPd46HOjya3rbAq39RxlpFRFjZ4hici4XIFKnB7Kylta\n"
                    + "Ph6nNq9m5tdFZ1SurgLlOaxg2AwLAia2V5L5/+ECgYAzmhmmP/Ap7HzYSB2DVfwB\n"
                    + "XNgvxN/V3HwtQQprGN5i5LexPFryyM9XyfVzsT0pbScd9wir5AuIsZvF7qqoxVkZ\n"
                    + "TSmM9BwNxYqO32O2rdKSvNSUXYzHC2qoZiT3jvbPwuk4Xb3y7p9neTx6tLcVhCh+\n"
                    + "6LT5xMZ5UxjQYvjmBRWLNwKBgQC5Ta9rzXHB5w7Y1w/hviHHKoHTOabdzPb3RQXD\n"
                    + "g7LqZwkdla4K+sZ/pwybDNU9C9TkTnizYG1agpTUYeg6KeDVLbHxcY4nm/Nct349\n"
                    + "t7zTu0uqHkArx7d9Ev88Yxgz1pk2nuJL951klSC+tNg97Zzqn0VSo6KmlmkKZXzC\n"
                    + "XCayIQKBgQC+hPlGE7T6WLKvsmaH3IJI/TAjbuSu25o+aar7ecPoeax6YnSgyF/8\n"
                    + "Xx7f0BFCYEzprftfqDBcfa6D8GKkcsqAMDeJRz8meD+55o3qdL6LKFkjCKvmRxuv\n"
                    + "OmO+42HqCD4mqxMU1rgcWOn+LLW3HSqbE5kYA9XDwEtxCnMiKqP7sA==\n"
                    + "-----END RSA PRIVATE KEY-----\n";
                do_appender_test<appender_file_binary_for_test>(result, "appender_file_binary_test_with_enc", true, pub_key, priv_key);
            }

        public:
            virtual test_result test() override
            {
                test_result result;
#ifdef BQ_UNITE_TEST_LOW_PERFORMANCE_MODE
                constexpr int32_t loop_count = 1;
#else
                constexpr int32_t loop_count = 4;
#endif
                do_binary_appender_test_with_enc(result);
                for (int32_t i = 0; i < loop_count; ++i) {
                    do_console_appender_test(result);
                }
                for (int32_t i = 0; i < loop_count; ++i) {
                    do_file_appender_test(result);
                }
                for (int32_t i = 0; i < loop_count; ++i) {
                    do_binary_appender_test(result);
                }
                for (int32_t i = 0; i < loop_count; ++i) {
                    do_binary_appender_rolling_test(result);
                }
                for (int32_t i = 0; i < loop_count; ++i) {
                    do_binary_appender_test_with_enc(result);
                }
                return result;
            }
        };
    }
}
