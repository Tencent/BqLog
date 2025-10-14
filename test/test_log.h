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
#include <string>
#include <locale>
#include <stdarg.h>
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_manager.h"
#include "test_category_log.h"

#define UTF8_STR(STR) (STR)
#define UTF16_STR(STR) (bq::u16string(STR))

#define UTF8_CHAR(CHAR) (CHAR)
#define UTF16_CHAR(CHAR) ((char16_t)(CHAR))

namespace bq {
    namespace test {

        static void log_creator_1(bq::string snapshot_config = "")
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[all]
                        log.buffer_size=65535
                        log.recovery=true
			            log.thread_mode=sync

			        )"
                    + snapshot_config);
        }

        static void log_creator_2(bq::string snapshot_config = "")
        {
            test_category_log::create_log("test_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]

			        )"
                    + snapshot_config);
        }

        static void log_creator_3(bq::string snapshot_config = "")
        {
            test_category_log::create_log("test_log3", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync

			        )"
                    + snapshot_config);
        }

        static void log_creator_4(bq::string snapshot_config = "")
        {
            test_category_log::create_log("test_log4", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]

			        )"
                    + snapshot_config);
        }

        static void log_creator_5(bq::string snapshot_config = "")
        {
            test_category_log::create_log("test_log5", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]

			        )"
                    + snapshot_config);
        }

        class create_log_thread : public bq::platform::thread {
        protected:
            virtual void run()
            {
                for (uint32_t i = 0; i < 100; ++i) {
                    log_creator_2(i % 2 == 0 ? "snapshot.buffer_size = 65536" : "");
                    log_creator_3(i % 2 == 0 ? "snapshot.buffer_size = 65536" : "");
                    log_creator_4(i % 2 == 0 ? "snapshot.buffer_size = 65536" : "");
                    log_creator_5(i % 2 == 0 ? "snapshot.buffer_size = 65536" : "");
                    log_creator_1(i % 2 == 0 ? "snapshot.buffer_size = 65536" : "");
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
                log_creator_1("snapshot.buffer_size = 65536");
                auto log_inst = test_category_log::get_log_by_name("test_log");
                test_1(result, log_inst);
                log_creator_2();
                test_2(result, log_inst);
                log_creator_1("snapshot.buffer_size = 65536");
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

                test_3(result, log_inst);

                test_4(result, log_inst);
                return result;
            }
        };
    }
}
