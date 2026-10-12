#pragma once
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

/*
 * P0 disk-full / OOM regression tests.
 *
 * These tests exercise the three "must not hang / crash / corrupt state" code
 * paths that the disk-full P0 fix targets:
 *   Case A: oversize alloc on a disk-full mmap path used to translate ENOSPC
 *           into err_wait_and_retry, which made the producer loop in
 *           __api_log_write_begin spin forever.
 *   Case B: when the oversize_buffer's mmap AND heap fallback both fail
 *           (simulated via test_inject::set_normal_buffer_alloc_fail), the old
 *           code asserted/segv'd in wt_alloc_oversize_write_chunk.
 *   Case C: appender_file_base::open_new_indexed_file_by_name used to loop
 *           on ENOSPC, incrementing max_index forever and stalling the worker
 *           thread.
 *   Case D: regression test for block_when_full mode - normal wait_and_retry
 *           must still spin until consumer frees space.
 *   Case E: end-to-end multi-thread fuzz: producers must not hang nor crash
 *           while the disk is intermittently full.
 *   Case F: text appender three-phase sequential semantics test:
 *           write N IDs in clean state, flip ENOSPC on writes, write N IDs
 *           that must be dropped, lift fault, write N more. After recovery
 *           the output file must contain phase-1 IDs and phase-3 IDs and
 *           ONLY the phase-2 IDs may be missing - file structure must remain
 *           parseable.
 *   Case G: encrypted compressed appender three-phase test - same pattern as
 *           F but exercises the encryption / segment / hash-cache paths that
 *           are most at risk (xor_key_blob_ pollution, half-written segment
 *           headers, format-template hash desync). After recovery the
 *           log_decoder must still successfully decode phase-1 and phase-3
 *           entries.
 *
 * All tests use bq::platform::test_inject (compiled in only when BQ_UNIT_TEST
 * is defined) so they run identically on every supported platform without
 * needing a real full disk.
 */

#include <thread>
#include <atomic>
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/types/buffer/log_buffer.h"
#include "bq_log/types/storage/instance_storage.h"
#include "bq_log/types/storage/log_storage.h"
#include "bq_log/bq_log_entry.h"

namespace bq {
    namespace test {

        // Wait at most `max_ms` for `predicate` to become true.
        // Returns true if the predicate fired in time, false on timeout.
        // Polls every 5ms.
        template <typename Predicate>
        static bool wait_for(uint64_t max_ms, Predicate&& predicate)
        {
            const uint64_t start = bq::platform::high_performance_epoch_ms();
            while (!predicate()) {
                if (bq::platform::high_performance_epoch_ms() - start >= max_ms) {
                    return false;
                }
                bq::platform::thread::sleep(5);
            }
            return true;
        }

        // RAII guard that resets the fault-injection state at the end of a test
        // case, even if an early `add_result` failure breaks out of the flow.
        struct scoped_fault_injection {
            scoped_fault_injection()
            {
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);
                bq::platform::test_inject::set_normal_buffer_alloc_fail(false);
            }
            ~scoped_fault_injection()
            {
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);
                bq::platform::test_inject::set_normal_buffer_alloc_fail(false);
            }
        };

        // Drain so ASan / LSan does not report leaks at process exit.
        //
        // log_buffer's per-producer-thread bookkeeping (log_tls_buffer_info and
        // its destruction_mark) is freed only by the consumer side, inside
        // log_buffer::deregister_seq when it sees the producer's
        // is_thread_finished_ marker. The full bq::log pipeline has a worker
        // thread doing this. Cases A / B / D here run a bare log_buffer with a
        // single producer and no consumer, so we drain manually before the
        // buffer dies.
        static void drain_buffer_until_empty(bq::log_buffer& buf)
        {
            while (true) {
                auto handle = buf.read_chunk();
                bool empty = (handle.result == bq::enum_buffer_result_code::err_empty_log_buffer);
                buf.return_read_chunk(handle);
                if (empty) {
                    break;
                }
            }
        }

        class test_disk_full : public test_base {
        private:
            static void wipe_log_state(const char* log_name)
            {
                auto* inst = bq::instance_storage::get();
                if (inst) {
                    inst->remove(inst->logs_dir() + "/" + bq::log_storage::escape_name(log_name));
                }
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("disk_full_test", 0));
            }

            // Case A: oversize alloc path must NOT hang under ENOSPC.
            //
            // We allocate a chunk bigger than the configured hp/lp buffer so
            // log_buffer routes us into wt_alloc_oversize_write_chunk. When we
            // inject ENOSPC on every open() call against bqlog_mmap files, the
            // oversize mmap creation fails. Pre-fix this returned err_wait_and_retry
            // and the thread spun forever; post-fix it returns err_not_enough_space.
            void case_a_oversize_no_hang(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case A: oversize alloc must not hang under ENOSPC...\n");
                wipe_log_state("disk_full_case_a");
                scoped_fault_injection fault_guard;

                log_buffer_config config;
                config.log_name = "disk_full_case_a";
                config.log_categories_name = { "_default" };
                config.need_recovery = true; // recovery=true forces oversize mmap creation
                config.policy = log_memory_policy::auto_expand_when_full;
                config.high_frequency_threshold_per_second = 1000;
                config.default_buffer_size = 4 * 1024;

                bq::log_buffer buf(config);

                // Inject ENOSPC against all bqlog_mmap files (covers oversize mmap).
                // The forced open() failure makes the library (correctly) log
                // "open_or_create_file failed" / "use memory instead of mmap file"
                // at error/warning level - that is the exact path under test, so we
                // silence the console for the injection window and restore the test
                // default (warning) right after, to keep the output clean.
                bq::util::set_log_device_console_min_level(bq::log_level::fatal);
                bq::platform::test_inject::set_path_filter("bqlog_mmap");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_open);

                std::atomic<bool> alloc_done(false);
                bq::enum_buffer_result_code observed_result = bq::enum_buffer_result_code::success;

                std::thread producer([&buf, &alloc_done, &observed_result]() {
                    // 256KB - bigger than the 4KB buffer, forces oversize.
                    auto handle = buf.alloc_write_chunk(256 * 1024, bq::platform::high_performance_epoch_ms());
                    observed_result = handle.result;
                    buf.commit_write_chunk(handle);
                    alloc_done.store(true, std::memory_order_release);
                });

                // Hang detector: 5s should be more than enough for any sane
                // alloc-or-fail. Pre-fix this would never come back.
                bool finished = wait_for(5000, [&alloc_done] {
                    return alloc_done.load(std::memory_order_acquire);
                });
                // Producer is done logging by the time alloc_done flips, so it is
                // safe to restore console verbosity here.
                bq::util::set_log_device_console_min_level(bq::log_level::warning);
                result.add_result(finished, "Case A: oversize alloc returned within 5s (no hang)");

                if (!finished) {
                    // If we hung, we cannot safely join. Detach and let the test
                    // process exit; downstream cases will be skipped.
                    producer.detach();
                    return;
                }
                producer.join();
                // Producer-only: no consumer thread will free the per-thread
                // TLS state, so drain manually. See drain_buffer_until_empty.
                drain_buffer_until_empty(buf);
                // Under "disk-full BUT memory available", normal_buffer falls back
                // to heap on mmap failure - the alloc returns success and the log
                // entry just lives in RAM (no recovery to disk). That's the
                // graceful-degradation behavior we want. The bug we're really
                // catching here is the hang above; the result code is just
                // confirming we got a clean answer (success or not_enough_space)
                // and not some sentinel.
                bool clean_result = (observed_result == bq::enum_buffer_result_code::success
                    || observed_result == bq::enum_buffer_result_code::err_not_enough_space);
                result.add_result(clean_result,
                    "Case A: oversize alloc returned a clean result (got %" PRId32 ")", (int32_t)observed_result);
            }

            // Case B: when both mmap and heap fallback fail, oversize_buffer
            // ctor used to construct a siso_ring_buffer over a null pointer
            // (segfault) and wt_alloc_oversize_write_chunk would assert. Now
            // we must surface err_io_failure_drop cleanly - the new error code
            // that signals "unrecoverable IO failure, drop and don't wait".
            void case_b_oversize_oom_no_crash(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case B: oversize buffer creation failure must not crash...\n");
                wipe_log_state("disk_full_case_b");
                scoped_fault_injection fault_guard;

                log_buffer_config config;
                config.log_name = "disk_full_case_b";
                config.log_categories_name = { "_default" };
                config.need_recovery = true;
                config.policy = log_memory_policy::auto_expand_when_full;
                config.high_frequency_threshold_per_second = 1000;
                config.default_buffer_size = 4 * 1024;

                bq::log_buffer buf(config);

                // After log_buffer is constructed (so its own internal mmaps got
                // built normally), force the NEXT normal_buffer to fail on heap
                // fallback. Combined with ENOSPC on open, this simulates a
                // "disk full + OOM" fault during oversize_buffer creation.
                // Same expected-failure logging as Case A (here mmap open AND heap
                // fallback both fail by design); silence the console for the
                // injection window and restore the default afterwards.
                bq::util::set_log_device_console_min_level(bq::log_level::fatal);
                bq::platform::test_inject::set_path_filter("bqlog_mmap");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_open);
                bq::platform::test_inject::set_normal_buffer_alloc_fail(true);

                std::atomic<bool> alloc_done(false);
                bq::enum_buffer_result_code observed_result = bq::enum_buffer_result_code::success;

                std::thread producer([&buf, &alloc_done, &observed_result]() {
                    auto handle = buf.alloc_write_chunk(256 * 1024, bq::platform::high_performance_epoch_ms());
                    observed_result = handle.result;
                    buf.commit_write_chunk(handle);
                    alloc_done.store(true, std::memory_order_release);
                });

                bool finished = wait_for(5000, [&alloc_done] {
                    return alloc_done.load(std::memory_order_acquire);
                });
                bq::util::set_log_device_console_min_level(bq::log_level::warning);
                result.add_result(finished, "Case B: oversize alloc on OOM returned within 5s (no hang)");
                if (!finished) {
                    producer.detach();
                    return;
                }
                producer.join();
                // Producer-only: drain manually, see drain_buffer_until_empty.
                drain_buffer_until_empty(buf);
                result.add_result(observed_result == bq::enum_buffer_result_code::err_io_failure_drop,
                    "Case B: oversize alloc on OOM returned err_io_failure_drop (got %" PRId32 ")", (int32_t)observed_result);
            }

            // Case C: appender open() under ENOSPC must not loop max_index forever.
            // We exercise the file appender directly via a real bq::log instance.
            void case_c_appender_no_index_drift(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case C: appender must not drift max_index under ENOSPC...\n");
                wipe_log_state("disk_full_case_c_log");
                scoped_fault_injection fault_guard;

                const bq::string out_dir = TO_ABSOLUTE_PATH("disk_full_test", 0);
                const bq::string log_config = R"(
                    appenders_config.file_appender.type=text_file
                    appenders_config.file_appender.levels=[all]
                    appenders_config.file_appender.file_name=disk_full_test/case_c
                    appenders_config.file_appender.base_dir_type=0
                    appenders_config.file_appender.enable_rolling_log_file=false
                    log.thread_mode=sync
                    log.buffer_size=8192
                )";

                bq::log my_log = bq::log::create_log("disk_full_case_c_log", log_config);
                result.add_result(my_log.is_valid(), "Case C: log object created");

                // Write one log so the appender opens its first file (...case_c_1.log).
                my_log.info("first line baseline");
                my_log.force_flush();

                size_t files_before = 0;
                {
                    bq::array<bq::string> names = bq::file_manager::get_sub_dirs_and_files_name(out_dir);
                    for (const auto& n : names) {
                        if (n.end_with(".log")) {
                            ++files_before;
                        }
                    }
                }
                result.add_result(files_before == 1, "Case C: baseline created exactly one log file (got %zu)", files_before);

                // Now: kick the appender into "needs new file" by removing the
                // current file from underneath it (this mimics an external
                // rotation / oversize trigger). We can't directly clear file_,
                // so instead we'll inject ENOSPC and fire many log calls; the
                // appender's first log call after init has already opened a
                // file. To force a re-open we'd normally need oversize. Instead,
                // let's just fire a flush+write loop while ENOSPC is on - the
                // existing file should keep absorbing writes, but we'll also
                // confirm no NEW indexed files appear.

                // Inject ENOSPC for any new file open under disk_full_test.
                bq::platform::test_inject::set_path_filter("disk_full_test");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_open);

                // Force the appender to attempt a new file: simplest cross-platform
                // way is to set max_file_size=tiny. But we configured no max_file_size,
                // and we can't change config mid-run. Instead, simulate by writing
                // enough data that flush_write_cache runs successfully (no ENOSPC
                // on writes themselves), but also remove the current file out from
                // under us so the next refresh_file_handle triggers re-open.
                //
                // Since we can't easily evict file_ from outside, the simpler and
                // more direct test is on the log_imp + appender via reset_config
                // with a different file_name (forces a new appender, hence new open).
                std::atomic<bool> reset_done(false);
                std::thread reset_thread([&my_log, &reset_done]() {
                    my_log.reset_config(R"(
                        appenders_config.file_appender_v2.type=text_file
                        appenders_config.file_appender_v2.levels=[all]
                        appenders_config.file_appender_v2.file_name=disk_full_test/case_c_v2
                        appenders_config.file_appender_v2.base_dir_type=0
                        appenders_config.file_appender_v2.enable_rolling_log_file=false
                        log.thread_mode=sync
                        log.buffer_size=8192
                    )");
                    reset_done.store(true, std::memory_order_release);
                });
                bool reset_finished = wait_for(5000, [&reset_done] {
                    return reset_done.load(std::memory_order_acquire);
                });
                result.add_result(reset_finished, "Case C: reset_config returned within 5s (no hang in open loop)");
                if (!reset_finished) {
                    reset_thread.detach();
                    return;
                }
                reset_thread.join();

                // While ENOSPC is on, the appender_v2 should NOT have created
                // case_c_v2_*.log files. Pre-fix it would have created files
                // 1..N as max_index drifted (or rather, would have looped trying;
                // since open returns ENOSPC, no file actually appears - the
                // observable smoking gun pre-fix was the hang, not stray files.
                // The strongest portable assertion is "the call returned" plus
                // "we didn't crash". Confirm files state below.

                bq::array<bq::string> names_during = bq::file_manager::get_sub_dirs_and_files_name(out_dir);
                size_t v2_files_during = 0;
                for (const auto& n : names_during) {
                    if (n.begin_with("case_c_v2") && n.end_with(".log")) {
                        ++v2_files_during;
                    }
                }
                result.add_result(v2_files_during == 0, "Case C: no v2 log files created while disk full (got %zu)", v2_files_during);

                // Lift the fault and write again - the new appender should now
                // succeed in opening case_c_v2_1.log.
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);

                my_log.info("post-recovery line");
                my_log.force_flush();

                // Allow the worker a tick to actually create the file.
                bool got_v2 = wait_for(2000, [&out_dir] {
                    bq::array<bq::string> ns = bq::file_manager::get_sub_dirs_and_files_name(out_dir);
                    for (const auto& n : ns) {
                        if (n.begin_with("case_c_v2") && n.end_with(".log")) {
                            return true;
                        }
                    }
                    return false;
                });
                result.add_result(got_v2, "Case C: appender recovered and created case_c_v2 file after disk recovery");

                // Cleanup
                my_log.reset_config(R"(
                    appenders_config.console_only.type=console
                    appenders_config.console_only.levels=[all]
                    log.thread_mode=sync
                )");
            }

            // Case D: regression - block_when_full producer must still spin
            // through wait_and_retry until the consumer drains space. This
            // protects the policy guard added in __api_log_write_begin from
            // accidentally breaking the legitimate block-mode behavior.
            void case_d_block_mode_still_works(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case D: block_when_full mode still works correctly...\n");
                wipe_log_state("disk_full_case_d");
                scoped_fault_injection fault_guard;

                // No fault injected. Just stand up a tiny block-mode log_buffer
                // and confirm a normal write finishes.
                log_buffer_config config;
                config.log_name = "disk_full_case_d";
                config.log_categories_name = { "_default" };
                config.need_recovery = false;
                config.policy = log_memory_policy::block_when_full;
                config.high_frequency_threshold_per_second = 1000;
                config.default_buffer_size = 4 * 1024;

                bq::log_buffer buf(config);

                std::atomic<bool> alloc_done(false);
                bq::enum_buffer_result_code observed_result = bq::enum_buffer_result_code::err_empty_log_buffer;
                std::thread producer([&buf, &alloc_done, &observed_result]() {
                    auto handle = buf.alloc_write_chunk(64, bq::platform::high_performance_epoch_ms());
                    observed_result = handle.result;
                    buf.commit_write_chunk(handle);
                    alloc_done.store(true, std::memory_order_release);
                });
                bool finished = wait_for(2000, [&alloc_done] {
                    return alloc_done.load(std::memory_order_acquire);
                });
                result.add_result(finished, "Case D: block-mode normal alloc returned within 2s");
                if (!finished) {
                    producer.detach();
                    return;
                }
                producer.join();
                // Producer-only: drain manually, see drain_buffer_until_empty.
                drain_buffer_until_empty(buf);
                result.add_result(observed_result == bq::enum_buffer_result_code::success,
                    "Case D: block-mode normal alloc returned success (got %" PRId32 ")", (int32_t)observed_result);
            }

            // Case E: end-to-end fuzz. 4 producer threads logging through a
            // real bq::log instance. Mid-flight, flip ENOSPC for ~200ms then
            // off. All producers must finish; no crash; no file drift.
            void case_e_end_to_end(test_result& result)
            {
                if (!recovery_available()) {
                    skip_recovery_tests("recovery disk-full stress");
                    return;
                }
                test_output_dynamic(bq::log_level::info, "[disk_full] Case E: end-to-end stress with intermittent ENOSPC...\n");
                wipe_log_state("disk_full_case_e_log");
                scoped_fault_injection fault_guard;

                const bq::string out_dir = TO_ABSOLUTE_PATH("disk_full_test", 0);
                const bq::string config_str = R"(
                    appenders_config.case_e_appender.type=text_file
                    appenders_config.case_e_appender.levels=[all]
                    appenders_config.case_e_appender.file_name=disk_full_test/case_e
                    appenders_config.case_e_appender.base_dir_type=0
                    appenders_config.case_e_appender.enable_rolling_log_file=false
                    log.thread_mode=async
                    log.recovery=true
                    log.buffer_size=65536
                    log.buffer_policy=expand
                )";

                bq::log my_log = bq::log::create_log("disk_full_case_e_log", config_str);
                result.add_result(my_log.is_valid(), "Case E: log object created");

                std::atomic<bool> stop(false);
                std::atomic<int32_t> messages_logged(0);

                std::vector<std::thread> producers;
                for (int32_t i = 0; i < 4; ++i) {
                    producers.emplace_back([&my_log, &stop, &messages_logged, i]() {
                        while (!stop.load(std::memory_order_acquire)) {
                            my_log.info("case_e producer {} message {}", i, messages_logged.load(std::memory_order_relaxed));
                            int32_t now = messages_logged.fetch_add(1, std::memory_order_relaxed) + 1;
                            // Throw in an oversize-class entry every so often.
                            if ((now & 0x1FF) == 0) {
                                bq::string big_payload;
                                big_payload.fill_uninitialized(70 * 1024);
                                memset(big_payload.begin(), 'X', big_payload.size());
                                my_log.info("case_e oversize: {}", big_payload);
                            }
                        }
                    });
                }

                // Run baseline for ~150ms.
                bq::platform::thread::sleep(150);

                // Inject ENOSPC on both open and write for 200ms.
                bq::platform::test_inject::set_path_filter(nullptr); // global
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_open);
                bq::platform::thread::sleep(100);
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_write);
                bq::platform::thread::sleep(100);

                // Lift faults; let producers run another 150ms in clean state.
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::thread::sleep(150);

                stop.store(true, std::memory_order_release);

                // Drain the worker pipeline synchronously before joining.
                my_log.force_flush();

                for (auto& t : producers) {
                    t.join();
                }
                result.add_result(messages_logged.load(std::memory_order_relaxed) > 0,
                    "Case E: producers logged at least one message (got %" PRId32 ")",
                    messages_logged.load(std::memory_order_relaxed));

                // Final flush so the appender state is clean before reset_config.
                bq::log::force_flush_all_logs();

                // Cleanup
                my_log.reset_config(R"(
                    appenders_config.console_only.type=console
                    appenders_config.console_only.levels=[all]
                    log.thread_mode=sync
                )");
            }

            // RSA keypair shared by case_g (encrypted compressed appender). Any
            // valid bqlog SSH-format keypair works; this is the same pair used
            // by test_log_3.cpp's encryption test, so we know decoder + encoder
            // round-trip through it cleanly.
            static const bq::string& g_pub_key()
            {
                static const bq::string k = bq::string(
                                                "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFc")
                    + "Nr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZE"
                    + "PwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+"
                    + "r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6";
                return k;
            }
            static const bq::string& g_priv_key()
            {
                static const bq::string k = bq::string("-----BEGIN RSA PRIVATE KEY-----\n")
                    + "MIIEpAIBAAKCAQEAsL90LQ1wf30DfhaJ8jh7ktrguiGXNem33eqkJOMpATrd503w\n"
                    + "RXDa/GVr73H+B9IRZK6TLUNACTtOexoVU0Agn5730rUqtmK2vjuVRqSOUOjS61Ll\n"
                    + "REYpozYMLcW17dz/DcPO4xCCYmP/JthVF4WRD8KAO7ftv/ed3DRNYrSbJCbaI8Gl\n"
                    + "JTDdHVwScbODT/DHwTz2UObzVfMYBH9UgkM0AQCbKH8tZDxx6iTQV1AbDQOOEj+R\n"
                    + "/eSTs/q9KGjZGMU/PGZOLW5OTd4ZOWl8mVaePTqoa1wj3+aMA2uC6pOFM1U5PRIp\n"
                    + "sb7UrolE86+yCYR2Mu47Bp0oKunR9MZVH8SnlwIDAQABAoIBACtaWpmuYTi0JkYo\n"
                    + "Kx/hoNXtoA+nq5pKwJHLOwXdPjKSCNnycQvnWZ9tFSN/V2r9qMyEUY9ZnnxlMqPZ\n"
                    + "Sv/Hi/j7Ghhx3Y8s+VwB62SPemT4JrwX8ipj91SULjqP80br3Re4PqfNZd3SX0Rc\n"
                    + "7co+Nc2izKdZPxTGHM9leNHMMP2VrVbZeSlQBnqlFVMqi2g9ukMGZG10vPdIJV7z\n"
                    + "5dqWaKuW/2F8dp/o6i/uUDWAH4fITLD1PLqx5/kP8ohOXup8wxYaY8jhKlvswuCh\n"
                    + "qlY773SamnrIGctQWe63Fe9q7hzs3vcCOfciFYsVX2qfHPvGORzd8DgMwe3TFbrM\n"
                    + "nmUijcECgYEA5zyarIPQfbEsmMtKgY7OSz+M1SaOE9r/I0Hl2dz7f0r7JocZ6KUj\n"
                    + "NinbX/zvJ7AMzDUefbptjf9F4vNa0eBZmSZDAiWm3byvMKS5uLboFNrKnYptIc3c\n"
                    + "0CzDzC+nMi4NrPoAZZrUyJw1Emr7gWVVG2FW2NatOVqfPq3XBbk9dncCgYEAw60H\n"
                    + "FqcvrSTAVSk9L+TB8Fn92p5YQtaV7CSZj9GQdvfs+pIkPlq+jYvSH7nYjK2Qlq+h\n"
                    + "sn+3YcVVczbGuhSLuq4bHPd46HOjya3rbAq39RxlpFRFjZ4hici4XIFKnB7Kylta\n"
                    + "Ph6nNq9m5tdFZ1SurgLlOaxg2AwLAia2V5L5/+ECgYAzmhmmP/Ap7HzYSB2DVfwB\n"
                    + "XNgvxN/V3HwtQQprGN5i5LexPFryyM9XyfVzsT0pbScd9wir5AuIsZvF7qqoxVkZ\n"
                    + "TSmM9BwNxYqO32O2rdKSvNSUXYzHC2qoZiT3jvbPwuk4Xb3y7p9neTx6tLcVhCh+\n"
                    + "6LT5xMZ5UxjQYvjmBRWLNwKBgQC5Ta9rzXHB5w7Y1w/hviHHKoHTOabdzPb3RQXD\n"
                    + "g7LqZwkdla4K+sZ/pwybDNU9C9TkTnizYG1agpTUYeg6KeDVLbHxcY4nm/Nct349\n"
                    + "t7zTu0uqHkArx7d9Ev88Yxgz1pk2nuJL951klSC+tNg97Zzqn0VSo6KmlmkKZXzC\n"
                    + "XCayIQKBgQC+hPlGE7T6WLKvsmaH3IJI/TAjbuSu25o+aar7ecPoeax6YnSgyF/8\n"
                    + "Xx7f0BFCYEzprftfqDBcfa6D8GKkcsqAMDeJRz8meD+55o3qdL6LKFkjCKvmRxuv\n"
                    + "OmO+42HqCD4mqxMU1rgcWOn+LLW3HSqbE5kYA9XDwEtxCnMiKqP7sA==\n"
                    + "-----END RSA PRIVATE KEY-----\n";
                return k;
            }

            // Find the most recently produced log file under `dir` whose name
            // begins with `prefix` and ends with `suffix`. Returns "" if none.
            static bq::string find_latest_log_file(const bq::string& dir, const bq::string& prefix, const bq::string& suffix)
            {
                bq::array<bq::string> names = bq::file_manager::get_sub_dirs_and_files_name(dir);
                bq::string best_path;
                uint64_t best_mtime = 0;
                for (const auto& n : names) {
                    if (!n.begin_with(prefix) || !n.end_with(suffix)) {
                        continue;
                    }
                    bq::string full = bq::file_manager::combine_path(dir, n);
                    if (!bq::file_manager::is_file(full)) {
                        continue;
                    }
                    uint64_t mtime = bq::file_manager::get_file_last_modified_epoch_ms(full);
                    if (best_path.is_empty() || mtime >= best_mtime) {
                        best_path = full;
                        best_mtime = mtime;
                    }
                }
                return best_path;
            }

            // Read a whole file into a string. Returns "" on any error / missing file.
            static bq::string slurp_file(const bq::string& path)
            {
                if (path.is_empty() || !bq::file_manager::is_file(path)) {
                    return bq::string();
                }
                return bq::file_manager::read_all_text(path);
            }

            // Case F: text appender three-phase sequential semantics test.
            // Verifies the disk_full_drop_ behavior end-to-end:
            //   phase 1: clean disk - write IDs [0, N) - all must reach the file
            //   phase 2: ENOSPC on write - write IDs [N, 2N) - the first one
            //                              is already cached when ENOSPC fires
            //                              (and IS retained, landing later);
            //                              the rest are dropped at log_impl
            //                              entry to bound cache growth
            //   phase 3: clean disk - write IDs [2N, 3N) - the very first
            //                         one is dropped because disk_full_drop_
            //                         only clears AFTER the next successful
            //                         flush; the remaining N-1 land. After
            //                         recovery the cached phase-2 trigger
            //                         entry also lands.
            // After phase 3 the file must contain all phase-1 IDs intact,
            // at least N-1 phase-3 IDs, and at most 1 phase-2 ID (the
            // trigger entry). File structure must remain parseable.
            void case_f_text_three_phase(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case F: text three-phase sequential semantics...\n");
                wipe_log_state("disk_full_case_f_log");
                scoped_fault_injection fault_guard;

                const bq::string out_dir = TO_ABSOLUTE_PATH("disk_full_test", 0);
                const bq::string log_config = R"(
                    appenders_config.case_f_appender.type=text_file
                    appenders_config.case_f_appender.levels=[all]
                    appenders_config.case_f_appender.file_name=disk_full_test/case_f
                    appenders_config.case_f_appender.base_dir_type=0
                    appenders_config.case_f_appender.enable_rolling_log_file=false
                    log.thread_mode=sync
                )";
                bq::log my_log = bq::log::create_log("disk_full_case_f_log", log_config);
                result.add_result(my_log.is_valid(), "Case F: log object created");

                constexpr int32_t N = 30;

                // Phase 1: clean disk.
                for (int32_t i = 0; i < N; ++i) {
                    my_log.info("CASE_F_PHASE1_ID={}", i);
                }
                my_log.force_flush();

                // Phase 2: simulate disk full on writes (NOT on opens, so
                // refresh_file_handle keeps file_ valid; the only failure path
                // is write_file -> ENOSPC -> disk_full_drop_=true).
                bq::platform::test_inject::set_path_filter("disk_full_test");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_write);

                // First entry of phase 2 will hit ENOSPC inside flush_write_cache,
                // setting disk_full_drop_=true. Every subsequent log_impl bails
                // immediately at should_drop_due_to_io_failure().
                for (int32_t i = N; i < 2 * N; ++i) {
                    my_log.info("CASE_F_PHASE2_ID={}", i);
                }
                my_log.force_flush();

                // Phase 3: lift fault. The next successful flush_write_cache
                // (real == need, no error) clears disk_full_drop_, and logging
                // resumes.
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);

                for (int32_t i = 2 * N; i < 3 * N; ++i) {
                    my_log.info("CASE_F_PHASE3_ID={}", i);
                }
                my_log.force_flush();

                // Detach the appender so its destructor flushes everything to disk
                // before we open the file for reading.
                my_log.reset_config(R"(
                    appenders_config.console_only.type=console
                    appenders_config.console_only.levels=[all]
                    log.thread_mode=sync
                )");

                bq::string log_path = find_latest_log_file(out_dir, "case_f", ".log");
                result.add_result(!log_path.is_empty(), "Case F: output log file exists");
                bq::string content = slurp_file(log_path);
                result.add_result(content.size() > 0, "Case F: output log file is non-empty");

                // Debug: dump first 600 bytes of output to a side file we can
                // inspect after the test runs. (printf may go to a stdout that
                // gets redirected away.)
                if (content.size() > 0) {
                    bq::string preview_path = TO_ABSOLUTE_PATH("case_f_preview.txt", 0);
                    bq::file_handle fh = bq::file_manager::instance().open_file(
                        preview_path,
                        bq::file_open_mode_enum::auto_create | bq::file_open_mode_enum::write);
                    if (fh) {
                        size_t dump = bq::min_value((size_t)600, (size_t)content.size());
                        bq::file_manager::instance().write_file(fh, content.c_str(), dump);
                        bq::file_manager::instance().close_file(fh);
                    }
                }

                // Phase 1: every ID must appear.
                int32_t phase1_found = 0;
                for (int32_t i = 0; i < N; ++i) {
                    char needle[64];
                    snprintf(needle, sizeof(needle), "CASE_F_PHASE1_ID=%" PRId32 "\n", i);
                    if (content.find(needle) != bq::string::npos) {
                        ++phase1_found;
                    }
                }
                result.add_result(phase1_found == N,
                    "Case F: all %" PRId32 " phase-1 IDs present (got %" PRId32 ")", N, phase1_found);

                // Phase 3: every ID must appear (recovery worked, hash table
                // intact, file pointer healthy, no half-line corruption).
                //
                // We allow up to 1 phase-3 ID to be missing: when the first
                // phase-3 entry hits log_impl, disk_full_drop_ is still set
                // (it only clears on the next successful flush, which runs
                // AFTER log_impl bails). That entry is dropped at the door,
                // by design - this is the cost of bounding cache growth
                // during the disk-full window. The next flush succeeds,
                // clears the flag, and phase-3 entries 2..N all reach disk.
                int32_t phase3_found = 0;
                for (int32_t i = 2 * N; i < 3 * N; ++i) {
                    char needle[64];
                    snprintf(needle, sizeof(needle), "CASE_F_PHASE3_ID=%" PRId32 "\n", i);
                    if (content.find(needle) != bq::string::npos) {
                        ++phase3_found;
                    }
                }
                result.add_result(phase3_found >= N - 1,
                    "Case F: phase-3 IDs present (got %" PRId32 " / %" PRId32 ", allow >= %" PRId32 ")", phase3_found, N, N - 1);

                // Phase 2: at most ONE phase-2 ID may appear - the very entry
                // that triggered ENOSPC. By the time write_file returned the
                // error, that entry was already in the cache (mark_write_
                // finished'd before flush ran). The user-stated rule is
                // "what's already in cache is NOT dropped" - the cache is
                // retried on every subsequent flush and that one entry lands
                // as soon as the disk is back. From phase-2 #2 onwards,
                // disk_full_drop_ is true and new entries are dropped at the
                // log_impl door, so they never reach the cache, never reach
                // disk - that's the bounding-memory contract.
                int32_t phase2_found = 0;
                for (int32_t i = N; i < 2 * N; ++i) {
                    char needle[64];
                    snprintf(needle, sizeof(needle), "CASE_F_PHASE2_ID=%" PRId32 "\n", i);
                    if (content.find(needle) != bq::string::npos) {
                        ++phase2_found;
                    }
                }
                result.add_result(phase2_found <= 1,
                    "Case F: at most 1 phase-2 ID leaked through disk-full window (got %" PRId32 ")", phase2_found);
            }

            // Case G: encrypted compressed appender, three-phase sequential.
            // This is the critical regression test for the binary appender's
            // disk-full crash story: append_new_segment / xor_key_blob_ / format
            // template hash table all live downstream of log_impl, so an entry
            // that gets through log_impl but cannot reach disk would corrupt
            // them. Recovery would then either crash (segfault on invalid
            // file_) or silently produce a file the decoder cannot parse.
            //
            // The test writes phase-1 IDs (clean), phase-2 IDs (under ENOSPC),
            // phase-3 IDs (clean again), then opens the file via log_decoder
            // with the matching private key and asserts that:
            //   - decoding does not crash;
            //   - phase-1 and phase-3 IDs are decodable;
            //   - phase-2 IDs were dropped at the log_impl boundary so the
            //     hash table / encryption / segment chain stay self-consistent.
            void case_g_encrypted_compressed_three_phase(test_result& result)
            {
                test_output_dynamic(bq::log_level::info, "[disk_full] Case G: encrypted compressed three-phase...\n");
                wipe_log_state("disk_full_case_g_log");
                scoped_fault_injection fault_guard;

                const bq::string out_dir = TO_ABSOLUTE_PATH("disk_full_test", 0);
                bq::string log_config = bq::string()
                    + "appenders_config.case_g_appender.type=compressed_file\n"
                    + "appenders_config.case_g_appender.levels=[all]\n"
                    + "appenders_config.case_g_appender.file_name=disk_full_test/case_g\n"
                    + "appenders_config.case_g_appender.base_dir_type=0\n"
                    + "appenders_config.case_g_appender.enable_rolling_log_file=false\n"
                    + "appenders_config.case_g_appender.pub_key=" + g_pub_key() + "\n"
                    + "log.thread_mode=sync\n";
                bq::log my_log = bq::log::create_log("disk_full_case_g_log", log_config);
                result.add_result(my_log.is_valid(), "Case G: log object created");

                constexpr int32_t N = 30;

                // Phase 1: clean.
                for (int32_t i = 0; i < N; ++i) {
                    my_log.info("CASE_G_PHASE1_ID={}", i);
                }
                my_log.force_flush();

                // Phase 2: ENOSPC on writes. With encryption, this is the path
                // we are most worried about - flush_write_cache encrypts cache
                // bytes in place, write_file fails (real_write_size = 0), the
                // base flush memcpy/memmoves the still-encrypted bytes back to
                // the head of cache, then re-XORs them to undo the encryption.
                // disk_full_drop_ goes high, and every phase-2 entry is then
                // dropped at log_impl entry BEFORE touching the format-template
                // hash table or segment chain.
                bq::platform::test_inject::set_path_filter("disk_full_test");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_write);

                for (int32_t i = N; i < 2 * N; ++i) {
                    my_log.info("CASE_G_PHASE2_ID={}", i);
                }
                my_log.force_flush();

                // Phase 3: clean again. First successful drain clears
                // disk_full_drop_, and logging resumes - phase-3 entries must
                // make it to disk AND must be decodable (i.e. encryption
                // key-stream / hash-table / segment chain stayed coherent).
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);

                for (int32_t i = 2 * N; i < 3 * N; ++i) {
                    my_log.info("CASE_G_PHASE3_ID={}", i);
                }
                my_log.force_flush();

                // Tear down the appender so its destructor flushes the cache
                // and we can decode the file from a fresh handle.
                my_log.reset_config(R"(
                    appenders_config.console_only.type=console
                    appenders_config.console_only.levels=[all]
                    log.thread_mode=sync
                )");

                // compressed_file extension is ".logcompr".
                bq::string log_path = find_latest_log_file(out_dir, "case_g", ".logcompr");
                result.add_result(!log_path.is_empty(), "Case G: output log file exists");

                int32_t phase1_decoded = 0, phase2_decoded = 0, phase3_decoded = 0;
                int32_t entries_decoded = 0;
                bool decoder_ok = true;
                if (!log_path.is_empty()) {
                    bq::tools::log_decoder decoder(log_path, g_priv_key());
                    while (true) {
                        auto rc = decoder.decode();
                        if (rc == bq::appender_decode_result::eof) {
                            break;
                        }
                        if (rc != bq::appender_decode_result::success) {
                            decoder_ok = false;
                            break;
                        }
                        ++entries_decoded;
                        const bq::string& entry = decoder.get_last_decoded_log_entry();
                        if (entry.find("CASE_G_PHASE1_ID=") != bq::string::npos) {
                            ++phase1_decoded;
                        } else if (entry.find("CASE_G_PHASE2_ID=") != bq::string::npos) {
                            ++phase2_decoded;
                        } else if (entry.find("CASE_G_PHASE3_ID=") != bq::string::npos) {
                            ++phase3_decoded;
                        }
                    }
                }
                result.add_result(decoder_ok,
                    "Case G: decoder ran to EOF without error (entries decoded: %" PRId32 ")", entries_decoded);
                result.add_result(phase1_decoded == N,
                    "Case G: all %" PRId32 " phase-1 entries decoded (got %" PRId32 ")", N, phase1_decoded);
                // See Case F for why up to 1 phase-3 entry may be missing
                // and up to 1 phase-2 entry may leak: the first phase-2
                // entry was already cached (mark_write_finished) when
                // ENOSPC fired, so it lands on disk after recovery; the
                // first phase-3 entry arrives while disk_full_drop_ is
                // still set (it clears on the next flush, AFTER log_impl
                // has dropped this entry).
                result.add_result(phase3_decoded >= N - 1,
                    "Case G: phase-3 entries decoded (got %" PRId32 " / %" PRId32 ", allow >= %" PRId32 ") - hash-cache / segment / encryption stayed coherent across disk-full window", phase3_decoded, N, N - 1);
                result.add_result(phase2_decoded <= 1,
                    "Case G: at most 1 phase-2 entry leaked through disk-full window (got %" PRId32 ")", phase2_decoded);
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                case_a_oversize_no_hang(result);
                case_b_oversize_oom_no_crash(result);
                case_c_appender_no_index_drift(result);
                case_d_block_mode_still_works(result);
                case_e_end_to_end(result);
                case_f_text_three_phase(result);
                case_g_encrypted_compressed_three_phase(result);
                bq::file_manager::remove_file_or_dir(TO_ABSOLUTE_PATH("disk_full_test", 0));
                return result;
            }
        };
    }
}
