#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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
  * \file rwlock.h
  *
  * \author pippocao
  * \date 2025/08/26
  *
  * simple substitute of c++ standard shared_mutex / rwlock.
  *
  */
#include "bq_common/platform/macros.h"

namespace bq {
    namespace platform {

        class rwlock {
        public:
            rwlock();

            rwlock(const rwlock& rhs) = delete;

            rwlock(rwlock&& rhs) = delete;

            ~rwlock();

            rwlock& operator=(const rwlock& rhs) = delete;

            rwlock& operator=(rwlock&& rhs) = delete;

            // shared (reader) side
            void read_lock();

            // return true if lock success
            bool try_read_lock();

            // return true if successed to unlock the shared lock
            bool read_unlock();

            // exclusive (writer) side
            void write_lock();

            // return true if lock success
            bool try_write_lock();

            // return true if successed to unlock the exclusive lock
            bool write_unlock();

        private:
            void* get_platform_handle();

        private:
            struct rwlock_platform_def* platform_data_;
        };
    }
}