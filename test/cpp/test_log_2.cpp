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
#include <thread>
#include <atomic>
#include <cstdio>
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        class test_force_flush_callback {
        public:
            static test_result* test_result_ptr;
            static uint64_t sync_log_id;
            static uint64_t async_log_id;
            static uint64_t idx;
            static uint64_t current_idx_sync;
            static uint64_t current_idx_async;
            static bool multi_thread_test_end;
            static void console_callback(uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length) {
                (void)category_idx;
                (void)log_level;
                (void)length;
                if (log_id != sync_log_id && log_id != async_log_id) {
                    return;
                }
                uint64_t& ref_idx = (log_id == sync_log_id) ? current_idx_sync : current_idx_async;
                ++ref_idx;
                char idx_tmp[32];
                snprintf(idx_tmp, sizeof(idx_tmp), "%" PRIu64, ref_idx);
                bq::string standard_end_str = (log_id == sync_log_id) ? (bq::string("force flush test sync log ") + idx_tmp) : (bq::string("force flush test async log ") + idx_tmp);
                bq::string log_content(content, static_cast<size_t>(length));
                test_result_ptr->add_result(log_content.end_with(standard_end_str), (log_id == sync_log_id) ? "force flush test sync" : "force flush test async");

            }
        };
        test_result* test_force_flush_callback::test_result_ptr = nullptr;
        uint64_t test_force_flush_callback::sync_log_id = 0;
        uint64_t test_force_flush_callback::async_log_id = 0;
        uint64_t test_force_flush_callback::idx = 0;
        uint64_t test_force_flush_callback::current_idx_sync = 0;
        uint64_t test_force_flush_callback::current_idx_async = 0; 
        bool test_force_flush_callback::multi_thread_test_end = false;

        void test_log::test_2(test_result& result, const test_category_log& log_inst)
        {
            result.add_result(log_inst.get_name() == "test_log", "log name test");

            {
                bq::string empty_str;
                bq::string full_str = "123";
                log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "Empty Str Test {}, {}", empty_str, full_str);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA.ClassA]\tEmpty Str Test , 123"), "log update 1");
            }

            {
                log_inst.fatal(log_inst.cat.ModuleA.SystemA, "connect {}:{}");
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA.ClassA]\tEmpty Str Test , 123"), "log update 2");
            }

            {
                bq::string empty_str;
                bq::string full_str = "123";
                log_inst.warning(log_inst.cat.ModuleA.SystemA.ClassA, "Empty Str Test {}, {}", empty_str.c_str(), full_str.c_str());
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA.ClassA]\tEmpty Str Test , 123"), "log update 3");
            }

            {
                bq::string ip = "9.134.131.77";
                uint16_t port = 18900;
                log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "connect {{}:{}}", ip, port);
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA.ClassA]\tconnect {9.134.131.77:18900}"), "brace test 1");
                log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "connect {{}:{}}");
                result.add_result(log_str.end_with("[F]\t[ModuleA.SystemA.ClassA]\tconnect {{}:{}}"), "brace test 2");
            }

            {
                int32_t* pointer = NULL;
                log_inst.error(log_inst.cat.ModuleB, "NULL Pointer Str Test {}, {}", pointer, pointer);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\tNULL Pointer Str Test null, null"), "log update 4");
            }
            {
                int64_t i{ 12 };
                log_inst.error(log_inst.cat.ModuleB, "|{:+10d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       +12|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:10b}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|      1100|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#10b}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|    0b1100|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#10B}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|    0B1100|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:10X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|         C|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#10X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       0XC|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<10d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|12        |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:>010d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0000000012|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<010d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|12        |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0x0000000c|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0xc       |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:>#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0x0000000c|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|000012|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:^06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|  12  |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:+06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|+00012|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<+06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|+12   |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:+06d}|", -i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-00012|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<+06d}|", -i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-12   |"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:06X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|00000C|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#06X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0X000C|"), "layout format");
                double dd{ 3.14159265758 / 2.3 };
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.365e+00|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", 103.1234);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.031e+02|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12d}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.3659098511|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       1.365|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12e}|", 10000000000000);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.000000e+11|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12E}|", (uint64_t)10000000000000);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.000000E+11|"), "layout format");

                i = 15841548461;
                log_inst.error(log_inst.cat.ModuleB, "|{:+10d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|+15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:10b}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1110110000001110101101100010101101|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#10b}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0b1110110000001110101101100010101101|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:10X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t| 3B03AD8AD|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#10X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0X3B03AD8AD|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<10d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:>010d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<010d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0x3b03ad8ad|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0x3b03ad8ad|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:>#010x}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0x3b03ad8ad|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:^06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:+06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|+15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<+06d}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|+15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:+06d}|", -i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:<+06d}|", -i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-15841548461|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:06X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|3B03AD8AD|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:#06X}|", i);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0X3B03AD8AD|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.365e+00|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", 103.1234);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.031e+02|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12d}|", 100);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|         100|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       1.365|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", 10000000000000.0);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.000e+13|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3E}|", 10000000000000.0);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.000E+13|"), "layout format");
            }

            {
                // Concurrency Stress Test 
                auto sync_log = bq::log::create_log("sync_log_stress", R"(
		            appenders_config.appender_1.type=console
                    log.thread_mode=sync
	            )");
                auto async_log = bq::log::create_log("async_log_stress", R"(
		            appenders_config.appender_1.type=console
                    log.thread_mode=async
	            )");

                std::atomic<int32_t> live_thread{0};
                std::atomic<int32_t> left_thread{100};
                bq::string appender;
                for(int32_t i = 0; i < 32; ++i) appender += "a";
                
                // Sync Test
                while(left_thread > 0 || live_thread > 0) {
                    if (left_thread > 0 && live_thread < 5) {
                        left_thread--;
                        live_thread++;
                        std::thread([&]() {
                            // printf("Sync Thread %d started\n", t_id);
                            bq::string log_content = "";
                            for(int32_t i = 0; i < 128; ++i) {
                                log_content += appender;
                                sync_log.info(log_content.c_str());
                            }
                            // printf("Sync Thread %d finished\n", t_id);
                            live_thread--;
                        }).detach();
                    } else {
                        bq::platform::thread::sleep(1);
                    }
                }
                
                // Async Test
                left_thread = 32;
                while(left_thread > 0 || live_thread > 0) {
                    if (left_thread > 0 && live_thread < 5) {
                        int32_t t_id = left_thread;
                        left_thread--;
                        live_thread++;
                        std::thread([&, t_id]() {
                            printf("Async Thread %d started\n", t_id);
                            bq::string log_content = "";
                            for(int32_t i = 0; i < 2048; ++i) { 
                                log_content += appender;
                                async_log.info(log_content.c_str());
                                if ((i + 1) % 500 == 0) printf("Async Thread %d iter %d\n", t_id, i + 1);
                            }
                            printf("Async Thread %d finished\n", t_id);
                            live_thread--;
                        }).detach();
                    } else {
                        bq::platform::thread::sleep(1);
                    }
                }
                async_log.force_flush();
                bq::log::register_console_callback(&test_log::console_callback); 
            }

            {
                const uint64_t seconds = 10;
                test_output_dynamic_param(bq::log_level::info, "testing force flush. wait for %" PRIu64 " seconds please...\n if error exist, an assert will be triggered\n", seconds);
                bq::log::register_console_callback(&test_force_flush_callback::console_callback); 
                test_force_flush_callback::test_result_ptr = &result;
                uint64_t start_time = bq::platform::high_performance_epoch_ms();
                auto sync_log = bq::log::create_log("sync_log", R"(
		            appenders_config.appender_1.type=console
                    log.thread_mode=sync
	            )");
                auto async_log = bq::log::create_log("async_log", R"(
		            appenders_config.appender_1.type=console
                    log.thread_mode=async
	            )");
                test_force_flush_callback::sync_log_id = sync_log.get_id();
                test_force_flush_callback::async_log_id = async_log.get_id();
                std::thread tr1([]() {
                    auto log_obj = bq::log::get_log_by_name("async_log");
                    while (!test_force_flush_callback::multi_thread_test_end) {
                        log_obj.force_flush();
                        bq::platform::thread::sleep(3);
                    }
                    });
                tr1.detach();
                std::thread tr2([]() {
                    while (!test_force_flush_callback::multi_thread_test_end) {
                        bq::log::force_flush_all_logs();
                        bq::platform::thread::sleep(3);
                    }
                    });
                tr2.detach();

                while (true) {
                    sync_log.info("force flush test sync log {}", ++test_force_flush_callback::idx);
                    async_log.info("force flush test async log {}", test_force_flush_callback::idx);
                    if (bq::platform::high_performance_epoch_ms() - start_time >= seconds * 1000) {
                        test_force_flush_callback::multi_thread_test_end = true;
                        break;
                    }
                }
                bq::log::force_flush_all_logs();
                result.add_result(test_force_flush_callback::idx == test_force_flush_callback::current_idx_sync, "force flush test sync total count");
                result.add_result(test_force_flush_callback::idx == test_force_flush_callback::current_idx_async, "force flush test async total count");

                bq::log::register_console_callback(&test_log::console_callback);
                test_output_dynamic(bq::log_level::info, "force flush testing is finished!\n");
            }
            
            {
                // snapshot test
                auto snapshot_log = test_category_log::create_log("snapshot_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
                        snapshot.buffer_size=100000
                        snapshot.levels=[info,error]
                        snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			        )");

                auto snapshot = snapshot_log.take_snapshot("gmt");
                snapshot_log.verbose("AAAA");
                result.add_result(snapshot_log.take_snapshot("gmt") == snapshot, "snapshot test 1");
                snapshot_log.info(snapshot_log.cat.ModuleA.SystemA, "AAAA");
                result.add_result(snapshot_log.take_snapshot("gmt") == snapshot, "snapshot test 2");
                snapshot_log.error(snapshot_log.cat.ModuleA.SystemA.ClassA, "AAAA");
                auto new_snapshot1 = snapshot_log.take_snapshot("gmt");
                result.add_result(new_snapshot1 != snapshot && new_snapshot1.end_with("AAAA\n"), "snapshot test 3");
                snapshot_log.error(snapshot_log.cat.ModuleA.SystemA.ClassA, "BBBB");
                auto snapshot_log2 = test_category_log::create_log("snapshot_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=localtime
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
                        snapshot.buffer_size=1000000       //more memory
                        snapshot.levels=[info,error]
                        snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			        )");
                snapshot_log2.error(snapshot_log2.cat.ModuleA.SystemA.ClassA, "CCCC");
                auto new_snapshot2 = snapshot_log2.take_snapshot("gmt");
                result.add_result(new_snapshot2.begin_with(new_snapshot1), "snapshot test 4");
                result.add_result(new_snapshot2.find("BBBB") != bq::string::npos, "snapshot test 5");
                result.add_result(new_snapshot2.find("CCCC") > new_snapshot2.find("BBBB"), "snapshot test 6");
            }
        }
    }
}
