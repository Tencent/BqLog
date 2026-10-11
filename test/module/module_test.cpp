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
// The C++20 module entry, exercised by a translation unit that only imports it:
//
//     import bq.log;
//
// Nothing here includes bq_log or bq_common -- that is the point. The file is
// added to the unit test binary whenever the toolchain can build modules, so
// every test run compiles the import entry and checks it against the very same
// library the include entry is tested with.
//
// It reports one summary line in the same shape as the rest of the suite. The
// test framework itself is not used, because it is reached through headers that
// this translation unit must not include; the pass/fail of this file is folded
// into the overall result by main.cpp.
import bq.log;

#include <stdint.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <iterator>

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
        bq::util::set_log_device_console_min_level(bq::log_level::verbose);
        bq::util::log_device_console(bq::log_level::error, "module entry check failed: %s\n", what);
        bq::util::set_log_device_console_min_level(bq::log_level::warning);
    }

    std::string read_file(const char* path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    size_t count_occurrences(const std::string& text, const std::string& needle)
    {
        size_t count = 0;
        for (size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
            ++count;
        }
        return count;
    }
}

// Called from main.cpp, which owns the aggregate result.
bool bq_module_entry_test()
{
    // bq::string comes from bq_common, so this also checks that importing bq.log
    // hands out the types the library is built on.
    bq::string sanity("abc");
    report("types from bq_common are exported", !sanity.is_empty() && sanity.find("b") != bq::string::npos);

    const char* file_name = "module_entry_test";
    char config[512];
    snprintf(config, sizeof(config),
        "appenders_config.test.type=text_file\n"
        "appenders_config.test.levels=[all]\n"
        "appenders_config.test.file_name=%s\n"
        "appenders_config.test.base_dir_type=0\n"
        "appenders_config.test.enable_rolling_log_file=false\n"
        "log.high_perform_mode_freq_threshold_per_second=1000000\n",
        file_name);

    bq::log log = bq::log::create_log(bq::string("module_entry"), bq::string(config));
    report("create_log through the module", log.is_valid());
    if (!log.is_valid()) {
        bq::util::set_log_device_console_min_level(bq::log_level::verbose);
        bq::util::log_device_console(bq::log_level::info, "test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
        bq::util::set_log_device_console_min_level(bq::log_level::warning);
        return false;
    }

    // The ordinary interface, and the same text through the macro-free fast one:
    // both must reach the same layout.
    log.info("module same message");
    log.fast_info<"module same message">();
    // A format with arguments, a member of another level, and the wider literals
    // of the format string (stored as UTF-8 and UTF-16 respectively).
    log.fast_info<"module fast {} {}">(1, 2);
    log.fast_warning<"module warned">();
    log.fast_info<u"module u16 \u4e2d\u6587 {}">(3);
    log.fast_info<L"module wide \u4e2d\u6587 {}">(4);
    log.force_flush();

    const std::string content = read_file("module_entry_test_1.log");
    report("the log file was written", !content.empty());
    report("ordinary interface", content.find("module same message") != std::string::npos);
    report("fast_info with arguments", content.find("module fast 1 2") != std::string::npos);
    report("fast_warning is a member of the module API", content.find("module warned") != std::string::npos);
    // \u4e2d\u6587 as UTF-8, the encoding the text appender writes
    report("u16 format string", content.find("module u16 \xe4\xb8\xad\xe6\x96\x87 3") != std::string::npos);
    report("wide format string", content.find("module wide \xe4\xb8\xad\xe6\x96\x87 4") != std::string::npos);
    // one record from log.info, one from fast_info, and nothing else
    report("one message, two interfaces", count_occurrences(content, "module same message") == 2);

    bq::util::set_log_device_console_min_level(bq::log_level::verbose);
    bq::util::log_device_console(bq::log_level::info, "test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
    bq::util::set_log_device_console_min_level(bq::log_level::warning);
    return checks_passed == checks_total;
}
