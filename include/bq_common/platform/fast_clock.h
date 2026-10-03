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
/*
 * Hardware counter clock read per thread. Each thread anchors the counter to the system wall clock and re-anchors it
 * every BQ_FAST_CLOCK_RESYNC_INTERVAL_MS, so cross-core skew, wall clock steps and drift last at most one interval.
 * Everything is inline; each module (executable, shared library) keeps its own state.
 */
#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/atomic/atomic.h"
#if defined(BQ_MSVC)
#include <intrin.h>
#endif
#if defined(BQ_MSVC) && defined(BQ_ARM_64)
// ARM64_SYSREG(op0, op1, CRn, CRm, op2) encodings, as winnt.h defines them; this header does not include windows.h.
#define BQ_ARM64_SYSREG_CNTVCT 0x5F02 // CNTVCT_EL0: 3, 3, 14, 0, 2
#define BQ_ARM64_SYSREG_CNTFRQ 0x5F00 // CNTFRQ_EL0: 3, 3, 14, 0, 0
#endif

// Must be the same in every translation unit of a module.
#ifndef BQ_FAST_CLOCK_RESYNC_INTERVAL_MS
#define BQ_FAST_CLOCK_RESYNC_INTERVAL_MS 1000
#endif

#if defined(BQ_ARM_64) || defined(BQ_X86_64)
#define BQ_FAST_CLOCK_SUPPORTED 1
#endif

namespace bq {
    namespace platform {
        // Per thread, zero initialized. epoch_ms_ is current until the counter reaches next_ms_counter_.
        struct fast_clock_thread_cache {
            uint64_t next_ms_counter_;
            uint64_t epoch_ms_;
            uint64_t resync_counter_;
            uint64_t base_counter_;
            uint64_t base_epoch_ms_;
            uint64_t base_ms_fraction_; // sub-millisecond part of the anchor, 64-bit binary fraction
            uint64_t ms_mul_; // milliseconds per tick, 64-bit binary fraction
            uint64_t last_sync_wall_ns_;
            uint32_t rate_mismatches_;
            uint32_t reserved_;
        };

        namespace fast_clock_detail {
            static constexpr uint64_t ns_per_ms = 1000000ULL;
            static constexpr uint64_t ns_per_second = 1000000000ULL;
            static constexpr uint64_t max_backward_hold_ms = 1000;
            static constexpr uint64_t frequency_measure_window_ns = 20 * ns_per_ms;
            static constexpr uint64_t frequency_settled_window_ns = 2000 * ns_per_ms;
            static constexpr uint64_t unsettled_resync_interval_ms = 50;
            static constexpr uint32_t max_rate_mismatches = 3;

            // Per module, constant initialized.
            template <typename T = void>
            struct shared_state {
                static atomic_trivially_constructible<uint64_t> counter_frequency_;
                static atomic_trivially_constructible<uint64_t> measure_counter_;
                static atomic_trivially_constructible<uint64_t> measure_wall_ns_;
                static atomic_trivially_constructible<uint32_t> untrusted_; // the counter rate kept disagreeing with the wall clock
            };
            template <typename T>
            atomic_trivially_constructible<uint64_t> shared_state<T>::counter_frequency_;
            template <typename T>
            atomic_trivially_constructible<uint64_t> shared_state<T>::measure_counter_;
            template <typename T>
            atomic_trivially_constructible<uint64_t> shared_state<T>::measure_wall_ns_;
            template <typename T>
            atomic_trivially_constructible<uint32_t> shared_state<T>::untrusted_;

            bq_forceinline uint64_t read_counter()
            {
#if defined(BQ_ARM_64) && (defined(BQ_GCC) || defined(BQ_CLANG))
                uint64_t value;
                __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
                return value;
#elif defined(BQ_ARM_64) && defined(BQ_MSVC)
                return static_cast<uint64_t>(_ReadStatusReg(BQ_ARM64_SYSREG_CNTVCT));
#elif defined(BQ_X86_64) && defined(BQ_MSVC)
                return __rdtsc();
#elif defined(BQ_X86_64)
                return __builtin_ia32_rdtsc();
#else
                return 0;
#endif
            }

            bq_forceinline uint64_t mul_high(uint64_t a, uint64_t b, uint64_t& out_low)
            {
#if defined(BQ_MSVC) && defined(BQ_X86_64)
                uint64_t high;
                out_low = _umul128(a, b, &high);
                return high;
#elif defined(BQ_MSVC) && defined(BQ_ARM_64)
                out_low = a * b;
                return __umulh(a, b);
#elif defined(__SIZEOF_INT128__)
                // __extension__: -pedantic rejects __int128 otherwise
                __extension__ typedef unsigned __int128 uint128_type;
                const uint128_type r = static_cast<uint128_type>(a) * b;
                out_low = static_cast<uint64_t>(r);
                return static_cast<uint64_t>(r >> 64);
#else
                (void)a;
                (void)b;
                out_low = 0;
                return 0;
#endif
            }

            // 0 while unknown. x86 has no portable way to read the TSC frequency, so it is measured from the first anchor
            // of the module: usable after 20 ms, then refined on every anchor until the span reaches 2 s (out_settled
            // false meanwhile), since wall clock jitter over a short span skews every timestamp by the same ratio.
            inline uint64_t counter_frequency(uint64_t counter, uint64_t wall_ns, bool& out_settled)
            {
                typedef shared_state<> s;
                uint64_t frequency = s::counter_frequency_.load_relaxed();
                out_settled = true;
#if defined(BQ_ARM_64)
                if (frequency) {
                    return frequency;
                }
#if defined(BQ_GCC) || defined(BQ_CLANG)
                __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
#elif defined(BQ_MSVC)
                frequency = static_cast<uint64_t>(_ReadStatusReg(BQ_ARM64_SYSREG_CNTFRQ));
#endif
                (void)counter;
                (void)wall_ns;
#else
                const uint64_t measure_wall_ns = s::measure_wall_ns_.load_acquire();
                if (measure_wall_ns == 0) {
                    uint64_t expected = 0;
                    if (s::measure_counter_.compare_exchange_strong(expected, counter, memory_order::relaxed, memory_order::relaxed)) {
                        s::measure_wall_ns_.store_release(wall_ns);
                    }
                    return 0;
                }
                if (frequency && wall_ns >= measure_wall_ns + frequency_settled_window_ns) {
                    return frequency;
                }
                out_settled = false;
                const uint64_t measure_counter = s::measure_counter_.load_relaxed();
                if (wall_ns < measure_wall_ns + frequency_measure_window_ns || counter <= measure_counter) {
                    return frequency;
                }
                frequency = static_cast<uint64_t>(static_cast<double>(counter - measure_counter) * static_cast<double>(ns_per_second) / static_cast<double>(wall_ns - measure_wall_ns));
#endif
                s::counter_frequency_.store_relaxed(frequency);
                return frequency;
            }

            inline bool resync_failed(fast_clock_thread_cache& cache)
            {
                cache.next_ms_counter_ = 0;
                cache.resync_counter_ = 0;
                return false;
            }

            // SYNC: bool (uint64_t& out_epoch_ns) const, a precise wall clock; false means the counter must not be used.
            template <typename SYNC>
            bq_noinline bool resync(fast_clock_thread_cache& cache, const SYNC& sync)
            {
                if (shared_state<>::untrusted_.load_relaxed()) {
                    return resync_failed(cache);
                }
                const uint64_t counter_before = read_counter();
                uint64_t wall_ns;
                const bool synced = sync(wall_ns);
                const uint64_t counter = counter_before + ((read_counter() - counter_before) >> 1);
                if (!synced) {
                    return resync_failed(cache);
                }
                bool frequency_settled;
                const uint64_t frequency = counter_frequency(counter, wall_ns, frequency_settled);
                if (frequency < 1000000ULL) {
                    return resync_failed(cache);
                }
                if (cache.last_sync_wall_ns_ && wall_ns > cache.last_sync_wall_ns_ && counter > cache.base_counter_) {
                    const double expected_ticks = static_cast<double>(wall_ns - cache.last_sync_wall_ns_) * static_cast<double>(frequency) / static_cast<double>(ns_per_second);
                    const double ticks = static_cast<double>(counter - cache.base_counter_);
                    const double tolerance = expected_ticks * 0.01 + static_cast<double>(frequency) / 10000.0;
                    if (ticks > expected_ticks + tolerance || ticks < expected_ticks - tolerance) {
                        // a single miss is a wall clock step or a resume from sleep; a persistent one is a wrong frequency
                        if (++cache.rate_mismatches_ >= max_rate_mismatches) {
                            shared_state<>::untrusted_.store_relaxed(1);
                            return resync_failed(cache);
                        }
                    } else {
                        cache.rate_mismatches_ = 0;
                    }
                }
                cache.base_counter_ = counter;
                cache.base_epoch_ms_ = wall_ns / ns_per_ms;
                cache.base_ms_fraction_ = static_cast<uint64_t>(static_cast<double>(wall_ns % ns_per_ms) * (18446744073709551616.0 / static_cast<double>(ns_per_ms)));
                cache.ms_mul_ = static_cast<uint64_t>(18446744073709551616.0 * 1000.0 / static_cast<double>(frequency));
                const uint64_t interval_ms = (frequency_settled || BQ_FAST_CLOCK_RESYNC_INTERVAL_MS < unsettled_resync_interval_ms)
                    ? static_cast<uint64_t>(BQ_FAST_CLOCK_RESYNC_INTERVAL_MS)
                    : unsettled_resync_interval_ms;
                cache.resync_counter_ = counter + frequency * interval_ms / 1000ULL;
                cache.last_sync_wall_ns_ = wall_ns;
                return true;
            }

            // Returns 0 when the counter must not be used.
            template <typename SYNC>
            inline uint64_t advance(fast_clock_thread_cache& cache, uint64_t counter, const SYNC& sync)
            {
                // counter below the anchor: moved to a core whose counter is behind
                BQ_UNLIKELY_IF((counter >= cache.resync_counter_) | (counter < cache.base_counter_))
                {
                    if (!resync(cache, sync)) {
                        return 0;
                    }
                    counter = cache.base_counter_;
                }
                uint64_t fraction;
                const uint64_t whole_ms = mul_high(counter - cache.base_counter_, cache.ms_mul_, fraction);
                const uint64_t sum = fraction + cache.base_ms_fraction_;
                uint64_t epoch_ms = cache.base_epoch_ms_ + whole_ms + (sum < fraction ? 1U : 0U);
                // a new anchor may land slightly behind the time already handed out; only a large step back is followed
                if (epoch_ms < cache.epoch_ms_ && cache.epoch_ms_ - epoch_ms <= max_backward_hold_ms) {
                    epoch_ms = cache.epoch_ms_;
                }
                const uint64_t boundary = counter + (~sum) / cache.ms_mul_ + 1;
                cache.next_ms_counter_ = boundary < cache.resync_counter_ ? boundary : cache.resync_counter_;
                cache.epoch_ms_ = epoch_ms;
                return epoch_ms;
            }
        }

        // Precise wall clock the counter is anchored to. false where the platform has none.
        inline bool fast_clock_sync_epoch_ns(uint64_t& out_epoch_ns)
        {
#if defined(BQ_WIN)
            struct timespec ts;
            if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
                return false;
            }
#elif defined(BQ_POSIX) && !defined(BQ_PS)
            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
                return false;
            }
#else
            (void)out_epoch_ns;
            return false;
#endif
#if defined(BQ_WIN) || (defined(BQ_POSIX) && !defined(BQ_PS))
            out_epoch_ns = static_cast<uint64_t>(ts.tv_sec) * fast_clock_detail::ns_per_second + static_cast<uint64_t>(ts.tv_nsec);
            return true;
#endif
        }

        // false: the caller falls back to the system clock.
        template <typename SYNC>
        bq_forceinline bool fast_clock_read_epoch_ms(fast_clock_thread_cache& cache, const SYNC& sync, uint64_t& out_epoch_ms)
        {
#if defined(BQ_FAST_CLOCK_SUPPORTED)
            const uint64_t counter = fast_clock_detail::read_counter();
            BQ_LIKELY_IF(counter < cache.next_ms_counter_)
            {
                out_epoch_ms = cache.epoch_ms_;
                return true;
            }
            out_epoch_ms = fast_clock_detail::advance(cache, counter, sync);
            return out_epoch_ms != 0;
#else
            (void)cache;
            (void)sync;
            (void)out_epoch_ms;
            return false;
#endif
        }

        bq_forceinline bool fast_clock_read_epoch_ms(fast_clock_thread_cache& cache, uint64_t& out_epoch_ms)
        {
            return fast_clock_read_epoch_ms(cache, fast_clock_sync_epoch_ns, out_epoch_ms);
        }
    }
}
