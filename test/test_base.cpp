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
            static bq::array<char> tmp({ '\0' });
            if (success) {
                ++success_count;
            } else {
                bq::platform::scoped_spin_lock lock(lock_);
                if (failed_infos.size() < 128) {
                    va_list args;
                    if (info) {
                        while (true) {
                            va_start(args, info);
                            bool failed = ((bq::array<char>::size_type)vsnprintf(&tmp[0], tmp.size(), info, args) + 1) >= tmp.size();
                            va_end(args);
                            if (failed) {
                                tmp.fill_uninitialized(tmp.size());
                            } else {
                                break;
                            }
                        }
                    } else {
                        tmp[0] = '\0';
                    }
                    const char* error_info_p = tmp.begin();
                    failed_infos.emplace_back(error_info_p);
                } else if (failed_infos.size() == 128) {
                    failed_infos.emplace_back("... Too many test case errors. A maximum of 128 can be displayed, and the rest are omitted. ....");
                }
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
        static bq::array<char> test_output_str_cache({ '\0' });
        void add_to_test_output_str(bool is_dynamic, bq::log_level level, const char* format, ...)
        {
            bq::platform::scoped_spin_lock lock(lock_);
            (void)level;
            va_list args;
            if (format) {
                while (true) {
                    va_start(args, format);
                    bool failed = ((bq::array<char>::size_type)vsnprintf(&test_output_str_cache[0], test_output_str_cache.size(), format, args) + 1) >= test_output_str_cache.size();
                    va_end(args);
                    if (failed) {
                        test_output_str_cache.fill_uninitialized(test_output_str_cache.size());
                    } else {
                        break;
                    }
                }
            } else {
                test_output_str_cache[0] = '\0';
            }
            if (is_dynamic) {
                test_output_str_dynamic = test_output_str_cache.begin();
            } else {
                test_output_str_static += test_output_str_cache.begin();
                test_output_str_static += "\n";
            }
        }

        std::string get_test_output()
        {
            return test_output_str_static + "\n" + test_output_str_dynamic;
        }
    }
}
