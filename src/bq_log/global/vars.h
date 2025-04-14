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
//
//  vars.h
//  Created by Yu Cao on 2025/4/11.
//  Manages the lifecycle and initialization order of global variables uniformly,
//  used to avoid the Static Initialization Order Fiasco.
//  In theory, all global variables with constructors or destructors
//  should be managed here.
#include <string.h>
#include <inttypes.h>
#include "bq_common/bq_common.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/decoder/appender_decoder_manager.h"

namespace bq {
    struct log_global_vars {
        const bq::string log_appender_type_names_[static_cast<int32_t>(appender_base::appender_type::type_count)] = {
                                                "console",
                                                "text_file",
                                                "raw_file",
                                                "compressed_file" };
        const bq::string log_level_str_[6] = {
                                                "[V]", // VERBOSE
                                                "[D]", // DEBUG
                                                "[I]", // INFO
                                                "[W]", // WARN
                                                "[E]", // ERROR
                                                "[F]" // FATAL
                                            };
        const char time_zone_str_[32] = { 0 };
        const int32_t time_zone_str_len = 0;
        const char* utc_time_zone_str_ = "UTC0 ";
        const int32_t utc_time_zone_str_len = (int32_t)strlen(utc_time_zone_str_);
        const char digit3_array[3000 + 16] = {};
        log_manager log_manager_inst_;
        appender_console::console_static_misc console_static_misc_;
        appender_decoder_manager appender_decoder_manager_inst_;
#if BQ_JAVA
        jclass cls_bq_log_ = nullptr;
        jmethodID mid_native_console_callbck_ = nullptr;
        jmethodID mid_native_console_buffer_fetch_and_remove_callbck_ = nullptr;
#endif

    private:
#if BQ_JAVA
        static void jni_onload_callback();
#endif
    public:
        log_global_vars()
        {
#if BQ_JAVA
            platform::jni_onload_register register_(&log_global_vars::jni_onload_callback);
#endif
        }
    };

    class log_global_vars_holder {
    private:
        friend log_global_vars& get_log_global_vars();
    private:
        static log_global_vars* global_vars_ptr_;
    };

    inline log_global_vars& init_log_global_vars()
    {
        init_common_global_vars();
        static log_global_vars global_vars_;
        return global_vars_;
    }

    bq_forceinline log_global_vars& get_log_global_vars()
    {
        if (!log_global_vars_holder::global_vars_ptr_) { //Zero Initialization
            log_global_vars_holder::global_vars_ptr_ = &init_log_global_vars();
        }
        return *log_global_vars_holder::global_vars_ptr_;
    }
}