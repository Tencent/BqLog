#pragma once
/*
 * Copyright (C) 2024 Tencent.
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
 * \file mutex.h
 *
 * \author pippocao
 * \date 2022/09/16
 *
 * simple substitute of c++ standard mutex.
 * we exclude STL and libc++ to reduce the final executable and library file size
 *
 */
#include "bq_common/platform/macros.h"

namespace bq {
    namespace platform {
        class mutex {
            friend class condition_variable;

        public:
            mutex();

            mutex(bool reentrant);

            mutex(const mutex& rhs) = delete;

            mutex(mutex&& rhs) = delete;

            ~mutex();

            mutex& operator=(const mutex& rhs) = delete;

            mutex& operator=(mutex&& rhs) = delete;

            void lock();

            // return true if lock success
            bool try_lock();

            // return true if caller thread is owner of mutex and successed to unlock the mutex
            bool unlock();

        private:
            void* get_platform_handle();

        private:
            bool reentrant_;
            struct mutex_platform_def* platform_data_;
        };

        class scoped_mutex {
        private:
            mutex& mutex_;

        public:
            scoped_mutex() = delete;

            explicit scoped_mutex(mutex& mutex)
                : mutex_(mutex)
            {
                mutex_.lock();
            }

            ~scoped_mutex()
            {
                mutex_.unlock();
            }
        };
    }
}
