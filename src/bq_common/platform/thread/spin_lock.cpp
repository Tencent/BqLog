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
 * \file spin_lock.h
 *
 * \author pippocao
 * \date 2023/08/21
 *
 *
 */
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {

        void mcs_spin_lock::yield()
        {
            bq::platform::thread::cpu_relax();
        }

        void spin_lock::yield()
        {
            bq::platform::thread::cpu_relax();
        }

        void spin_lock_rw_crazy::yield()
        {
            bq::platform::thread::cpu_relax();
        }

        void spin_lock_rw_crazy::wait()
        {
            bq::platform::thread::sleep(0);
        }

        uint64_t spin_lock_rw_crazy::get_epoch()
        {
            return bq::platform::high_performance_epoch_ms();
        }
    }
}
