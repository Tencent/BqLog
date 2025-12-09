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

namespace bq {
    namespace test {
        void clear_appender_file_base_test_folder() {
            if (bq::file_manager::is_dir(TO_ABSOLUTE_PATH("appender_test", 0))) {
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("appender_test", 0));
            }
        }

        class appender_file_base_for_test : public appender_file_base {
            template<typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name);
        protected:
            virtual bool parse_exist_log_file(parse_file_context& context) override {
                (void)context;
                return true;
            }
            virtual bq::string get_file_ext_name() override {
                return ".ft";
            }
            virtual bool init_impl(const bq::property_value& config_obj) {
                appender_file_base::init_impl(config_obj);
                open_new_indexed_file_by_name();
                return true;
            }
        public:
            void read_mode() {
                bq::file_manager::instance().seek(get_file_handle(), bq::file_manager::seek_option::begin, 0);
            }
        };
        class appender_file_binary_for_test : public appender_file_binary {
            template<typename AppenderType>
            friend void do_appender_test(test_result& result, const bq::string test_name);
        protected:
            virtual bool parse_exist_log_file(parse_file_context& context) override {
                return appender_file_binary::parse_exist_log_file(context);
            }
            virtual bq::string get_file_ext_name() override {
                return ".bt";
            }
            virtual appender_file_binary::appender_format_type get_appender_format() const override{
                return appender_format_type::raw;
            }
            virtual uint32_t get_binary_format_version() const override{
                return 1;
            }
            virtual bool init_impl(const bq::property_value& config_obj) {
                appender_file_binary::init_impl(config_obj);
                open_new_indexed_file_by_name();
                return true;
            }
        public:
            void read_mode() {
                parse_file_context context("test_appender_file");;
                parse_exist_log_file(context); //used to seek file to begin of data section
            }
        };


        template<typename AppenderType>
        void do_appender_test(test_result& result, const bq::string test_name) {
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
                        appenders_config.appender_console.type=console
	                )");
            log_obj.init(test_name, log_config, categories);
            bq::property_value appender_config = bq::property_value::create_from_string(R"(
		                type=text_file
		                levels=[all]
		                file_name= appender_test/file_appender
                        base_dir_type=0
                        enable_rolling_log_file=false
                        )");
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
                    appender_write.mark_write_finished();
                    total_write_size += new_size;
                }
                if (total_write_size > 128 * 1024 * 1024) {
                    appender_write.flush_cache();
                    appender_write.flush_io();
                    break;
                }
            }
            AppenderType appender_read;
            appender_read.init("test_appender", appender_config, &log_obj);
            appender_read.read_mode();
            size_t left_read_size = total_write_size;
            size_t byte_pos = 0;
            bool check_result = true;
            while (left_read_size > 0 && check_result) {
                auto handle = appender_read.read_with_cache(1024);
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


        class test_log_appender : public test_base {
        private:
            void do_console_appender_test(test_result& result) {
                test_output_dynamic(bq::log_level::info, "appender_console test begin, please wait...                \r");
                clear_appender_file_base_test_folder();
                (void)result;
            }
            void do_file_appender_test(test_result& result) {
                do_appender_test<appender_file_base_for_test>(result, "appender_file_base_test");
                
            }
            void do_binary_appender_test(test_result& result) {
                do_appender_test<appender_file_binary_for_test>(result, "appender_file_binary_test");
            }
            void do_binary_appender_test_with_enc(test_result& result) {
                do_appender_test<appender_file_binary_for_test>(result, "appender_file_binary_test");
            }
        public:
            virtual test_result test() override
            {
                test_result result;
                for (int32_t i = 0; i < 8; ++i) {
                    do_file_appender_test(result);
                    do_binary_appender_test(result);
                    do_console_appender_test(result);
                    do_binary_appender_test_with_enc(result);
                }
                return result;
            }
        };
    }
}
