#pragma once
/*
 * Copyright (C) 2024 Tencent.
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
#include "bq_common/bq_common.h"

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

#define test_output(LEVEL, FORMAT)                                          \
    {                                                                       \
        bq::test::add_to_test_output_str(false, LEVEL, FORMAT);             \
        bq::util::set_log_device_console_min_level(bq::log_level::verbose); \
        bq::util::log_device_console(LEVEL, FORMAT);                        \
        bq::util::set_log_device_console_min_level(bq::log_level::error);   \
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
        bq::util::set_log_device_console_min_level(bq::log_level::error);    \
    }

#define test_output_dynamic_param(LEVEL, FORMAT, ...)                       \
    {                                                                       \
        bq::test::add_to_test_output_str(true, LEVEL, FORMAT, __VA_ARGS__); \
        printf(FORMAT, __VA_ARGS__);                                        \
        fflush(stdout);                                                     \
    }

        extern std::string get_test_output();
    }
}

#define TEST_GROUP_BEGIN(group_name)        \
    bool __test_result_##group_name = true; \
    test_output_param(bq::log_level::debug, "-----------------------------Test %s Begin----------------------------------", #group_name);

#define TEST_GROUP(group_name, name_space, test_type_name)                             \
    {                                                                                  \
        name_space::test_type_name inst_##test_type_name;                              \
        bq::test::test_result result__##test_type_name = inst_##test_type_name.test(); \
        result__##test_type_name.output(#test_type_name);                              \
        __test_result_##group_name &= result__##test_type_name.is_all_pass();          \
    }

#define TEST_GROUP_END(group_name)                                                                                                                                                                                                                       \
    if (__test_result_##group_name) {                                                                                                                                                                                                                    \
        test_output_param(bq::log_level::info, "--------------------------------CONGRATULATION!!! ALL TEST CASES IS PASSED!--------------------------------\n----------------------------Test %s End------------------------------------", #group_name); \
    } else {                                                                                                                                                                                                                                             \
        test_output_param(bq::log_level::fatal, "----------------------------------TEST CASES IS FAILED!------------------------------\n-----------------------------Test %s End----------------------------------", #group_name);                       \
    }

#define TEST_GROUP_RESULT(group_name) __test_result_##group_name
