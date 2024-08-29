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
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/types/ring_buffer.h"
#include "bq_log/log/log_manager.h"
#include "test_category_log.h"

#define UTF8_STR(STR) (STR)
#define UTF16_STR(STR) (bq::u16string(STR))

#define UTF8_CHAR(CHAR) (CHAR)
#define UTF16_CHAR(CHAR) ((char16_t)(CHAR))

namespace bq {
    namespace test {

        static void create_test_log1()
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[all]
            log.buffer_size=65535
            log.reliable_level=normal
			log.thread_mode=sync
			)");
        }

        static void create_test_log2()
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			)");
        }

        static void create_test_log3()
        {
            test_category_log::create_log("test_log3", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			)");
        }

        static void create_test_log4()
        {
            test_category_log::create_log("test_log4", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			)");
        }

        static void create_test_log5()
        {
            test_category_log::create_log("test_log5", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			)");
        }

        static void create_test_log_file_appender()
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[all]
						
						appenders_config.CompressedAppender.type=compressed_file
						appenders_config.CompressedAppender.time_zone=default local time
						appenders_config.CompressedAppender.levels=[error,fatal]
						appenders_config.CompressedAppender.file_name=bqLog/UnitTestLog/test3
						appenders_config.CompressedAppender.is_in_sandbox=false
						appenders_config.CompressedAppender.max_file_size=100000000
						appenders_config.CompressedAppender.expire_time_days=2
						
						appenders_config.RawAppender.type=raw_file
						appenders_config.RawAppender.time_zone=default local time
						appenders_config.RawAppender.levels=[error,fatal]
						appenders_config.RawAppender.file_name=bqLog/UnitTestLog/test3
						appenders_config.RawAppender.is_in_sandbox=false
						appenders_config.RawAppender.max_file_size=100000000
						appenders_config.RawAppender.expire_time_days=2
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA,ModuleB]
			)");
        }

        class create_log_thread : public bq::platform::thread {
        protected:
            virtual void run()
            {
                for (uint32_t i = 0; i < 100; ++i) {
                    create_test_log2();
                    create_test_log3();
                    create_test_log4();
                    create_test_log5();
                    create_test_log1();
                }
            }
        };

        class snapshot_thread : public bq::platform::thread {
        private:
            bq::log* log_ptr;

        public:
            snapshot_thread(bq::log& log)
            {
                log_ptr = &log;
                log_ptr->enable_snapshot(64 * 1024);
            }

        protected:
            virtual void run()
            {
                log_ptr->enable_snapshot(64 * 1024);
                auto begin_epoch_ms = bq::platform::high_performance_epoch_ms();
                while (!is_cancelled()) {
                    bq::string snapshot_str = log_ptr->take_snapshot(false);
                    sleep(0);
                    if (bq::platform::high_performance_epoch_ms() - begin_epoch_ms > 5000) {
                        break;
                    }
                }
            }
        };

        class test_log : public test_base {
            friend class log_test_wrapper;

        private:
            static bq::string log_str;
            static const char* log_c_str; // friendly to IDE debugger which can not use natvis.

        private:
            static void console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length);

            void test_1(test_result& result, const test_category_log& log);
            void test_2(test_result& result, const test_category_log& log);
			void test_3(test_result& result, const test_category_log& log);
			void test_4(test_result& result, const test_category_log& log);

        public:
            virtual test_result test() override
            {
                test_result result;
                create_test_log1();
                auto log_inst = test_category_log::get_log_by_name("test_log");
                test_1(result, log_inst);
                create_test_log2();
                test_2(result, log_inst);
                create_test_log1();
                test_1(result, log_inst);

                create_log_thread thread1;
                create_log_thread thread2;
                create_log_thread thread3;
                create_log_thread thread4;
                create_log_thread thread5;
                test_output(bq::log_level::info, "begin multi-thread log creator test, please wait...")
                    thread1.start();
                thread2.start();
                thread3.start();
                thread4.start();
                thread5.start();
                thread1.join();
                thread2.join();
                thread3.join();
                thread4.join();
                thread5.join();

                snapshot_thread snapeshot(log_inst);
                snapeshot.start();

                test_1(result, log_inst);

                create_test_log_file_appender();
                test_3(result, log_inst);
                snapeshot.cancel();
                snapeshot.join();

				test_4(result, log_inst);
                return result;
            }
        };
    }
}
