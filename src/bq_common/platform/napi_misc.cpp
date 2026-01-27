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
    bq::napi_str_result<bq::string, 256> read_utf8_str_tls(napi_env env, napi_value v, size_t u8string_bytes)
    {
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

    uint64_t get_u64_from_bigint(napi_env env, napi_value v)
    {
        bool lossless = false;
        uint64_t out = 0;
        napi_valuetype t = napi_undefined;
        if (napi_typeof(env, v, &t) == napi_ok) {
            if (t == napi_bigint) {
                napi_get_value_bigint_uint64(env, v, &out, &lossless);
                return out;
            } else if (t == napi_number) {
                // best-effort for JS number inputs (may lose precision > 2^53)
                double d = 0;
                napi_get_value_double(env, v, &d);
                if (d < 0)
                    d = 0;
                out = (uint64_t)(d);
                return out;
            } else if (t == napi_string) {
                // parse decimal string (simple, without locale)
                size_t len = 0;
                napi_get_value_string_utf8(env, v, nullptr, 0, &len);
                char* buf = (char*)malloc(len + 1);
                if (!buf)
                    return 0;
                size_t got = 0;
                napi_get_value_string_utf8(env, v, buf, len + 1, &got);
                uint64_t acc = 0;
                for (size_t i = 0; i < got; ++i) {
                    char c = buf[i];
                    if (c >= '0' && c <= '9') {
                        acc = acc * 10 + (uint64_t)(c - '0');
                    } else {
                        break;
                    }
                }
                free(buf);
                return acc;
            }
        }
        return out;
    }

    bq::string dup_string_from_napi(napi_env env, napi_value v)
    {
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

    char* dup_cstr_from_napi(napi_env env, napi_value v)
    {
        size_t len = 0;
        napi_get_value_string_utf8(env, v, NULL, 0, &len);
        char* out = (char*)malloc(len + 1);
        if (!out)
            return NULL;
        size_t got = 0;
        napi_get_value_string_utf8(env, v, out, len + 1, &got);
        out[got] = '\0';
        return out;
    }

    void free_cstr_from_napi(char* p)
    {
        if (p)
            free(p);
    }

    namespace platform {
        static BQ_TLS napi_env tls_current_env_ = nullptr;

        struct native_func_call_ctx {
            void (*fn)(napi_env, void*);
            void* param;
        };

        static void dispatcher_call_native(napi_env env, napi_value /*js_cb*/, void* /*ctx*/, void* data)
        {
            native_func_call_ctx* ctx = reinterpret_cast<native_func_call_ctx*>(data);
            if (ctx) {
                ctx->fn(env, ctx->param);
                delete ctx;
            }
        }

        bool napi_call_native_func_in_js_thread(void (*fn)(napi_env, void*), void* param)
        {
            auto& common_vars = common_global_vars::get();
            if (tls_current_env_ && tls_current_env_ == common_vars.napi_main_env_) {
                fn(tls_current_env_, param);
                return true;
            }
            native_func_call_ctx* ctx = new native_func_call_ctx();
            ctx->fn = fn;
            ctx->param = param;
            napi_status s = napi_call_threadsafe_function(common_vars.napi_tsfn_native_, ctx, napi_tsfn_blocking);
            return s == napi_ok;
        }

        napi_callback_dispatcher::napi_callback_dispatcher(napi_call_handler handler)
            : handler_(handler)
        {
            auto& common_vars = common_global_vars::get();
            bq::platform::scoped_mutex lock(common_vars.napi_env_mutex_);
            common_vars.napi_dispatchers_.push_back(this);
        }

        static void tsfn_finalize_cb(napi_env env, void* finalize_data, void* finalize_hint)
        {
            (void)finalize_hint;
            napi_ref ref = (napi_ref)finalize_data;
            if (ref) {
                napi_delete_reference(env, ref);
            }
        }

        void napi_callback_dispatcher::register_callback(napi_env env, napi_value func)
        {
            auto& common_vars = common_global_vars::get();
            bq::platform::scoped_mutex lock(common_vars.napi_env_mutex_);
            unregister_callback(env);
            napi_callback_entry entry;
            entry.env = env;
            napi_create_reference(env, func, 1, &entry.js_cb_ref);
            napi_value name {};
            napi_create_string_utf8(env, "bqlog-dispatcher-tsfn", NAPI_AUTO_LENGTH, &name);
            napi_create_threadsafe_function(env, nullptr, nullptr, name, 1024, 1, entry.js_cb_ref, tsfn_finalize_cb, nullptr, dispatcher_call_native, &entry.tsfn);
            napi_unref_threadsafe_function(env, entry.tsfn);
            entries_.push_back(entry);
        }

        void napi_callback_dispatcher::unregister_callback(napi_env env)
        {
            for (size_t i = 0; i < entries_.size(); ++i) {
                if (entries_[i].env == env) {
                    napi_release_threadsafe_function(entries_[i].tsfn, napi_tsfn_release);
                    entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(i));
                    return;
                }
            }
        }

        struct dispatcher_call_ctx {
            napi_call_handler handler;
            napi_ref js_cb_ref;
            void* param;
        };

        static void dispatcher_fallback_adapter(napi_env env, void* data)
        {
            dispatcher_call_ctx* ctx = (dispatcher_call_ctx*)data;
            if (ctx) {
                ctx->handler(env, ctx->js_cb_ref, ctx->param);
                delete ctx;
            }
        }

        void napi_callback_dispatcher::invoke(void* data)
        {
            auto& common_vars = common_global_vars::get();
            bq::platform::scoped_mutex lock(common_vars.napi_env_mutex_);
            if (entries_.is_empty())
                return;

            if (tls_current_env_) {
                for (auto& entry : entries_) {
                    if (entry.env == tls_current_env_) {
                        handler_(tls_current_env_, entry.js_cb_ref, data);
                        return;
                    }
                }
            }

            auto& primary = entries_[0];
            dispatcher_call_ctx* ctx = new dispatcher_call_ctx();
            ctx->handler = handler_;
            ctx->js_cb_ref = primary.js_cb_ref;
            ctx->param = data;

            native_func_call_ctx* n_ctx = new native_func_call_ctx();
            n_ctx->fn = dispatcher_fallback_adapter;
            n_ctx->param = ctx;
            napi_call_threadsafe_function(primary.tsfn, n_ctx, napi_tsfn_blocking);
        }

        void napi_callback_dispatcher::cleanup_env(napi_env env)
        {
            unregister_callback(env);
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
            napi_status s = napi_call_threadsafe_function(common_global_vars::get().napi_tsfn_js_, ctx, napi_tsfn_blocking);
            return s == napi_ok;
        }

        napi_init_function_register::napi_init_function_register(void (*init_callback)(napi_env env, napi_value exports))
        {
            bq::platform::scoped_mutex lock(common_global_vars::get().napi_init_mutex_);
            common_global_vars::get().napi_init_native_callbacks_inst_.push_back(init_callback);
        }

        __napi_func_register_helper::__napi_func_register_helper(const char* name, napi_callback func)
        {
            bq::platform::scoped_mutex lock(common_global_vars::get().napi_init_mutex_);
            common_global_vars::get().napi_registered_functions_.push_back(napi_property_descriptor { name, 0, func, 0, 0, 0, napi_default, 0 });
        }

        static void dispatcher_call_js(napi_env env, napi_value /*js_cb*/, void* /*ctx*/, void* data)
        {
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

        static void cleanup_hook(void* p)
        {
            (void)p;
            auto& common_vars = common_global_vars::get();
            napi_env env = tls_current_env_;
            {
                bq::platform::scoped_mutex lock(common_vars.napi_env_mutex_);
                for (auto d : common_vars.napi_dispatchers_) {
                    d->cleanup_env(env);
                }
            }
            if (tls_current_env_ == common_vars.napi_main_env_) {
                common_vars.napi_main_env_ = nullptr;
            }
            tls_current_env_ = nullptr;
            if (common_vars.napi_tsfn_native_) {
                napi_release_threadsafe_function(common_vars.napi_tsfn_native_, napi_tsfn_release);
            }
            if (common_vars.napi_tsfn_js_) {
                napi_release_threadsafe_function(common_vars.napi_tsfn_js_, napi_tsfn_release);
            }
        }

        // Module initializer
        napi_value napi_init(napi_env env, napi_value exports)
        {
            tls_current_env_ = env;
            auto& common_vars = common_global_vars::get();
            bq::platform::scoped_mutex lock(common_vars.napi_init_mutex_);
            if (!common_vars.napi_is_initialized_) {
                common_vars.napi_is_initialized_ = true;
                common_vars.napi_main_env_ = env;

                napi_value name_native {};
                BQ_NAPI_CALL(env, nullptr, napi_create_string_utf8(env, "bqlog-tsfn-native-wrapper", NAPI_AUTO_LENGTH, &name_native));
                BQ_NAPI_CALL(env, nullptr, napi_create_threadsafe_function(env, nullptr, nullptr, name_native, 1024, 1, nullptr, nullptr, nullptr, dispatcher_call_native, &common_vars.napi_tsfn_native_));
                napi_unref_threadsafe_function(env, common_vars.napi_tsfn_native_);
                napi_value name_js {};
                BQ_NAPI_CALL(env, nullptr, napi_create_string_utf8(env, "bqlog-tsfn-js-wrapper", NAPI_AUTO_LENGTH, &name_js));
                BQ_NAPI_CALL(env, nullptr, napi_create_threadsafe_function(env, nullptr, nullptr, name_js, 1024, 1, nullptr, nullptr, nullptr, dispatcher_call_js, &common_vars.napi_tsfn_js_));
                napi_unref_threadsafe_function(env, common_vars.napi_tsfn_js_);
            }
            else {
                if (common_vars.napi_tsfn_native_) {
                    napi_acquire_threadsafe_function(common_vars.napi_tsfn_native_);
                }
                if (common_vars.napi_tsfn_js_) {
                    napi_acquire_threadsafe_function(common_vars.napi_tsfn_js_);
                }
            }
            BQ_NAPI_CALL(env, nullptr, napi_add_env_cleanup_hook(env, cleanup_hook, nullptr));

            for (auto callback : common_vars.napi_init_native_callbacks_inst_) {
                callback(env, exports);
            }

            auto& reg_funcs = common_vars.napi_registered_functions_;
            if (reg_funcs.size() > 0) {
                BQ_NAPI_CALL(env, nullptr, napi_define_properties(env, exports, reg_funcs.size(), reg_funcs.begin()));
            }
            return exports;
        }

#if defined(BQ_DYNAMIC_LIB)
        NAPI_MODULE(bqlog, napi_init)
#endif
    }
}
#endif