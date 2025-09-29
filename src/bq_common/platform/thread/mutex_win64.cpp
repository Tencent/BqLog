﻿/*
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
#ifdef BQ_WIN
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
namespace bq {
    namespace platform {
        struct mutex_platform_def {
            CRITICAL_SECTION cs_;
            bq::platform::atomic<thread::thread_id> owner_thread_id_; // windows platform mutex is always Reentrant lock, we have to simulate the non-recursive mutex by ourself.
                                                                      // it's working because no thread id is 0 on windows.
                                                                      // refer to:https://docs.microsoft.com/en-us/windows/win32/procthread/thread-handles-and-identifiers
            mutex_platform_def()
                : cs_(CRITICAL_SECTION())
                , owner_thread_id_(0)
            {
            }
        };

        mutex::mutex()
            : reentrant_(true)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            if (platform_data_) {
                new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
                platform_data_->owner_thread_id_.store_seq_cst(0);
                InitializeCriticalSection(&platform_data_->cs_);
            } else {
                assert(false && "not enough memory for mutex");
            }
        }

        mutex::mutex(bool reentrant /* = true */)
            : reentrant_(reentrant)
        {
            platform_data_ = (mutex_platform_def*)malloc(sizeof(mutex_platform_def));
            if (platform_data_) {
                new (platform_data_, bq::enum_new_dummy::dummy) mutex_platform_def();
                platform_data_->owner_thread_id_.store_seq_cst(0);
                InitializeCriticalSection(&platform_data_->cs_);
            } else {
                assert(false && "not enough memory for mutex");
            }
        }

        mutex::~mutex()
        {
            DeleteCriticalSection(&platform_data_->cs_);
            free(platform_data_);
            platform_data_ = nullptr;
        }

        void mutex::lock()
        {
            EnterCriticalSection(&platform_data_->cs_);
            if (!reentrant_) {
                thread::thread_id current_thread_id = thread::get_current_thread_id();
                if (platform_data_->owner_thread_id_.exchange_relaxed(current_thread_id) == current_thread_id) {
                    bq::util::log_device_console(log_level::error, "%s : %d : you're try to reenter a non-recursive mutex", __FILE__, __LINE__);
                    // assert(false && "mutex recursive lock");
                }
            }
        }

        bool mutex::try_lock()
        {
            if (TryEnterCriticalSection(&platform_data_->cs_)) {
                if (!reentrant_) {
                    thread::thread_id current_thread_id = thread::get_current_thread_id();
                    if (platform_data_->owner_thread_id_.exchange_relaxed(current_thread_id) == current_thread_id) {
                        bq::util::log_device_console(log_level::error, "%s : %d : you're try to reenter a non-recursive mutex", __FILE__, __LINE__);
                        // assert(false && "mutex recursive lock");
                    }
                }
                return true;
            } else {
                return false;
            }
        }

        bool mutex::unlock()
        {
            platform_data_->owner_thread_id_.store(0, memory_order::release);
            LeaveCriticalSection(&platform_data_->cs_);
            return true;
        }

        void* mutex::get_platform_handle()
        {
            return (void*)(&platform_data_->cs_);
        }
    }
}
#endif
