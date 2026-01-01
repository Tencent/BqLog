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
#if defined(BQ_X86)
#ifndef BQ_MSVC
#include <cpuid.h>
#endif
#endif
#if defined(BQ_APPLE)
#include <sys/sysctl.h>
#elif defined(BQ_LINUX)
#include <sys/auxv.h>                                                                                                                                                                 │
#include <asm/hwcap.h>
#endif

namespace bq {
    static common_global_vars* common_global_var_default_initer_ = &common_global_vars::get();
    static bq::platform::spin_lock_zero_init destructor_mutex_;  //zero initialization
    static bq::array<global_var_destructiable*>* destructible_vars_;   //(nullptr)zero initialization
    static global_vars_destructor global_var_destructor_;


    static bool check_avx2_support() {
        bool result = false;
#if defined(BQ_X86)
        int32_t regs[4];
        int32_t id;
#if defined(BQ_MSVC)
        __cpuid(regs, 0);
        id = regs[0];
#else
        id = static_cast<int32_t>(__get_cpuid_max(0, (uint32_t*)regs));
#endif
        if (id >= 7) {
#if defined(BQ_MSVC)
            __cpuidex(regs, 7, 0);
#else
            __cpuid_count(7, 0, regs[0], regs[1], regs[2], regs[3]);
#endif
            // EBX bit 5 is AVX2
            result = (regs[1] & (1 << 5)) != 0;
        }
#endif
        return result;
    }

    static bool check_crc32_support() {
        bool result = false;
#if defined(BQ_X86)
        int32_t regs[4];
#if defined(BQ_MSVC)
        __cpuid(regs, 1);
#else
        __cpuid(1, regs[0], regs[1], regs[2], regs[3]);
#endif
        // ECX (regs[2]) bit 20 is SSE4.2
        result = (regs[2] & (1 << 20)) != 0;

#elif defined(BQ_ARM)
#if defined(BQ_WIN)
        result = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE) != 0;
#elif defined(BQ_APPLE)
        int32_t val = 0;
        size_t len = sizeof(val);
        if (sysctlbyname("hw.optional.armv8_crc32", &val, &len, nullptr, 0) == 0) {
            result = (val != 0);
        }
#elif defined(BQ_LINUX) || defined(BQ_ANDROID)
        unsigned long aux = getauxval(AT_HWCAP);
        result = (aux & HWCAP_CRC32) != 0;
#endif
#endif
        return result;
    }

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
        avx2_support_ = check_avx2_support();
        crc32_supported_ = check_crc32_support();
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