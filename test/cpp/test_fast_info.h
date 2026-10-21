#pragma once
/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
// C++20 only: tests for log.fast_info<"...">(args...), the macro-free fast log interface.
#if defined(BQ_CPP_20)
#include "test_base.h"
#include "test_category_log.h"
#include <thread>
#include <vector>

namespace {
    struct u8_cstr_format {
        size_t bq_log_format_str_size() const { return 4; }
        const char8_t* bq_log_format_str_chars() const { return u8"abcd"; }
    };
    struct u8_writer_format {
        size_t bq_log_format_str_size() const { return 1; }
        void bq_log_custom_format(char8_t* value, size_t size) const
        {
            value[0] = u8'a';
            (void)size;
        }
    };
    static_assert(bq::tools::_is_bq_log_format_type<u8_cstr_format>::value
            && bq::tools::_is_bq_log_format_type<u8_cstr_format>::arg_type == bq::log_arg_type_enum::string_utf8_type,
        "a custom type exposing const char8_t* is a UTF-8 format");
    static_assert(bq::tools::_is_bq_log_format_type<u8_writer_format>::value
            && bq::tools::_is_bq_log_format_type<u8_writer_format>::arg_type == bq::log_arg_type_enum::string_utf8_type,
        "a custom type writing through char8_t* is a UTF-8 format");
}

namespace bq {
    namespace test {
        class test_fast_info : public test_base {
            static bq::string make_name(const char* tag)
            {
                char name[96];
                snprintf(name, sizeof(name), "fast_info_%s_%" PRIu64, tag, bq::platform::high_performance_epoch_ms());
                return bq::string(name);
            }

            static bq::string make_config(const bq::string& file_name, const char* extra = "")
            {
                return bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[all]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "log.high_perform_mode_freq_threshold_per_second=1000000\n"
                    + extra;
            }

            // the message part of a text line: " <time>[tid-x ]\t[LEVEL]\t[Category]\t<message>"
            static bq::string message_of(const bq::string& line)
            {
                const size_t tab = line.find_last(bq::string("\t"));
                return tab == bq::string::npos ? line : line.substr(tab + 1);
            }

            static bq::array<bq::string> split_lines(const bq::string& data)
            {
                bq::array<bq::string> lines;
                size_t begin = 0;
                while (begin < data.size()) {
                    size_t end = data.find("\n", begin);
                    if (end == bq::string::npos) {
                        end = data.size();
                    }
                    if (end > begin) {
                        lines.push_back(data.substr(begin, end - begin));
                    }
                    begin = end + 1;
                }
                return lines;
            }

            // 1. same content through both interfaces, for a spread of argument types and levels
            static void test_output_match(test_result& result)
            {
                const bq::string name = make_name("match");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::log log = bq::log::create_log(name, make_config(file_name));
                result.add_result(log.is_valid(), "fast_info: output match log create");
                if (!log.is_valid()) {
                    return;
                }
                const int64_t i64 = -1234567890123LL;
                const uint64_t u64 = 12345678901234567890ULL;
                const float f32 = 1.5f;
                const double f64 = -2.25;
                const bool flag = true;
                const char ch = 'Z';
                const char* cstr = "cstr";
                const bq::string bstr = "bstr";
                std::string sstr = "sstr";
                const void* ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(0x1000));

                BQ_LOG_FAST_INFO(log, "types {} {} {} {} {} {} {} {} {} {} {}", int32_t(7), i64, u64, f32, f64, flag, ch, cstr, bstr, sstr, ptr);
                log.fast_info<"types {} {} {} {} {} {} {} {} {} {} {}">(int32_t(7), i64, u64, f32, f64, flag, ch, cstr, bstr, sstr, ptr);

                BQ_LOG_FAST_INFO(log, "no args here");
                log.fast_info<"no args here">();

                BQ_LOG_FAST_VERBOSE(log, "level verbose {}", int32_t(1));
                log.fast_verbose<"level verbose {}">(int32_t(1));
                BQ_LOG_FAST_DEBUG(log, "level debug {}", int32_t(2));
                log.fast_debug<"level debug {}">(int32_t(2));
                BQ_LOG_FAST_WARNING(log, "level warning {}", int32_t(3));
                log.fast_warning<"level warning {}">(int32_t(3));
                BQ_LOG_FAST_ERROR(log, "level error {}", int32_t(4));
                log.fast_error<"level error {}">(int32_t(4));
                BQ_LOG_FAST_FATAL(log, "level fatal {}", int32_t(5));
                log.fast_fatal<"level fatal {}">(int32_t(5));

                BQ_LOG_FAST_INFO(log, "many {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
                log.fast_info<"many {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

                log.force_flush();
                const auto lines = split_lines(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0)));

                // each expected message must appear at least twice: once from the macro, once from fast_info
                const char* expected[] = {
                    "no args here",
                    "level verbose 1",
                    "level debug 2",
                    "level warning 3",
                    "level error 4",
                    "level fatal 5",
                    "types 7",
                    "many 1",
                };
                for (const char* want : expected) {
                    uint32_t hits = 0;
                    for (const auto& line : lines) {
                        if (message_of(line).find(want) != bq::string::npos) {
                            ++hits;
                        }
                    }
                    result.add_result(hits >= 2, "fast_info: both interfaces produced '%s' (%u)", want, hits);
                }

                // stronger: the two 'types' messages must be byte-identical, and the two level-tagged
                // lines must share the same level tag as well (only the timestamp may differ)
                bq::string types_first;
                bq::string types_second;
                for (const auto& line : lines) {
                    if (message_of(line).find("types 7") != bq::string::npos) {
                        if (types_first.is_empty()) {
                            types_first = message_of(line);
                        } else if (types_second.is_empty()) {
                            types_second = message_of(line);
                        }
                    }
                }
                result.add_result(!types_first.is_empty() && types_first == types_second,
                    "fast_info: macro and fast_info agree on every argument type");
                bq::string level_first;
                bq::string level_second;
                for (const auto& line : lines) {
                    if (message_of(line).find("level warning 3") != bq::string::npos) {
                        const size_t lvl = line.find("\t[");
                        const bq::string tag = lvl == bq::string::npos ? bq::string() : line.substr(lvl, 6);
                        if (level_first.is_empty()) {
                            level_first = tag;
                        } else if (level_second.is_empty()) {
                            level_second = tag;
                        }
                    }
                }
                result.add_result(!level_first.is_empty() && level_first == level_second,
                    "fast_info: fast_warning carries the warning level tag");
            }

            // 1b. every literal encoding the interface accepts agrees with the macro path
            static void test_char_encodings(test_result& result)
            {
                const bq::string name = make_name("encoding");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::log log = bq::log::create_log(name, make_config(file_name));
                result.add_result(log.is_valid(), "fast_info: encoding log create");
                if (!log.is_valid()) {
                    return;
                }
                // Each encoding is written twice, once through the macro and once through fast_info.
                // U+4E2D U+6587 is spelled with \u escapes so this header stays ASCII: the literal's
                // own character type alone decides the encoding of each case.
                BQ_LOG_FAST_INFO(log, "encFmt_narrow {}", int32_t(1));
                log.fast_info<"encFmt_narrow {}">(int32_t(1));

                BQ_LOG_FAST_INFO(log, u8"encFmt_u8 \u4e2d\u6587 {}", int32_t(2));
                log.fast_info<u8"encFmt_u8 \u4e2d\u6587 {}">(int32_t(2));
                // a char8_t* is a UTF-8 string on both interfaces, not an address
                const char8_t* u8_arg = u8"\u4e2d\u6587-arg";
                BQ_LOG_FAST_INFO(log, "encFmtArg_u8 {}", u8_arg);
                log.fast_info<"encFmtArg_u8 {}">(u8_arg);

                BQ_LOG_FAST_INFO(log, u"encFmt_u16 \u4e2d\u6587 {}", int32_t(3));
                log.fast_info<u"encFmt_u16 \u4e2d\u6587 {}">(int32_t(3));

                BQ_LOG_FAST_INFO(log, U"encFmt_u32 \u4e2d\u6587 {}", int32_t(4));
                log.fast_info<U"encFmt_u32 \u4e2d\u6587 {}">(int32_t(4));

                // wchar_t is UTF-16 where it is 2 bytes wide (Windows) and UTF-32 where it is 4
                BQ_LOG_FAST_INFO(log, L"encFmt_wide \u4e2d\u6587 {}", int32_t(5));
                log.fast_info<L"encFmt_wide \u4e2d\u6587 {}">(int32_t(5));

                // a no-argument wide format takes the head-only branch of the hot path
                BQ_LOG_FAST_INFO(log, u"encFmtHead_u16 \u4e2d\u6587");
                log.fast_info<u"encFmtHead_u16 \u4e2d\u6587">();
                BQ_LOG_FAST_INFO(log, L"encFmtHead_wide \u4e2d\u6587");
                log.fast_info<L"encFmtHead_wide \u4e2d\u6587">();

                log.force_flush();
                const auto lines = split_lines(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0)));

                struct encoding_case {
                    const char* key;
                    bool expects_cjk; // the key has no prefix relationship with any other key
                };
                const encoding_case cases[] = {
                    { "encFmt_narrow", false },
                    { "encFmt_u8", true },
                    { "encFmt_u16", true },
                    { "encFmt_u32", true },
                    { "encFmt_wide", true },
                    { "encFmtHead_u16", true },
                    { "encFmtHead_wide", true },
                    { "encFmtArg_u8", true },
                };
                // the text appender always writes UTF-8, so the CJK part must come out as E4 B8 AD E6 96 87
                const char* const cjk_utf8 = "\xe4\xb8\xad\xe6\x96\x87";
                for (const auto& item : cases) {
                    bq::string from_macro;
                    bq::string from_fast_info;
                    for (const auto& line : lines) {
                        const bq::string msg = message_of(line);
                        if (msg.find(item.key) == bq::string::npos) {
                            continue;
                        }
                        if (from_macro.is_empty()) {
                            from_macro = msg;
                        } else if (from_fast_info.is_empty()) {
                            from_fast_info = msg;
                        }
                    }
                    result.add_result(!from_macro.is_empty() && from_macro == from_fast_info,
                        "fast_info: '%s' is identical through the macro and through fast_info", item.key);
                    if (item.expects_cjk) {
                        result.add_result(from_macro.find(cjk_utf8) != bq::string::npos,
                            "fast_info: '%s' kept its non-ascii characters", item.key);
                    }
                }
            }

            // 2. one call site, two logs: level and buffer follow the log passed in
            static void test_cross_log(test_result& result)
            {
                const bq::string name_a = make_name("cross_a");
                const bq::string name_b = make_name("cross_b");
                const bq::string file_a = bq::string("fast_mode_output/") + name_a;
                const bq::string file_b = bq::string("fast_mode_output/") + name_b;
                bq::log a = bq::log::create_log(name_a, make_config(file_a));
                bq::log b = bq::log::create_log(name_b, make_config(file_b));
                result.add_result(a.is_valid() && b.is_valid(), "fast_info: cross log create");
                if (!a.is_valid() || !b.is_valid()) {
                    return;
                }
                // one call site, alternating logs
                const auto write_both = [](const bq::log& la, const bq::log& lb, int32_t i) {
                    la.fast_info<"cross {}">(i);
                    lb.fast_info<"cross {}">(i);
                };
                for (int32_t i = 0; i < 1000; ++i) {
                    write_both(a, b, i);
                }
                a.force_flush();
                b.force_flush();
                const bq::string data_a = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_a + "_1.log", 0));
                const bq::string data_b = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_b + "_1.log", 0));
                uint32_t count_a = 0;
                uint32_t count_b = 0;
                for (int32_t i = 0; i < 1000; ++i) {
                    char needle[32];
                    snprintf(needle, sizeof(needle), "cross %d", i);
                    count_a += data_a.find(needle) != bq::string::npos ? 1U : 0U;
                    count_b += data_b.find(needle) != bq::string::npos ? 1U : 0U;
                }
                result.add_result(count_a == 1000 && count_b == 1000,
                    "fast_info: each log got only its own records (a=%u b=%u)", count_a, count_b);

                // disable info on b through reset_config: the shared site keeps writing to a only
                b.reset_config(make_config(file_b, "appenders_config.test.levels=[warning]\n"));
                for (int32_t i = 2000; i < 2100; ++i) {
                    write_both(a, b, i);
                }
                a.force_flush();
                b.force_flush();
                result.add_result(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_a + "_1.log", 0)).find("cross 2050") != bq::string::npos,
                    "fast_info: level change on one log keeps the other writing");
                result.add_result(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_b + "_1.log", 0)).find("cross 2050") == bq::string::npos,
                    "fast_info: info disabled on one log stops only that log");
            }

            // 3. site sharing: same (format, level, category, wire types) is one registry entry
            static void test_site_sharing(test_result& result)
            {
                const bq::string name = make_name("share");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::log log = bq::log::create_log(name, make_config(file_name));
                result.add_result(log.is_valid(), "fast_info: share log create");
                if (!log.is_valid()) {
                    return;
                }
                const size_t before = bq::meta_registry::instance().test_entry_count();
                log.fast_info<"share alpha {}">(int32_t(1));
                log.fast_info<"share alpha {}">(int32_t(2)); // same site and wire type: deduped
                const size_t after_int = bq::meta_registry::instance().test_entry_count();
                result.add_result(after_int == before + 1,
                    "fast_info: same format and wire types share one entry (%zu -> %zu)", before, after_int);
                log.fast_info<"share alpha {}">(int64_t(3)); // different wire type: one more
                const size_t after_i64 = bq::meta_registry::instance().test_entry_count();
                result.add_result(after_i64 == after_int + 1,
                    "fast_info: a different wire type makes a second entry (%zu -> %zu)", after_int, after_i64);
                log.fast_info<"share beta {}">(int32_t(4)); // different format: one more
                const size_t after_other = bq::meta_registry::instance().test_entry_count();
                result.add_result(after_other == after_i64 + 1,
                    "fast_info: a different format makes a third entry (%zu -> %zu)", after_i64, after_other);
                // int32_t and int are the same wire type: no new entry
                log.fast_info<"share beta {}">(int(5));
                result.add_result(bq::meta_registry::instance().test_entry_count() == after_other,
                    "fast_info: int and int32_t share the same wire type");
            }

            // 4. a site whose registration the registry refuses: the calls take the ordinary path
            // and the registration is not attempted again
            static void test_capacity_refusal(test_result& result)
            {
                const bq::string name = make_name("capacity");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::log log = bq::log::create_log(name, make_config(file_name, "log.thread_mode=async\n"));
                result.add_result(log.is_valid(), "fast_info capacity: log create");
                if (!log.is_valid()) {
                    return;
                }
                auto& registry = bq::meta_registry::instance();
                const uint32_t saved_max_entries = registry.max_entries();
                // a full registry: the format this test registers is refused
                registry.test_set_max_entries(static_cast<uint32_t>(registry.test_entry_count()));
                const uint32_t registers_before = registry.test_register_count();
                log.fast_info<"fast_info capacity {}">(int32_t(11));
                const uint32_t registers_after_first = registry.test_register_count();
                result.add_result(registers_after_first == registers_before + 1,
                    "fast_info capacity: one registration attempt for the refused site");
                log.fast_info<"fast_info capacity {}">(int32_t(12));
                result.add_result(registry.test_register_count() == registers_after_first,
                    "fast_info capacity: a refused site does not register again");
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                result.add_result(data.find("fast_info capacity 11") != bq::string::npos
                        && data.find("fast_info capacity 12") != bq::string::npos,
                    "fast_info capacity: both calls are written through the ordinary path");
                // one shared fast_info site, threads released together: the threads that lose the site
                // lock must find the refusal and take the ordinary path, never write a record with a
                // meta address that was never registered
                std::atomic<int32_t> race_written { 0 };
                std::atomic<int32_t> race_go { 0 };
                std::vector<std::thread> race_threads;
                for (int32_t t = 0; t < 16; ++t) {
                    race_threads.emplace_back([&log, &race_written, &race_go, t]() {
                        while (race_go.load() == 0) {
                        }
                        for (int32_t i = 0; i < 100; ++i) {
                            if (log.fast_info<"fast_info capacity race {}">(t * 1000 + i)) {
                                race_written.fetch_add(1);
                            }
                        }
                    });
                }
                race_go.store(1);
                for (auto& race_thread : race_threads) {
                    race_thread.join();
                }
                result.add_result(race_written.load() == 16 * 100,
                    "fast_info capacity: every racing call wrote through the ordinary path");
                log.force_flush();
                const bq::string race_data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                result.add_result(race_data.find("fast_info capacity race 0") != bq::string::npos
                        && race_data.find("fast_info capacity race 15099") != bq::string::npos,
                    "fast_info capacity: the first and last racing records are in the log file");
                registry.test_set_max_entries(bq::max_value(saved_max_entries,
                    static_cast<uint32_t>(registry.test_entry_count())));
            }

            // 6. special logs: invalid, sync mode, stack trace level, masked category
            static void test_special_logs(test_result& result)
            {
                bq::log invalid = bq::log::get_log_by_name("fast_info_not_exist");
                result.add_result(!invalid.is_valid(), "fast_info: invalid log is not valid");
                result.add_result(!invalid.fast_info<"invalid {}">(int32_t(1)),
                    "fast_info: invalid log returns false without crashing");
                result.add_result(!invalid.fast_info<"invalid no args">(),
                    "fast_info: invalid log returns false for a no-argument call");

                const bq::string sync_name = make_name("sync");
                const bq::string sync_file = bq::string("fast_mode_output/") + sync_name;
                bq::log sync_log = bq::log::create_log(sync_name, make_config(sync_file, "log.thread_mode=sync\n"));
                result.add_result(sync_log.is_valid(), "fast_info: sync log create");
                if (sync_log.is_valid()) {
                    result.add_result(sync_log.fast_info<"sync mode {}">(int32_t(42)),
                        "fast_info: sync log fast_info returns true");
                    result.add_result(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(sync_file + "_1.log", 0)).find("sync mode 42") != bq::string::npos,
                        "fast_info: sync log output is correct");
                }

                // stack trace level: falls back to do_log, output still carries the message
                const bq::string stack_name = make_name("stack");
                const bq::string stack_file = bq::string("fast_mode_output/") + stack_name;
                bq::log stack_log = bq::log::create_log(stack_name, make_config(stack_file, "log.thread_mode=sync\nlog.print_stack_levels=[info]\n"));
                result.add_result(stack_log.is_valid(), "fast_info: stack log create");
                if (stack_log.is_valid()) {
                    stack_log.fast_info<"stack info {}">(int32_t(7));
                    const bq::string stack_data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(stack_file + "_1.log", 0));
                    result.add_result(stack_data.find("stack info 7") != bq::string::npos,
                        "fast_info: a stack trace level still logs the message");
                    result.add_result(stack_data.find("stack_info_test_frame") != bq::string::npos || stack_data.size() > 0,
                        "fast_info: a stack trace level produced output");
                }

                // category masked out: filtered; unmasked: written
                const bq::string mask_name = make_name("mask");
                const bq::string mask_file = bq::string("fast_mode_output/") + mask_name;
                bq::test_category_log mask_log = bq::test_category_log::create_log(mask_name,
                    make_config(mask_file, "log.categories_mask=[ModuleB]\n"));
                result.add_result(mask_log.is_valid(), "fast_info: masked category log create");
                if (mask_log.is_valid()) {
                    mask_log.fast_info<mask_log.cat.ModuleA.SystemA, "masked out {}">(int32_t(1));
                    mask_log.fast_info<mask_log.cat.ModuleB, "kept {}">(int32_t(2));
                    mask_log.force_flush();
                    const bq::string mask_data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(mask_file + "_1.log", 0));
                    result.add_result(mask_data.find("masked out") == bq::string::npos,
                        "fast_info: a masked category is filtered");
                    result.add_result(mask_data.find("kept 2") != bq::string::npos,
                        "fast_info: an unmasked category is written");
                }
            }

            // 7. generated category classes: the category name appears, and no category means index 0
            static void test_category(test_result& result)
            {
                const bq::string name = make_name("cat");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::test_category_log log = bq::test_category_log::create_log(name,
                    make_config(file_name, "log.categories_mask=[ModuleA.SystemA,ModuleB]\n"));
                result.add_result(log.is_valid(), "fast_info: category log create");
                if (!log.is_valid()) {
                    return;
                }
                log.fast_info<log.cat.ModuleA.SystemA, "cat system {}">(int32_t(1));
                log.fast_warning<log.cat.ModuleB, "cat module b">();
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                result.add_result(data.find("[ModuleA.SystemA]\tcat system 1") != bq::string::npos,
                    "fast_info: generated category name is correct");
                result.add_result(data.find("[ModuleB]\tcat module b") != bq::string::npos,
                    "fast_info: generated category without arguments is correct");
                // category 0 needs a log whose categories_mask does not exclude it
                const bq::string plain_name = make_name("cat_zero");
                const bq::string plain_file = bq::string("fast_mode_output/") + plain_name;
                bq::log plain = bq::log::create_log(plain_name, make_config(plain_file));
                result.add_result(plain.is_valid(), "fast_info: category zero log create");
                if (plain.is_valid()) {
                    plain.fast_info<"cat default">();
                    plain.force_flush();
                    result.add_result(bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(plain_file + "_1.log", 0)).find("cat default") != bq::string::npos,
                        "fast_info: no category writes to category 0");
                }
            }

            static constexpr int32_t concurrent_thread_count = 64;
            static constexpr int32_t concurrent_site_count = 16;

            // 8. concurrent first calls: no lost record, no duplicate registration
            static void test_concurrent_first_call(test_result& result)
            {
                const bq::string name = make_name("race");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                bq::log log = bq::log::create_log(name, make_config(file_name));
                result.add_result(log.is_valid(), "fast_info: race log create");
                if (!log.is_valid()) {
                    return;
                }
                const size_t before = bq::meta_registry::instance().test_entry_count();
                std::vector<std::thread> threads;
                for (int32_t t = 0; t < concurrent_thread_count; ++t) {
                    threads.emplace_back([&log, t]() {
                        for (int32_t s = 0; s < concurrent_site_count; ++s) {
                            hit_site(log, s, t * 100);
                        }
                    });
                }
                for (auto& thread : threads) {
                    thread.join();
                }
                log.force_flush();
                const size_t after = bq::meta_registry::instance().test_entry_count();
                result.add_result(after == before + static_cast<size_t>(concurrent_site_count),
                    "fast_info: concurrent first calls register each site once (%zu -> %zu, want +%d)",
                    before, after, concurrent_site_count);
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                uint32_t found = 0;
                for (int32_t s = 0; s < concurrent_site_count; ++s) {
                    char needle[48];
                    snprintf(needle, sizeof(needle), "race site %d value", s);
                    found += data.find(needle) != bq::string::npos ? 1U : 0U;
                }
                result.add_result(found == static_cast<uint32_t>(concurrent_site_count),
                    "fast_info: concurrent first calls lose no record (%u of %d)", found, concurrent_site_count);
            }

            static void test_live_meta_other_thread(test_result& result)
            {
                const bq::string name = make_name("live_meta");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                const bq::string config = bq::string("appenders_config.test.type=text_file\n")
                    + "appenders_config.test.levels=[all]\n"
                    + "appenders_config.test.file_name=" + file_name + "\n"
                    + "appenders_config.test.base_dir_type=0\n"
                    + "appenders_config.test.enable_rolling_log_file=false\n"
                    + "log.high_perform_mode_freq_threshold_per_second=1\n";
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "fast_info: live meta log create");
                if (!log.is_valid()) {
                    return;
                }
                constexpr int32_t site_count = 4;
                constexpr int32_t record_count = 200;
                // one record per site registers it; this thread is gone before any of the rest
                std::thread registrar([&log]() {
                    for (int32_t s = 0; s < site_count; ++s) {
                        hit_site(log, s, -1);
                    }
                });
                registrar.join();
                for (int32_t i = 0; i < record_count; ++i) {
                    hit_site(log, i % site_count, i);
                }
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                uint32_t missing = 0;
                for (int32_t i = 0; i < record_count; ++i) {
                    char needle[48];
                    snprintf(needle, sizeof(needle), "race site %d value %d", i % site_count, i);
                    missing += data.find(needle) == bq::string::npos ? 1U : 0U;
                }
                result.add_result(missing == 0,
                    "fast_info: live site keeps every record (%u missing)", missing);
            }

            // 9. crash recovery: records written by fast_info come back with their format
            static void test_recovery(test_result& result)
            {
                if (!recovery_available()) {
                    return;
                }
                const bq::string name = make_name("recovery");
                const bq::string file_name = bq::string("fast_mode_output/") + name;
                const bq::string config = make_config(file_name, "log.recovery=true\nlog.thread_mode=async\nlog.buffer_size=65536\nlog.buffer_policy_when_full=expand\n");
                constexpr int32_t record_count = 200;
                {
                    bq::log log = bq::log::create_log(name, config);
                    result.add_result(log.is_valid(), "fast_info: recovery log create");
                    if (!log.is_valid()) {
                        return;
                    }
                    for (int32_t i = 0; i < record_count; ++i) {
                        log.fast_info<"recover {}">(i);
                    }
                    // crash without flushing and without consuming
                    bq::log_manager::instance().test_crash_log(log.get_id());
                }
                if (auto* inst = bq::instance_storage::get()) {
                    inst->test_begin_new_run(); // simulate a relaunch
                }
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "fast_info: recovery log recreate");
                if (!log.is_valid()) {
                    return;
                }
                log.force_flush();
                const bq::string data = bq::file_manager::read_all_text(TO_ABSOLUTE_PATH(file_name + "_1.log", 0));
                uint32_t found = 0;
                for (int32_t i = 0; i < record_count; ++i) {
                    char needle[32];
                    snprintf(needle, sizeof(needle), "recover %d", i);
                    found += data.find(needle) != bq::string::npos ? 1U : 0U;
                }
                result.add_result(found == record_count,
                    "fast_info: all records recovered with their format (%u of %d)", found, record_count);
            }

            // A distinct literal site per index; the string is built by adjacent literal concatenation
            // so each switch arm instantiates its own site with its own format string.
#define FAST_INFO_RACE_SITE(N, LOG, VALUE) (LOG).fast_info<"race site " #N " value {}">(VALUE)

            static void hit_site(const bq::log& log, int32_t site, int32_t value)
            {
                switch (site) {
                case 0:
                    FAST_INFO_RACE_SITE(0, log, value);
                    break;
                case 1:
                    FAST_INFO_RACE_SITE(1, log, value);
                    break;
                case 2:
                    FAST_INFO_RACE_SITE(2, log, value);
                    break;
                case 3:
                    FAST_INFO_RACE_SITE(3, log, value);
                    break;
                case 4:
                    FAST_INFO_RACE_SITE(4, log, value);
                    break;
                case 5:
                    FAST_INFO_RACE_SITE(5, log, value);
                    break;
                case 6:
                    FAST_INFO_RACE_SITE(6, log, value);
                    break;
                case 7:
                    FAST_INFO_RACE_SITE(7, log, value);
                    break;
                case 8:
                    FAST_INFO_RACE_SITE(8, log, value);
                    break;
                case 9:
                    FAST_INFO_RACE_SITE(9, log, value);
                    break;
                case 10:
                    FAST_INFO_RACE_SITE(10, log, value);
                    break;
                case 11:
                    FAST_INFO_RACE_SITE(11, log, value);
                    break;
                case 12:
                    FAST_INFO_RACE_SITE(12, log, value);
                    break;
                case 13:
                    FAST_INFO_RACE_SITE(13, log, value);
                    break;
                case 14:
                    FAST_INFO_RACE_SITE(14, log, value);
                    break;
                default:
                    FAST_INFO_RACE_SITE(15, log, value);
                    break;
                }
            }
#undef FAST_INFO_RACE_SITE

        public:
            test_result test() override
            {
                test_result result;
                test_output_match(result);
                test_char_encodings(result);
                test_cross_log(result);
                test_site_sharing(result);
                test_capacity_refusal(result);
                test_special_logs(result);
                test_category(result);
                test_concurrent_first_call(result);
                test_live_meta_other_thread(result);
                test_recovery(result);
                return result;
            }
        };
    }
}
#endif
