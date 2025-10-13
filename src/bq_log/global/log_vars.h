#pragma once
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
//  vars.h
//  Created by Yu Cao on 2025/4/11.
//  Manages the lifecycle and initialization order of global variables uniformly,
//  used to avoid the Static Initialization Order Fiasco.
//  In theory, all global variables with constructors or destructors
//  should be managed here.
#include "bq_common/bq_common_public_include.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/decoder/appender_decoder_manager.h"

namespace bq {

    // To ensure these variables can be used at any time without being destroyed, we accept a small amount of memory leakage.
    struct log_global_vars : public global_vars_base<log_global_vars, common_global_vars> {
        const char* log_appender_type_names_[static_cast<int32_t>(appender_base::appender_type::type_count)] = {
            "console",
            "text_file",
            "raw_file",
            "compressed_file"
        };
        const char log_level_str_[6][3] = {
            { '[', 'V', ']' }, // VERBOSE
            { '[', 'D', ']' }, // DEBUG
            { '[', 'I', ']' }, // INFO
            { '[', 'W', ']' }, // WARN
            { '[', 'E', ']' }, // ERROR
            { '[', 'F', ']' } // FATAL
        };
        const char time_zone_str_[32] = { 0 };
        const int32_t time_zone_str_len = 0;
        const char* utc_time_zone_str_ = "UTC0 ";
        const int32_t utc_time_zone_str_len = (int32_t)strlen(utc_time_zone_str_);
        const char digit3_array[3000 + 16] = {};
#if defined(BQ_JAVA)
        jclass cls_bq_log_ = nullptr;
        jmethodID mid_native_console_callback_ = nullptr;
        jmethodID mid_native_console_buffer_fetch_and_remove_callback_ = nullptr;
#endif
        appender_console::console_static_misc* console_static_misc_ = new appender_console::console_static_misc();
        appender_decoder_manager* appender_decoder_manager_inst_ = new appender_decoder_manager();
        log_manager* log_manager_inst_ = new log_manager();

    private:
#if defined(BQ_JAVA)
        static void jni_onload_callback();
#endif
        void init_layout_values();

    protected:
        virtual void partial_destruct() override;

    public:
        log_global_vars();

        virtual ~log_global_vars(){}
    };
}