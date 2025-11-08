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

#if defined(CLOCK_MONOTONIC) && !defined(BQ_APPLE)
            if (0 == pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC)) {
                platform_data_->use_monotonic_clock = true;
            }
#endif

            int cond_init_result = pthread_cond_init(&platform_data_->cond_handle, platform_data_->use_monotonic_clock ? &cond_attr : nullptr);
            if (cond_init_result != 0) {
                bq::util::log_device_console(log_level::warning, "pthread_cond_init failed with custom clock, error code : %d, fallback to default clock", cond_init_result);
                platform_data_->use_monotonic_clock = false;
                cond_init_result = pthread_cond_init(&platform_data_->cond_handle, nullptr);
                if (cond_init_result != 0) {
                    bq::util::log_device_console(log_level::fatal, "%s : %d : pthread_cond_init failed, error code:%d", __FILE__, __LINE__, cond_init_result);
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

        void condition_variable::wait(bq::platform::mutex& lock)
        {
            auto result = pthread_cond_wait(&platform_data_->cond_handle, (pthread_mutex_t*)(lock.get_platform_handle()));
            if (result != 0) {
                bq::util::log_device_console(log_level::error, "pthread_cond_wait failed, error code : %d", result);
            }
        }

        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms)
        {
            struct timespec now;
            struct timespec outtime;
            bool clock_success = false;

            clockid_t clock_id = platform_data_->use_monotonic_clock ? CLOCK_MONOTONIC : CLOCK_REALTIME;
            if (0 == clock_gettime(clock_id, &now)) {
                clock_success = true;
            }
            uint64_t total_nsec = 0;
            if (!clock_success) {
                struct timeval now_tv;
                gettimeofday(&now_tv, NULL);
                total_nsec = static_cast<uint64_t>(now_tv.tv_sec) * 1000000000 + static_cast<uint64_t>(now_tv.tv_usec) * 1000;
                total_nsec += wait_time_ms * 1000000;
                outtime.tv_sec = static_cast<decltype(outtime.tv_sec)>(total_nsec / 1000000000);
                outtime.tv_nsec = static_cast<decltype(outtime.tv_nsec)>(total_nsec % 1000000000);
            } else {
                uint64_t add_seconds = wait_time_ms / 1000;
                uint64_t add_nanoseconds = (wait_time_ms % 1000) * 1000000ULL;
                total_nsec = static_cast<uint64_t>(now.tv_nsec) + add_nanoseconds;
                outtime.tv_sec = now.tv_sec + static_cast<decltype(outtime.tv_nsec)>(add_seconds + (total_nsec / 1000000000ULL));
                outtime.tv_nsec = static_cast<decltype(outtime.tv_nsec)>(total_nsec % 1000000000ULL);
            }

            auto result = pthread_cond_timedwait(&platform_data_->cond_handle, (pthread_mutex_t*)(lock.get_platform_handle()), &outtime);
            if (result == ETIMEDOUT) {
                return false;
            }
            if (result != 0) {
                bq::util::log_device_console(log_level::error, "pthread_cond_timedwait failed,  error code : %d, time nsec:%" PRIu64 "", result, total_nsec);
            }
            return true;
        }

        void condition_variable::notify_one() noexcept
        {
            pthread_cond_signal(&platform_data_->cond_handle);
        }

        void condition_variable::notify_all() noexcept
        {
            pthread_cond_broadcast(&platform_data_->cond_handle);
        }
    }
}
#endif
