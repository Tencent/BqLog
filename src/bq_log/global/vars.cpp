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
//
//  vars.cpp
//  Created by Yu Cao on 2025/4/11.
//
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "bq_log/global/vars.h"

namespace bq {
    log_global_vars* log_global_var_default_initer_ = &log_global_vars::get();

#if defined(BQ_JAVA)
    void log_global_vars::jni_onload_callback()
    {
        bq::platform::jni_env env_holder;
        JNIEnv* env = env_holder.env;
        auto bq_log_cls = env->FindClass("bq/log");
        if(bq_log_cls){
            log_global_vars::get().cls_bq_log_ = bq_log_cls;
            log_global_vars::get().mid_native_console_callbck_ = env->GetStaticMethodID(bq_log_cls, "native_console_callbck", "(JIILjava/lang/String;)V");
            log_global_vars::get().mid_native_console_buffer_fetch_and_remove_callbck_ = env->GetStaticMethodID(bq_log_cls, "native_console_buffer_fetch_and_remove_callbck", "(Lbq/log$console_callbck_delegate;JIILjava/lang/String;)V");
        }
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            bq::util::log_device_console(bq::log_level::error, "exception occured while finding class \"bq.log\", make sure Java wrapper is included in your project, or errors will occur when you call java methods");
        }
    }
#endif

    void log_global_vars::init_layout_values()
    {
        uint64_t epoch = bq::platform::high_performance_epoch_ms();
        struct tm local;
        if (!bq::util::get_local_time_by_epoch(epoch, local)) {
            const_cast<int32_t&>(time_zone_str_len) = snprintf(const_cast<char*>(time_zone_str_), sizeof(time_zone_str_), "%s", "UNKNOWN TIMEZONE");
            return;
        }
        time_t local_time = mktime(const_cast<struct tm*>(&local));
        if (local_time == (time_t)(-1)) {
            const_cast<int32_t&>(time_zone_str_len) = snprintf(const_cast<char*>(time_zone_str_), sizeof(time_zone_str_), "%s", "UNKNOWN TIMEZONE");
            return;
        }

        struct tm utc0;
        if (!bq::util::get_gmt_time_by_epoch(epoch, utc0)) {
            const_cast<int32_t&>(time_zone_str_len) = snprintf(const_cast<char*>(time_zone_str_), sizeof(time_zone_str_), "%s", "UNKNOWN TIMEZONE");
            return;
        }
        time_t utc_time = mktime(const_cast<struct tm*>(&utc0));
        if (utc_time == (time_t)(-1)) {
            const_cast<int32_t&>(time_zone_str_len) = snprintf(const_cast<char*>(time_zone_str_), sizeof(time_zone_str_), "%s", "UNKNOWN TIMEZONE");
            return;
        }

        double timezone_offset = difftime(local_time, utc_time);
        int32_t offset_hours = (int32_t)timezone_offset / 3600;
        const_cast<int32_t&>(time_zone_str_len) = snprintf(const_cast<char*>(time_zone_str_), sizeof(time_zone_str_), "UTC%+03d", offset_hours);

        for (uint32_t i = 0; i < 1000; ++i) {
            // 16 may overflow, it only make compilers ignore warning.
            snprintf(const_cast<char*>(digit3_array) + i * 3, 16, "%03d", i);
        }
    }
}