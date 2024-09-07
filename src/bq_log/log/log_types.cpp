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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "bq_common/bq_common.h"
#include "bq_log/log/log_types.h"

namespace bq {
    void thread_info::init()
    {
        if (is_inited()) {
            return;
        }
#ifndef BQ_WIN
        magic_number_ = MAGIC_NUM_VALUE;
#endif
        thread_id_ = bq::platform::thread::get_current_thread_id();
        bq::string thread_name_tmp = bq::platform::thread::get_current_thread_name();
        thread_name_len_ = (uint8_t)bq::min_value((size_t)MAX_THREAD_NAME_LEN, thread_name_tmp.size());
        if (thread_name_len_ > 0) {
            memcpy(thread_name_, thread_name_tmp.c_str(), thread_name_len_);
        }
    }
}
