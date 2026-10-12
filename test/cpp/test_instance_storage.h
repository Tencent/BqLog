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
            static bool instance_available()
            {
                return bq::instance_storage::get() != nullptr;
            }

            // a minimal valid record: its size is the payload, content is opaque to the buffer
            static void write_marker_record(log_buffer& buffer, uint64_t marker)
            {
                auto handle = buffer.alloc_write_chunk(sizeof(marker), bq::platform::high_performance_epoch_ms());
                if (handle.result != enum_buffer_result_code::success) {
                    buffer.commit_write_chunk(handle);
                    return;
                }
                memcpy(handle.data_addr, &marker, sizeof(marker));
                buffer.commit_write_chunk(handle);
            }

            static bool read_find_marker(log_buffer& buffer, uint64_t marker)
            {
                for (int32_t i = 0; i < 64; ++i) {
                    auto chunk = buffer.read_chunk();
                    if (chunk.result != enum_buffer_result_code::success) {
                        return false;
                    }
                    bq::scoped_log_buffer_handle<log_buffer> guard(buffer, chunk);
                    if (chunk.data_size == sizeof(marker) && memcmp(chunk.data_addr, &marker, sizeof(marker)) == 0) {
                        return true;
                    }
                }
                return false;
            }

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
                if (!platform::is_cross_process_file_lock_supported()) {
                    test_output(bq::log_level::info, "SKIP file lock tests: no cross-process file lock on this platform\n");
                    return;
                }
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
                if (!recovery_available()) {
                    skip_recovery_tests("instance identity");
                    return;
                }
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
                // a path that escapes the root lexically must also be refused
                const bq::string escape = inst1->root() + "/../../bqlog_mmap_test_escape.txt";
                auto h_escape = bq::file_manager::instance().open_file(TO_ABSOLUTE_PATH("bqlog_mmap_test_escape.txt", 0),
                    bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                if (h_escape.is_valid()) {
                    bq::file_manager::instance().close_file(h_escape);
                }
                inst1->remove(escape);
                result.add_result(bq::file_manager::is_file(TO_ABSOLUTE_PATH("bqlog_mmap_test_escape.txt", 0)),
                    "remove() refuses a path that escapes the root with /../");
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("bqlog_mmap_test_escape.txt", 0));
                // a real file inside the root is removed
                const bq::string inside_file = inst1->logs_dir() + "/remove_me.txt";
                auto h_in = bq::file_manager::instance().open_file(inside_file, bq::file_open_mode_enum::write | bq::file_open_mode_enum::auto_create);
                if (h_in.is_valid()) {
                    bq::file_manager::instance().close_file(h_in);
                }
                result.add_result(bq::file_manager::is_file(inside_file), "remove(): inner file created");
                inst1->remove(inside_file);
                result.add_result(!bq::file_manager::is_file(inside_file), "remove() deletes a path inside the instance root");
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

            // Asking for recovery where the platform cannot provide it must degrade with a warning
            // and still hand back a usable log; it must never take the process down.
            static void test_explicit_recovery_without_support(test_result& result)
            {
                char name[96];
                snprintf(name, sizeof(name), "instance_forced_recovery_%" PRIu64, bq::platform::high_performance_epoch_ms());
                const bq::string config = "appenders_config.console.type=console\n"
                                          "appenders_config.console.levels=[all]\n"
                                          "log.recovery=true\n"
                                          "log.thread_mode=sync\n";
                bq::log log = bq::log::create_log(name, config);
                result.add_result(log.is_valid(), "recovery=true still creates a usable log where unsupported");
                log.info("forced recovery line");
                log.force_flush();
            }

            static void test_recovery_lp_mmap(test_result& result)
            {
                if (!recovery_available()) {
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

            // T05: run version increments per run and never visits 0
            static void test_run_version_increment(test_result& result)
            {
                auto* inst = bq::instance_storage::get();
                if (!inst) {
                    result.add_result(true, "run version increment (no instance claimed, mmap unsupported)");
                    return;
                }
                inst->test_set_run(65534, 100);
                inst->test_begin_new_run();
                result.add_result(inst->run_version() == 65535, "run version 65534 -> 65535");
                result.add_result(inst->run_serial() == 101, "run serial 100 -> 101");
                inst->test_begin_new_run();
                result.add_result(inst->run_version() == 1, "run version skips 0: 65535 -> 1");
                result.add_result(inst->run_serial() == 102, "run serial 101 -> 102");
            }

            // T05: recovery keeps working when the 16-bit version crosses its wrap point.
            // Each round writes a normal record and an oversize record (which lands in an
            // OS file and carries its own version context), then dies unread; the next
            // round must recover both exactly once. Covers the oversize wraparound bug.
            static void test_run_version_wraparound_recovery(test_result& result)
            {
                if (!recovery_available() || !instance_available()) {
                    return;
                }
                auto* inst = bq::instance_storage::get();
                // force the version near the wrap point; serial stays monotonic so
                // nothing in this run's other recovery logs is judged stale
                inst->test_set_run(65530, inst->run_serial());
                char name[96];
                snprintf(name, sizeof(name), "instance_version_wrap_%" PRIu64, bq::platform::high_performance_epoch_ms());
                log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                constexpr uint64_t normal_marker_base = 0xABCD0000;
                constexpr uint64_t oversize_marker_base = 0xEF010000;
                constexpr uint32_t oversize_size = 128 * 1024;
                uint32_t recovered_runs = 0;
                for (uint32_t step = 0; step < 10; ++step) {
                    {
                        auto storage = bq::log_storage::create(config.log_name, true);
                        config.storage = storage.get();
                        log_buffer buffer(config);
                        if (step > 0) {
                            uint32_t normal_found = 0;
                            uint32_t oversize_found = 0;
                            while (true) {
                                auto chunk = buffer.read_chunk();
                                if (chunk.result != enum_buffer_result_code::success) {
                                    break;
                                }
                                bq::scoped_log_buffer_handle<log_buffer> guard(buffer, chunk);
                                if (chunk.data_size == sizeof(uint64_t)) {
                                    uint64_t marker;
                                    memcpy(&marker, chunk.data_addr, sizeof(marker));
                                    if (marker == normal_marker_base + step - 1) {
                                        ++normal_found;
                                    }
                                } else if (chunk.data_size == oversize_size) {
                                    uint64_t marker;
                                    memcpy(&marker, chunk.data_addr, sizeof(marker));
                                    if (marker == oversize_marker_base + step - 1) {
                                        ++oversize_found;
                                    }
                                }
                            }
                            if (normal_found == 1 && oversize_found == 1) {
                                ++recovered_runs;
                            } else {
                                result.add_result(false, "version wraparound run %u: normal_found:%u oversize_found:%u (expect 1/1)",
                                    step, normal_found, oversize_found);
                            }
                        }
                        write_marker_record(buffer, normal_marker_base + step);
                        bq::array<uint8_t> oversize;
                        oversize.fill_uninitialized(oversize_size);
                        memset(&oversize[0], 0, oversize_size);
                        const uint64_t oversize_marker = oversize_marker_base + step;
                        memcpy(&oversize[0], &oversize_marker, sizeof(oversize_marker));
                        auto handle = buffer.alloc_write_chunk(oversize_size, bq::platform::high_performance_epoch_ms());
                        if (handle.result == enum_buffer_result_code::success) {
                            memcpy(handle.data_addr, &oversize[0], oversize_size);
                        }
                        buffer.commit_write_chunk(handle);
                        result.add_result(handle.result == enum_buffer_result_code::success, "version wraparound run %u oversize write", step);
                    }
                    inst->test_begin_new_run();
                }
                result.add_result(recovered_runs == 9, "version wraparound: normal+oversize recovered in every crossed run, got:%" PRIu32 "/9", recovered_runs);
            }

            // T05: oversize (OS) records recovered across a version wrap. The OS
            // context always carries the version of the run that wrote it, so this
            // exercises recovery of OS files whose context version is numerically
            // larger than the current run (65535 -> 1).
            static void test_run_version_oversize_wrap_parent(test_result& result)
            {
                if (!recovery_available() || !instance_available()) {
                    return;
                }
                auto* inst = bq::instance_storage::get();
                char name[96];
                snprintf(name, sizeof(name), "instance_version_oswrap_%" PRIu64, bq::platform::high_performance_epoch_ms());
                log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                constexpr uint64_t parent_marker = 0x11AA0001;
                constexpr uint64_t oversize_marker = 0x22BB0002;
                constexpr uint32_t oversize_size = 128 * 1024;
                {
                    inst->test_set_run(65535, inst->run_serial());
                    auto storage = bq::log_storage::create(config.log_name, true);
                    config.storage = storage.get();
                    log_buffer buffer(config);
                    write_marker_record(buffer, parent_marker);
                }
                inst->test_begin_new_run(); // version wraps to 1
                {
                    auto storage = bq::log_storage::create(config.log_name, true);
                    config.storage = storage.get();
                    log_buffer buffer(config);
                    bq::array<uint8_t> oversize;
                    oversize.fill_uninitialized(oversize_size);
                    memset(&oversize[0], 0, oversize_size);
                    memcpy(&oversize[0], &oversize_marker, sizeof(oversize_marker));
                    auto handle = buffer.alloc_write_chunk(oversize_size, bq::platform::high_performance_epoch_ms());
                    result.add_result(handle.result == enum_buffer_result_code::success, "oversize wrap parent: oversize write");
                    if (handle.result == enum_buffer_result_code::success) {
                        memcpy(handle.data_addr, &oversize[0], oversize_size);
                    }
                    buffer.commit_write_chunk(handle);
                }
                inst->test_begin_new_run(); // version 2
                auto storage = bq::log_storage::create(config.log_name, true);
                config.storage = storage.get();
                log_buffer buffer(config);
                bool parent_found = false;
                bool oversize_found = false;
                while (true) {
                    auto chunk = buffer.read_chunk();
                    if (chunk.result != enum_buffer_result_code::success) {
                        break;
                    }
                    bq::scoped_log_buffer_handle<log_buffer> guard(buffer, chunk);
                    if (chunk.data_size == sizeof(uint64_t)) {
                        uint64_t marker;
                        memcpy(&marker, chunk.data_addr, sizeof(marker));
                        parent_found |= marker == parent_marker;
                    } else if (chunk.data_size == oversize_size) {
                        uint64_t marker;
                        memcpy(&marker, chunk.data_addr, sizeof(marker));
                        oversize_found |= marker == oversize_marker;
                    }
                }
                result.add_result(parent_found, "oversize wrap parent: parent record from run 65535 recovered");
                result.add_result(oversize_found, "oversize wrap parent: oversize record from run 1 recovered");
            }

            // T05: more than MAX_RECOVERY_VERSION_RANGE runs later, data is stale and discarded
            static void test_run_version_stale_discard(test_result& result)
            {
                if (!recovery_available() || !instance_available()) {
                    return;
                }
                auto* inst = bq::instance_storage::get();
                char name[96];
                snprintf(name, sizeof(name), "instance_version_stale_%" PRIu64, bq::platform::high_performance_epoch_ms());
                log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                {
                    auto storage = bq::log_storage::create(config.log_name, true);
                    config.storage = storage.get();
                    log_buffer buffer(config);
                    write_marker_record(buffer, 0xDEAD0001);
                }
                for (uint32_t i = 0; i < bq::MAX_RECOVERY_VERSION_RANGE + 1; ++i) {
                    inst->test_begin_new_run();
                }
                auto storage = bq::log_storage::create(config.log_name, true);
                config.storage = storage.get();
                log_buffer buffer(config);
                result.add_result(!read_find_marker(buffer, 0xDEAD0001),
                    "version out of window: stale recovery data discarded after %u runs",
                    static_cast<unsigned>(bq::MAX_RECOVERY_VERSION_RANGE + 1));
            }

            // T05: within MAX_RECOVERY_VERSION_RANGE runs, data is still recovered
            static void test_run_version_window_recovery(test_result& result)
            {
                if (!recovery_available() || !instance_available()) {
                    return;
                }
                auto* inst = bq::instance_storage::get();
                char name[96];
                snprintf(name, sizeof(name), "instance_version_window_%" PRIu64, bq::platform::high_performance_epoch_ms());
                log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                {
                    auto storage = bq::log_storage::create(config.log_name, true);
                    config.storage = storage.get();
                    log_buffer buffer(config);
                    write_marker_record(buffer, 0xFACE0002);
                }
                for (uint32_t i = 0; i < bq::MAX_RECOVERY_VERSION_RANGE - 1; ++i) {
                    inst->test_begin_new_run();
                }
                auto storage = bq::log_storage::create(config.log_name, true);
                config.storage = storage.get();
                log_buffer buffer(config);
                result.add_result(read_find_marker(buffer, 0xFACE0002),
                    "version in window: data recovered after %u runs",
                    static_cast<unsigned>(bq::MAX_RECOVERY_VERSION_RANGE - 1));
            }

            // T05: mmap data whose run serial is ahead of this run (e.g. copied directory)
            // must never be recovered
            static void test_run_version_serial_guard(test_result& result)
            {
                if (!recovery_available() || !instance_available()) {
                    return;
                }
                auto* inst = bq::instance_storage::get();
                char name[96];
                snprintf(name, sizeof(name), "instance_version_serial_%" PRIu64, bq::platform::high_performance_epoch_ms());
                log_buffer_config config;
                config.log_name = name;
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                {
                    auto storage = bq::log_storage::create(config.log_name, true);
                    config.storage = storage.get();
                    log_buffer buffer(config);
                    write_marker_record(buffer, 0xBEEF0003);
                }
                // pretend this run started far behind the run that wrote the data
                const uint64_t current_serial = inst->run_serial();
                inst->test_set_run(static_cast<uint16_t>(inst->run_version() + 1), current_serial > 100 ? current_serial - 100 : 0);
                auto storage = bq::log_storage::create(config.log_name, true);
                config.storage = storage.get();
                log_buffer buffer(config);
                result.add_result(!read_find_marker(buffer, 0xBEEF0003),
                    "run serial ahead of current: recovery data from a copied directory discarded");
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                const recovery_capability capability = bq::log::get_recovery_capability();
                result.add_result(capability == recovery_capability::ready
                        || capability == recovery_capability::no_memory_map
                        || capability == recovery_capability::no_instance_lock,
                    "recovery capability reports one of its three states");
                test_escape_name(result);
                test_lock_primitive(result);
                test_instance_identity(result);
                test_cleanup_legacy_dirs(result);
                test_no_recovery_no_disk(result);
                if (recovery_available()) {
                    test_recovery_lp_mmap(result);
                    test_run_version_increment(result);
                    test_run_version_wraparound_recovery(result);
                    test_run_version_oversize_wrap_parent(result);
                    test_run_version_stale_discard(result);
                    test_run_version_window_recovery(result);
                    test_run_version_serial_guard(result);
                } else {
                    skip_recovery_tests("instance recovery");
                    test_explicit_recovery_without_support(result);
                }
                return result;
            }
        };
    }
}
