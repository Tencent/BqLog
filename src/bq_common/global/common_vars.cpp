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
#include "bq_common/global/common_vars.h"
#ifdef BQ_WIN
#include "bq_common/platform/win64_includes_begin.h"
#endif

namespace bq {
    static common_global_vars* common_global_var_default_initer_ = &common_global_vars::get();

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
        delete file_manager_inst_;
        file_manager_inst_ = nullptr;
    }

}
#ifdef BQ_WIN
#include "bq_common/platform/win64_includes_end.h"
#endif