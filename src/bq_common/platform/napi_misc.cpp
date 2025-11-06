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

#include "bq_common/platform/napi_misc.h"
#if defined(BQ_NAPI)
#include <sys/types.h>
#include "bq_common/bq_common.h"

namespace bq {
    BQ_TLS_NON_POD(bq::array<napi_value>, tls_argv_array_)
    BQ_TLS_NON_POD(bq::string, _tls_utf8_str)

    // ----------------------------- helpers -----------------------------
    bq::napi_str_result<bq::string, 256> read_utf8_str_tls(napi_env env, napi_value v, size_t u8string_bytes) {
        auto& tls_str = bq::_tls_utf8_str.get();
        if (!v) {
            tls_str.clear();
            return bq::napi_str_result<bq::string, 256>(tls_str);
        }
        if (tls_str.is_empty()) {
            tls_str.fill_uninitialized(10);
        }
        size_t len8 = u8string_bytes;
        if (u8string_bytes == SIZE_MAX) {
            napi_get_value_string_utf16(env, v, nullptr, 0, &len8);
        }
        tls_str.clear();
        if (len8 > 0) {
            tls_str.fill_uninitialized(len8);
            size_t got8 = 0;
            napi_get_value_string_utf8(env, v, tls_str.begin(), len8 + 1, &got8);
            assert(got8 == len8);
        }
        return bq::napi_str_result<bq::string, 256>(tls_str);
    }

    uint64_t get_u64_from_bigint(napi_env env, napi_value v) {
        bool lossless = false;
        uint64_t out = 0;
        napi_valuetype t = napi_undefined;
        if (napi_typeof(env, v, &t) == napi_ok) {
            if (t == napi_bigint) {
                napi_get_value_bigint_uint64(env, v, &out, &lossless);
                return out;
            }
            else if (t == napi_number) {
                // best-effort for JS number inputs (may lose precision > 2^53)
                double d = 0;
                napi_get_value_double(env, v, &d);
                if (d < 0) d = 0;
                out = (uint64_t)(d);
                return out;
            }
            else if (t == napi_string) {
                // parse decimal string (simple, without locale)
                size_t len = 0;
                napi_get_value_string_utf8(env, v, nullptr, 0, &len);
                char* buf = (char*)malloc(len + 1);
                if (!buf) return 0;
                size_t got = 0;
                napi_get_value_string_utf8(env, v, buf, len + 1, &got);
                uint64_t acc = 0;
                for (size_t i = 0; i < got; ++i) {
                    char c = buf[i];
                    if (c >= '0' && c <= '9') {
                        acc = acc * 10 + (uint64_t)(c - '0');
                    }
                    else {
                        break;
                    }
                }
                free(buf);
                return acc;
            }
        }
        return out;
    }

    bq::string dup_string_from_napi(napi_env env, napi_value v) {
        size_t len = 0;
        napi_get_value_string_utf8(env, v, NULL, 0, &len);
        bq::string out;
        if (len > 0) {
            out.fill_uninitialized(len);
            size_t got = 0;
            napi_get_value_string_utf8(env, v, (char*)out.begin(), len + 1, &got);
            assert(got == len);
        }
        return out;
    }

    char* dup_cstr_from_napi(napi_env env, napi_value v) {
        size_t len = 0;
        napi_get_value_string_utf8(env, v, NULL, 0, &len);
        char* out = (char*)malloc(len + 1);
        if (!out) return NULL;
        size_t got = 0;
        napi_get_value_string_utf8(env, v, out, len + 1, &got);
        out[got] = '\0';
        return out;
    }

    void free_cstr_from_napi(char* p) {
        if (p) free(p);
    }


    namespace platform {
        //Only single env is supported for now
        static napi_threadsafe_function tsfn_dispatcher_for_native_func_ = nullptr;
        static napi_threadsafe_function tsfn_dispatcher_for_js_func_ = nullptr;

        static spin_lock_zero_init napi_init_mutex_;
        static bool is_napi_initialized_ = false;
        static bq::array<napi_property_descriptor>* napi_registered_init_functions_ = nullptr;

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
            bq::platform::scoped_mutex lock(common_global_vars::get().napi_init_mutex_);
            common_global_vars::get().napi_init_callbacks_inst_.push_back(init_callback);
        }

        __napi_func_register_helper::__napi_func_register_helper(const char* name, napi_callback func)
        {
            bq::platform::scoped_spin_lock lock(napi_init_mutex_);
            if (is_napi_initialized_) {
                //already initialized, ignore new registrations
                return;
            }
            if (!napi_registered_init_functions_) {
                napi_registered_init_functions_ = new bq::array<napi_property_descriptor>();
            }
            napi_registered_init_functions_->push_back(napi_property_descriptor{name, 0, func, 0, 0, 0, napi_default, 0});
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
                    BQ_NAPI_CALL_VOID(env, napi_get_global(env, &this_context));
                }
                napi_value js_func = nullptr;
                BQ_NAPI_CALL_VOID(env, napi_get_reference_value(env, ctx->js_func_ref, &js_func));
                auto& tls_arr = tls_argv_array_.get();
                tls_arr.clear();
                for (size_t i = 0; i < ctx->argc; i++) {
                    napi_value argv = nullptr;
                    if (ctx->argv_ref && ctx->argv_ref[i]) {
                        BQ_NAPI_CALL_VOID(env, napi_get_reference_value(env, ctx->argv_ref[i], &argv));
                    }
                    tls_arr.push_back(argv);
                }
                napi_value result = nullptr;
                BQ_NAPI_CALL_VOID(env, napi_call_function(env, this_context, js_func, ctx->argc, tls_arr.begin(), &result));
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
            bq::platform::scoped_spin_lock lock(napi_init_mutex_);
            if(is_napi_initialized_){
                return nullptr;
            }
            is_napi_initialized_ = true;

            napi_value name_native{};
            BQ_NAPI_CALL(env, nullptr, napi_create_string_utf8(env, "bqlog-tsfn-native-wrapper", NAPI_AUTO_LENGTH, &name_native));
            BQ_NAPI_CALL(env, nullptr, napi_create_threadsafe_function(
                env, nullptr, nullptr, name_native,
                1024, 1, nullptr, nullptr, nullptr,
                dispatcher_call_native, &tsfn_dispatcher_for_native_func_
            ));
            napi_value name_js{};
            BQ_NAPI_CALL(env, nullptr, napi_create_string_utf8(env, "bqlog-tsfn-js-wrapper", NAPI_AUTO_LENGTH, &name_js));
            BQ_NAPI_CALL(env, nullptr, napi_create_threadsafe_function(
                env, nullptr, nullptr, name_js,
                1024, 1, nullptr, nullptr, nullptr,
                dispatcher_call_js, &tsfn_dispatcher_for_js_func_
            ));
            BQ_NAPI_CALL(env, nullptr, napi_add_env_cleanup_hook(env, cleanup_hook, nullptr));

            for (auto callback : common_global_vars::get().napi_init_callbacks_inst_) {
                callback(env, exports);
            }

            if (napi_registered_init_functions_) {
                BQ_NAPI_CALL(env, nullptr, napi_define_properties(env, exports, napi_registered_init_functions_->size(), napi_registered_init_functions_->begin()));
                delete napi_registered_init_functions_;
            }
            return exports;
        }

#if defined(BQ_DYNAMIC_LIB)
        NAPI_MODULE(bqlog, napi_init)
#endif
    }
}
#endif
