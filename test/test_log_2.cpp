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
#include "test_log.h"
#include "bq_common/platform/thread/thread.h"

namespace bq {
    namespace test {
        static bq::string multi_thread_string_test_str = "1234567890";

        class multi_thread_string_test_modifier : public bq::platform::thread {
        protected:
            virtual void run() override
            {
                while (!is_cancelled()) {
                    multi_thread_string_test_str = "1234567890";
                    multi_thread_string_test_str = "#";
                }
            }
        };

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
                test_output_dynamic(bq::log_level::info, "testing multithread string log. wait for 10 seconds please...\n if error exist, an assert will be triggered\n");

                multi_thread_string_test_modifier modifier_thread;
                modifier_thread.start();

                uint64_t start_time = bq::platform::high_performance_epoch_ms();
                while (true) {
                    bq::string test_str = "123456789";
                    for (uint32_t i = 0; i < 512; ++i) {
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:0>+10d}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:0^+10d}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:0<+10d}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:<>10d}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:<^10d}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:0>15} |{:<<10d}|", multi_thread_string_test_str, test_str, i);

                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:0>+10.3f}|", multi_thread_string_test_str, test_str, i * 3.1415);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:<<+10b}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:<<+#10b}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:<<+#10x}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:<<#10e}|", multi_thread_string_test_str, test_str, i);
                        log_inst.fatal(log_inst.cat.ModuleA.SystemA.ClassA, "multi_thread Str Test {} {:<15} |{:0>10.2f}|", multi_thread_string_test_str, test_str, i * 3.1415);
                    }
                    int64_t i { 12 };
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
                    double dd { 3.14159265758 / 2.3 };
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

                    if (bq::platform::high_performance_epoch_ms() - start_time >= 10 * 1000) {
                        break;
                    }
                }
                modifier_thread.cancel();
                modifier_thread.join();
                test_output_dynamic(bq::log_level::info, "multithread string log testing is finished!\n");
            }

            {
                // snapshot test
                auto snapshot_log = test_category_log::create_log("snapshot_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
                        snapshot.buffer_size=100000
                        snapshot.levels=[info,error]
                        snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			        )");

                auto snapshot = snapshot_log.take_snapshot(true);
                snapshot_log.verbose("AAAA");
                result.add_result(snapshot_log.take_snapshot(true) == snapshot, "snapshot test 1");
                snapshot_log.info(snapshot_log.cat.ModuleA.SystemA, "AAAA");
                result.add_result(snapshot_log.take_snapshot(true) == snapshot, "snapshot test 2");
                snapshot_log.error(snapshot_log.cat.ModuleA.SystemA.ClassA, "AAAA");
                auto new_snapshot1 = snapshot_log.take_snapshot(true);
                result.add_result(new_snapshot1 != snapshot && new_snapshot1.end_with("AAAA\n"), "snapshot test 3");
                snapshot_log.error(snapshot_log.cat.ModuleA.SystemA.ClassA, "BBBB");
                auto snapshot_log2 = test_category_log::create_log("snapshot_log", R"(
						appenders_config.ConsoleAppender.type=console
						appenders_config.ConsoleAppender.time_zone=default local time
						appenders_config.ConsoleAppender.levels=[error,fatal]
					
						log.thread_mode=sync
						log.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
                        snapshot.buffer_size=1000000       //more memory
                        snapshot.levels=[info,error]
                        snapshot.categories_mask=[ModuleA.SystemA.ClassA,ModuleB]
			        )");
                snapshot_log2.error(snapshot_log2.cat.ModuleA.SystemA.ClassA, "CCCC");
                auto new_snapshot2 = snapshot_log2.take_snapshot(true);
                result.add_result(new_snapshot2.begin_with(new_snapshot1), "snapshot test 4");
                result.add_result(new_snapshot2.find("BBBB") != bq::string::npos, "snapshot test 5");
                result.add_result(new_snapshot2.find("CCCC") > new_snapshot2.find("BBBB"), "snapshot test 6");
            }
        }
    }
}
