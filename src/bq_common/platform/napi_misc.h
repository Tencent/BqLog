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

#include "bq_common/bq_common_public_include.h"
#if defined(BQ_NAPI)
#include <math.h>
#include <node_api.h>

namespace bq {
    template<typename T, size_t MAX_RESERVE_SIZE>
    struct napi_str_result {
    private:
        T& str_;
    public:
        bq_forceinline const T& str() const { return str_; }
        napi_str_result(T& str) : str_(str) {
        }
        ~napi_str_result() {
            str_.clear();
            if (str_.capacity() > MAX_RESERVE_SIZE) {
                str_.set_capacity(MAX_RESERVE_SIZE - 1, true);
            }
        }
    };

    // ----------------------------- helpers -----------------------------

    inline napi_value make_napi_undefined(napi_env env) {
        napi_value v; napi_get_undefined(env, &v); return v;
    }
    inline napi_value make_napi_bool(napi_env env, bool b) {
        napi_value v; napi_get_boolean(env, b, &v); return v;
    }
    inline napi_value make_napi_i32(napi_env env, int32_t x) {
        napi_value v; napi_create_int32(env, x, &v); return v;
    }
    inline napi_value make_napi_u32(napi_env env, uint32_t x) {
        napi_value v; napi_create_uint32(env, x, &v); return v;
    }
    inline napi_value make_napi_u64(napi_env env, uint64_t x) {
        napi_value v; napi_create_bigint_uint64(env, x, &v); return v;
    }
    inline napi_value make_napi_str_utf8(napi_env env, const char* s) {
        napi_value v; napi_create_string_utf8(env, s ? s : "", NAPI_AUTO_LENGTH, &v); return v;
    }

    inline bool get_napi_bool(napi_env env, napi_value v) {
        bool b = false; napi_get_value_bool(env, v, &b); return b;
    }
    inline int32_t get_napi_i32(napi_env env, napi_value v) {
        int32_t x = 0; napi_get_value_int32(env, v, &x); return x;
    }
    inline uint32_t get_napi_u32(napi_env env, napi_value v) {
        uint32_t x = 0; napi_get_value_uint32(env, v, &x); return x;
    }

    inline bool js_is_integer(double d) {
        return isfinite(d) && d == trunc(d);
    }

    inline bool js_is_safe_integer(double d) {
        if (!js_is_integer(d)) {
            return false;
        }
        // |d| <= 2^53 - 1
        return fabs(d) <= 9007199254740991.0; // 9007199254740991 = 2^53 - 1
    }

    bq::napi_str_result<bq::string, 256> read_utf8_str_tls(napi_env env, napi_value v, size_t u8string_bytes);

    uint64_t get_u64_from_bigint(napi_env env, napi_value v);

    bq::string dup_string_from_napi(napi_env env, napi_value v);

    char* dup_cstr_from_napi(napi_env env, napi_value v);

    void free_cstr_from_napi(char* p);

    namespace platform {
        // guarded call helpers
#define BQ_NAPI_CALL(env, return_value, expr) do { \
            napi_status _status = (expr); \
            if (_status != napi_ok) { \
                const napi_extended_error_info* _info = nullptr; \
                napi_get_last_error_info((env), &_info); \
                char msg_[256]; \
                snprintf(msg_, sizeof(msg_), "%s:%d:%s : NAPI call failed: %s", __FILE__, __LINE__, __FUNCTION__, (_info && _info->error_message) ? _info->error_message : "napi error"); \
                napi_throw_error((env), nullptr, msg_); \
                return return_value; \
            } \
        } while(0)

#define BQ_NAPI_CALL_VOID(env, expr) do { \
            napi_status _status = (expr); \
            if (_status != napi_ok) { \
                const napi_extended_error_info* _info = nullptr; \
                napi_get_last_error_info((env), &_info); \
                char msg_[256]; \
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
