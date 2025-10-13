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
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        struct condition_variable_platform_def {
            pthread_cond_t cond_handle = PTHREAD_COND_INITIALIZER;
        };

        condition_variable::condition_variable()
        {
            platform_data_ = (condition_variable_platform_def*)malloc(sizeof(condition_variable_platform_def));
            new (platform_data_, bq::enum_new_dummy::dummy) condition_variable_platform_def();
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
            struct timeval now;
            struct timespec outtime;

            gettimeofday(&now, NULL);
            uint64_t total_nsec = (uint64_t)now.tv_sec * 1000000000 + (uint64_t)now.tv_usec * 1000;
            total_nsec += wait_time_ms * 1000000;
            outtime.tv_sec = (decltype(outtime.tv_sec))(total_nsec / 1000000000);
            outtime.tv_nsec = (decltype(outtime.tv_nsec))(total_nsec % 1000000000);
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
