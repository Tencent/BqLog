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
#include "bq_log/global/log_vars.h"
#include "bq_common/bq_common.h"
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
            log_global_vars::get().mid_native_console_callback_ = env->GetStaticMethodID(bq_log_cls, "native_console_callback", "(JIILjava/lang/String;)V");
            log_global_vars::get().mid_native_console_buffer_fetch_and_remove_callback_ = env->GetStaticMethodID(bq_log_cls, "native_console_buffer_fetch_and_remove_callback", "(Lbq/log$console_callback_delegate;JIILjava/lang/String;)V");
        }
        
    }
#endif

    void log_global_vars::init_layout_values()
    {
        for (uint32_t i = 0; i < 1000; ++i) {
            // 16 may overflow, it only make compilers ignore warning.
            snprintf(const_cast<char*>(digit3_array) + i * 3, 16, "%03d", i);
        }
    }

    void log_global_vars::partial_destruct()
    {
        delete log_manager_inst_;
        log_manager_inst_ = nullptr;
        delete appender_decoder_manager_inst_;
        appender_decoder_manager_inst_ = nullptr;
        delete console_static_misc_;
        console_static_misc_ = nullptr;
    }


    log_global_vars::log_global_vars()
    {
#if defined(BQ_JAVA)
        platform::jni_onload_register register_(&log_global_vars::jni_onload_callback);
#endif
        init_layout_values();
    }

}