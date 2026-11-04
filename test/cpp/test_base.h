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
#include <string>
#include <vector>
#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"

namespace bq {
    namespace test {
        struct test_result {
        private:
            bq::platform::atomic<uint32_t> success_count = 0;
            bq::platform::atomic<uint32_t> total_count = 0;
            std::vector<std::string> failed_infos;

        public:
            test_result operator+(const test_result& rhs);

            void add_result(bool success, const char* info, ...);

            void output(const std::string& type_name);

            bool is_all_pass() const { return success_count.load() == total_count.load(); }
        };

        class test_base {
        public:
            virtual test_result test() = 0;
        };

        extern void add_to_test_output_str(bool is_dynamic, bq::log_level level, const char* format, ...);

        // the case that is running, so a timeout or a core dump can name it
        inline const char*& current_test_case()
        {
            static const char* name = "<none>";
            return name;
        }

#define test_output(LEVEL, FORMAT)                                          \
    {                                                                       \
        bq::test::add_to_test_output_str(false, LEVEL, FORMAT);             \
        bq::util::set_log_device_console_min_level(bq::log_level::verbose); \
        bq::util::log_device_console(LEVEL, FORMAT);                        \
        bq::util::set_log_device_console_min_level(bq::log_level::warning); \
    }

#define test_output_dynamic(LEVEL, FORMAT)                     \
    {                                                          \
        bq::test::add_to_test_output_str(true, LEVEL, FORMAT); \
        printf(FORMAT);                                        \
        fflush(stdout);                                        \
    }

#define test_output_param(LEVEL, FORMAT, ...)                                \
    {                                                                        \
        bq::test::add_to_test_output_str(false, LEVEL, FORMAT, __VA_ARGS__); \
        bq::util::set_log_device_console_min_level(bq::log_level::verbose);  \
        bq::util::log_device_console(LEVEL, FORMAT, __VA_ARGS__);            \
        bq::util::set_log_device_console_min_level(bq::log_level::warning);  \
    }

#define test_output_dynamic_param(LEVEL, FORMAT, ...)                       \
    {                                                                       \
        bq::test::add_to_test_output_str(true, LEVEL, FORMAT, __VA_ARGS__); \
        printf(FORMAT, __VA_ARGS__);                                        \
        fflush(stdout);                                                     \
    }

        // Files that ship with the sources - the legacy format samples, the fuzzer regression input -
        // are addressed relative to the header that reads them: <repo>/test/cpp/<header> means
        // <repo>/test/data/<sub>. That holds wherever the binary runs from a checkout. A binary that
        // runs away from it, on a device or copied out of the build tree, is pointed at the directory
        // with BQ_TEST_DATA_DIR instead. The source-tree answer is returned even when it does not
        // exist, so a failure names the path that was expected rather than an empty string.
        inline bq::string test_data_dir(const char* header_file, const char* sub_dir)
        {
            // Two separators back from the file name: <...>/<test dir>/<header>.
            size_t prev_sep = 0;
            size_t last_sep = 0;
            for (size_t i = 0; header_file[i] != '\0'; ++i) {
                if (header_file[i] == '/' || header_file[i] == '\\') {
                    prev_sep = last_sep;
                    last_sep = i;
                }
            }
            char buffer[4096];
            snprintf(buffer, sizeof(buffer), "%.*s/data/%s", static_cast<int32_t>(prev_sep), header_file, sub_dir);
            bq::string from_source(buffer);
            if (bq::platform::is_dir(from_source.c_str())) {
                return from_source;
            }
            const char* injected = getenv("BQ_TEST_DATA_DIR");
            if (injected && injected[0] != '\0') {
                bq::string from_env(injected);
                from_env += "/";
                from_env += sub_dir;
                if (bq::platform::is_dir(from_env.c_str())) {
                    return from_env;
                }
            }
            return from_source;
        }

        // The recovery suites ask the library instead of testing the platform themselves.
        inline bool recovery_available()
        {
            return bq::log::get_recovery_capability() == bq::recovery_capability::ready;
        }

        inline void skip_recovery_tests(const char* subject)
        {
            test_output_param(bq::log_level::info, "SKIP %s tests: no platform recovery support\n", subject);
        }

        extern std::string get_test_output();
    }
}

#define TEST_GROUP_BEGIN(group_name)        \
    bool __test_result_##group_name = true; \
    test_output_param(bq::log_level::debug, "-----------------------------Test %s Begin----------------------------------", #group_name);

#define TEST_GROUP(group_name, name_space, test_type_name)                             \
    {                                                                                  \
        bq::test::current_test_case() = #test_type_name;                               \
        name_space::test_type_name inst_##test_type_name;                              \
        bq::test::test_result result__##test_type_name = inst_##test_type_name.test(); \
        result__##test_type_name.output(#test_type_name);                              \
        __test_result_##group_name &= result__##test_type_name.is_all_pass();          \
    }

// Runs the group only where the platform provides what it exercises; the degraded CI
// configuration reports why it was skipped instead of failing for an absent capability.
#define TEST_GROUP_IF(cond, group_name, name_space, test_type_name)                                     \
    if (cond) {                                                                                         \
        TEST_GROUP(group_name, name_space, test_type_name);                                             \
    } else {                                                                                            \
        test_output_param(bq::log_level::info, "SKIP %s: capability not available\n", #test_type_name); \
    }

#define TEST_GROUP_END(group_name)                                                                                                                                                                                                                              \
    if (__test_result_##group_name) {                                                                                                                                                                                                                           \
        test_output_param(bq::log_level::info, "--------------------------------ALL TEST CASES OF GROUP %s IS PASSED!--------------------------------\n----------------------------Test %s End------------------------------------", #group_name, #group_name); \
    } else {                                                                                                                                                                                                                                                    \
        test_output_param(bq::log_level::fatal, "----------------------------------TEST CASES OF GROUP %s IS FAILED!------------------------------\n-----------------------------Test %s End----------------------------------", #group_name, #group_name);     \
    }

#define TEST_GROUP_RESULT(group_name) __test_result_##group_name
