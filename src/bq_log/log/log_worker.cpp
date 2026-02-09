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
#include "bq_log/log/log_worker.h"
#include "bq_log/bq_log.h"
#include "bq_log/global//log_vars.h"
#include "bq_log/log/log_manager.h"
#ifdef BQ_POSIX
#ifndef BQ_PS
#include <signal.h>
#endif
#endif
namespace bq {
    static bq::platform::atomic<int32_t> log_worker_name_seq = 0;
    BQ_TLS_NON_POD(log_worker_watch_dog, tls_log_worker_watch_dog_)

    log_worker::log_worker()
        : manager_(nullptr)
        , log_target_(nullptr)
        , thread_mode_(log_thread_mode::sync)
        , mutex_(true)
        , wait_flag_(false)
        , awake_flag_(false)
    {
    }

    log_worker::~log_worker()
    {
    }

    void log_worker::init(log_thread_mode thead_mode, log_imp* log_target)
    {
        thread_mode_ = thead_mode;
        if (thread_mode_ == log_thread_mode::independent) {
            log_target_ = log_target;
        }
        if (thread_mode_ == log_thread_mode::async) {
            set_thread_name("BqLogW_Pub");
        } else {
            int32_t name_seq = log_worker_name_seq.fetch_add_relaxed(1);
            char name_tmp[16];
            snprintf(name_tmp, sizeof(name_tmp), "BqLogW_%d", name_seq);
            set_thread_name(name_tmp);
        }
    }

    void log_worker::awake_and_wait_begin(log_imp* log_target_for_pub_worker /*=nullptr*/)
    {
        while (wait_flag_.load(platform::memory_order::acquire) != false) {
            // wait previous awake request finish.
            bq::platform::thread::yield();
            bq::platform::thread::sleep(0);
        }
        if (thread_mode_ == log_thread_mode::async) {
            log_target_ = log_target_for_pub_worker;
        }
        wait_flag_.store(true, platform::memory_order::release);
        trigger_.notify_all();
    }

    void log_worker::awake_and_wait_join()
    {
        while (wait_flag_.load(platform::memory_order::acquire) != false) {
            bq::platform::thread::yield();
            bq::platform::thread::sleep(0);
        }
    }

    void log_worker::run()
    {
        assert(thread_mode_ != log_thread_mode::sync && "log_worker started without init");
        tls_log_worker_watch_dog_.get().thread_id_ = bq::platform::thread::get_current_thread_id();
        tls_log_worker_watch_dog_.get().worker_ptr_ = this;
#ifdef BQ_POSIX
        // we need flush ring_buffer in signal handler.
        // but handler can not be called in worker thread.(the flush operation is not re-entrant)
        // so we have to block signals for this thread.
        sigset_t forbidden_sigset;
        sigfillset(&forbidden_sigset);
        pthread_sigmask(SIG_BLOCK, &forbidden_sigset, NULL);
#endif
        manager_ = &log_manager::instance();
        while (true) {
            // force flush process
            if (wait_flag_.load(platform::memory_order::acquire)) {
                // make sure force flush is fully processed.
                manager_->process_by_worker(log_target_, true);
                if (thread_mode_ == log_thread_mode::async) {
                    log_target_ = nullptr;
                }
                wait_flag_.store(false, platform::memory_order::release);
            }
            // normal process
            else {
                manager_->process_by_worker((thread_mode_ == log_thread_mode::async) ? nullptr : log_target_, false);
            }
            if (is_cancelled()) {
                break;
            }
            mutex_.lock();
            awake_flag_.store_relaxed(true);
            trigger_.wait_for(mutex_, process_interval_ms);
            awake_flag_.store_relaxed(false);
            mutex_.unlock();
        }
    }

    log_worker_watch_dog::~log_worker_watch_dog()
    {
        if (bq::platform::thread::get_current_thread_id() != thread_id_) {
            return;
        }
        // Reliable in most cases, except log_manager is destructing in other thread.
        log_manager::instance().try_restart_worker(worker_ptr_);
    }
}
