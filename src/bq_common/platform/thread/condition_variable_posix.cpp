/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/platform/thread/condition_variable_posix.h"
#ifdef BQ_POSIX
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "bq_common/bq_common.h"

#if defined(CLOCK_MONOTONIC) && !defined(BQ_APPLE)
#define BQ_POSIX_MONOTONIC_SUPPORTED 1
#endif

namespace bq {
    namespace platform {
        struct condition_variable_platform_def {
            pthread_cond_t cond_handle;
            bool use_monotonic_clock;
        };

        condition_variable::condition_variable()
        {
            platform_data_ = (condition_variable_platform_def*)malloc(sizeof(condition_variable_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) condition_variable_platform_def();
            platform_data_->use_monotonic_clock = false;

            pthread_condattr_t cond_attr;
            pthread_condattr_init(&cond_attr);

#if defined(BQ_POSIX_MONOTONIC_SUPPORTED)
            // Prefer CLOCK_MONOTONIC when supported
            if (0 == pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC)) {
                platform_data_->use_monotonic_clock = true;
            }
#endif

            int32_t cond_init_result = pthread_cond_init(&platform_data_->cond_handle, platform_data_->use_monotonic_clock ? &cond_attr : nullptr);
            if (cond_init_result != 0) {
                bq::util::log_device_console(log_level::warning, "pthread_cond_init with custom clock failed: %d, falling back", cond_init_result);
                platform_data_->use_monotonic_clock = false;
                cond_init_result = pthread_cond_init(&platform_data_->cond_handle, nullptr);
                if (cond_init_result != 0) {
                    bq::util::log_device_console(log_level::fatal, "%s:%d pthread_cond_init failed: %d", __FILE__, __LINE__, cond_init_result);
                }
            }

            pthread_condattr_destroy(&cond_attr);
        }

        condition_variable::~condition_variable()
        {
            if (platform_data_) {
                pthread_cond_destroy(&(platform_data_->cond_handle));
            }
            free(platform_data_);
            platform_data_ = nullptr;
        }
        static constexpr uint64_t one_billion = 1000000000ULL;

        void condition_variable::wait(bq::platform::mutex& lock)
        {
            int32_t result = pthread_cond_wait(&platform_data_->cond_handle, (pthread_mutex_t*)(lock.get_platform_handle()));
            if (result != 0) {
                bq::util::log_device_console(log_level::error, "pthread_cond_wait failed: %d", result);
            }
        }

        static inline void add_timespec_ns(const struct timespec& base, uint64_t add_ns, struct timespec* out)
        {
            uint64_t ns = static_cast<uint64_t>(base.tv_nsec) + add_ns;
            out->tv_sec  = static_cast<decltype(out->tv_sec)>(static_cast<int64_t>(base.tv_sec) + static_cast<int64_t>(ns / one_billion));
            out->tv_nsec = static_cast<decltype(out->tv_nsec)>(ns % one_billion);
        }

        static uint64_t get_current_ns(bool prefer_monotonic)
        {
            struct timespec ts {};
#if defined(BQ_POSIX_MONOTONIC_SUPPORTED)
            if (prefer_monotonic) {
                if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
                    return static_cast<uint64_t>(ts.tv_sec) * one_billion + static_cast<uint64_t>(ts.tv_nsec);
                }
            }
#endif
            (void)prefer_monotonic;
            if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                return static_cast<uint64_t>(ts.tv_sec) * one_billion + static_cast<uint64_t>(ts.tv_nsec);
            }
            return bq::platform::high_performance_epoch_ms() * 1000000ULL;
        }

        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms)
        {
            const uint64_t wait_ns_total = wait_time_ms * 1000000ULL;
            const uint64_t quantum_ns = 50ULL * 1000000ULL; // 50ms
            const bool use_monotonic = platform_data_->use_monotonic_clock;

            uint64_t start_ns = get_current_ns(use_monotonic);
            uint64_t deadline_ns = start_ns + wait_ns_total;

            for (;;) {
                uint64_t now_ns = get_current_ns(use_monotonic);

                if (now_ns >= deadline_ns) return false;

                uint64_t remain_ns = deadline_ns - now_ns;
                if (remain_ns > quantum_ns) remain_ns = quantum_ns;

                struct timespec outtime {};

#if defined(BQ_POSIX_MONOTONIC_SUPPORTED)
                if (use_monotonic) {
                    struct timespec now_mono {};
                    if (clock_gettime(CLOCK_MONOTONIC, &now_mono) != 0) {
                        now_mono.tv_sec = static_cast<decltype(now_mono.tv_sec)>(now_ns / one_billion);
                        now_mono.tv_nsec = static_cast<decltype(now_mono.tv_nsec)>(now_ns % one_billion);
                    }
                    add_timespec_ns(now_mono, remain_ns, &outtime);
                } else
#endif
                {
                    struct timespec now_rt {};
                    if (clock_gettime(CLOCK_REALTIME, &now_rt) != 0) {
                        struct timeval tv {};
                        gettimeofday(&tv, nullptr);
                        now_rt.tv_sec = static_cast<decltype(now_rt.tv_sec)>(tv.tv_sec);
                        now_rt.tv_nsec = static_cast<decltype(now_rt.tv_nsec)>(tv.tv_usec * 1000L);
                    }
                    add_timespec_ns(now_rt, remain_ns, &outtime);
                }

                int32_t result = pthread_cond_timedwait(&platform_data_->cond_handle,
                                                    (pthread_mutex_t*)(lock.get_platform_handle()),
                                                    &outtime);
                if (result == 0) return true;
                if (result == ETIMEDOUT) continue;

                bq::util::log_device_console(log_level::error, "pthread_cond_timedwait failed: %d", result);
                return true;
            }
        }

        void condition_variable::notify_one() noexcept
        {
            pthread_cond_signal(&platform_data_->cond_handle);
        }

        void condition_variable::notify_all() noexcept
        {
            pthread_cond_broadcast(&platform_data_->cond_handle);
        }

    } // namespace platform
} // namespace bq
#endif