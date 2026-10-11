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
// Nothing here includes bq_log, bq_common or the C++ standard library: bq::string comes
// from the module, and GCC 15's module TS crashes on an STL header after an import.
#define _CRT_SECURE_NO_WARNINGS // the fopen/fread below read the log file on purpose
import bq.log;

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    // the whole log file, read with C stdio
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
    bq::string sanity("abc");
    report("bq::string comes from the module", !sanity.is_empty() && sanity.find("b") != bq::string::npos);

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

    // the file of a previous run would be appended to, and then the record counts below
    // would include records that are not ours
    remove("module_entry_test_1.log");
    remove("module_entry_test.log");

    bq::log log = bq::log::create_log(bq::string("module_entry"), bq::string(config));
    report("create_log through the module", log.is_valid());
    if (!log.is_valid()) {
        printf("test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
        fflush(stdout);
        return -1;
    }

    // The ordinary interface and the same text through the macro-free fast one: both
    // have to reach the same layout. Then a format with arguments, another level, and
    // the wider literals of the format string.
    log.info("module same message");
    log.fast_info<"module same message">();
    log.fast_info<"module fast {} {}">(1, 2);
    log.fast_warning<"module warned">();
    log.fast_info<u"module u16 \u4e2d\u6587 {}">(3);
    log.fast_info<L"module wide \u4e2d\u6587 {}">(4);
    log.force_flush();

    char content[4096];
    const bool file_read = read_file("module_entry_test_1.log", content, sizeof(content));
    report("the log file was written", file_read);
    report("ordinary interface", strstr(content, "module same message") != nullptr);
    report("fast_info with arguments", strstr(content, "module fast 1 2") != nullptr);
    report("fast_warning is available", strstr(content, "module warned") != nullptr);
    // \u4e2d\u6587 as UTF-8, the encoding the text appender writes
    report("u16 format string", strstr(content, "module u16 \xe4\xb8\xad\xe6\x96\x87 3") != nullptr);
    report("wide format string", strstr(content, "module wide \xe4\xb8\xad\xe6\x96\x87 4") != nullptr);
    // one record from log.info, one from fast_info, and nothing else
    report("one message, two interfaces", count_occurrences(content, "module same message") == 2);

    printf("test case %s result: %u/%u\n", "test_module", checks_passed, checks_total);
    fflush(stdout);
    return checks_passed == checks_total ? 0 : -1;
}
