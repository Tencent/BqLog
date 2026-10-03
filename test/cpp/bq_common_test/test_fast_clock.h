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

            static void test_accuracy(test_result& result)
            {
                bq::platform::fast_clock_thread_cache cache = {};
                offset_sync sync;
                if (!warm_up(cache, sync)) {
                    result.add_result(true, "fast clock not available on this machine, accuracy test skipped");
                    return;
                }
                uint64_t max_error_ms = 0;
                for (int32_t i = 0; i < 30; ++i) {
                    uint64_t ms = 0;
                    const bool ok = bq::platform::fast_clock_read_epoch_ms(cache, sync, ms);
                    result.add_result(ok, "fast clock read %" PRId32, i);
                    max_error_ms = bq::max_value(max_error_ms, abs_diff(ms, precise_epoch_ms()));
                    bq::platform::thread::sleep(50);
                }
                result.add_result(max_error_ms <= 2, "fast clock error %" PRIu64 " ms over 1.5 s", max_error_ms);
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
                    uint64_t ms = 0;
                    if (!bq::platform::fast_clock_read_epoch_ms(cache, sync, ms)) {
                        continue;
                    }
                    ++reads;
                    if (abs_diff(ms, precise_epoch_ms()) > 1) {
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
                        const uint64_t now = bq::platform::high_performance_epoch_ms();
                        if (now < last) {
                            monotonic_ = false;
                        }
                        if ((reads_ & 1023) == 0) {
                            max_error_ms_ = bq::max_value(max_error_ms_, abs_diff(now, precise_epoch_ms()));
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
