/*
 * Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
// The module entry, exercised by a translation unit that only imports it. It is a binary
// of its own, because a translation unit may not import and include the same library, and
// the test runners start it right after BqLogUnitTest.
//
// It walks the public surface the API reference documents -- the static interface, the
// ordinary and the fast log levels, every supported argument type, the appender controls,
// the snapshot, the console callbacks and the binary log decoder -- so that everything the
// module is supposed to hand out is known to compile and to work through `import bq.log;`.
//
// The system headers and macros.h come before the imports: bq.log.cppm pre-includes the same
// system headers in its global module fragment, and GCC 16 reports them as a redefinition when
// it parses them again after the import. Nothing here includes bq_log or the C++ standard
// library: bq::string comes from the module, and GCC's module TS crashes on an STL header after
// an import. macros.h is the one project header it takes, for the calling convention of the
// console callback below.
#define _CRT_SECURE_NO_WARNINGS // the fopen/fread below read the log file on purpose

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bq_common/platform/macros.h"

import bq.log;
import bq.test_category_log;

namespace {
    uint32_t checks_total = 0;
    uint32_t checks_passed = 0;

    void report(const char* what, bool ok)
    {
        ++checks_total;
        if (ok) {
            ++checks_passed;
            return;
        }
        printf("module entry check failed: %s\n", what);
        fflush(stdout);
    }

    // ---- the console callback surface -------------------------------------------------
    uint32_t console_callback_hits = 0;
    uint64_t console_callback_log_id = 0;
    int32_t console_callback_category = 0;
    bq::log_level console_callback_level = bq::log_level::verbose;

    // The exported typedef carries BQ_STDCALL, so the definition has to match it.
    void BQ_STDCALL on_console(uint64_t log_id, int32_t category_idx, bq::log_level level, const char* content, int32_t length)
    {
        (void)content;
        (void)length;
        ++console_callback_hits;
        console_callback_log_id = log_id;
        console_callback_category = category_idx;
        console_callback_level = level;
    }

    // ---- C stdio helpers: the module does not export a file manager --------------------
    char file_content[64 * 1024];

    bool read_file(const char* path, char* buffer, size_t buffer_size)
    {
        FILE* file = fopen(path, "rb");
        if (file == nullptr) {
            return false;
        }
        const size_t read_size = fread(buffer, 1, buffer_size - 1, file);
        fclose(file);
        buffer[read_size] = '\0';
        return read_size > 0;
    }

    bool file_exists(const char* path)
    {
        FILE* file = fopen(path, "rb");
        if (file == nullptr) {
            return false;
        }
        fclose(file);
        return true;
    }

    size_t count_occurrences(const char* text, const char* needle)
    {
        size_t count = 0;
        const size_t needle_size = strlen(needle);
        for (const char* found = strstr(text, needle); found != nullptr; found = strstr(found + needle_size, needle)) {
            ++count;
        }
        return count;
    }
}

int main()
{
    // ---- types that come from the module -----------------------------------------------
    bq::string sanity("abc");
    report("bq::string comes from the module", !sanity.is_empty() && sanity.find("b") != bq::string::npos);
    report("bq::string::size", sanity.size() == 3U);
    report("bq::string::c_str", strcmp(sanity.c_str(), "abc") == 0);
    report("bq::string::begin_with and end_with", sanity.begin_with(bq::string("ab")) && sanity.end_with(bq::string("bc")));
    report("bq::string::trim", bq::string("  x  ").trim() == bq::string("x"));
    report("bq::string::operator+", (bq::string("a") + bq::string("b")) == bq::string("ab"));
    report("bq::string::substr", bq::string("abcd").substr(1, 2) == bq::string("bc"));

    bq::array<bq::string> parts = bq::string("a,b,c").split(bq::string(","));
    report("bq::string::split returns bq::array<bq::string>", parts.size() == 3U && !parts[0].is_empty());

    // ---- the global interface ----------------------------------------------------------
    const bq::string base_dir = bq::log::get_file_base_dir(0);
    report("get_file_base_dir returns a directory", !base_dir.is_empty());
    bq::log::reset_base_dir(0, base_dir);
    report("reset_base_dir leaves the base dir in place", !bq::log::get_file_base_dir(0).is_empty());

    const bq::recovery_capability capability = bq::log::get_recovery_capability();
    report("get_recovery_capability returns a documented value",
        capability == bq::recovery_capability::ready
            || capability == bq::recovery_capability::no_memory_map
            || capability == bq::recovery_capability::no_instance_lock);
    bq::log::enable_auto_crash_handle();
    report("enable_auto_crash_handle is callable", true);

    // ---- a log with one appender of every kind, plus a snapshot ------------------------
    const char* log_file = "module_api_test";
    char config[1400];
    snprintf(config, sizeof(config),
        "log.thread_mode=sync\n"
        "log.recovery=false\n"
        "log.print_stack_levels=[error,fatal]\n"
        "snapshot.buffer_size=8192\n"
        "snapshot.levels=[all]\n"
        "appenders_config.console.type=console\n"
        "appenders_config.console.levels=[all]\n"
        "appenders_config.text.type=text_file\n"
        "appenders_config.text.levels=[all]\n"
        "appenders_config.text.file_name=%s\n"
        "appenders_config.text.base_dir_type=0\n"
        "appenders_config.text.enable_rolling_log_file=false\n"
        "appenders_config.compressed.type=compressed_file\n"
        "appenders_config.compressed.levels=[all]\n"
        "appenders_config.compressed.file_name=%s_c\n"
        "appenders_config.compressed.base_dir_type=0\n"
        "appenders_config.compressed.enable_rolling_log_file=false\n"
        "appenders_config.compressed.always_create_new_file=true\n",
        log_file, log_file);

    // a previous run would leave files that the record counts below would then include
    remove("module_api_test_1.log");
    remove("module_api_test.log");
    remove("module_api_test_c_1.logcompr");
    remove("module_api_decoded.txt");

    bq::log::register_console_callback(&on_console);

    bq::log log = bq::log::create_log(bq::string("module_api"), bq::string(config));
    report("create_log through the module", log.is_valid());
    if (!log.is_valid()) {
        printf("test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
        fflush(stdout);
        return -1;
    }

    // ---- instance accessors ------------------------------------------------------------
    report("get_id is non-zero", log.get_id() != 0);
    report("get_name", log.get_name() == bq::string("module_api"));
    report("is_enabled_for", log.is_enabled_for(0, bq::log_level::info));
    report("is_stack_trace_enabled_for", log.is_stack_trace_enabled_for(bq::log_level::error));
    report("is_enabled_without_stack_trace_for", log.is_enabled_without_stack_trace_for(0, bq::log_level::info));

    const bq::log found = bq::log::get_log_by_name(bq::string("module_api"));
    report("get_log_by_name finds the log", found.is_valid() && found.get_id() == log.get_id());
    report("get_log_by_name reports a miss as invalid", !bq::log::get_log_by_name(bq::string("module_api_missing")).is_valid());

    const bq::log copied(log);
    report("copy constructor", copied.is_valid() && copied.get_id() == log.get_id());
    bq::log assigned = bq::log::get_log_by_name(bq::string("module_api"));
    assigned = log;
    report("copy assignment", assigned.is_valid() && assigned.get_id() == log.get_id());

    // ---- every level, with and without arguments ---------------------------------------
    report("verbose returns true", log.verbose("module level verbose"));
    report("debug returns true", log.debug("module level debug"));
    report("info returns true", log.info("module level info"));
    report("warning returns true", log.warning("module level warning"));
    report("error returns true", log.error("module level error"));
    report("fatal returns true", log.fatal("module level fatal"));
    log.verbose("module verbose arg {}", 1);
    log.debug("module debug arg {}", 2);
    log.info("module info arg {}", 3);
    log.warning("module warning arg {}", 4);
    log.error("module error arg {}", 5);
    log.fatal("module fatal arg {}", 6);

    // ---- the macro-free fast interface -------------------------------------------------
    log.fast_verbose<"module fast verbose">();
    log.fast_debug<"module fast debug">();
    log.fast_info<"module fast info">();
    log.fast_warning<"module fast warning">();
    log.fast_error<"module fast error">();
    log.fast_fatal<"module fast fatal">();
    log.fast_info<"module fast info arg {}">(7);
    log.fast_info<u"module fast u16 \u4e2d\u6587 {}">(8);
    log.fast_info<L"module fast wide \u4e2d\u6587 {}">(9);

    // ---- the documented argument types -------------------------------------------------
    int32_t i32 = 7;
    const char* text = "module text";
    log.info("module params scalars {} {} {} {} {} {} {} {}",
        true, 'z', static_cast<int8_t>(1), static_cast<uint8_t>(2), static_cast<int16_t>(3),
        static_cast<uint16_t>(4), i32, static_cast<uint32_t>(5));
    log.info("module params wide ints {} {} {} {}",
        static_cast<int64_t>(6), static_cast<uint64_t>(7), static_cast<int32_t>(-8), static_cast<uint32_t>(9));
    log.info("module params floats {} {} {}", 1.5f, 2.5, static_cast<float>(-0.25));
    log.info("module params strings {} {} {} {} {} {}",
        text, "literal", bq::string("bq"), u8"utf8", u"utf16", U"utf32");
    log.info("module params wide strings {} {} {} {}",
        L"wide", static_cast<char16_t>(0x4e2d), static_cast<char32_t>(0x6587), static_cast<wchar_t>(0x41));
    log.info("module params null and pointer {} {}", nullptr, static_cast<const void*>(&i32));

    // ---- the console callback ----------------------------------------------------------
    log.force_flush();
    report("register_console_callback delivers records", console_callback_hits > 0);
    report("console callback carries the log id", console_callback_log_id == log.get_id());
    report("console callback carries the category", console_callback_category == 0);
    report("console callback carries the level", console_callback_level == bq::log_level::info);
    bq::log::unregister_console_callback(&on_console);

    // ---- the buffered console path -----------------------------------------------------
    bq::log::set_console_buffer_enable(true);
    console_callback_hits = 0;
    log.info("module buffered console");
    log.force_flush();
    uint32_t fetched = 0;
    while (bq::log::fetch_and_remove_console_buffer(&on_console) && fetched < 1000U) {
        ++fetched;
    }
    report("fetch_and_remove_console_buffer drains the console buffer", fetched > 0);
    bq::log::set_console_buffer_enable(false);

    // ---- the free-standing console output ----------------------------------------------
    bq::log::console(bq::log_level::info, "module console from a c string");
    bq::log::console(bq::log_level::warning, bq::string("module console from a bq::string"));
    report("log::console is callable in both overloads", true);

    // ---- the snapshot ------------------------------------------------------------------
    // The buffer only holds the most recent records, so the marker below is the one to look
    // for: an older message may already have been overwritten.
    log.info("module snapshot marker");
    const bq::string snapshot = log.take_snapshot(bq::string("UTC+8"));
    report("take_snapshot returns recent records", snapshot.find(bq::string("module snapshot marker")) != bq::string::npos);

    // ---- the text appender really received everything ----------------------------------
    log.force_flush();
    const bool text_read = read_file("module_api_test_1.log", file_content, sizeof(file_content));
    report("the text appender wrote a file", text_read);
    if (text_read) {
        report("ordinary interface", strstr(file_content, "module level info") != nullptr);
        report("every level", strstr(file_content, "module level verbose") != nullptr && strstr(file_content, "module level debug") != nullptr && strstr(file_content, "module level warning") != nullptr && strstr(file_content, "module level error") != nullptr && strstr(file_content, "module level fatal") != nullptr);
        report("format arguments", strstr(file_content, "module info arg 3") != nullptr);
        report("one record per call", count_occurrences(file_content, "module level info") == 1U);
        report("fast_info with arguments", strstr(file_content, "module fast info arg 7") != nullptr);
        report("fast_warning is available", strstr(file_content, "module fast warning") != nullptr);
        report("u16 format string", strstr(file_content, "module fast u16 \xe4\xb8\xad\xe6\x96\x87 8") != nullptr);
        report("wide format string", strstr(file_content, "module fast wide \xe4\xb8\xad\xe6\x96\x87 9") != nullptr);
        // \u4e2d\u6587 as UTF-8, the encoding the text appender writes
        report("every argument type reached the file", strstr(file_content, "module params strings") != nullptr && strstr(file_content, "module text") != nullptr && strstr(file_content, "literal") != nullptr && strstr(file_content, "utf8") != nullptr && strstr(file_content, "utf16") != nullptr && strstr(file_content, "utf32") != nullptr);
        report("null and pointer arguments reached the file", strstr(file_content, "module params null and pointer null 0x") != nullptr);
    }

    // ---- appender control --------------------------------------------------------------
    log.set_appender_enable(bq::string("text"), false);
    log.info("module disabled marker");
    log.force_flush();
    bool disabled_ok = read_file("module_api_test_1.log", file_content, sizeof(file_content));
    report("a disabled appender writes nothing", disabled_ok && strstr(file_content, "module disabled marker") == nullptr);

    log.set_appender_enable(bq::string("text"), true);
    log.info("module enabled marker");
    log.force_flush();
    report("a re-enabled appender writes again",
        read_file("module_api_test_1.log", file_content, sizeof(file_content))
            && strstr(file_content, "module enabled marker") != nullptr);

    // ---- the generated category log, through its module entry ---------------------------
    // The category log generator writes test_category_log.cppm beside test_category_log.h; this
    // is the same class, imported instead of included.
    char cat_config[768];
    snprintf(cat_config, sizeof(cat_config),
        "log.thread_mode=sync\n"
        "log.recovery=false\n"
        "appenders_config.cat.type=text_file\n"
        "appenders_config.cat.levels=[all]\n"
        "appenders_config.cat.file_name=%s_cat\n"
        "appenders_config.cat.base_dir_type=0\n"
        "appenders_config.cat.enable_rolling_log_file=false\n",
        log_file);
    remove("module_api_test_cat_1.log");

    bq::test_category_log cat_log = bq::test_category_log::create_log(bq::string("module_api_cat"), bq::string(cat_config));
    report("the generated category log is valid", cat_log.is_valid());
    report("category_log::get_categories_count", cat_log.get_categories_count() == 5U);
    const bq::array<bq::string>& category_names = cat_log.get_categories_name_array();
    report("category_log::get_categories_name_array",
        category_names.size() == 5U && category_names[1] == bq::string("ModuleA"));

    report("info with a category returns true", cat_log.info(cat_log.cat.ModuleA.SystemA.ClassA, "module category info {}", 1));
    cat_log.info("module category default");
    cat_log.fast_info<cat_log.cat.ModuleB, "module category fast {}">(2);
    cat_log.force_flush();

    report("the category appender wrote a file",
        read_file("module_api_test_cat_1.log", file_content, sizeof(file_content))
            && strstr(file_content, "module category info 1") != nullptr
            && strstr(file_content, "module category default") != nullptr
            && strstr(file_content, "module category fast 2") != nullptr);
    report("the records carry their category",
        strstr(file_content, "[ModuleA.SystemA.ClassA]") != nullptr
            && strstr(file_content, "[ModuleB]") != nullptr);

    // ---- the binary log decoder --------------------------------------------------------
    const char* compressed_file = "module_api_test_c_1.logcompr";
    report("the compressed appender wrote a file", file_exists(compressed_file));
    const bq::string compressed_path(compressed_file);
    bq::tools::log_decoder decoder(compressed_path);
    bool found_marker = false;
    uint32_t decoded_entries = 0;
    while (decoder.decode() == bq::appender_decode_result::success && decoded_entries < 10000U) {
        ++decoded_entries;
        if (decoder.get_last_decoded_log_entry().find(bq::string("module level info")) != bq::string::npos) {
            found_marker = true;
            break;
        }
    }
    report("log_decoder decodes the compressed file", decoded_entries > 0);
    report("log_decoder returns the records that were written", found_marker);
    report("get_last_decode_result agrees with the last decode", decoder.get_last_decode_result() == bq::appender_decode_result::success);

    report("decode_file writes a text file",
        bq::tools::log_decoder::decode_file(bq::string(compressed_file), bq::string("module_api_decoded.txt"))
            && file_exists("module_api_decoded.txt"));

    const bq::string missing_path("module_api_no_such_file.logcompr");
    bq::tools::log_decoder missing_decoder(missing_path);
    report("log_decoder reports a missing file", missing_decoder.decode() != bq::appender_decode_result::success);

    // ---- runtime configuration change --------------------------------------------------
    const bool reset_ok = log.reset_config(bq::string(
        "log.thread_mode=sync\n"
        "log.recovery=false\n"
        "appenders_config.console.type=console\n"
        "appenders_config.console.levels=[all]\n"));
    report("reset_config returns true", reset_ok);
    log.info("module after reset_config");

    bq::log::force_flush_all_logs();
    report("force_flush_all_logs is callable", true);

    printf("test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
    fflush(stdout);
    return checks_passed == checks_total ? 0 : -1;
}
