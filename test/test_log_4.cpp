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
#include "test_log.h"

namespace bq {
    namespace test {

        struct test_4_log_entry {
            uint64_t log_id;
            int32_t category_idx;
            int32_t log_level;
            bq::string content;
        };
        enum class test_4_check_type {
            callback,
            fetch
        };

        static bq::platform::thread* test_4_callback_thread_ = nullptr;
        static bq::platform::thread* test_4_fetch_thread_ = nullptr;
        static test_result* result_ = nullptr;
        static constexpr int32_t total_test_4_test_round_ = 200000;
        static bq::platform::spin_lock test_4_lock_;
        static bq::platform::atomic<int32_t> left_test_4_round_ = total_test_4_test_round_;
        static test_4_check_type test_4_check_status_ = test_4_check_type::callback;
        static bq::array<test_4_log_entry> test_4_check_array_;

        static bool check(test_4_check_type type, uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
        {
            (void)length;
            bq::platform::scoped_spin_lock lock(test_4_lock_);
            left_test_4_round_.add_fetch_seq_cst(-1);

            bool result = true;
            if (test_4_check_array_.is_empty()) {
                test_4_check_array_.push_back(test_4_log_entry { log_id, category_idx, log_level, bq::string(content) });
                test_4_check_status_ = type;
            } else if (test_4_check_status_ == type) {
                test_4_check_array_.push_back(test_4_log_entry { log_id, category_idx, log_level, bq::string(content) });
            } else {
                const auto& first = test_4_check_array_[0];
                result = (log_id == first.log_id && category_idx == first.category_idx && log_level == first.log_level && content == first.content);
                test_4_check_array_.erase(test_4_check_array_.begin());
            }
            if (!result) {
                test_4_callback_thread_->cancel();
                test_4_fetch_thread_->cancel();
            }
            return result;
        }

        static void BQ_STDCALL test_4_console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
        {
            bool check_result = check(test_4_check_type::callback, log_id, category_idx, log_level, content, length);
            result_->add_result(check_result, "console callback check failed:%s", content);
        }

        static void BQ_STDCALL test_4_console_fetch(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
        {
            bool check_result = check(test_4_check_type::fetch, log_id, category_idx, log_level, content, length);
            result_->add_result(check_result, "console callback check failed:%s", content);
        }

        class test_4_callback_thread : public bq::platform::thread {
        protected:
            void run() override
            {
                bq::log::register_console_callback(test_4_console_callback);
                auto log1 = test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[all]
						log.thread_mode=sync
						log.buffer_size=64000000
			    )");
                auto log2 = test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[all]
						log.thread_mode=independent
						log.buffer_size=64000000
			    )");
                auto log3 = test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[all]
						log.thread_mode=async
						log.buffer_size=64000000
			    )");
                for (int32_t i = 0; i < total_test_4_test_round_; ++i) {
                    if (is_cancelled()) {
                        break;
                    }
                    log1.info(log1.cat.ModuleA.SystemA, "Log1 log, {}", i);
                    log2.warning(log2.cat.ModuleA.SystemA, "Log2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 logLog2 log, {}", i);
                    log3.fatal(log3.cat.ModuleA.SystemA, u"Log3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 logLog3 log, {}", i);
                }
                log1.force_flush();
                log2.force_flush();
                log3.force_flush();
                bq::log::unregister_console_callback(test_4_console_callback);
            }
        };

        class test_4_fetch_thread : public bq::platform::thread {
        protected:
            void run() override
            {
                while (left_test_4_round_.load() > 0) {
                    if (is_cancelled()) {
                        break;
                    }
                    if (!bq::log::fetch_and_remove_console_buffer(test_4_console_fetch)) {
                        bq::platform::thread::yield();
                    }
                }
            }
        };

        void test_log::test_4(test_result& result, const test_category_log& log_inst)
        {
            (void)log_inst;
            test_output(bq::log_level::info, "console test begin              \n");
            result_ = &result;
            bq::log::set_console_buffer_enable(true);
            test_4_callback_thread_ = new test_4_callback_thread();
            test_4_callback_thread_->start();
            test_4_fetch_thread_ = new test_4_fetch_thread();
            test_4_fetch_thread_->start();
            test_4_callback_thread_->join();
            delete test_4_callback_thread_;
            test_4_callback_thread_ = nullptr;
            test_4_fetch_thread_->join();
            delete test_4_fetch_thread_;
            test_4_fetch_thread_ = nullptr;
            result_ = nullptr;

            bq::log::set_console_buffer_enable(false);
            test_output(bq::log_level::info, "console test finished           \n");
        }
    }
}
