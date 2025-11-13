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
//  common_vars.cpp
//  Created by Yu Cao on 2025/4/11.
//
#include "bq_common/global/common_vars.h"
#ifdef BQ_WIN
#include "bq_common/platform/win64_includes_begin.h"
#endif

namespace bq {
    static common_global_vars* common_global_var_default_initer_ = &common_global_vars::get();
    static bq::platform::spin_lock_zero_init destructor_mutex_;  //zero initialization
    static bq::array<global_var_destructiable*>* destructible_vars_;   //(nullptr)zero initialization
    static global_vars_destructor global_var_destructor_;


    global_vars_destructor& get_global_var_destructor() {
        return global_var_destructor_;
    }

    void global_vars_destructor::register_destructible_var(global_var_destructiable* var)
    {
        bq::platform::scoped_spin_lock lock(destructor_mutex_);
        if (!destructible_vars_) {
            destructible_vars_ = new bq::array<global_var_destructiable*>();
        }
        destructible_vars_->push_back(var);
    }


    global_vars_destructor::~global_vars_destructor()
    {
        bq::platform::scoped_spin_lock lock(destructor_mutex_);
        if (destructible_vars_) {
            for (size_t i = destructible_vars_->size(); i > 0; --i) {
                (*destructible_vars_)[i - 1]->partial_destruct();
            }
        }
        delete destructible_vars_;
        destructible_vars_ = nullptr;
    }

    common_global_vars::common_global_vars()
    {
#if defined(BQ_ANDROID)
        platform::jni_onload_register register_(&bq::platform::android_jni_onload);
#elif defined(BQ_WIN)
        stack_trace_process_ = GetCurrentProcess();
#endif
    }

    void common_global_vars::partial_destruct()
    {
        /*delete file_manager_inst_;
        file_manager_inst_ = nullptr;*/
    }

}
#ifdef BQ_WIN
#include "bq_common/platform/win64_includes_end.h"
#endif