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
#include "bq_common/bq_common.h"

namespace bq {
    struct common_global_vars {
        bq::platform::base_dir_initializer base_dir_init_inst_;
        bq::hash_map<bq::platform::file_node_info, bq::platform::file_open_mode_enum> file_exclusive_cache_;
        bq::platform::mutex file_exclusive_mutex_;
        bq::platform::mutex stack_trace_mutex_;
        bq::array<char> device_console_buffer_ = { '\0' };
        bq::platform::mutex console_mutex_;

#if BQ_JAVA
        bq::array<void (*)()> jni_onload_callbacks_inst_;
#endif
#if BQ_ANDROID
        jobject android_asset_manager_java_instance_ = nullptr;
        AAssetManager* android_asset_manager_inst_ = nullptr;
        bq::string android_id_;
        bq::string android_package_name_;
        bq::string apk_path_;
        bq::platform::spin_lock apk_path_spin_lock_;
#endif
#if BQ_WIN
        HANDLE stack_trace_process_ = GetCurrentProcess();
        bq::platform::atomic<bool> stack_trace_sym_initialized_ = false;
#endif

        file_manager file_manager_inst_;
        property_value null_property_value_;

        common_global_vars()
        {
        }
    };

    class common_global_vars_holder {
    private:
        friend common_global_vars& get_common_global_vars();
    private:
        static common_global_vars* global_vars_ptr_;
    };

    inline common_global_vars& init_common_global_vars()
    {
        static common_global_vars global_vars_;
        return global_vars_;
    }

    bq_forceinline common_global_vars& get_common_global_vars()
    {
        if (!common_global_vars_holder::global_vars_ptr_) { //Zero Initialization
            common_global_vars_holder::global_vars_ptr_ = &init_common_global_vars();
        }
        return *common_global_vars_holder::global_vars_ptr_;
    }
}