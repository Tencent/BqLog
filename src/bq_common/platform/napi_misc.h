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
#if defined(BQ_NAPI)
#include <node_api.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "bq_common/platform/macros.h"
#include "bq_common/types/array.h"
#include "bq_common/types/string.h"

namespace bq {
    namespace platform {
        // guarded call helpers
#define BQ_NAPI_CALL(env, expr) do { \
            napi_status _status = (expr); \
            if (_status != napi_ok) { \
                const napi_extended_error_info* _info = nullptr; \
                napi_get_last_error_info((env), &_info); \
                char msg_[512]; \
                snprintf(msg_, sizeof(msg_), "%s:%d:%s : NAPI call failed: %s", __FILE__, __LINE__, __FUNCTION__, (_info && _info->error_message) ? _info->error_message : "napi error"); \
                napi_throw_error((env), nullptr, msg_); \
                return nullptr; \
            } \
        } while(0)

#define BQ_NAPI_CALL_VOID(env, expr) do { \
            napi_status _status = (expr); \
            if (_status != napi_ok) { \
                const napi_extended_error_info* _info = nullptr; \
                napi_get_last_error_info((env), &_info); \
                char msg_[512]; \
                snprintf(msg_, sizeof(msg_), "%s:%d:%s : NAPI call failed: %s", __FILE__, __LINE__, __FUNCTION__, (_info && _info->error_message) ? _info->error_message : "napi error"); \
                napi_throw_error((env), nullptr, msg_); \
                return; \
            } \
        } while(0)

        bool napi_call_native_func_in_js_thread(void(*fn)(napi_env, void*), void* param);

        bool napi_call_js_func_in_js_thread(napi_ref js_func_ref, napi_ref this_context_ref, size_t argc, napi_ref* argv_ref);

        struct napi_init_function_register {
            napi_init_function_register(void (*init_callback)(napi_env env, napi_value exports));
        };

        struct __napi_func_register_helper {
        public:
            __napi_func_register_helper(const char* name, napi_callback func);
        };

        napi_value napi_init(napi_env env, napi_value exports);
}

#define BQ_NAPI_DEF(FUNC_NAME, PARAMS_0_TYPE, PARAMS_0_NAME, PARAMS_1_TYPE, PARAMS_1_NAME) \
            static napi_value napi_##FUNC_NAME(napi_env env, napi_callback_info info); \
            bq::platform::__napi_func_register_helper napi_register_helper_##FUNC_NAME("__bq_napi_"#FUNC_NAME, &napi_##FUNC_NAME); \
            static napi_value napi_##FUNC_NAME(napi_env env, napi_callback_info info)
}
#endif
