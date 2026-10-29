/* Copyright (C) 2025 Tencent.
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
//  log_vars.cpp
//  Created by Yu Cao on 2025/4/11.
//
#include "native_src_bq_log_global_log_vars.h"
#include "native_src_bq_common_bq_common.h"

#if defined(BQ_NAPI)
extern void console_callback_handler(napi_env env, napi_ref js_cb_ref, void* param);
#endif

namespace bq {
    log_global_vars* log_global_var_default_initer_ = &log_global_vars::get();

#if defined(BQ_JAVA)
    void log_global_vars::jni_onload_callback()
    {
        bq::platform::jni_env env_holder;
        JNIEnv* env = env_holder.env;
        auto bq_log_cls = env->FindClass("bq/log");
        if (bq_log_cls) {
            log_global_vars::get().cls_bq_log_ = (jclass)env->NewGlobalRef(bq_log_cls);
            env->DeleteLocalRef(bq_log_cls);
            log_global_vars::get().mid_native_console_callback_ = env->GetStaticMethodID(log_global_vars::get().cls_bq_log_, "native_console_callback", "(JIILjava/lang/String;)V");
            log_global_vars::get().mid_native_console_buffer_fetch_and_remove_callback_ = env->GetStaticMethodID(log_global_vars::get().cls_bq_log_, "native_console_buffer_fetch_and_remove_callback", "(Lbq/log$console_callback_delegate;JIILjava/lang/String;)V");
        }
    }
#endif

    void log_global_vars::partial_destruct()
    {
        bool force_flush_success = false;
        for (uint32_t i = 0; i < 128; ++i) {
            if (log_manager_inst_.try_flush_all()) {
                force_flush_success = true;
                break;
            }
            bq::platform::thread::sleep(1);
        }
        if (!force_flush_success) {
            bq::util::log_device_console(bq::log_level::warning, "Force flush timed out on shutdown, some buffered logs may be lost");
        }
    }

    // A forked child keeps nothing of the parent's runtime: its descriptors are closed (which
    // releases no lock), its mappings lose access, and both collections start over.
    static void on_fork_in_child()
    {
        bq::file_manager::instance().discard_all_after_fork();
        bq::memory_map::protect_all_after_fork();
        auto& vars = log_global_vars::get();
        if (vars.instance_storage_) {
            vars.instance_storage_->close_lock_after_fork();
        }
        common_global_vars::reinit_after_fork();
        log_global_vars::reinit_after_fork();
    }

    log_global_vars::log_global_vars()
#if defined(BQ_NAPI)
        : console_callback_dispatcher_(console_callback_handler)
#endif
    {
#if defined(BQ_JAVA)
        platform::jni_onload_register register_(&log_global_vars::jni_onload_callback);
#endif
        // The collection is constructed again in a forked child, but the handler it inherits is
        // still registered there, so it must not be chained twice.
        static bool fork_handler_registered = false;
        if (!fork_handler_registered) {
            fork_handler_registered = true;
            platform::register_after_fork_child_handler(&on_fork_in_child);
        }
    }

}
