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
#include <random>
#include <locale.h>
#include <math.h>
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        class test_force_flush_callback {
        public:
            static test_result* test_result_ptr;
            static uint64_t sync_log_id;
            static uint64_t async_log_id;
            static bq::platform::atomic<uint64_t> idx;
            static bq::platform::atomic<uint64_t> current_idx_sync;
            static bq::platform::atomic<uint64_t> current_idx_async;
            static bool multi_thread_test_end;
            static void console_callback(uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length)
            {
                (void)category_idx;
                (void)log_level;
                (void)length;
                if (log_id != sync_log_id && log_id != async_log_id) {
                    return;
                }
                bq::platform::atomic<uint64_t>& ref_idx = (log_id == sync_log_id) ? current_idx_sync : current_idx_async;
                ref_idx.fetch_add_seq_cst(1);
                char idx_tmp[32];
                snprintf(idx_tmp, sizeof(idx_tmp), "%" PRIu64, ref_idx.load_seq_cst());
                bq::string standard_end_str = (log_id == sync_log_id) ? (bq::string("force flush test sync log ") + idx_tmp) : (bq::string("force flush test async log ") + idx_tmp);
                bq::string log_content(content, static_cast<size_t>(length));
                test_result_ptr->add_result(log_content.end_with(standard_end_str), (log_id == sync_log_id) ? "force flush test sync" : "force flush test async");
            }
        };
        test_result* test_force_flush_callback::test_result_ptr = nullptr;
        uint64_t test_force_flush_callback::sync_log_id = 0;
        uint64_t test_force_flush_callback::async_log_id = 0;
        bq::platform::atomic<uint64_t> test_force_flush_callback::idx = 0;
        bq::platform::atomic<uint64_t> test_force_flush_callback::current_idx_sync = 0;
        bq::platform::atomic<uint64_t> test_force_flush_callback::current_idx_async = 0;
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
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.366e+00|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", 103.1234);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.031e+02|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12d}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.3659098511|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       1.366|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{}|", -0.5);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-0.500000000000000|"), "layout format negative fraction");
                log_inst.error(log_inst.cat.ModuleB, "|{:+.1f}|", -0.25f);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-0.2|"), "layout format negative fraction with sign");
                log_inst.error(log_inst.cat.ModuleB, "|{:.2f}|", 0.96);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|0.96|"), "layout format rounding");
                log_inst.error(log_inst.cat.ModuleB, "|{:.0f}|", 1.75);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|2|"), "layout format rounding to integer");
                log_inst.error(log_inst.cat.ModuleB, "|{:.1f}|", 1e20);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|100000000000000000000.0|"), "layout format beyond int64");
                log_inst.error(log_inst.cat.ModuleB, "|{}|{}|{}|", static_cast<double>(NAN), static_cast<double>(INFINITY), -static_cast<double>(INFINITY));
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|nan|inf|-inf|"), "layout format nan and infinity");
                log_inst.error(log_inst.cat.ModuleB, "|{:12e}|", 10000000000000);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.000000e+13|"), "layout format integer e-style");
                log_inst.error(log_inst.cat.ModuleB, "|{:12E}|", (uint64_t)10000000000000);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.000000E+13|"), "layout format integer e-style");
                log_inst.error(log_inst.cat.ModuleB, "v=[{:e}] end", 100000);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\tv=[1.000000e+05] end"), "layout format integer e-style keeps preceding text");
                log_inst.error(log_inst.cat.ModuleB, "|{:e}|{:+.2e}|{:.1e}|{:.0e}|", -1500, (uint16_t)1500, 99999, 25);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|-1.500000e+03|+1.50e+03|1.0e+05|2e+01|"), "layout format integer e-style sign and rounding");
                {
                    // A spec longer than 10 characters is not supported and must not keep its width:
                    // the output buffer is not grown for it, so padding near the end of the buffer
                    // used to write past it. Try every line length around the initial 1024 byte buffer.
                    bool long_spec_ok = true;
                    bool width_ok = true;
                    bq::string padding;
                    bq::string spaces;
                    for (uint32_t space = 0; space < 98; ++space) {
                        spaces.push_back(' ');
                    }
                    for (uint32_t pad = 0; pad < 1100; ++pad) {
                        log_inst.error(log_inst.cat.ModuleB, (padding + "{:<99--------}").c_str(), 1);
                        long_spec_ok &= log_str.end_with("[E]\t[ModuleB]\t" + padding + "1");
                        log_inst.error(log_inst.cat.ModuleB, (padding + "{:<99}").c_str(), 1);
                        width_ok &= log_str.end_with("[E]\t[ModuleB]\t" + padding + "1" + spaces);
                        padding.push_back('a');
                    }
                    result.add_result(long_spec_ok, "layout format spec longer than 10 characters");
                    result.add_result(width_ok, "layout format width near the end of the buffer");
                }
                {
                    const char* current_locale = setlocale(LC_NUMERIC, nullptr);
                    bq::string saved_locale = current_locale ? current_locale : "C";
                    const char* comma_locales[] = { "de-DE", "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8" };
                    bool locale_applied = false;
                    for (const char* name : comma_locales) {
                        if (setlocale(LC_NUMERIC, name)) {
                            locale_applied = true;
                            break;
                        }
                    }
                    if (locale_applied) {
                        log_inst.error(log_inst.cat.ModuleB, "|{:.2f}|{:.1f}|", 1.25, 1e20);
                        result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.25|100000000000000000000.0|"), "layout format fixed notation ignores LC_NUMERIC");
                        log_inst.error(log_inst.cat.ModuleB, "|{:e}|{:12.3E}|{:.25f}|", 1.5, 103.1234, 0.1);
                        result.add_result(log_str.end_with("[E]\t[ModuleB]\t|1.500000e+00|   1.031E+02|0.1000000000000000055511151|"), "layout format snprintf path ignores LC_NUMERIC");
                        setlocale(LC_NUMERIC, saved_locale.c_str());
                    }
                }
                {
                    std::mt19937_64 rng(20261009);
                    char fmt[32];
                    char expected[512];
                    for (int32_t precision = 0; precision <= 19; ++precision) {
                        snprintf(fmt, sizeof(fmt), "|{:.%" PRId32 "f}|", precision);
                        for (int32_t n = 0; n < 300; ++n) {
                            double value = 0;
                            uint64_t bits = rng();
                            switch (n % 4) {
                            case 0:
                                value = std::uniform_real_distribution<double>(-1000.0, 1000.0)(rng);
                                break;
                            case 1:
                                // any magnitude below 2^64, subnormals included
                                bits = (bits & 0x800FFFFFFFFFFFFFULL) | (((bits >> 52) % (1023 + 64)) << 52);
                                memcpy(&value, &bits, sizeof(value));
                                break;
                            case 2:
                                // exact binary ties
                                value = static_cast<double>(static_cast<int64_t>(bits % 2000) - 1000) + static_cast<double>((bits >> 32) % 16) / 16.0;
                                break;
                            default:
                                value = static_cast<double>(static_cast<float>(std::uniform_real_distribution<double>(-100000.0, 100000.0)(rng)));
                                break;
                            }
                            snprintf(expected, sizeof(expected), "|%.*f|", static_cast<int>(precision), value);
                            log_inst.error(log_inst.cat.ModuleB, fmt, value);
                            result.add_result(log_str.end_with(expected), "layout format %s of %.17g matches printf %s", fmt, value, expected);
                        }
                    }
                }

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
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.366e+00|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3e}|", 103.1234);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|   1.031e+02|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12d}|", 100);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|         100|"), "layout format");
                log_inst.error(log_inst.cat.ModuleB, "|{:12.3}|", dd);
                result.add_result(log_str.end_with("[E]\t[ModuleB]\t|       1.366|"), "layout format");
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

                std::atomic<int32_t> live_thread { 0 };
                std::atomic<int32_t> left_thread { 100 };
                bq::string appender;
                for (int32_t i = 0; i < 32; ++i)
                    appender += "a";

                // Sync Test
                while (left_thread > 0 || live_thread > 0) {
                    if (left_thread > 0 && live_thread < 5) {
                        left_thread--;
                        live_thread++;
                        std::thread([&]() {
                            bq::string log_content = "";
                            for (int32_t i = 0; i < 128; ++i) {
                                log_content += appender;
                                sync_log.info(log_content.c_str());
                            }
                            live_thread--;
                        }).detach();
                    } else {
                        bq::platform::thread::sleep(1);
                    }
                }

                // Async Test
                left_thread = 128;
                while (left_thread > 0 || live_thread > 0) {
                    if (left_thread > 0 && live_thread < 5) {
                        left_thread--;
                        live_thread++;
                        std::thread([&]() {
                            bq::string log_content = "";
                            for (int32_t i = 0; i < 2048; ++i) {
                                log_content += appender;
                                async_log.info(log_content.c_str());
                            }
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
                    sync_log.info("force flush test sync log {}", test_force_flush_callback::idx.add_fetch_seq_cst(1));
                    async_log.info("force flush test async log {}", test_force_flush_callback::idx.load_seq_cst());
                    if (bq::platform::high_performance_epoch_ms() - start_time >= seconds * 1000) {
                        test_force_flush_callback::multi_thread_test_end = true;
                        break;
                    }
                }
                bq::log::force_flush_all_logs();
                bool force_flush_sync_passed = (test_force_flush_callback::idx.load_seq_cst() == test_force_flush_callback::current_idx_sync.load_seq_cst());
                bool force_flush_async_passed = (test_force_flush_callback::idx.load_seq_cst() == test_force_flush_callback::current_idx_async.load_seq_cst());
                result.add_result(force_flush_sync_passed, "force flush test sync total count, %" PRIu64 ", %" PRIu64, test_force_flush_callback::idx.load_seq_cst(), test_force_flush_callback::current_idx_sync.load_seq_cst());
                result.add_result(force_flush_async_passed, "force flush test async total count, %" PRIu64 ", %" PRIu64, test_force_flush_callback::idx.load_seq_cst(), test_force_flush_callback::current_idx_async.load_seq_cst());
                if (!force_flush_sync_passed || !force_flush_async_passed) {
                    bq::log::force_flush_all_logs();
                    test_output_param(bq::log_level::error, "force flush re-check, idx:%" PRIu64 ", sync:%" PRIu64 ", async:%" PRIu64, test_force_flush_callback::idx.load_seq_cst(), test_force_flush_callback::current_idx_sync.load_seq_cst(), test_force_flush_callback::current_idx_async.load_seq_cst());
                }

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
