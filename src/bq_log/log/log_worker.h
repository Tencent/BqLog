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
#include "bq_common/bq_common.h"
#include "bq_log/log/log_types.h"

#ifdef BQ_PS
// TODO
#else
#include <signal.h>
#endif
namespace bq {
    class log_manager;
    class log_imp;
    class log_worker : public bq::platform::thread {
    public:
        static constexpr uint64_t process_interval_ms = 66;

    private:
        log_manager* manager_;
        log_imp* log_target_;
        log_thread_mode thread_mode_;
        platform::condition_variable trigger_;
        platform::mutex mutex_;
        platform::atomic<bool> wait_flag_;
        platform::atomic<bool> awake_flag_;

    public:
        log_worker();
        ~log_worker();

        void init(log_thread_mode thread_mode, log_imp* log_target);

        bq_forceinline bool is_public_worker() const
        {
            return thread_mode_ == log_thread_mode::async;
        }

        bq_forceinline void awake()
        {
            bool origin_value = awake_flag_.exchange_relaxed(false);
            if (origin_value) {
                mutex_.lock();
                trigger_.notify_all();
                mutex_.unlock();
            }
        }

        // These two function must be called in pair
        void awake_and_wait_begin(log_imp* log_target_for_pub_worker = nullptr);
        void awake_and_wait_join();

    protected:
        virtual void run() override;
    };
}
