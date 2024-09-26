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
#include <stdio.h>
#include <stdlib.h>
#include "bq_common/bq_common.h"
#include "test_base.h"

namespace bq {
    namespace test {
        static bq::platform::spin_lock lock_;
        test_result test_result::operator+(const test_result& rhs)
        {
            std::vector<std::string> total_infos = failed_infos;
            total_infos.insert(total_infos.end(), rhs.failed_infos.begin(), rhs.failed_infos.end());
            test_result total_result;
            total_result.success_count = success_count.load() + rhs.success_count.load();
            total_result.total_count = total_count.load() + rhs.total_count.load();
            total_result.failed_infos = total_infos;
            return total_result;
        }

        void test_result::add_result(bool success, const char* info, ...)
        {
            ++total_count;
            if (success) {
                ++success_count;
            } else {
                lock_.lock();
                if (failed_infos.size() < 128) {
                    va_list args;
                    va_start(args, info);
                    static char tmp[1024 * 16];
                    vsnprintf(tmp, sizeof(tmp), info, args);
                    va_end(args);
                    const char* error_info_p = tmp;
                    failed_infos.emplace_back(error_info_p);
                } else if (failed_infos.size() == 128) {
                    failed_infos.emplace_back("... Too many test case errors. A maximum of 128 can be displayed, and the rest are omitted. ....");
                }
                lock_.unlock();
            }
        }

        void test_result::output(const std::string& type_name)
        {
            auto level = (is_all_pass()) ? bq::log_level::info : bq::log_level::error;
            test_output_param(level, "test case %s result: %u/%u", type_name.c_str(), success_count.load(), total_count.load());
            for (std::vector<std::string>::size_type i = 0; i < failed_infos.size(); ++i) {
                test_output_param(level, "\t%s", failed_infos[i].c_str());
            }
            test_output(bq::log_level::info, " ");
        }

        static std::string test_output_str_static;
        static std::string test_output_str_dynamic;
        static bq::array<char> test_output_str_cache;
        void add_to_test_output_str(bool is_dynamic, bq::log_level level, const char* format, ...)
        {
            test_output_str_cache.set_capacity(1024);
            (void)level;
            va_list args;
            va_start(args, format);
            while ((bq::array<char>::size_type)vsnprintf(test_output_str_cache.begin().operator->(), test_output_str_cache.capacity(), format, args) >= test_output_str_cache.capacity()) {
                test_output_str_cache.set_capacity(2 * test_output_str_cache.capacity());
            }
            va_end(args);
            if (is_dynamic) {
                test_output_str_dynamic = test_output_str_cache.begin().operator->();
            } else {
                test_output_str_static += test_output_str_cache.begin().operator->();
                test_output_str_static += "\n";
            }
        }

        std::string get_test_output()
        {
            return test_output_str_static + "\n" + test_output_str_dynamic;
        }
    }
}
