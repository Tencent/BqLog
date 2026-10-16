#pragma once
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
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_common/platform/fast_clock.h"

#include <chrono>

namespace bq {
    namespace test {
        class test_fast_clock : public test_base {
            static uint64_t abs_diff(uint64_t a, uint64_t b)
            {
                return a > b ? a - b : b - a;
            }

            // Real wall clock plus an adjustable offset, to simulate wall clock steps.
            struct offset_sync {
                int64_t offset_ns_ = 0;
                bool fail_ = false;
                bool operator()(uint64_t& out_epoch_ns) const
                {
                    if (fail_ || !bq::platform::fast_clock_sync_epoch_ns(out_epoch_ns)) {
                        return false;
                    }
                    out_epoch_ns = static_cast<uint64_t>(static_cast<int64_t>(out_epoch_ns) + offset_ns_);
                    return true;
                }
            };

            static uint64_t precise_epoch_ms()
            {
                uint64_t ns = 0;
                bq::platform::fast_clock_sync_epoch_ns(ns);
                return ns / 1000000ULL;
            }

            static void force_resync(bq::platform::fast_clock_thread_cache& cache)
            {
                cache.next_ms_counter_ = 0;
                cache.resync_counter_ = 0;
            }

            // Reads until the frequency is known (measured on x86, read from a register on ARM).
            static bool warm_up(bq::platform::fast_clock_thread_cache& cache, const offset_sync& sync)
            {
                const uint64_t end_ms = bq::platform::system_epoch_ms() + 200;
                uint64_t ms = 0;
                while (bq::platform::system_epoch_ms() < end_ms) {
                    force_resync(cache);
                    if (bq::platform::fast_clock_read_epoch_ms(cache, sync, ms)) {
                        return true;
                    }
                    bq::platform::thread::sleep(5);
                }
                return false;
            }

            // Monotonic milliseconds. steady_clock cannot be moved by the host or by NTP, so it is the
            // reference that tells a wall clock correction apart from fast clock drift.
            static uint64_t monotonic_ms()
            {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            }

            // Did the wall clock move by something other than the time that really passed? 20 ms is far
            // above the wall clock's resolution and a sleep's overshoot, and far below a correction.
            static bool wall_clock_stepped(uint64_t wall_from, uint64_t mono_from, uint64_t wall_to, uint64_t mono_to)
            {
                const int64_t really_passed = static_cast<int64_t>(mono_to - mono_from);
                const int64_t wall_moved = static_cast<int64_t>(wall_to) - static_cast<int64_t>(wall_from);
                const int64_t gap = wall_moved > really_passed ? wall_moved - really_passed : really_passed - wall_moved;
                return gap > 20;
            }

            static void test_accuracy(test_result& result)
            {
                // the detector itself, on fixed pairs, so its meaning is pinned down
                result.add_result(!wall_clock_stepped(1000, 1000, 1050, 1050), "a plain 50 ms sleep is not a wall clock step");
                result.add_result(!wall_clock_stepped(1000, 1000, 1300, 1300), "a sleep that overshoots is not a wall clock step");
                result.add_result(wall_clock_stepped(1000, 1000, 1000 + 42674, 1050), "a 42 s wall clock step is seen as one");

                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, accuracy test skipped");
                    return;
                }
                // 50 samples 50 ms apart, past the window over which x86 refines the measured counter
                // frequency. A round in which the wall clock was corrected does not count: the fast clock
                // only re-anchors to the wall clock every second, so until it does it cannot agree with a
                // clock that just moved, and that is a clock correction rather than drift. Re-anchor and
                // measure that round again - and complain if the host keeps stepping, rather than pass.
                uint64_t max_error_ms = 0;
                uint32_t samples = 0;
                uint32_t stepped_rounds = 0;
                uint64_t wall_last = precise_epoch_ms();
                uint64_t mono_last = monotonic_ms();
                while (samples < 50 && stepped_rounds < 40) {
                    bq::platform::thread::sleep(50);
                    const uint64_t wall_before = precise_epoch_ms();
                    uint64_t ms = 0;
                    const bool ok = bq::platform::fast_clock_read_epoch_ms(cache, sync, ms);
                    const uint64_t wall_after = precise_epoch_ms();
                    const uint64_t mono_after = monotonic_ms();
                    const bool stepped = wall_clock_stepped(wall_last, mono_last, wall_after, mono_after);
                    wall_last = wall_after;
                    mono_last = mono_after;
                    if (stepped) {
                        force_resync(cache);
                        ++stepped_rounds;
                        continue;
                    }
                    result.add_result(ok, "fast clock read %" PRIu32, samples);
                    const uint64_t error_ms = ms < wall_before ? wall_before - ms : (ms > wall_after ? ms - wall_after : 0);
                    max_error_ms = bq::max_value(max_error_ms, error_ms);
                    ++samples;
                }
                result.add_result(stepped_rounds < 40, "the wall clock was not corrected during the measurement (%" PRIu32 " rounds skipped)", stepped_rounds);
                // 5 ms, not 2: a loaded virtual machine refines the counter frequency slowly enough to
                // drift past 2 ms, which used to fail the BSD/Solaris jobs at random.
                result.add_result(max_error_ms <= 5, "fast clock error %" PRIu64 " ms over 2.5 s", max_error_ms);
                result.add_result(abs_diff(bq::platform::high_performance_epoch_ms(), bq::platform::system_epoch_ms()) <= 20, "high_performance_epoch_ms agrees with the system clock");
            }

            static void test_ms_boundary(test_result& result)
            {
                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, ms boundary test skipped");
                    return;
                }
                uint64_t reads = 0;
                uint64_t far_off = 0;
                uint64_t last = 0;
                bool monotonic = true;
                const uint64_t end_ms = bq::platform::system_epoch_ms() + 300;
                while (bq::platform::system_epoch_ms() < end_ms) {
                    // bracket the read with the wall clock, so a preemption between the reads cannot count as an error
                    const uint64_t before_ms = precise_epoch_ms();
                    uint64_t ms = 0;
                    if (!bq::platform::fast_clock_read_epoch_ms(cache, sync, ms)) {
                        continue;
                    }
                    const uint64_t after_ms = precise_epoch_ms();
                    ++reads;
                    if (ms + 1 < before_ms || ms > after_ms + 1) {
                        ++far_off;
                    }
                    if (ms < last) {
                        monotonic = false;
                    }
                    last = ms;
                    if (reads % 100000 == 0) {
                        force_resync(cache);
                    }
                }
                result.add_result(reads > 1000 && far_off == 0, "cached millisecond agrees with the wall clock (%" PRIu64 " reads, %" PRIu64 " off)", reads, far_off);
                result.add_result(monotonic, "cached millisecond is monotonic across resyncs");
            }

            static void test_wall_steps(test_result& result)
            {
                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, wall step test skipped");
                    return;
                }
                uint64_t ms = 0;
                // forward step: followed at the next resync
                sync.offset_ns_ = 5000000000LL;
                force_resync(cache);
                result.add_result(bq::platform::fast_clock_read_epoch_ms(cache, sync, ms) && abs_diff(ms, precise_epoch_ms() + 5000) <= 2, "forward wall step followed");
                // small step back: held, time never goes backwards
                const uint64_t before = ms;
                sync.offset_ns_ = 5000000000LL - 300000000LL;
                force_resync(cache);
                result.add_result(bq::platform::fast_clock_read_epoch_ms(cache, sync, ms) && ms >= before, "small backward step held");
                // large step back: followed
                sync.offset_ns_ = -5000000000LL;
                force_resync(cache);
                result.add_result(bq::platform::fast_clock_read_epoch_ms(cache, sync, ms) && abs_diff(ms + 5000, precise_epoch_ms()) <= 2, "large backward step followed");
                result.add_result(!bq::platform::fast_clock_detail::shared_state<>::untrusted_.load_relaxed(), "isolated wall steps keep the counter trusted");
            }

            static void test_resync_interval(test_result& result)
            {
                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, resync interval test skipped");
                    return;
                }
                uint64_t ms = 0;
                bq::platform::fast_clock_read_epoch_ms(cache, sync, ms);
                // a step is not seen before the interval ends, and is seen after it
                sync.offset_ns_ = 10000000000LL;
                bq::platform::fast_clock_read_epoch_ms(cache, sync, ms);
                const bool unseen = abs_diff(ms, precise_epoch_ms()) <= 2;
                bq::platform::thread::sleep(BQ_FAST_CLOCK_RESYNC_INTERVAL_MS + 50);
                bq::platform::fast_clock_read_epoch_ms(cache, sync, ms);
                result.add_result(unseen && abs_diff(ms, precise_epoch_ms() + 10000) <= 2, "wall step seen after one resync interval");
            }

            static void test_sync_failure(test_result& result)
            {
                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, sync failure test skipped");
                    return;
                }
                uint64_t ms = 0;
                sync.fail_ = true;
                force_resync(cache);
                result.add_result(!bq::platform::fast_clock_read_epoch_ms(cache, sync, ms), "failed sync falls back");
                result.add_result(!bq::platform::fast_clock_read_epoch_ms(cache, sync, ms), "failed sync keeps falling back until a sync succeeds");
                sync.fail_ = false;
                result.add_result(bq::platform::fast_clock_read_epoch_ms(cache, sync, ms), "counter used again after a successful sync");
            }

            class reader_thread : public bq::platform::thread {
            public:
                bq::platform::atomic<bool>* stop_ = nullptr;
                bool monotonic_ = true;
                uint64_t reads_ = 0;
                uint64_t max_error_ms_ = 0;
                void run() override
                {
                    uint64_t last = 0;
                    while (!stop_->load_acquire()) {
                        const bool check = (reads_ & 1023) == 0;
                        const uint64_t before_ms = check ? precise_epoch_ms() : 0;
                        const uint64_t now = bq::platform::high_performance_epoch_ms();
                        if (now < last) {
                            monotonic_ = false;
                        }
                        if (check) {
                            // bracketed by the wall clock, so preemption between the reads is not an error
                            const uint64_t after_ms = precise_epoch_ms();
                            const uint64_t error_ms = now < before_ms ? before_ms - now : (now > after_ms ? now - after_ms : 0);
                            max_error_ms_ = bq::max_value(max_error_ms_, error_ms);
                        }
                        last = now;
                        ++reads_;
                    }
                }
            };

            static void test_threads(test_result& result)
            {
                bq::platform::atomic<bool> stop(false);
                reader_thread readers[4];
                for (auto& r : readers) {
                    r.stop_ = &stop;
                    r.start();
                }
                bq::platform::thread::sleep(BQ_FAST_CLOCK_RESYNC_INTERVAL_MS + 500);
                stop.store_release(true);
                for (auto& r : readers) {
                    r.join();
                    result.add_result(r.monotonic_, "per-thread time is monotonic (%" PRIu64 " reads)", r.reads_);
                    result.add_result(r.max_error_ms_ <= 20, "per-thread time error %" PRIu64 " ms", r.max_error_ms_);
                }
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                test_accuracy(result);
                test_ms_boundary(result);
                test_wall_steps(result);
                test_resync_interval(result);
                test_sync_failure(result);
                test_threads(result);
                return result;
            }
        };
    }
}
