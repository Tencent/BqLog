#pragma once
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
/*!
 * \file condition_variable.h
 *
 * \author pippocao
 * \date 2022/09/16
 *
 * simple substitute of c++ standard condition_variable.
 * we exclude STL and libc++ to reduce the final executable and library file size
 *
 */
#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/thread/mutex.h"
#include "bq_common/platform/platform_misc.h"

namespace bq {
    namespace platform {
        class condition_variable {
        public:
            condition_variable();

            condition_variable(const condition_variable& rhs) = delete;

            condition_variable(condition_variable&& rhs) = delete;

            ~condition_variable();

            condition_variable& operator=(const condition_variable& rhs) = delete;

            condition_variable& operator=(condition_variable&& rhs) = delete;

            void wait(bq::platform::mutex& lock);
            template <typename Predicate>
            void wait(bq::platform::mutex& lock, Predicate stop_waiting);

            // return value: true means triggered, false means timeout
            bool wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms);
            // return value: true means triggered, false means timeout
            template <typename Predicate>
            bool wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms, Predicate stop_waiting);

            void notify_one() noexcept;

            void notify_all() noexcept;

        private:
            struct condition_variable_platform_def* platform_data_;
        };

        template <typename Predicate>
        void condition_variable::wait(bq::platform::mutex& lock, Predicate stop_waiting)
        {
            while (!stop_waiting()) {
                wait(lock);
            }
        }

        template <typename Predicate>
        bool condition_variable::wait_for(bq::platform::mutex& lock, uint64_t wait_time_ms, Predicate stop_waiting)
        {
            uint64_t last_epoch = platform::high_performance_epoch_ms();;
            uint64_t current_epoch = last_epoch;
            while (!stop_waiting()) {
                if (current_epoch >= last_epoch + wait_time_ms) {
                    return false;
                }
                wait_time_ms -= (current_epoch - last_epoch);
                bool result = wait_for(lock, wait_time_ms);
                if (!result) {
                    return false;
                }
                last_epoch = current_epoch;
                current_epoch = platform::high_performance_epoch_ms();
            }
            return true;
        }
    }
}
