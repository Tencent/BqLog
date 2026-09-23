/* Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "native_src_bq_common_platform_platform_misc.h"
#include "native_src_bq_common_bq_common.h"
namespace bq {
    namespace platform {
        static bq::platform::spin_lock_zero_init lock_;

        bq::string get_base_dir(int32_t base_dir_type)
        {
            bq::platform::scoped_spin_lock lock(lock_);
            if (base_dir_type == 0) {
                return common_global_vars::get().base_dir_init_inst_.get_base_dir_0();
            } else if (base_dir_type == 1) {
                return common_global_vars::get().base_dir_init_inst_.get_base_dir_1();
            } else {
                bq::util::log_device_console(bq::log_level::warning, "[get_base_dir] unknown base dir type:%" PRId32, base_dir_type);
                return common_global_vars::get().base_dir_init_inst_.get_base_dir_1();
            }
        }

        void base_dir_initializer::set_base_dir_0(const bq::string& dir)
        {
            bq::platform::scoped_spin_lock lock(lock_);
            base_dir_0_ = dir;
#if !defined(NDEBUG)
            bq::util::log_device_console(log_level::info, "set base dir type 0: %s", dir.c_str());
#endif
        }
        void base_dir_initializer::set_base_dir_1(const bq::string& dir)
        {
            bq::platform::scoped_spin_lock lock(lock_);
            base_dir_1_ = dir;
#if !defined(NDEBUG)
            bq::util::log_device_console(log_level::info, "set base dir type 1: %s", dir.c_str());
#endif
        }

#if defined(BQ_UNIT_TEST)
        namespace test_inject {
            // All state is process-global. Tests are sequential; if a future test
            // wants to be parallel-safe across log objects, the path filter scopes
            // the fault to a known directory.
            static bq::platform::atomic<int32_t> s_fault_kind(static_cast<int32_t>(fault_kind::none));
            static bq::platform::spin_lock_zero_init s_filter_lock;
            static bq::string s_path_filter;
            static bq::platform::atomic<int32_t> s_normal_buffer_alloc_fail(0);

            void set_fault(fault_kind kind)
            {
                s_fault_kind.store(static_cast<int32_t>(kind), bq::platform::memory_order::seq_cst);
            }
            fault_kind get_fault()
            {
                return static_cast<fault_kind>(s_fault_kind.load(bq::platform::memory_order::acquire));
            }
            void set_path_filter(const char* substring)
            {
                bq::platform::scoped_spin_lock lk(s_filter_lock);
                s_path_filter = substring ? substring : "";
            }
            bool path_matches_filter(const char* path)
            {
                bq::platform::scoped_spin_lock lk(s_filter_lock);
                if (s_path_filter.is_empty()) {
                    return true;
                }
                if (!path) {
                    return false;
                }
                return bq::string(path).find(s_path_filter) != bq::string::npos;
            }
            void set_normal_buffer_alloc_fail(bool fail)
            {
                s_normal_buffer_alloc_fail.store(fail ? 1 : 0, bq::platform::memory_order::seq_cst);
            }
            bool get_normal_buffer_alloc_fail()
            {
                return s_normal_buffer_alloc_fail.load(bq::platform::memory_order::acquire) != 0;
            }
        }
#endif
    }
}
