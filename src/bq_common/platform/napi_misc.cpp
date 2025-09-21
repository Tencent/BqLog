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
#include "bq_common/bq_common.h"

#if defined(BQ_NAPI)

#include <sys/types.h>
#if defined(BQ_HMOS)
//TODO harmony OS headers
#endif

namespace bq {
    BQ_TLS_NON_POD(bq::array<napi_value>, tls_argv_array_);
    namespace platform {
        //Only single env is supported for now
        static napi_threadsafe_function tsfn_dispatcher_for_native_func_;
        static napi_threadsafe_function tsfn_dispatcher_for_js_func_;

        static bq::array<napi_property_descriptor> napi_registered_init_functions_;
        static bq::platform::mutex napi_init_function_registration_mutex_;

        struct native_func_call_ctx {
            void(*fn)(napi_env, void*);
            void* param;
        };
        bool napi_call_native_func_in_js_thread(void(*fn)(napi_env, void*), void* param)
        {
            native_func_call_ctx* ctx = new native_func_call_ctx();
            ctx->fn = fn;
            ctx->param = param;
            napi_status s = napi_call_threadsafe_function(tsfn_dispatcher_for_native_func_, ctx, napi_tsfn_blocking);
            return s == napi_ok;
        }

        struct js_func_call_ctx {
            size_t argc;
            napi_ref* argv_ref;
            napi_ref js_func_ref;
            napi_ref this_context_ref;
        };

        bool napi_call_js_func_in_js_thread(napi_ref js_func_ref, napi_ref this_context_ref, size_t argc, napi_ref* argv_ref)
        {
            js_func_call_ctx* ctx = new js_func_call_ctx();
            ctx->argc = argc;
            ctx->argv_ref = argv_ref;
            ctx->js_func_ref = js_func_ref;
            ctx->this_context_ref = this_context_ref;
            napi_status s = napi_call_threadsafe_function(tsfn_dispatcher_for_js_func_, ctx, napi_tsfn_blocking);
            return s == napi_ok;
        }

        napi_init_function_register::napi_init_function_register(void (*init_callback)(napi_env env, napi_value exports))
        {
            common_global_vars::get().napi_init_callbacks_inst_.push_back(init_callback);
        }

        __napi_func_register_helper::__napi_func_register_helper(const char* name, napi_callback func)
        {
            bq::platform::scoped_mutex lock(napi_init_function_registration_mutex_);
            napi_registered_init_functions_.push_back(napi_property_descriptor{name, 0, func, 0, 0, 0, napi_default, 0});
        }

        static void dispatcher_call_native(napi_env env, napi_value /*js_cb*/, void* /*ctx*/, void* data) {
            native_func_call_ctx* ctx = reinterpret_cast<native_func_call_ctx*>(data);
            if (ctx){
                ctx->fn(env, ctx->param);
                delete ctx;
            }
        }

        static void dispatcher_call_js(napi_env env, napi_value /*js_cb*/, void* /*ctx*/, void* data) {
            js_func_call_ctx* ctx = reinterpret_cast<js_func_call_ctx*>(data);
            if (ctx && ctx->js_func_ref) {
                napi_value this_context = nullptr;
                if (!ctx->this_context_ref) {
                    napi_get_global(env, &this_context);
                }
                napi_value js_func = nullptr;
                napi_get_reference_value(env, ctx->js_func_ref, &js_func);
                auto& tls_arr = tls_argv_array_.get();
                tls_arr.clear();
                for (size_t i = 0; i < ctx->argc; i++) {
                    napi_value argv = nullptr;
                    if (ctx->argv_ref && ctx->argv_ref[i]) {
                        napi_get_reference_value(env, ctx->argv_ref[i], &argv);
                    }
                    tls_arr.push_back(argv);
                }
                napi_value result = nullptr;
                napi_call_function(env, this_context, js_func, ctx->argc, tls_arr.begin(), &result);
                delete ctx;
            }
        }

        static void cleanup_hook(void* p) {
            (void)p;
            napi_release_threadsafe_function(tsfn_dispatcher_for_native_func_, napi_tsfn_abort);
            napi_release_threadsafe_function(tsfn_dispatcher_for_js_func_, napi_tsfn_abort);
        }

        // Module initializer 
        napi_value napi_init(napi_env env, napi_value exports)
        {
            napi_create_threadsafe_function(
                env, nullptr, nullptr, nullptr,
                1024, 1, nullptr, nullptr, nullptr,
                dispatcher_call_native, &tsfn_dispatcher_for_native_func_
            );
            napi_create_threadsafe_function(
                env, nullptr, nullptr, nullptr,
                1024, 1, nullptr, nullptr, nullptr,
                dispatcher_call_js, &tsfn_dispatcher_for_js_func_
            );
            napi_add_env_cleanup_hook(env, cleanup_hook, nullptr);

            for (auto callback : common_global_vars::get().napi_init_callbacks_inst_) {
                callback(env, exports);
            }

            bq::platform::scoped_mutex lock(napi_init_function_registration_mutex_);
            if (napi_registered_init_functions_.size() > 0) {
                napi_define_properties(env, exports, napi_registered_init_functions_.size(), napi_registered_init_functions_.begin());
            }
            return exports;
        }

#if defined(BQ_DYNAMIC_LIB)
        NAPI_MODULE(bqlog, napi_init)
#endif
    }
}
#endif
