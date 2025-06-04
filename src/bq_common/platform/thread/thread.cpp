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
#include "bq_common/bq_common.h"
bool bq::platform::thread::cancel()
{
    enum_thread_status running_status = enum_thread_status::running;
    if (!status_.compare_exchange_strong(running_status, enum_thread_status::pendding_cancel)) {
        bq::util::log_device_console(log_level::warning, "you're trying to cancel a thread which is not in running status, status enum:%" PRId32 "", static_cast<int32_t>(status_.load()));
        return false;
    }
    return true;
}

bool bq::platform::thread::is_cancelled()
{
    return status_.load(memory_order::acquire) == enum_thread_status::pendding_cancel;
}
