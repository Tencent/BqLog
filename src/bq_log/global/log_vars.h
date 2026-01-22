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
//  log_vars.h
//  Created by Yu Cao on 2025/4/11.
//  Manages the lifecycle and initialization order of global variables uniformly,
//  used to avoid the Static Initialization Order Fiasco.
//  In theory, all global variables with constructors or destructors
//  should be managed here.
#include "bq_common/bq_common_public_include.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/decoder/appender_decoder_manager.h"
#include "bq_log/types/buffer/miso_ring_buffer.h"

namespace bq {

    // To ensure these variables can be used at any time without being destroyed, we accept a small amount of memory leakage.
    struct log_global_vars : public global_vars_base<log_global_vars, common_global_vars> {
        friend struct scoped_thread_check_disable;
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
        const char digit3_array[3000 + 16] = {};
        const char* log_recover_start_str_ = "/************************************* BQLOG RECOVER START *************************************/";
        const char* log_recover_end_str_ = "/************************************* BQLOG RECOVER END *************************************/";
#if defined(BQ_JAVA)
        jclass cls_bq_log_ = nullptr;
        jmethodID mid_native_console_callback_ = nullptr;
        jmethodID mid_native_console_buffer_fetch_and_remove_callback_ = nullptr;
#endif
#if defined(BQ_NAPI)
        bq::miso_ring_buffer console_msg_buffer_{ bq::log_buffer_config{ "napi_console_cb_msg", bq::array<bq::string>{}, 1024 * 8, false, bq::log_memory_policy::block_when_full, 0 } };
#endif
        appender_console::console_static_misc console_static_misc_;
        appender_decoder_manager appender_decoder_manager_inst_;
        log_manager log_manager_inst_;
    private:
#if defined(BQ_LOG_BUFFER_DEBUG)
        bq::platform::atomic<int32_t> thread_check_enable_counter_ = 0;
#endif

    private:
#if defined(BQ_JAVA)
        static void jni_onload_callback();
#endif
        void init_layout_values() const;

    protected:
        virtual void partial_destruct() override;

    public:
        log_global_vars();

        virtual ~log_global_vars() override { }

        bq_forceinline bool is_thread_check_enabled() const
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            return thread_check_enable_counter_.load_relaxed() == 0;
#else
            return false;
#endif
        }
    };

    struct scoped_thread_check_disable {
        scoped_thread_check_disable()
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            bq::log_global_vars::get().thread_check_enable_counter_.fetch_add_relaxed(1);
#endif
        }
        ~scoped_thread_check_disable()
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            bq::log_global_vars::get().thread_check_enable_counter_.fetch_sub_relaxed(1);
#endif
        }
    };
}
