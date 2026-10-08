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
#include <atomic>
#include <random>
#include <thread>
#include <vector>
#include "test_base.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/appender/appender_file_binary.h"
#include "bq_log/log/appender/appender_file_compressed.h"
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
                return appender_format_type::compressed;
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

            bool write_payload_for_test(const bq::array<uint8_t>& data)
            {
                auto handle = alloc_write_cache(data.size());
                memcpy(handle.data(), data.begin(), data.size());
                return_write_cache(handle);
                mark_write_finished();
                flush_write_cache();
                return get_pendding_flush_written_size() == 0;
            }
        };
        class appender_file_binary_recovery_for_test : public appender_file_binary_for_test {
        public:
            bool begin_log_buffer_recovery_for_test()
            {
                _log_entry_head_def head {};
                head.timestamp_epoch = bq::platform::high_performance_epoch_ms();
                log_entry_handle handle(reinterpret_cast<const uint8_t*>(&head), sizeof(head));
                return on_log_item_recovery_begin(handle);
            }

            const bq::string& get_output_path_for_test() const
            {
                return const_cast<appender_file_binary_recovery_for_test*>(this)->get_file_handle().abs_file_path();
            }
        };
        class appender_file_binary_appender_recovery_for_test : public appender_file_binary {
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
                return appender_format_type::compressed;
            }
            virtual uint32_t get_binary_format_version() const override
            {
                return 1;
            }
            virtual bool init_impl(const bq::property_value& config_obj) override
            {
                return appender_file_binary::init_impl(config_obj);
            }

        public:
            bool append_appender_recovery_to_file_for_test(const bq::string& path)
            {
                bq::array<uint8_t> empty;
                return append_appender_recovery_to_file_for_test(path, empty);
            }

            bool append_appender_recovery_to_file_for_test(const bq::string& path, const bq::array<uint8_t>& data)
            {
                if (!open_file_with_write_exclusive(path)) {
                    return false;
                }
                if (!on_appender_file_recovery_begin()) {
                    return false;
                }
                if (!data.is_empty()) {
                    auto handle = alloc_write_cache(data.size());
                    memcpy(handle.data(), data.begin(), data.size());
                    return_write_cache(handle);
                    mark_write_finished();
                    flush_write_cache();
                }
                return get_pendding_flush_written_size() == 0;
            }
        };
        class appender_file_compressed_mmap_for_test : public appender_file_compressed {
        protected:
            virtual bool init_impl(const bq::property_value& config_obj) override
            {
                if (!appender_file_compressed::init_impl(config_obj)) {
                    return false;
                }
                set_flush_when_destruct(false);
                return true;
            }

        public:
            bq::string get_output_path_for_test()
            {
                return get_file_handle().abs_file_path();
            }

            bq::string get_mmap_path_for_test()
            {
                return get_mmap_file_path();
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

        public:
            bool read_payload_for_test(const bq::array<uint8_t>& expected)
            {
                auto handle = read_with_cache(expected.size());
                return handle.len() == expected.size()
                    && memcmp(handle.data(), expected.begin(), expected.size()) == 0;
            }

            bool is_eof_for_test()
            {
                return read_with_cache(1).len() == 0;
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
            enum class recovery_encryption_mode : uint8_t {
                plaintext,
                key_a,
                key_b,
            };

            bool recovery_mmap_test_done_ = false;

            static appender_file_binary::appender_encryption_type get_encryption_type(recovery_encryption_mode mode)
            {
                return mode == recovery_encryption_mode::plaintext
                    ? appender_file_binary::appender_encryption_type::plaintext
                    : appender_file_binary::appender_encryption_type::rsa_aes_xor;
            }

            static const char* get_encryption_mode_name(recovery_encryption_mode mode)
            {
                switch (mode) {
                case recovery_encryption_mode::plaintext:
                    return "plain";
                case recovery_encryption_mode::key_a:
                    return "key_a";
                case recovery_encryption_mode::key_b:
                    return "key_b";
                }
                return "unknown";
            }

            static bq::string get_recovery_pub_key(recovery_encryption_mode mode, const bq::string& pub_key_a, const bq::string& pub_key_b)
            {
                if (mode == recovery_encryption_mode::key_a) {
                    return pub_key_a;
                }
                if (mode == recovery_encryption_mode::key_b) {
                    return pub_key_b;
                }
                return "";
            }

            static bq::array<uint8_t> make_recovery_log_entry(uint64_t id)
            {
                static const char format[] = "recovery";
                const size_t format_size = sizeof(format) - 1;
                const size_t ext_offset = sizeof(_log_entry_head_def) + bq::align_4(format_size);
                bq::array<uint8_t> data;
                data.fill_uninitialized(ext_offset + sizeof(_log_entry_ext_head_def));
                memset(data.begin(), 0, data.size());
                auto* head = reinterpret_cast<_log_entry_head_def*>(&data[0]);
                head->timestamp_epoch = bq::platform::high_performance_epoch_ms() + id;
                head->ext_info_offset = static_cast<uint32_t>(ext_offset);
                head->category_idx = 0;
                head->log_thread_id = 1;
                head->format_hash = id;
                head->log_format_str_type = static_cast<uint8_t>(log_arg_type_enum::string_utf8_type);
                head->level = static_cast<uint8_t>(log_level::info);
                head->log_format_data_len = static_cast<uint32_t>(format_size);
                memcpy(data.begin() + static_cast<ptrdiff_t>(sizeof(_log_entry_head_def)), format, format_size);
                reinterpret_cast<_log_entry_ext_head_def*>(&data[ext_offset])->thread_name_len_ = 0;
                return data;
            }

            static bq::array<uint8_t> make_segment_payload(uint32_t case_index, uint32_t segment_index)
            {
                bq::array<uint8_t> data;
                data.fill_uninitialized(73 + segment_index * 29);
                for (size_t i = 0; i < data.size(); ++i) {
                    data[i] = static_cast<uint8_t>((case_index * 31 + segment_index * 17 + i) & 0xFF);
                }
                return data;
            }

            static bool write_recovery_log_entry(log_buffer& buffer, const bq::array<uint8_t>& data)
            {
                auto write_handle = buffer.alloc_write_chunk(static_cast<uint32_t>(data.size()), reinterpret_cast<const _log_entry_head_def*>(&data[0])->timestamp_epoch);
                if (write_handle.result != enum_buffer_result_code::success) {
                    buffer.commit_write_chunk(write_handle);
                    return false;
                }
                memcpy(write_handle.data_addr, data.begin(), data.size());
                buffer.commit_write_chunk(write_handle);
                return true;
            }

            static bq::property_value make_recovery_log_config(const bq::string& file_name, const bq::string& pub_key)
            {
                bq::string config = "log.thread_mode=async\n"
                                    "log.recovery=true\n"
                                    "log.buffer_size=4096\n"
                                    "appenders_config.appender.type=compressed_file\n"
                                    "appenders_config.appender.levels=[all]\n"
                                    "appenders_config.appender.file_name="
                    + file_name + "\n"
                                  "appenders_config.appender.base_dir_type=0\n"
                                  "appenders_config.appender.enable_rolling_log_file=false\n";
                if (!pub_key.is_empty()) {
                    config += "appenders_config.appender.pub_key=" + pub_key + "\n";
                }
                return bq::property_value::create_from_string(config);
            }

            static bq::property_value make_recovery_parent_log_config()
            {
                return bq::property_value::create_from_string(R"(
                    log.thread_mode=async
                    log.recovery=true
                    log.buffer_size=4096
                    appenders_config.parent_console.type=console
                    appenders_config.parent_console.levels=[all]
                )");
            }

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
                bq::string pub_key_b = bq::string("ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwOxf6ZHiNn4T1jcjuCoBDbQ9nvDn3HEwyRCYjiHh/f6Hz+CmNQqd6ErccjjqQO/B5R2LpC2/BrESSXa08u3oe1e+AhFCcdFmTOvzlamfTjOwFEwiOxt2aBOkFmhe4UTibKxNWK4ODOgSpN/4xZqo+Njpx/NRyGwj6b0oxUrdN+LIXU4NhOBz8aovF9wdbmgvAAUdRToSthO1gS1k5w15/XvtGV2mxRtU3gGtrmpl6KvWq+r3oYSAgBO4N+4DQ7oGsp/k5NkaZXumlUD4LRPLTuh/iEx7V68FMTh6BJklkE6ZtBxwJD94NO4Yut3LPM2bUE2aLCf5unloezAdytKrB pippocao@PIPPOCAO-PC6");
                if (!recovery_mmap_test_done_) {
                    do_binary_appender_reset_to_plaintext_test(result, pub_key);
                    do_binary_segment_encryption_matrix_test(result, pub_key, priv_key);
                    do_binary_segment_key_boundary_test(result, pub_key, priv_key, pub_key_b);
                    do_binary_recovery_segment_test(result, pub_key);
                    do_binary_recovery_mmap_test(result, pub_key, pub_key_b);
                    recovery_mmap_test_done_ = true;
                }
                do_appender_test<appender_file_binary_for_test>(result, "appender_file_binary_test_with_enc", true, pub_key, priv_key);
            }

            void do_binary_appender_reset_to_plaintext_test(test_result& result, const bq::string& pub_key)
            {
                clear_appender_file_base_test_folder();
                log_imp log_obj;
                bq::array<bq::string> categories;
                categories.push_back("");
                bq::property_value log_config = bq::property_value::create_from_string(R"(
                    log.recovery=false
                    appenders_config.appender_0.type=console
                )");
                log_obj.init("appender_reset_encryption_test", log_config, categories);

                bq::property_value encrypted_config = bq::property_value::create_from_string(
                    "type=compressed_file\n"
                    "levels=[all]\n"
                    "file_name=appender_test/reset_encryption\n"
                    "base_dir_type=0\n"
                    "enable_rolling_log_file=false\n"
                    "pub_key="
                    + pub_key);
                bq::property_value plaintext_config = bq::property_value::create_from_string(R"(
                    type=compressed_file
                    levels=[all]
                    file_name=appender_test/reset_encryption
                    base_dir_type=0
                    enable_rolling_log_file=false
                )");

                appender_file_binary_for_test appender;
                bool initialized = appender.init("test_appender", encrypted_config, &log_obj);
                bool reused = initialized && appender.reset(plaintext_config);
                result.add_result(initialized && !reused, "encrypted appender must be recreated when pub_key is removed");
            }

            static bool read_binary_segment_chain_for_test(const bq::string& path,
                bq::array<appender_file_binary::appender_file_segment_head>& segment_heads,
                bq::array<uint64_t>* fingerprints = nullptr,
                bq::array<uint64_t>* key_data_hashes = nullptr)
            {
                segment_heads.clear();
                if (fingerprints) {
                    fingerprints->clear();
                }
                if (key_data_hashes) {
                    key_data_hashes->clear();
                }
                auto file = bq::file_manager::instance().open_file(path, bq::file_open_mode_enum::read);
                if (!file) {
                    return false;
                }
                const size_t file_size = bq::file_manager::instance().get_file_size(file);
                appender_file_binary::appender_file_header file_head;
                bool success = bq::file_manager::instance().seek(file, bq::file_manager::seek_option::begin, 0)
                    && file_size >= sizeof(file_head)
                    && bq::file_manager::instance().read_file(file, &file_head, sizeof(file_head)) == sizeof(file_head);
                uint64_t segment_pos = sizeof(file_head);
                while (success) {
                    if (segment_pos > file_size || file_size - segment_pos < sizeof(appender_file_binary::appender_file_segment_head)
                        || !bq::file_manager::instance().seek(file, bq::file_manager::seek_option::begin, static_cast<int64_t>(segment_pos))) {
                        success = false;
                        break;
                    }
                    appender_file_binary::appender_file_segment_head segment_head;
                    if (bq::file_manager::instance().read_file(file, &segment_head, sizeof(segment_head)) != sizeof(segment_head)) {
                        success = false;
                        break;
                    }
                    segment_heads.push_back(segment_head);
                    uint64_t segment_end = segment_head.next_seg_pos == UINT64_MAX ? static_cast<uint64_t>(file_size) : segment_head.next_seg_pos;
                    uint64_t min_segment_end = segment_pos + sizeof(segment_head);
                    uint64_t fingerprint = 0;
                    uint64_t key_data_hash = 0;
                    if (segment_head.enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor) {
                        min_segment_end += appender_file_binary::get_encryption_keys_size();
                        if (segment_end < min_segment_end
                            || bq::file_manager::instance().read_file(file, &fingerprint, sizeof(fingerprint)) != sizeof(fingerprint)) {
                            success = false;
                            break;
                        }
                        bq::array<uint8_t> key_data;
                        key_data.fill_uninitialized(appender_file_binary::get_encryption_keys_size() - sizeof(fingerprint));
                        if (bq::file_manager::instance().read_file(file, key_data.begin(), key_data.size()) != key_data.size()) {
                            success = false;
                            break;
                        }
                        key_data_hash = bq::util::get_hash_64(key_data.begin(), key_data.size());
                    } else if (segment_head.enc_type != appender_file_binary::appender_encryption_type::plaintext) {
                        success = false;
                        break;
                    } else if (segment_end < min_segment_end) {
                        success = false;
                        break;
                    }
                    if (fingerprints) {
                        fingerprints->push_back(fingerprint);
                    }
                    if (key_data_hashes) {
                        key_data_hashes->push_back(key_data_hash);
                    }
                    if (segment_head.next_seg_pos == UINT64_MAX) {
                        break;
                    }
                    if (segment_head.next_seg_pos <= segment_pos
                        || segment_head.next_seg_pos > file_size
                        || file_size - segment_head.next_seg_pos < sizeof(segment_head)
                        || segment_heads.size() > 16) {
                        success = false;
                        break;
                    }
                    segment_pos = segment_head.next_seg_pos;
                }
                bq::file_manager::instance().close_file(file);
                return success && !segment_heads.is_empty();
            }

            void do_binary_segment_encryption_matrix_test(test_result& result, const bq::string& pub_key, const bq::string& private_key)
            {
                clear_appender_file_base_test_folder();
                rsa::public_key parsed_key;
                const bool key_ok = rsa::parse_public_key_ssh(pub_key, parsed_key);
                const uint64_t expected_fingerprint = key_ok ? rsa::get_public_key_fingerprint(parsed_key) : 0;
                result.add_result(key_ok, "parse segment matrix RSA key");
                if (!key_ok) {
                    return;
                }

                log_imp log_obj;
                bq::array<bq::string> categories;
                categories.push_back("");
                const auto log_config = bq::property_value::create_from_string(R"(
                    log.recovery=false
                    appenders_config.appender_0.type=console
                )");
                if (!log_obj.init("binary_segment_encryption_matrix", log_config, categories)) {
                    result.add_result(false, "init segment matrix parent log");
                    return;
                }

                constexpr uint32_t segment_count = 4;
                for (uint32_t mask = 0; mask < (1U << segment_count); ++mask) {
                    char case_id[32];
                    snprintf(case_id, sizeof(case_id), "%" PRIu32, mask);
                    const bq::string file_name = bq::string("appender_test/segment_matrix_") + case_id;
                    const bq::string file_path = TO_ABSOLUTE_PATH(file_name + "_1.bt", 0);
                    bool case_ok = true;

                    {
                        appender_file_binary_for_test writer;
                        const bq::string segment_pub_key = (mask & 1U) ? pub_key : "";
                        case_ok = writer.init("segment_matrix_first", make_binary_recovery_test_config(file_name, segment_pub_key), &log_obj)
                            && writer.write_payload_for_test(make_segment_payload(mask, 0));
                    }
                    for (uint32_t i = 1; i < segment_count && case_ok; ++i) {
                        appender_file_binary_appender_recovery_for_test writer;
                        const bq::string segment_pub_key = (mask & (1U << i)) ? pub_key : "";
                        case_ok = writer.init("segment_matrix_append", make_binary_recovery_test_config(file_name, segment_pub_key), &log_obj)
                            && writer.append_appender_recovery_to_file_for_test(file_path, make_segment_payload(mask, i));
                    }

                    bq::array<appender_file_binary::appender_file_segment_head> segment_heads;
                    bq::array<uint64_t> fingerprints;
                    bq::array<uint64_t> key_data_hashes;
                    case_ok = case_ok
                        && read_binary_segment_chain_for_test(file_path, segment_heads, &fingerprints, &key_data_hashes)
                        && segment_heads.size() == segment_count;
                    for (uint32_t i = 0; i < segment_count && case_ok; ++i) {
                        const bool encrypted = (mask & (1U << i)) != 0;
                        case_ok = segment_heads[i].enc_type == (encrypted ? appender_file_binary::appender_encryption_type::rsa_aes_xor : appender_file_binary::appender_encryption_type::plaintext)
                            && fingerprints[i] == (encrypted ? expected_fingerprint : 0);
                        if (encrypted) {
                            for (uint32_t j = 0; j < i; ++j) {
                                if ((mask & (1U << j)) && key_data_hashes[i] == key_data_hashes[j]) {
                                    case_ok = false;
                                    break;
                                }
                            }
                        }
                    }

                    auto file = bq::file_manager::instance().open_file(file_path, bq::file_open_mode_enum::read);
                    appender_decoder_for_test decoder;
                    case_ok = case_ok && file
                        && decoder.init(file, mask == 0 ? "" : private_key) == appender_decode_result::success;
                    for (uint32_t i = 0; i < segment_count && case_ok; ++i) {
                        case_ok = decoder.read_payload_for_test(make_segment_payload(mask, i));
                    }
                    case_ok = case_ok && decoder.is_eof_for_test();
                    bq::file_manager::instance().close_file(file);
                    result.add_result(case_ok, "binary segment encryption sequence:%" PRIu32, mask);
                }
            }

            void do_binary_segment_key_boundary_test(test_result& result, const bq::string& pub_key_a, const bq::string& private_key_a, const bq::string& pub_key_b)
            {
                clear_appender_file_base_test_folder();
                log_imp log_obj;
                bq::array<bq::string> categories;
                categories.push_back("");
                const auto log_config = bq::property_value::create_from_string(R"(
                    log.recovery=false
                    appenders_config.appender_0.type=console
                )");
                bool test_ok = log_obj.init("binary_segment_key_boundary", log_config, categories);
                const bq::string file_name = "appender_test/segment_key_boundary";
                const bq::string file_path = TO_ABSOLUTE_PATH(file_name + "_1.bt", 0);

                {
                    appender_file_binary_for_test writer;
                    test_ok = test_ok
                        && writer.init("segment_key_first", make_binary_recovery_test_config(file_name, pub_key_a), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(100, 0));
                }
                const size_t file_size_before_mismatch = bq::file_manager::get_file_size(file_path);
                bool different_key_rejected = false;
                {
                    appender_file_binary_appender_recovery_for_test writer;
                    different_key_rejected = writer.init("segment_key_b", make_binary_recovery_test_config(file_name, pub_key_b), &log_obj)
                        && !writer.append_appender_recovery_to_file_for_test(file_path, make_segment_payload(100, 1));
                }
                test_ok = test_ok && different_key_rejected
                    && bq::file_manager::get_file_size(file_path) == file_size_before_mismatch;

                {
                    appender_file_binary_appender_recovery_for_test writer;
                    test_ok = test_ok
                        && writer.init("segment_key_plain", make_binary_recovery_test_config(file_name, ""), &log_obj)
                        && writer.append_appender_recovery_to_file_for_test(file_path, make_segment_payload(100, 1));
                }
                {
                    appender_file_binary_appender_recovery_for_test writer;
                    test_ok = test_ok
                        && writer.init("segment_key_alias", make_binary_recovery_test_config(file_name, pub_key_a + " alias"), &log_obj)
                        && writer.append_appender_recovery_to_file_for_test(file_path, make_segment_payload(100, 2));
                }

                bq::array<appender_file_binary::appender_file_segment_head> segment_heads;
                bq::array<uint64_t> fingerprints;
                bq::array<uint64_t> key_data_hashes;
                test_ok = test_ok
                    && read_binary_segment_chain_for_test(file_path, segment_heads, &fingerprints, &key_data_hashes)
                    && segment_heads.size() == 3
                    && fingerprints[0] != 0
                    && fingerprints[1] == 0
                    && fingerprints[2] == fingerprints[0]
                    && key_data_hashes[0] != key_data_hashes[2];

                auto file = bq::file_manager::instance().open_file(file_path, bq::file_open_mode_enum::read);
                appender_decoder_for_test decoder;
                test_ok = test_ok && file
                    && decoder.init(file, private_key_a) == appender_decode_result::success
                    && decoder.read_payload_for_test(make_segment_payload(100, 0))
                    && decoder.read_payload_for_test(make_segment_payload(100, 1))
                    && decoder.read_payload_for_test(make_segment_payload(100, 2))
                    && decoder.is_eof_for_test();
                bq::file_manager::instance().close_file(file);
                result.add_result(test_ok, "binary segment single RSA key boundary");

                const bq::string wrong_key_name = "appender_test/segment_wrong_private_key";
                const bq::string wrong_key_path = TO_ABSOLUTE_PATH(wrong_key_name + "_1.bt", 0);
                bool wrong_private_key_rejected = false;
                {
                    appender_file_binary_for_test writer;
                    wrong_private_key_rejected = writer.init("segment_wrong_private", make_binary_recovery_test_config(wrong_key_name, pub_key_b), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(101, 0));
                }
                file = bq::file_manager::instance().open_file(wrong_key_path, bq::file_open_mode_enum::read);
                appender_decoder_for_test wrong_key_decoder;
                wrong_private_key_rejected = wrong_private_key_rejected && file
                    && wrong_key_decoder.init(file, private_key_a) == appender_decode_result::failed_decode_error;
                bq::file_manager::instance().close_file(file);
                result.add_result(wrong_private_key_rejected, "binary segment wrong private key rejected");

                const bq::string old_version_name = "appender_test/segment_old_version";
                const bq::string old_version_path = TO_ABSOLUTE_PATH(old_version_name + "_1.bt", 0);
                {
                    appender_file_binary_for_test writer;
                    test_ok = writer.init("segment_old_version", make_binary_recovery_test_config(old_version_name, ""), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(102, 0));
                }
                file = bq::file_manager::instance().open_file(old_version_path, bq::file_open_mode_enum::read_write);
                appender_file_binary::appender_file_header file_head;
                bool old_version_rejected = test_ok && file
                    && bq::file_manager::instance().read_file(file, &file_head, sizeof(file_head), bq::file_manager::seek_option::begin, 0) == sizeof(file_head);
                file_head.version = 0;
                old_version_rejected = old_version_rejected
                    && bq::file_manager::instance().write_file(file, &file_head, sizeof(file_head), bq::file_manager::seek_option::begin, 0) == sizeof(file_head);
                bq::file_manager::instance().close_file(file);
                const size_t old_version_size = bq::file_manager::get_file_size(old_version_path);
                {
                    appender_file_binary_appender_recovery_for_test writer;
                    old_version_rejected = old_version_rejected
                        && writer.init("segment_old_version_append", make_binary_recovery_test_config(old_version_name, ""), &log_obj)
                        && !writer.append_appender_recovery_to_file_for_test(old_version_path)
                        && bq::file_manager::get_file_size(old_version_path) == old_version_size;
                }
                result.add_result(old_version_rejected, "binary segment old format recovery rejected");

                const bq::string truncated_name = "appender_test/segment_truncated_metadata";
                const bq::string truncated_path = TO_ABSOLUTE_PATH(truncated_name + "_1.bt", 0);
                {
                    appender_file_binary_for_test writer;
                    test_ok = writer.init("segment_truncated", make_binary_recovery_test_config(truncated_name, pub_key_a), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(103, 0));
                }
                file = bq::file_manager::instance().open_file(truncated_path, bq::file_open_mode_enum::read_write);
                const size_t truncated_size = sizeof(appender_file_binary::appender_file_header)
                    + sizeof(appender_file_binary::appender_file_segment_head) + sizeof(uint64_t) + 16;
                bool truncated_rejected = test_ok && file
                    && bq::file_manager::instance().truncate_file(file, truncated_size);
                bq::file_manager::instance().close_file(file);
                {
                    appender_file_binary_appender_recovery_for_test writer;
                    truncated_rejected = truncated_rejected
                        && writer.init("segment_truncated_append", make_binary_recovery_test_config(truncated_name, ""), &log_obj)
                        && !writer.append_appender_recovery_to_file_for_test(truncated_path)
                        && bq::file_manager::get_file_size(truncated_path) == truncated_size;
                }
                result.add_result(truncated_rejected, "binary segment truncated encryption metadata rejected");

                clear_appender_file_base_test_folder();
                const bq::string encryption_switch_name = "appender_test/segment_encryption_switch";
                const bq::string plaintext_path = TO_ABSOLUTE_PATH(encryption_switch_name + "_1.bt", 0);
                const bq::string encrypted_path = TO_ABSOLUTE_PATH(encryption_switch_name + "_2.bt", 0);
                bool encryption_switch_ok = false;
                {
                    appender_file_binary_for_test writer;
                    encryption_switch_ok = writer.init("segment_switch_plain", make_binary_recovery_test_config(encryption_switch_name, ""), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(104, 0));
                }
                {
                    appender_file_binary_for_test writer;
                    encryption_switch_ok = encryption_switch_ok
                        && writer.init("segment_switch_encrypted", make_binary_recovery_test_config(encryption_switch_name, pub_key_a), &log_obj)
                        && writer.write_payload_for_test(make_segment_payload(104, 1));
                }
                bq::array<appender_file_binary::appender_file_segment_head> plaintext_segments;
                bq::array<appender_file_binary::appender_file_segment_head> encrypted_segments;
                encryption_switch_ok = encryption_switch_ok
                    && read_binary_segment_chain_for_test(plaintext_path, plaintext_segments)
                    && plaintext_segments.size() == 1
                    && plaintext_segments[0].enc_type == appender_file_binary::appender_encryption_type::plaintext
                    && read_binary_segment_chain_for_test(encrypted_path, encrypted_segments)
                    && encrypted_segments.size() == 1
                    && encrypted_segments[0].enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor;
                result.add_result(encryption_switch_ok, "normal appender encryption switch starts a new file");
            }

            static bq::property_value make_binary_recovery_test_config(const bq::string& file_name, const bq::string& pub_key)
            {
                bq::string config = "type=compressed_file\n"
                                    "levels=[all]\n"
                                    "file_name="
                    + file_name + "\n"
                                  "base_dir_type=0\n"
                                  "enable_rolling_log_file=false\n";
                if (!pub_key.is_empty()) {
                    config += "pub_key=" + pub_key + "\n";
                }
                return bq::property_value::create_from_string(config);
            }

            void do_binary_recovery_mmap_case(test_result& result, uint32_t case_index, recovery_encryption_mode source_mode, recovery_encryption_mode target_mode, const bq::string& pub_key_a, const bq::string& pub_key_b)
            {
                char case_id[32];
                snprintf(case_id, sizeof(case_id), "%" PRIu32, case_index);
                const bq::string log_name = bq::string("binary_recovery_mmap_") + case_id;
                const bq::string file_name = "appender_test/" + log_name;
                const bq::string source_file_path = TO_ABSOLUTE_PATH(file_name + "_1.logcompr", 0);
                const bq::string target_file_path = TO_ABSOLUTE_PATH(file_name + "_2.logcompr", 0);
                const bq::string source_pub_key = get_recovery_pub_key(source_mode, pub_key_a, pub_key_b);
                const bq::string target_pub_key = get_recovery_pub_key(target_mode, pub_key_a, pub_key_b);
                bq::array<bq::string> categories;
                categories.push_back("");
                const bq::array<uint8_t> source_entry = make_recovery_log_entry(static_cast<uint64_t>(case_index) * 3 + 1);
                const bq::array<uint8_t> buffer_entry = make_recovery_log_entry(static_cast<uint64_t>(case_index) * 3 + 2);

                bool source_appender_ok = false;
                bool source_appender_mmap_ok = false;
                bool source_buffer_ok = false;
                {
                    log_imp parent_log;
                    source_appender_ok = parent_log.init(log_name, make_recovery_parent_log_config(), categories);
                    appender_file_compressed_mmap_for_test source_appender;
                    source_appender_ok = source_appender_ok
                        && source_appender.init("appender", make_binary_recovery_test_config(file_name, source_pub_key), &parent_log)
                        && source_appender.log(log_entry_handle(source_entry.begin(), static_cast<uint32_t>(source_entry.size())));
                    source_appender_mmap_ok = source_appender_ok
                        && source_appender.get_output_path_for_test() == source_file_path
                        && bq::file_manager::is_file(source_appender.get_mmap_path_for_test());
                    std::atomic<bool> source_buffer_result(false);
                    std::thread source_buffer_thread([&parent_log, &buffer_entry, &source_buffer_result]() {
                        source_buffer_result.store(write_recovery_log_entry(parent_log.get_buffer(), buffer_entry), std::memory_order_relaxed);
                    });
                    source_buffer_thread.join();
                    source_buffer_ok = source_appender_ok && source_buffer_result.load(std::memory_order_relaxed);
                }

                bool target_log_ok = false;
                {
                    log_imp target_log;
                    target_log_ok = target_log.init(log_name, make_recovery_log_config(file_name, target_pub_key), categories);
                    if (target_log_ok) {
                        target_log.process(true);
                    }
                }

                bq::array<appender_file_binary::appender_file_segment_head> source_segments;
                bq::array<appender_file_binary::appender_file_segment_head> target_segments;
                const auto source_enc_type = get_encryption_type(source_mode);
                const auto target_enc_type = get_encryption_type(target_mode);
                const bool append_to_source = source_mode == recovery_encryption_mode::plaintext && target_mode == recovery_encryption_mode::plaintext;
                const bool appender_recovery_compatible = source_mode == recovery_encryption_mode::plaintext
                    || target_mode == recovery_encryption_mode::plaintext
                    || source_mode == target_mode;
                bool segment_chain_ok = false;
                if (append_to_source) {
                    segment_chain_ok = read_binary_segment_chain_for_test(source_file_path, source_segments)
                        && source_segments.size() == 3
                        && source_segments[0].enc_type == source_enc_type
                        && source_segments[1].enc_type == target_enc_type
                        && source_segments[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_appender
                        && source_segments[2].enc_type == target_enc_type
                        && source_segments[2].seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer
                        && !bq::file_manager::is_file(target_file_path);
                } else {
                    segment_chain_ok = read_binary_segment_chain_for_test(source_file_path, source_segments)
                        && source_segments.size() == (appender_recovery_compatible ? 2 : 1)
                        && source_segments[0].enc_type == source_enc_type
                        && read_binary_segment_chain_for_test(target_file_path, target_segments)
                        && target_segments.size() == 2
                        && target_segments[0].enc_type == target_enc_type
                        && target_segments[1].enc_type == target_enc_type
                        && target_segments[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer;
                    if (segment_chain_ok && appender_recovery_compatible) {
                        segment_chain_ok = source_segments[1].enc_type == target_enc_type
                            && source_segments[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_appender;
                    }
                }
                result.add_result(source_appender_ok && source_appender_mmap_ok && source_buffer_ok && target_log_ok && segment_chain_ok,
                    "mmap recovery case:%" PRIu32 " source:%s target:%s", case_index, get_encryption_mode_name(source_mode), get_encryption_mode_name(target_mode));

                const bq::string mmap_dir = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + log_name, 0);
                if (bq::file_manager::is_dir(mmap_dir)) {
                    bq::file_manager::remove_file_or_dir(mmap_dir);
                }
            }

            void do_binary_recovery_mmap_test(test_result& result, const bq::string& pub_key_a, const bq::string& pub_key_b)
            {
                clear_appender_file_base_test_folder();
                uint32_t case_index = 0;
                for (uint8_t source = 0; source < 3; ++source) {
                    for (uint8_t target = 0; target < 3; ++target) {
                        do_binary_recovery_mmap_case(result, case_index++, static_cast<recovery_encryption_mode>(source), static_cast<recovery_encryption_mode>(target), pub_key_a, pub_key_b);
                    }
                }

                std::minstd_rand random(0x5b0c2a71);
                recovery_encryption_mode source_mode = static_cast<recovery_encryption_mode>(random() % 3);
                for (uint32_t i = 0; i < 16; ++i) {
                    const recovery_encryption_mode target_mode = static_cast<recovery_encryption_mode>(random() % 3);
                    do_binary_recovery_mmap_case(result, case_index++, source_mode, target_mode, pub_key_a, pub_key_b);
                    source_mode = target_mode;
                }
                clear_appender_file_base_test_folder();
            }

            void do_binary_recovery_segment_test(test_result& result, const bq::string& pub_key)
            {
                clear_appender_file_base_test_folder();
                log_imp log_obj;
                bq::array<bq::string> categories;
                categories.push_back("");
                bq::property_value log_config = bq::property_value::create_from_string(R"(
                    log.recovery=true
                    appenders_config.appender_0.type=console
                )");
                log_obj.init("binary_recovery_segment_test", log_config, categories);

                const bq::string plain_file_name = "appender_test/recovery_plain";
                const auto plain_config = make_binary_recovery_test_config(plain_file_name, "");
                const bq::string plain_file_path = TO_ABSOLUTE_PATH(plain_file_name + "_1.bt", 0);
                {
                    appender_file_binary_recovery_for_test original;
                    result.add_result(original.init("plain_recovery", plain_config, &log_obj), "create plaintext recovery source file");
                }
                bool plain_recovery_ok = false;
                bq::string plain_recovery_output_path;
                {
                    appender_file_binary_recovery_for_test recovery_writer;
                    plain_recovery_ok = recovery_writer.init("plain_recovery", plain_config, &log_obj)
                        && recovery_writer.begin_log_buffer_recovery_for_test();
                    plain_recovery_output_path = recovery_writer.get_output_path_for_test();
                }
                bq::array<appender_file_binary::appender_file_segment_head> segment_heads;
                const bool plain_chain_ok = read_binary_segment_chain_for_test(plain_file_path, segment_heads)
                    && segment_heads.size() == 2
                    && segment_heads[0].enc_type == appender_file_binary::appender_encryption_type::plaintext
                    && segment_heads[1].enc_type == appender_file_binary::appender_encryption_type::plaintext
                    && segment_heads[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer;
                result.add_result(plain_recovery_ok, "plaintext log-buffer recovery emits a recovery segment");
                result.add_result(plain_recovery_output_path == plain_file_path, "plaintext log-buffer recovery reuses the original indexed file, got:%s", plain_recovery_output_path.c_str());
                result.add_result(plain_chain_ok, "plaintext log-buffer recovery appends to original segment chain, segment_count:%zu", segment_heads.size());

                clear_appender_file_base_test_folder();
                const bq::string encrypted_file_name = "appender_test/recovery_encrypted";
                const auto encrypted_config = make_binary_recovery_test_config(encrypted_file_name, pub_key);
                const auto encrypted_plain_config = make_binary_recovery_test_config(encrypted_file_name, "");
                const bq::string encrypted_file_path_1 = TO_ABSOLUTE_PATH(encrypted_file_name + "_1.bt", 0);
                const bq::string encrypted_file_path_2 = TO_ABSOLUTE_PATH(encrypted_file_name + "_2.bt", 0);
                {
                    appender_file_binary_recovery_for_test original;
                    result.add_result(original.init("encrypted_recovery", encrypted_config, &log_obj), "create encrypted recovery source file");
                }
                bool new_file_recovery_ok = false;
                bq::string new_file_recovery_output_path;
                {
                    appender_file_binary_recovery_for_test recovery_writer;
                    new_file_recovery_ok = recovery_writer.init("encrypted_recovery", encrypted_plain_config, &log_obj)
                        && recovery_writer.begin_log_buffer_recovery_for_test();
                    new_file_recovery_output_path = recovery_writer.get_output_path_for_test();
                }
                segment_heads.clear();
                const bool new_file_chain_ok = bq::file_manager::is_file(encrypted_file_path_1)
                    && read_binary_segment_chain_for_test(encrypted_file_path_2, segment_heads)
                    && segment_heads.size() == 2
                    && segment_heads[0].enc_type == appender_file_binary::appender_encryption_type::plaintext
                    && segment_heads[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer;
                result.add_result(new_file_recovery_ok, "encrypted original recovery emits into a new file");
                result.add_result(new_file_recovery_output_path == encrypted_file_path_2, "encrypted original recovery selects the next indexed file, got:%s", new_file_recovery_output_path.c_str());
                result.add_result(new_file_chain_ok, "encrypted original starts a new plaintext recovery file, segment_count:%zu", segment_heads.size());

                clear_appender_file_base_test_folder();
                const bq::string plain_to_encrypted_file_name = "appender_test/recovery_plain_to_encrypted";
                const auto plain_to_encrypted_plain_config = make_binary_recovery_test_config(plain_to_encrypted_file_name, "");
                const auto plain_to_encrypted_encrypted_config = make_binary_recovery_test_config(plain_to_encrypted_file_name, pub_key);
                const bq::string plain_to_encrypted_file_path_1 = TO_ABSOLUTE_PATH(plain_to_encrypted_file_name + "_1.bt", 0);
                const bq::string plain_to_encrypted_file_path_2 = TO_ABSOLUTE_PATH(plain_to_encrypted_file_name + "_2.bt", 0);
                {
                    appender_file_binary_recovery_for_test original;
                    result.add_result(original.init("plain_to_encrypted_original", plain_to_encrypted_plain_config, &log_obj), "create plaintext source for encrypted recovery");
                }
                bool encrypted_writer_recovery_ok = false;
                bq::string encrypted_writer_recovery_output_path;
                {
                    appender_file_binary_recovery_for_test recovery_writer;
                    encrypted_writer_recovery_ok = recovery_writer.init("plain_to_encrypted_writer", plain_to_encrypted_encrypted_config, &log_obj)
                        && recovery_writer.begin_log_buffer_recovery_for_test();
                    encrypted_writer_recovery_output_path = recovery_writer.get_output_path_for_test();
                }
                segment_heads.clear();
                const bool encrypted_writer_chain_ok = bq::file_manager::is_file(plain_to_encrypted_file_path_1)
                    && read_binary_segment_chain_for_test(plain_to_encrypted_file_path_2, segment_heads)
                    && segment_heads.size() == 2
                    && segment_heads[0].enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor
                    && segment_heads[1].enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor
                    && segment_heads[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer;
                result.add_result(encrypted_writer_recovery_ok, "encrypted log-buffer recovery emits into a new file after a plaintext original");
                result.add_result(encrypted_writer_recovery_output_path == plain_to_encrypted_file_path_2, "encrypted recovery selects the next indexed file after a plaintext original, got:%s", encrypted_writer_recovery_output_path.c_str());
                result.add_result(encrypted_writer_chain_ok, "plaintext original is not reused by encrypted recovery, segment_count:%zu", segment_heads.size());

                clear_appender_file_base_test_folder();
                const bq::string appender_file_name = "appender_test/recovery_appender";
                const auto appender_plain_config = make_binary_recovery_test_config(appender_file_name, "");
                const bq::string appender_file_path = TO_ABSOLUTE_PATH(appender_file_name + "_1.bt", 0);
                {
                    appender_file_binary_recovery_for_test original;
                    result.add_result(original.init("appender_recovery_plain", appender_plain_config, &log_obj), "create appender recovery source file");
                }
                bool appender_recovery_ok = false;
                {
                    appender_file_binary_appender_recovery_for_test recovery_writer;
                    const auto writer_config = make_binary_recovery_test_config("appender_test/recovery_writer", pub_key);
                    appender_recovery_ok = recovery_writer.init("appender_recovery_writer", writer_config, &log_obj)
                        && recovery_writer.append_appender_recovery_to_file_for_test(appender_file_path);
                }
                segment_heads.clear();
                const bool appender_chain_ok = read_binary_segment_chain_for_test(appender_file_path, segment_heads)
                    && segment_heads.size() == 2
                    && segment_heads[0].next_seg_pos != UINT64_MAX
                    && segment_heads[1].enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor
                    && segment_heads[1].seg_type == appender_file_binary::appender_segment_type::recovery_by_appender;
                result.add_result(appender_recovery_ok, "appender recovery appends across an encryption boundary");
                result.add_result(appender_chain_ok, "appender recovery links plaintext and encrypted segments without an orphan tail, segment_count:%zu", segment_heads.size());

                clear_appender_file_base_test_folder();
                const bq::string invalid_file_name = "appender_test/recovery_invalid";
                const auto invalid_plain_config = make_binary_recovery_test_config(invalid_file_name, "");
                const bq::string invalid_file_path = TO_ABSOLUTE_PATH(invalid_file_name + "_1.bt", 0);
                {
                    appender_file_binary_recovery_for_test original;
                    result.add_result(original.init("invalid_recovery_plain", invalid_plain_config, &log_obj), "create invalid-topology recovery source file");
                }
                auto invalid_file = bq::file_manager::instance().open_file(invalid_file_path, bq::file_open_mode_enum::read_write);
                const uint64_t invalid_next_pos = static_cast<uint64_t>(bq::file_manager::instance().get_file_size(invalid_file)) + 1;
                const bool corrupt_link_ok = invalid_file
                    && bq::file_manager::instance().write_file(invalid_file, &invalid_next_pos, sizeof(invalid_next_pos), bq::file_manager::seek_option::begin, sizeof(appender_file_binary::appender_file_header)) == sizeof(invalid_next_pos);
                bq::file_manager::instance().close_file(invalid_file);
                const size_t invalid_file_size_before = bq::file_manager::get_file_size(invalid_file_path);
                bool invalid_recovery_ok = false;
                {
                    appender_file_binary_appender_recovery_for_test recovery_writer;
                    const auto writer_config = make_binary_recovery_test_config("appender_test/invalid_recovery_writer", pub_key);
                    invalid_recovery_ok = recovery_writer.init("invalid_recovery_writer", writer_config, &log_obj)
                        && recovery_writer.append_appender_recovery_to_file_for_test(invalid_file_path);
                }
                const bool invalid_file_unchanged = bq::file_manager::get_file_size(invalid_file_path) == invalid_file_size_before;
                result.add_result(corrupt_link_ok && !invalid_recovery_ok && invalid_file_unchanged, "invalid appender recovery topology is discarded without appending an orphan segment");

                const auto unavailable_config = make_binary_recovery_test_config("appender_test/recovery_unavailable", "");
                bq::platform::test_inject::set_path_filter("recovery_unavailable");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_open);
                appender_file_binary_recovery_for_test unavailable_writer;
                const bool unavailable_init_ok = unavailable_writer.init("unavailable_recovery", unavailable_config, &log_obj);
                const bool unavailable_recovery_ok = unavailable_writer.begin_log_buffer_recovery_for_test();
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);
                result.add_result(unavailable_init_ok && !unavailable_recovery_ok, "log-buffer recovery is discarded when a new indexed file cannot be opened");
            }

            // Hand-built compressed log files: the decoder must reject malformed input instead of
            // allocating from untrusted lengths or formatting garbage.
            // Values of the private appender_file_compressed::item_type and template_sub_type enums.
            enum : uint8_t {
                malformed_item_log_template = 0,
                malformed_item_log_entry = 128,
                malformed_template_format_utf8 = 0,
                malformed_template_thread_info = 1
            };

            static void malformed_put_u32(std::vector<uint8_t>& out, uint32_t value)
            {
                for (uint32_t i = 0; i < 4; ++i) {
                    out.push_back(static_cast<uint8_t>(value >> (8 * i)));
                }
            }

            static std::vector<uint8_t> malformed_file_head(uint32_t category_name_len, const char* category_name)
            {
                std::vector<uint8_t> out;
                malformed_put_u32(out, appender_file_compressed::format_version);
                out.push_back(static_cast<uint8_t>(appender_file_binary::appender_format_type::compressed));
                out.insert(out.end(), 3, static_cast<uint8_t>(0));
                out.insert(out.end(), 8, static_cast<uint8_t>(0xFF)); // next_seg_pos = UINT64_MAX: last segment
                out.push_back(static_cast<uint8_t>(appender_file_binary::appender_segment_type::normal));
                out.push_back(static_cast<uint8_t>(appender_file_binary::appender_encryption_type::plaintext));
                out.insert(out.end(), 2, static_cast<uint8_t>(0));
                const uint8_t magic_and_local_time[] = { 2, 2, 7, 0 };
                out.insert(out.end(), magic_and_local_time, magic_and_local_time + sizeof(magic_and_local_time));
                malformed_put_u32(out, 0); // gmt_offset_hours
                malformed_put_u32(out, 0); // gmt_offset_minutes
                malformed_put_u32(out, 0); // time_zone_diff_to_gmt_ms
                char time_zone_str[32] = "UTC0";
                out.insert(out.end(), time_zone_str, time_zone_str + sizeof(time_zone_str));
                malformed_put_u32(out, 1); // category_count
                malformed_put_u32(out, category_name_len);
                out.insert(out.end(), category_name, category_name + strlen(category_name));
                return out;
            }

            static void malformed_add_item(std::vector<uint8_t>& out, uint8_t item_type, const std::vector<uint8_t>& payload)
            {
                out.push_back(item_type);
                out.push_back(static_cast<uint8_t>(0x80 | payload.size())); // 1 byte VLQ, payload < 128 bytes
                out.insert(out.end(), payload.begin(), payload.end());
            }

            // template "x={}", one thread info, then one log entry with the given argument bytes
            static std::vector<uint8_t> malformed_file_with_entry(const std::vector<uint8_t>& arg_bytes)
            {
                std::vector<uint8_t> out = malformed_file_head(0, "");
                malformed_add_item(out, malformed_item_log_template, { malformed_template_format_utf8, static_cast<uint8_t>(log_level::info), 0x80, 'x', '=', '{', '}' });
                malformed_add_item(out, malformed_item_log_template, { malformed_template_thread_info, 0x80, 0x80, 't' });
                std::vector<uint8_t> entry = { 0x80, 0x80, 0x80 }; // epoch offset, template index, thread info index
                entry.insert(entry.end(), arg_bytes.begin(), arg_bytes.end());
                malformed_add_item(out, malformed_item_log_entry, entry);
                return out;
            }

            static appender_decode_result malformed_decode(const char* name, const std::vector<uint8_t>& bytes, bq::string& out_text)
            {
                bq::string path = TO_ABSOLUTE_PATH(bq::string("bq_decoder_malformed_") + name + ".logcompr", 0);
                {
                    auto handle = bq::file_manager::instance().open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    bq::file_manager::instance().truncate_file(handle, 0);
                    bq::file_manager::instance().write_file(handle, bytes.data(), bytes.size());
                    bq::file_manager::instance().flush_file(handle);
                }
                appender_decode_result result;
                {
                    bq::tools::log_decoder decoder(path);
                    result = decoder.decode();
                    out_text = decoder.get_last_decoded_log_entry();
                }
                bq::file_manager::remove_file_or_dir(path);
                return result;
            }

            void do_decoder_malformed_file_test(test_result& result)
            {
                bq::string text;
                const uint8_t int32_type = static_cast<uint8_t>(log_arg_type_enum::int32_type);
                const uint8_t double_type = static_cast<uint8_t>(log_arg_type_enum::double_type);
                result.add_result(malformed_decode("valid", malformed_file_with_entry({ int32_type, 0x8A /* zigzag(5) */ }), text) == appender_decode_result::success && text.end_with("x=5"),
                    "decoder malformed file: hand-built control file decodes");
                result.add_result(malformed_decode("huge_name", malformed_file_head(0xFFFFFFF0U, "abc"), text) != appender_decode_result::success,
                    "decoder malformed file: category name length larger than the file is rejected");
                result.add_result(malformed_decode("unknown_type", malformed_file_with_entry({ 0x7F }), text) != appender_decode_result::success,
                    "decoder malformed file: unknown argument type is rejected");
                result.add_result(malformed_decode("short_double", malformed_file_with_entry({ double_type, 1, 2 }), text) != appender_decode_result::success,
                    "decoder malformed file: truncated double argument is rejected");
                std::vector<uint8_t> huge_item = malformed_file_head(0, "");
                huge_item.push_back(malformed_item_log_entry);
                const uint8_t vlq_4g[] = { 0x08, 0xFF, 0xFF, 0xFF, 0xE0 }; // 5 byte VLQ: item size close to 4 GiB
                huge_item.insert(huge_item.end(), vlq_4g, vlq_4g + sizeof(vlq_4g));
                huge_item.insert(huge_item.end(), 16, static_cast<uint8_t>(0));
                result.add_result(malformed_decode("huge_item", huge_item, text) != appender_decode_result::success,
                    "decoder malformed file: item size larger than the file is rejected");
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
                do_decoder_malformed_file_test(result);
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
