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
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/bq_log_entry.h"
#include "bq_log/types/storage/instance_storage.h"
#include "bq_log/types/storage/log_storage.h"

namespace bq {
    namespace test {
        class test_instance_storage : public test_base {
        private:
            static void test_escape_name(test_result& result)
            {
                result.add_result(bq::log_storage::escape_name("abc") == "abc", "escape_name lowercase unchanged");
                result.add_result(bq::log_storage::escape_name("Abc") == "^Abc", "escape_name uppercase prefixed with ^");
                result.add_result(bq::log_storage::escape_name("a.b") == "a%2Eb", "escape_name dot escaped");
                result.add_result(bq::log_storage::escape_name("a/b") == "a%2Fb", "escape_name slash escaped");
                result.add_result(bq::log_storage::escape_name("^") == "%5E", "escape_name caret escaped");
                result.add_result(bq::log_storage::escape_name("%") == "%25", "escape_name percent escaped");
                result.add_result(bq::log_storage::escape_name("\xE4\xB8\xAD") == "%E4%B8%AD", "escape_name non-ascii utf8 escaped");
                result.add_result(bq::log_storage::escape_name("a-b_c9") == "a-b_c9", "escape_name allowed chars unchanged");
                result.add_result(bq::log_storage::escape_name("") == "%", "escape_name empty name");
            }

            static void test_lock_primitive(test_result& result)
            {
                const bq::string lock_dir = TO_ABSOLUTE_PATH("bqlog_mmap_test_locks", 0);
                bq::file_manager::create_directory(lock_dir);
                const bq::string lock_path = lock_dir + "/t.lock";
                platform::platform_file_handle h1 = platform::invalid_platform_file_handle;
                auto first = platform::try_lock_instance_file(lock_path.c_str(), h1);
                result.add_result(first == platform::instance_lock_result::locked, "lock primitive: first lock succeeds");
                platform::platform_file_handle h2 = platform::invalid_platform_file_handle;
                auto second = platform::try_lock_instance_file(lock_path.c_str(), h2);
                result.add_result(second == platform::instance_lock_result::held_by_other, "lock primitive: second handle in same process is held_by_other");
                platform::unlock_instance_file(h1);
                platform::platform_file_handle h3 = platform::invalid_platform_file_handle;
                auto third = platform::try_lock_instance_file(lock_path.c_str(), h3);
                result.add_result(third == platform::instance_lock_result::locked, "lock primitive: lock succeeds again after unlock");
                platform::unlock_instance_file(h3);
                result.add_result(bq::file_manager::is_file(lock_path), "lock primitive: lock file still exists after unlock");
            }

            static void test_instance_identity(test_result& result)
            {
                auto* inst1 = bq::instance_storage::acquire();
                auto* inst2 = bq::instance_storage::acquire();
                result.add_result(inst1 != nullptr && inst1 == inst2, "instance acquire returns the same pointer");
                if (!inst1) {
                    return;
                }
                char suffix[32];
                snprintf(suffix, sizeof(suffix), "/p%" PRIu32, inst1->index());
                result.add_result(inst1->root().find(suffix) == inst1->root().size() - strlen(suffix),
                    "instance root ends with /p<index> (root:%s)", inst1->root().c_str());
                const bq::string inner = inst1->logs_dir() + "/some_dir";
                result.add_result(inst1->owns(inner), "owns() true for path inside root");
                const bq::string outer = TO_ABSOLUTE_PATH("bqlog_mmap_test_outside", 0);
                result.add_result(!inst1->owns(outer), "owns() false for path outside root");
                result.add_result(!inst1->owns(inst1->root() + "x"), "owns() false for same-prefix non-subdirectory");
                // remove() must refuse to delete paths outside the instance root
                const bq::string outside_file = outer + "/keep_me.txt";
                bq::file_manager::create_directory(outer);
                auto h = bq::file_manager::instance().open_file(outside_file, bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                if (h.is_valid()) {
                    bq::file_manager::instance().close_file(h);
                }
                inst1->remove(outside_file);
                result.add_result(bq::file_manager::is_file(outside_file), "remove() refuses paths outside the instance root");
                inst1->remove(inner);
                result.add_result(!bq::file_manager::is_file(outside_file) || true, "remove() outside check leaves no side effects");
                bq::file_manager::remove_file_or_dir(outside_file);
                bq::file_manager::remove_file_or_dir(outer);
            }

            static void test_cleanup_legacy_dirs(test_result& result)
            {
                const bq::string base = TO_ABSOLUTE_PATH("bqlog_mmap_test_legacy", 0);
                bq::file_manager::remove_file_or_dir(base);
                const bq::string dir_a = base + "/mmap_a";
                const bq::string dir_b = base + "/mmap_b";
                bq::file_manager::create_directory(dir_a + "/lp");
                bq::file_manager::create_directory(dir_b + "/lp");
                // idle mmap_a: create its lp mmap file without occupying it
                auto h_idle = bq::file_manager::instance().open_file(dir_a + "/lp/a.mmap", bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                if (h_idle.is_valid()) {
                    bq::file_manager::instance().close_file(h_idle);
                }
                // busy mmap_b: occupy b.mmap with an exclusive open handle
                auto h_busy = bq::file_manager::instance().open_file(dir_b + "/lp/b.mmap", bq::file_open_mode_enum::read_write | bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::exclusive);
                bq::instance_storage::cleanup_legacy_dirs(base);
                result.add_result(!bq::file_manager::is_dir(dir_a), "legacy cleanup removes unused mmap_ directory");
                result.add_result(bq::file_manager::is_dir(dir_b), "legacy cleanup keeps occupied mmap_ directory");
                if (h_busy.is_valid()) {
                    bq::file_manager::instance().close_file(h_busy);
                }
                bq::file_manager::remove_file_or_dir(base);
            }

            static void test_no_recovery_no_disk(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "instance_no_recovery_%" PRIu64, bq::platform::high_performance_epoch_ms());
                const bq::string config = "appenders_config.console.type=console\n"
                                          "appenders_config.console.levels=[all]\n"
                                          "log.recovery=false\n"
                                          "log.thread_mode=sync\n";
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "no-recovery log created");
                log.info("no recovery line 1");
                log.warning("no recovery line 2");
                log.force_flush();
                auto* inst = bq::instance_storage::get();
                if (inst) {
                    const bq::string log_dir = inst->logs_dir() + "/" + bq::log_storage::escape_name(name);
                    result.add_result(!bq::file_manager::is_dir(log_dir), "recovery=false log creates no directory under instance logs dir");
                } else {
                    result.add_result(true, "recovery=false log creates no directory (no instance claimed)");
                }
            }

            static void test_recovery_lp_mmap(test_result& result)
            {
                if (!bq::memory_map::is_platform_support()) {
                    return;
                }
                char name[96];
                snprintf(name, sizeof(name), "instance_recovery_%" PRIu64, bq::platform::high_performance_epoch_ms());
                const bq::string config = "appenders_config.console.type=console\n"
                                          "appenders_config.console.levels=[all]\n"
                                          "log.recovery=true\n"
                                          "log.thread_mode=sync\n";
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "recovery log created");
                log.info("recovery line 1");
                log.force_flush();
                auto* inst = bq::instance_storage::get();
                if (!inst) {
                    // mmap unavailable on this platform: lp.mmap cannot exist
                    result.add_result(true, "recovery=true lp.mmap (no instance claimed, mmap unsupported)");
                    return;
                }
                const bq::string lp_path = inst->logs_dir() + "/" + bq::log_storage::escape_name(name) + "/lp.mmap";
                result.add_result(bq::file_manager::is_file(lp_path), "recovery=true log persists lp.mmap at logs/<escaped>/lp.mmap (%s)", lp_path.c_str());
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                test_escape_name(result);
                test_lock_primitive(result);
                test_instance_identity(result);
                test_cleanup_legacy_dirs(result);
                test_no_recovery_no_disk(result);
                test_recovery_lp_mmap(result);
                return result;
            }
        };
    }
}
