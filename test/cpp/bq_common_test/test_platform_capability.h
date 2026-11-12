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
#pragma once
#include <string.h>
#include <time.h>
#include "test_base.h"
#include "bq_common/bq_common.h"

namespace bq {
    namespace test {
        class test_platform_capability : public test_base {
        private:
            static bool has(uint32_t caps, uint32_t bit)
            {
                return (caps & bit) != 0;
            }

            static void test_report(test_result& result)
            {
                const uint32_t caps = bq::log::get_platform_capabilities();
                namespace pc = bq::platform_capability;
                result.add_result(has(caps, pc::memory_map) == (BQ_HAS_MMAP != 0), "report: mmap");
                result.add_result(has(caps, pc::multi_process) == (BQ_HAS_MULTI_PROCESS != 0), "report: multi process");
                result.add_result(has(caps, pc::cross_process_file_lock) == (BQ_HAS_CROSS_PROCESS_FILE_LOCK != 0), "report: file lock");
                result.add_result(has(caps, pc::crash_handler) == (BQ_HAS_CRASH_HANDLER != 0), "report: crash handler");
                result.add_result(has(caps, pc::backtrace) == (BQ_HAS_BACKTRACE != 0), "report: backtrace");
                result.add_result(has(caps, pc::dirent) == (BQ_HAS_DIRENT != 0), "report: dirent");
                result.add_result(has(caps, pc::thread_name) == (BQ_HAS_THREAD_NAME != 0), "report: thread name");
                result.add_result(has(caps, pc::default_base_dir) == (BQ_HAS_DEFAULT_BASE_DIR != 0), "report: default base dir");
                result.add_result(has(caps, pc::user_counter) == (BQ_HAS_USER_COUNTER != 0), "report: user counter");
                result.add_result(bq::memory_map::is_platform_support() == has(caps, pc::memory_map), "report: mmap matches the runtime check");
            }

            static void test_recovery_rule(test_result& result)
            {
                const uint32_t caps = bq::log::get_platform_capabilities();
                const bool lock_or_alone = !has(caps, bq::platform_capability::multi_process) || bq::platform::is_cross_process_file_lock_supported();
                const bool expected = has(caps, bq::platform_capability::memory_map) && lock_or_alone;
                result.add_result((bq::log::get_recovery_capability() == bq::recovery_capability::ready) == expected,
                    "recovery needs mmap and either a lock or a single process");
            }

            static void test_mount_paths(test_result& result)
            {
#if defined(BQ_POSIX)
                result.add_result(bq::file_manager::is_absolute("sdmc:/logs"), "sdmc:/ is absolute");
                result.add_result(bq::file_manager::is_absolute("save:/a/b.log"), "save:/ is absolute");
                result.add_result(!bq::file_manager::is_absolute("logs/a:/b"), "a colon after the first slash is not a mount");
                result.add_result(!bq::file_manager::is_absolute("a:b"), "a colon without a slash is not a mount");
                result.add_result(TO_ABSOLUTE_PATH("save:/logs/a.log", 0) == "save:/logs/a.log", "a mount path is not joined to the base dir");
#else
                (void)result;
#endif
            }

            static void test_epoch_clock(test_result& result)
            {
                // A monotonic clock reads as hours or days since boot, a wall clock as decades.
                const uint64_t wall_ms = static_cast<uint64_t>(time(nullptr)) * 1000;
                const uint64_t epoch_ms = bq::platform::system_epoch_ms();
                const uint64_t diff = epoch_ms > wall_ms ? epoch_ms - wall_ms : wall_ms - epoch_ms;
                result.add_result(diff < 10 * 1000, "system_epoch_ms is the wall clock (diff %" PRIu64 " ms)", diff);
            }

            static void test_thread_name(test_result& result)
            {
#if !BQ_HAS_THREAD_NAME
                const bq::string name = bq::platform::thread::get_current_thread_name();
                result.add_result(name.begin_with("pthread_") && name.size() > strlen("pthread_"), "a handle based thread name (%s)", name.c_str());
#else
                (void)result;
#endif
            }

            static void test_remove_without_dirent(test_result& result)
            {
#if !BQ_HAS_DIRENT
                const bq::string dir = TO_ABSOLUTE_PATH("capability_no_dirent", 0);
                bq::file_manager::create_directory(dir + "/sub");
                result.add_result(!bq::file_manager::remove_file_or_dir(dir), "a non-empty directory is reported, not silently kept");
                result.add_result(bq::file_manager::is_dir(dir + "/sub"), "its content is still there");
                result.add_result(bq::file_manager::remove_file_or_dir(dir + "/sub"), "an empty directory is removed");
                result.add_result(bq::file_manager::remove_file_or_dir(dir), "and then its parent");
#else
                (void)result;
#endif
            }

            static void test_max_file_size_cap(test_result& result)
            {
                const int64_t no_limit = 0;
                result.add_result(bq::platform::cap_max_file_size(0, no_limit) == 0, "an unbounded file stays unbounded where the platform has no limit");
                result.add_result(bq::platform::cap_max_file_size(1234, no_limit) == 1234, "a configured size is kept where the platform has no limit");

                const int64_t limit = static_cast<int64_t>(100);
                result.add_result(bq::platform::cap_max_file_size(0, limit) == limit, "an unbounded file is capped where the platform is bounded");
                result.add_result(bq::platform::cap_max_file_size(50, limit) == 50, "a size below the platform limit is kept");
                result.add_result(bq::platform::cap_max_file_size(200, limit) == limit, "a size above the platform limit is lowered to it");
                result.add_result(bq::platform::cap_max_file_size(limit, limit) == limit, "capping is idempotent");

                // A limit exists exactly where a file position does not fit in off_t, which is the
                // whole reason an appender has to stop one file short of that size.
#if defined(BQ_POSIX)
                const bool has_64_bit_positions = sizeof(off_t) >= sizeof(int64_t);
#else
                const bool has_64_bit_positions = true;
#endif
                const int64_t reported = bq::platform::get_max_supported_file_size();
                result.add_result((reported == 0) == has_64_bit_positions,
                    "the file size limit follows off_t: %" PRId64, reported);
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                test_report(result);
                test_recovery_rule(result);
                test_mount_paths(result);
                test_epoch_clock(result);
                test_thread_name(result);
                test_remove_without_dirent(result);
                test_max_file_size_cap(result);
                return result;
            }
        };
    }
}
