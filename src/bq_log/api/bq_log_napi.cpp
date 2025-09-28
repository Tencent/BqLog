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

 // N-API bindings for BqLog core. Module init/registration has been moved to
 // src/bq_common/platform/napi_misc.cpp as requested.



#if defined(BQ_NAPI)
#include <stddef.h>
#include <math.h>
#include "bq_log/bq_log.h"
#include "bq_log/global/vars.h"
#include "bq_common/bq_common.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/types/buffer/log_buffer.h"

namespace bq {
    BQ_TLS_NON_POD(bq::string, _tls_utf8_str);

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

    static inline napi_value _make_undefined(napi_env env) {
        napi_value v; napi_get_undefined(env, &v); return v;
    }
    static inline napi_value _make_bool(napi_env env, bool b) {
        napi_value v; napi_get_boolean(env, b, &v); return v;
    }
    static inline napi_value _make_i32(napi_env env, int32_t x) {
        napi_value v; napi_create_int32(env, x, &v); return v;
    }
    static inline napi_value _make_u32(napi_env env, uint32_t x) {
        napi_value v; napi_create_uint32(env, x, &v); return v;
    }
    static inline napi_value _make_u64(napi_env env, uint64_t x) {
        napi_value v; napi_create_bigint_uint64(env, x, &v); return v;
    }
    static inline napi_value _make_str_utf8(napi_env env, const char* s) {
        napi_value v; napi_create_string_utf8(env, s ? s : "", NAPI_AUTO_LENGTH, &v); return v;
    }

    static inline bool _get_bool(napi_env env, napi_value v) {
        bool b = false; napi_get_value_bool(env, v, &b); return b;
    }
    static inline int32_t _get_i32(napi_env env, napi_value v) {
        int32_t x = 0; napi_get_value_int32(env, v, &x); return x;
    }
    static inline uint32_t _get_u32(napi_env env, napi_value v) {
        uint32_t x = 0; napi_get_value_uint32(env, v, &x); return x;
    }

    static inline bool js_is_integer(double d) {
        return isfinite(d) && d == trunc(d);
    }

    static inline bool js_is_safe_integer(double d) {
        if (!js_is_integer(d)) {
            return false;
        }
        // |d| <= 2^53 - 1
        return fabs(d) <= 9007199254740991.0; // 9007199254740991 = 2^53 - 1
    }


    static inline bq::napi_str_result<bq::string, 256> read_utf8_str_tls(napi_env env, napi_value v, size_t u8string_bytes) {
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

    static inline uint64_t _get_u64_from_bigint(napi_env env, napi_value v) {
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

    static inline char* _dup_cstr_from_napi(napi_env env, napi_value v) {
        size_t len = 0;
        napi_get_value_string_utf8(env, v, NULL, 0, &len);
        char* out = (char*)malloc(len + 1);
        if (!out) return NULL;
        size_t got = 0;
        napi_get_value_string_utf8(env, v, out, len + 1, &got);
        out[got] = '\0';
        return out;
    }

    static inline void _free_cstr(char* p) {
        if (p) free(p);
    }
}


// ----------------------------- console TSFN -----------------------------

BQ_PACK_BEGIN
struct console_msg_head {
    uint64_t    log_id;
    int32_t     category_idx;
    int32_t     level;
    uint32_t    length;
}
BQ_PACK_END

static napi_ref console_callback_{ NULL };
static bq::miso_ring_buffer console_msg_buffer_(bq::log_buffer_config({ "napi_console_cb_msg", bq::array<bq::string>(), 1024 * 8, false, bq::log_memory_policy::block_when_full, 0 }));


static void tsfn_console_call_js(napi_env env, void* param) {
    console_msg_head* msg = nullptr;
    bool from_buffer = (param == (void*)&console_msg_buffer_);
    bq::log_buffer_read_handle handle;
    if (from_buffer)
    {
        handle = console_msg_buffer_.read_chunk();
        assert(bq::enum_buffer_result_code::success == handle.result);
        msg = (console_msg_head*)handle.data_addr;
    }
    else {
        msg = (console_msg_head*)param;
    }
    if (msg && env && console_callback_) {
        napi_value argv[4];
        argv[0] = bq::_make_u64(env, msg->log_id);
        argv[1] = bq::_make_i32(env, msg->category_idx);
        argv[2] = bq::_make_i32(env, msg->level);
        napi_create_string_utf8(env, (char*)(msg + 1), msg->length, &argv[3]);
        napi_value undefined = bq::_make_undefined(env);
        napi_value js_cb = NULL;
        napi_get_reference_value(env, console_callback_, &js_cb);
        napi_call_function(env, undefined, js_cb, 4, argv, NULL);
    }
    if (from_buffer) {
        console_msg_buffer_.return_read_chunk(handle);
    }
}

static void BQ_STDCALL on_console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length) {
    (void)length;
    uint32_t size = static_cast<uint32_t>(sizeof(console_msg_head)) + static_cast<uint32_t>(length);
    auto handle = console_msg_buffer_.alloc_write_chunk(size);
    console_msg_head* msg = nullptr;
    bool success = bq::enum_buffer_result_code::success != handle.result;
    if (success) {
        msg = (console_msg_head*)handle.data_addr;
    }
    else {
        msg = (console_msg_head*)malloc(size);
    }
    if (msg) {
        msg->log_id = log_id;
        msg->category_idx = category_idx;
        msg->level = log_level;
        msg->length = static_cast<uint32_t>(length);
        if (length > 0) {
            memcpy(msg + 1, content, static_cast<size_t>(length));
        }
    }
    console_msg_buffer_.commit_write_chunk(handle);
    bq::platform::napi_call_native_func_in_js_thread(&tsfn_console_call_js, success ? (void*)&console_msg_buffer_ : (void*)msg);
}

// ----------------------------- exported N-API functions -----------------------------

// get_log_version(): string
BQ_NAPI_DEF(get_log_version, napi_env, env, napi_callback_info, info)
{
    (void)info;
    const char* ver = bq::api::__api_get_log_version();
    return bq::_make_str_utf8(env, ver);
}

// enable_auto_crash_handler(): void
BQ_NAPI_DEF(enable_auto_crash_handler, napi_env, env, napi_callback_info, info)
{
    (void)env; (void)info;
    bq::log::enable_auto_crash_handle();
    return bq::_make_undefined(env);
}

// create_log(name: string, config: string, categories_count: number, categories: string[]): bigint
BQ_NAPI_DEF(create_log, napi_env, env, napi_callback_info, info)
{
    size_t argc = 4;
    napi_value argv[4] = { 0,0,0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 4) {
        napi_throw_type_error(env, NULL, "create_log(name, config[, categories_count, categories]) requires  4 arguments");
        return NULL;
    }

    char* log_name = bq::_dup_cstr_from_napi(env, argv[0]);
    char* log_config = bq::_dup_cstr_from_napi(env, argv[1]);
    uint32_t categories_count = 0;
    const char** category_names = NULL;

    categories_count = bq::_get_u32(env, argv[2]);
    bool is_array = false;
    napi_is_array(env, argv[3], &is_array);
    if (is_array) {
        uint32_t arr_len = 0;
        napi_get_array_length(env, argv[3], &arr_len);
        if (categories_count > arr_len) categories_count = arr_len;
        if (categories_count > 0) {
            category_names = (const char**)malloc(sizeof(const char*) * (size_t)categories_count);
            if (!category_names) categories_count = 0;
            for (uint32_t i = 0; category_names && i < categories_count; ++i) {
                napi_value elem = NULL;
                napi_get_element(env, argv[3], i, &elem);
                category_names[i] = bq::_dup_cstr_from_napi(env, elem);
            }
        }
    }

    uint64_t log_id = bq::api::__api_create_log(log_name ? log_name : "", log_config ? log_config : "", (uint32_t)categories_count, (const char**)category_names);

    // free temp strings
    if (category_names) {
        for (uint32_t i = 0; i < categories_count; ++i) {
            bq::_free_cstr(const_cast<char*>(category_names[i]));
        }
        free((void*)category_names);
    }
    bq::_free_cstr(log_name);
    bq::_free_cstr(log_config);

    return bq::_make_u64(env, log_id);
}

// log_reset_config(log_name: string, config: string): void
BQ_NAPI_DEF(log_reset_config, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "log_name and config required"); return NULL; }

    char* log_name = bq::_dup_cstr_from_napi(env, argv[0]);
    char* log_config = bq::_dup_cstr_from_napi(env, argv[1]);
    bq::api::__api_log_reset_config(log_name ? log_name : "", log_config ? log_config : "");
    bq::_free_cstr(log_name);
    bq::_free_cstr(log_config);
    return bq::_make_undefined(env);
}

struct log_napi_wrap_inst {
    uint64_t log_id_;
};
static void NAPI_CDECL log_inst_finalize(napi_env env,
    void* finalize_data,
    void* finalize_hint)
{
    (void)env;
    (void)finalize_hint;
    log_napi_wrap_inst* wrap_inst = (log_napi_wrap_inst*)finalize_data;
    if (wrap_inst) {
        delete wrap_inst;
    }
}
// attach_log_inst(log_inst: log, log_id: bigint): void
BQ_NAPI_DEF(attach_log_inst, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 2) {
        napi_throw_type_error(env, NULL, "log_inst and log_id:bigint required in attach_log_inst");
        return NULL;
    }
    napi_valuetype t0; napi_typeof(env, argv[0], &t0);
    if (t0 != napi_object) {
        napi_throw_type_error(env, NULL, "first arg must be object");
        return nullptr;
    }
    uint64_t log_id = bq::_get_u64_from_bigint(env, argv[1]);
    log_napi_wrap_inst* wrap_inst = new log_napi_wrap_inst{ log_id };
    BQ_NAPI_CALL(env, nullptr, napi_wrap(env, argv[0], wrap_inst, log_inst_finalize, nullptr, nullptr));
    return bq::_make_undefined(env);
}


struct category_base_napi_wrap_inst {
    uint32_t category_index_;
};
static void NAPI_CDECL category_base_inst_finalize(napi_env env,
    void* finalize_data,
    void* finalize_hint)
{
    (void)env;
    (void)finalize_hint;
    category_base_napi_wrap_inst* wrap_inst = (category_base_napi_wrap_inst*)finalize_data;
    if (wrap_inst) {
        delete wrap_inst;
    }
}
// attach_category_base_inst(category_base_inst: category, category_index: number): void
BQ_NAPI_DEF(attach_category_base_inst, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 2) {
        napi_throw_type_error(env, NULL, "category_base_inst and  category_index: number required in attach_category_base_inst");
        return NULL;
    }
    napi_valuetype t0; napi_typeof(env, argv[0], &t0);
    if (t0 != napi_object) {
        napi_throw_type_error(env, NULL, "first arg must be object");
        return nullptr;
    }
    uint32_t category_index = bq::_get_u32(env, argv[1]);
    category_base_napi_wrap_inst* wrap_inst = new category_base_napi_wrap_inst{ category_index };
    BQ_NAPI_CALL(env, nullptr, napi_wrap(env, argv[0], wrap_inst, category_base_inst_finalize, nullptr, nullptr));
    return bq::_make_undefined(env);
}

template<bool INCLUDE_TYPE_INFO>
struct js_string_custom_formater {
private:
    bq::tools::size_seq<INCLUDE_TYPE_INFO, const char*> size_seq_;
    napi_env env_;
    napi_value js_string_value1_;
    napi_value js_string_value2_;
    size_t js_string_utf8_size_;
public:
    bool reset(napi_env env, napi_value js_str_val1, napi_value js_str_val2 = nullptr) {
        env_ = env;
        js_string_value1_ = js_str_val1;
        js_string_value2_ = js_str_val2;
        js_string_utf8_size_ = 0;
        if (js_string_value1_) {
            BQ_NAPI_CALL(env_, false, napi_get_value_string_utf8(env_, js_string_value1_, nullptr, 0, &js_string_utf8_size_));
        }
        if (js_string_value2_) {
            size_t sz2 = 0;
            BQ_NAPI_CALL(env_, false, napi_get_value_string_utf8(env_, js_string_value2_, nullptr, 0, &sz2));
            js_string_utf8_size_ += sz2;
        }
        size_seq_ = bq::tools::make_single_string_size_seq<INCLUDE_TYPE_INFO, char>(js_string_utf8_size_);
        return true;
    }

    const bq::tools::size_seq<INCLUDE_TYPE_INFO, const char*>& get_size_seq() const {
        return size_seq_;
    }

    size_t bq_log_format_str_size() const {
        return js_string_utf8_size_;
    }

    void bq_log_custom_format(char* dest, size_t data_size) const {
#ifndef NDEBUG
        assert(data_size >= js_string_utf8_size_);
#endif
        if (dest) {
            size_t copied = 0;
            if (js_string_value1_) {
                //1 byte has already been preserved for '\0', don't worry about overflow here
                napi_get_value_string_utf8(env_, js_string_value1_, dest, data_size + 1, &copied);
            }
#ifndef NDEBUG
            assert(copied <= data_size);
#endif
            if (js_string_value2_) {
                size_t left = data_size - copied;
                //1 byte has already been preserved for '\0', don't worry about overflow here
                napi_get_value_string_utf8(env_, js_string_value2_, dest + copied, left + 1, &copied);
#ifndef NDEBUG
                assert(copied <= left);
#endif
            }
        }
    }
};

struct arg_info {
    napi_valuetype type_;
    size_t data_size_;
    size_t storage_size_;
    js_string_custom_formater<true> custom_formatter_;
};

#define RECORD_ARG_BY_SIZE_SEQ(SIZE_SEQ_NAME)  arg_info_array[i].data_size_ = SIZE_SEQ_NAME.get_element().get_value();\
                                        arg_info_array[i].storage_size_ = SIZE_SEQ_NAME.get_element().get_aligned_value();
#define RECORD_STRING_ARG(STRING_VALUE) argv_ptr[i] = STRING_VALUE;\
                                        arg_info_ptr[i].type_ = napi_string;\
                                        arg_info_ptr[i].custom_formatter_.reset(env, STRING_VALUE);\
                                        arg_info_array[i].data_size_ = arg_info_ptr[i].custom_formatter_.get_size_seq().get_element().get_value();\
                                        arg_info_array[i].storage_size_ = bq::align_4(arg_info_ptr[i].custom_formatter_.get_size_seq().get_element().get_value() + 1); \
                                        //napi_get_value_string_utf8 always write a trailing '\0', so no need to +1 here to prevent the overflow.         

#define TRANS_AND_RECORD_STRING_ARG napi_value string_cast_value;\
                                        BQ_NAPI_CALL(env, nullptr, napi_coerce_to_string(env, argv_ptr[i], &string_cast_value));\
                                        RECORD_STRING_ARG(string_cast_value)

static napi_value napi_do_log(bq::log_level log_level, napi_env env, napi_callback_info info)
{
    constexpr size_t max_static_argc = 32;
    const char* symbo_str = "[Symbol]";
    size_t argc = max_static_argc;
    napi_value argv[max_static_argc] = { 0 };
    arg_info arg_info_array[max_static_argc] = {};
    napi_value this_arg;
    BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_cb_info(env, info, &argc, argv, &this_arg, NULL));
    if (argc < 1) {
        napi_throw_type_error(env, NULL, "log.napi_do_log(level, [category], format_content[, args...]) arguments count error");
        return bq::_make_bool(env, false);
    }

    log_napi_wrap_inst* log_js_inst = nullptr;
    BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_unwrap(env, this_arg, reinterpret_cast<void**>(&log_js_inst)));
    if (!log_js_inst)
    {
        napi_throw_type_error(env, NULL, "log.napi_do_log(level, [category], format_content[, args...]) failed to wrap log instance");
        return bq::_make_bool(env, false);
    }
    category_base_napi_wrap_inst* category_js_inst = nullptr;
    bool has_category_arg = false;
    uint32_t category_index = 0;   //0 means default category index
    if (napi_ok == napi_unwrap(env, argv[0], reinterpret_cast<void**>(&category_js_inst)))
    {
        has_category_arg = true;
        category_index = category_js_inst->category_index_;
        if (argc < 2) {
            napi_throw_type_error(env, NULL, "log.napi_do_log(level, [category], format_content[, args...]) arguments count error");
            return bq::_make_bool(env, false);
        }
    }
    bq::log_imp* log_impl = bq::log_manager::get_log_by_id(log_js_inst->log_id_);
    if (!log_impl || !log_impl->is_enable_for(category_index, log_level)) {
        return bq::_make_bool(env, false);
    }

    napi_value* argv_ptr = argv;
    arg_info* arg_info_ptr = arg_info_array;
    bq::array<napi_value> heap_argv;
    bq::array<arg_info> heap_arg_info;
    if (argc > max_static_argc) {
        heap_argv.fill_uninitialized(argc);
        heap_arg_info.fill_uninitialized(argc);
        argv_ptr = heap_argv.begin();
        arg_info_ptr = heap_arg_info.begin();
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_cb_info(env, info, &argc, argv_ptr, NULL, NULL));
    }

    bool enable_stack_trace = log_impl->is_stack_trace_enable_for(log_level);
    napi_value stack_val = nullptr;
    if (enable_stack_trace) {
        napi_value empty_msg;
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &empty_msg));

        napi_value err;
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_create_error(env, nullptr, empty_msg, &err));
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_set_named_property(env, err, "name", empty_msg));

        napi_value global, error_ctor, cap_fun;
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_global(env, &global));
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_named_property(env, global, "Error", &error_ctor));
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_named_property(env, error_ctor, "captureStackTrace", &cap_fun));
        napi_value argv_cap[1] = { err };
        napi_call_function(env, error_ctor, cap_fun, 1, argv_cap, nullptr);
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_named_property(env, err, "stack", &stack_val));
    }
    js_string_custom_formater<false> format_custom_formatter;
    if (!format_custom_formatter.reset(env, argv_ptr[has_category_arg ? 1 : 0], stack_val)) {
        return bq::_make_bool(env, false);
    }

    //napi_get_value_string_utf8 always write a trailing '\0', so no need to +1 here to prevent the overflow. 
    size_t aligned_format_data_size = bq::align_4(format_custom_formatter.get_size_seq().get_element().get_value() + 1);
    size_t total_data_size = sizeof(bq::_log_entry_head_def) + aligned_format_data_size;
    for (size_t i = (has_category_arg ? 2 : 1); i < argc; ++i) {
        napi_valuetype input_param_type = napi_undefined;
        BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_typeof(env, argv_ptr[i], &input_param_type));
        arg_info_array[i].type_ = input_param_type;
        switch (input_param_type)
        {
        case napi_boolean:
        {
            auto arg_size_seq = bq::tools::make_size_seq<true>(true);
            RECORD_ARG_BY_SIZE_SEQ(arg_size_seq)
        }
        break;
        case napi_number:
        {
            double d = 0;
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_value_double(env, argv_ptr[i], &d));
            if (bq::js_is_safe_integer(d)) {
                int64_t int_value = (int64_t)d;
                auto arg_size_seq = bq::tools::make_size_seq<true>(int_value);
                RECORD_ARG_BY_SIZE_SEQ(arg_size_seq)
            }
            else if (bq::js_is_integer(d) || isnan(d) || isinf(d)) {
                TRANS_AND_RECORD_STRING_ARG
            }
            else {
                auto arg_size_seq = bq::tools::make_size_seq<true>(d);
                RECORD_ARG_BY_SIZE_SEQ(arg_size_seq)
            }
        }
        break;
        case napi_string:
        {
            RECORD_STRING_ARG(argv_ptr[i])
        }
        break;
        case napi_symbol:
        {
            auto arg_size_seq = bq::tools::make_size_seq<true>(symbo_str);
            RECORD_ARG_BY_SIZE_SEQ(arg_size_seq)
        }
        break;
        case napi_external:
        {
            void* p = NULL;
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_value_external(env, argv_ptr[i], &p));
            char buf[64];
            size_t str_size = static_cast<size_t>(snprintf(buf, sizeof(buf), "[External %p]", p));
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_create_string_utf8(env, buf, str_size, &(argv_ptr[i])));
            RECORD_STRING_ARG(argv_ptr[i])
        }
        break;
        case napi_bigint:
        {
            bool lossless = false;
            uint64_t u64_value = 0;
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_value_bigint_uint64(env, argv_ptr[i], &u64_value, &lossless));
            if (lossless) {
                auto arg_size_seq = bq::tools::make_size_seq<true>(u64_value);
                RECORD_ARG_BY_SIZE_SEQ(arg_size_seq)
            }
            else {
                TRANS_AND_RECORD_STRING_ARG
            }
        }
        break;
        case napi_undefined:
        case napi_null:
        case napi_object:
        case napi_function:
        {
            TRANS_AND_RECORD_STRING_ARG
        }
        break;
        default:
            break;
        }
        total_data_size += arg_info_array[i].storage_size_;
    }
    auto handle = bq::api::__api_log_buffer_alloc(log_js_inst->log_id_, (uint32_t)total_data_size);
    if (handle.result != bq::enum_buffer_result_code::success) {
        return bq::_make_bool(env, false);
    }
    bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
    head->category_idx = category_index;
    head->level = static_cast<uint8_t>(log_level);
    head->log_format_str_type = static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type);
    size_t log_args_offset = static_cast<size_t>(sizeof(bq::_log_entry_head_def) + aligned_format_data_size);
    head->log_args_offset = (uint32_t)log_args_offset;
    uint8_t* log_format_content_addr = handle.data_addr + sizeof(bq::_log_entry_head_def);
    bq::tools::_type_copy<false>(format_custom_formatter, log_format_content_addr, format_custom_formatter.get_size_seq().get_element().get_value());
    uint8_t* log_args_addr = handle.data_addr + log_args_offset;

    for (size_t i = (has_category_arg ? 2 : 1); i < argc; ++i) {
        switch (arg_info_ptr[i].type_) {
        case napi_boolean:
        {
            bool bool_value = bq::_get_bool(env, argv[i]);
            bq::tools::_type_copy<true>(bool_value, log_args_addr, arg_info_ptr[i].data_size_);
        }
        break;
        case napi_number:
        {
            double d = 0;
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_value_double(env, argv_ptr[i], &d));
            if (bq::js_is_safe_integer(d)) {
                int64_t int_value = (int64_t)d;
                bq::tools::_type_copy<true>(int_value, log_args_addr, arg_info_ptr[i].data_size_);
            }
            else {
                bq::tools::_type_copy<true>(d, log_args_addr, arg_info_ptr[i].data_size_);
            }
        }
        break;
        case napi_string:
        {
            bq::tools::_type_copy<true>(arg_info_ptr[i].custom_formatter_, log_args_addr, arg_info_ptr[i].data_size_);
        }
        break;
        case napi_symbol:
        {
            bq::tools::_type_copy<true>(symbo_str, log_args_addr, arg_info_ptr[i].data_size_);
        }
        break;
        case napi_bigint:
        {
            bool lossless = false;
            uint64_t u64_value = 0;
            BQ_NAPI_CALL(env, bq::_make_bool(env, false), napi_get_value_bigint_uint64(env, argv_ptr[i], &u64_value, &lossless));
            assert(log_args_offset && "[NAPI do_log bigint]impossible execution path!");
            bq::tools::_type_copy<true>(u64_value, log_args_addr, arg_info_ptr[i].data_size_);
        }
        break;
        default:
            assert(false && "[NAPI do_log]impossible execution path!");
            break;
        }
        log_args_addr += arg_info_ptr[i].storage_size_;
    }
    bq::api::__api_log_buffer_commit(log_js_inst->log_id_, handle);
    return bq::_make_bool(env, true);
}

// log.verbose([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(verbose, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::verbose, env, info);
}
// log.debug([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(debug, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::debug, env, info);
}
// log.info([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(info, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::info, env, info);
}
// log.warning([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(warning, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::warning, env, info);
}
// log.error([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(error, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::error, env, info);
}
// log.fatal([category: category_base], format_content: string, ...args: unknown[]): boolean
BQ_NAPI_DEF(fatal, napi_env, env, napi_callback_info, info) {
    return napi_do_log(bq::log_level::fatal, env, info);
}

// set_appenders_enable(log_id: bigint, appender_name: string, enable: boolean): void
BQ_NAPI_DEF(set_appenders_enable, napi_env, env, napi_callback_info, info)
{
    size_t argc = 3; napi_value argv[3] = { 0,0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 3) { napi_throw_type_error(env, NULL, "id, appender_name, enable required"); return NULL; }

    uint64_t log_id = bq::_get_u64_from_bigint(env, argv[0]);
    char* appender_name = bq::_dup_cstr_from_napi(env, argv[1]);
    bool enable = bq::_get_bool(env, argv[2]);

    bq::api::__api_set_appenders_enable(log_id, appender_name ? appender_name : "", enable);
    bq::_free_cstr(appender_name);
    return bq::_make_undefined(env);
}

// get_logs_count(): number
BQ_NAPI_DEF(get_logs_count, napi_env, env, napi_callback_info, info)
{
    (void)info;
    uint32_t c = bq::api::__api_get_logs_count();
    return bq::_make_u32(env, c);
}

// get_log_id_by_index(index: number): bigint
BQ_NAPI_DEF(get_log_id_by_index, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "index required"); return NULL; }
    uint32_t idx = bq::_get_u32(env, argv[0]);
    uint64_t id = bq::api::__api_get_log_id_by_index(idx);
    return bq::_make_u64(env, id);
}

// get_log_name_by_id(log_id: bigint): string|null
BQ_NAPI_DEF(get_log_name_by_id, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }

    uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
    bq::_api_string_def name_def;
    if (!bq::api::__api_get_log_name_by_id(id, &name_def)) {
        return nullptr;
    }
    return bq::_make_str_utf8(env, name_def.str);
}

// get_log_categories_count(log_id: bigint): number
BQ_NAPI_DEF(get_log_categories_count, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
    uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
    uint32_t count = bq::api::__api_get_log_categories_count(id);
    return bq::_make_u32(env, count);
}

// get_log_category_name_by_index(log_id: bigint, index: number): string|null
BQ_NAPI_DEF(get_log_category_name_by_index, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "log_id and index required"); return NULL; }

    uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
    uint32_t index = bq::_get_u32(env, argv[1]);
    bq::_api_string_def name_def;
    if (!bq::api::__api_get_log_category_name_by_index(id, index, &name_def)) {
        return nullptr;
    }
    return bq::_make_str_utf8(env, name_def.str);
}

// log_device_console(level: number, content: string): void
BQ_NAPI_DEF(log_device_console, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "level and content required"); return NULL; }

    int32_t level = bq::_get_i32(env, argv[0]);
    auto tls_str = bq::read_utf8_str_tls(env, argv[1], SIZE_MAX);
    bq::api::__api_log_device_console((bq::log_level)level, tls_str.str().c_str());
    return bq::_make_undefined(env);
}

// force_flush(log_id: bigint): void
BQ_NAPI_DEF(force_flush, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
    uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
    bq::api::__api_force_flush(id);
    return bq::_make_undefined(env);
}

// get_file_base_dir(in_sand_box: boolean): string
BQ_NAPI_DEF(get_file_base_dir, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "in_sand_box required"); return NULL; }
    bool in_sand_box = bq::_get_bool(env, argv[0]);
    const char* path = bq::api::__api_get_file_base_dir(in_sand_box);
    return bq::_make_str_utf8(env, path);
}

struct decoder_napi_wrap_inst {
    uint32_t handle_;
};
static void NAPI_CDECL decoder_inst_finalize(napi_env env,
    void* finalize_data,
    void* finalize_hint)
{
    (void)env;
    (void)finalize_hint;
    decoder_napi_wrap_inst* wrap_inst = (decoder_napi_wrap_inst*)finalize_data;
    if (wrap_inst) {
        bq::api::__api_log_decoder_destroy(wrap_inst->handle_);
        delete wrap_inst;
    }
}

// log_decoder_create(path: string, priv_key: string): number (negative on error)
BQ_NAPI_DEF(log_decoder_create, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, bq::_make_i32(env, -1), napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "path and priv_key required"); return NULL; }

    char* path = bq::_dup_cstr_from_napi(env, argv[0]);
    char* priv_key = bq::_dup_cstr_from_napi(env, argv[1]);

    uint32_t handle = 0;
    bq::appender_decode_result result = bq::api::__api_log_decoder_create(path ? path : "", priv_key ? priv_key : "", &handle);

    bq::_free_cstr(path);
    bq::_free_cstr(priv_key);

    if (result != bq::appender_decode_result::success) {
        // negative error code in int32
        return bq::_make_i32(env, (int32_t)result * (int32_t)(-1));
    }
    else {
        return bq::_make_i32(env, static_cast<int32_t>(handle));
    }
}

// log_decoder_attach
// attach_decoder_inst(decoder_inst: log_decoder, handle: number): void
BQ_NAPI_DEF(attach_decoder_inst, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 2) {
        napi_throw_type_error(env, NULL, "decoder_inst and handle:number required in attach_decoder_inst");
        return bq::_make_undefined(env);
    }
    napi_valuetype t0; napi_typeof(env, argv[0], &t0);
    if (t0 != napi_object) {
        napi_throw_type_error(env, NULL, "first arg must be object");
        return nullptr;
    }
    uint32_t handle = bq::_get_u32(env, argv[1]);
    decoder_napi_wrap_inst* wrap_inst = new decoder_napi_wrap_inst{ handle };
    BQ_NAPI_CALL(env, nullptr, napi_wrap(env, argv[0], wrap_inst, decoder_inst_finalize, nullptr, nullptr));
    return bq::_make_undefined(env);
}


// log_decoder_decode(handle: bigint): { code: number, text: string }
BQ_NAPI_DEF(log_decoder_decode, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "handle required"); return NULL; }

    uint32_t handle = bq::_get_u32(env, argv[0]);
    bq::_api_string_def text = { NULL, 0 };
    bq::appender_decode_result result = bq::api::__api_log_decoder_decode(handle, &text);

    napi_value obj = NULL; napi_create_object(env, &obj);
    napi_value v_code = bq::_make_i32(env, (int32_t)result);
    napi_value v_text = bq::_make_str_utf8(env, (result == bq::appender_decode_result::success) ? text.str : "");
    napi_set_named_property(env, obj, "code", v_code);
    napi_set_named_property(env, obj, "text", v_text);
    return obj;
}

// log_decode(in_path: string, out_path: string, priv_key: string): boolean
BQ_NAPI_DEF(log_decode, napi_env, env, napi_callback_info, info)
{
    size_t argc = 3; napi_value argv[3] = { 0,0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 3) { napi_throw_type_error(env, NULL, "in_path, out_path and priv_key required"); return NULL; }

    char* in_path = bq::_dup_cstr_from_napi(env, argv[0]);
    char* out_path = bq::_dup_cstr_from_napi(env, argv[1]);
    char* priv_key = bq::_dup_cstr_from_napi(env, argv[2]);

    bool ok = bq::api::__api_log_decode(in_path ? in_path : "", out_path ? out_path : "", priv_key ? priv_key : "");

    bq::_free_cstr(in_path);
    bq::_free_cstr(out_path);
    bq::_free_cstr(priv_key);

    return bq::_make_bool(env, ok);
}

// take_snapshot_string(log_id: bigint, use_gmt_time: boolean): string
BQ_NAPI_DEF(take_snapshot_string, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0,0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "log_id and use_gmt_time required"); return NULL; }

    uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
    bool use_gmt_time = bq::_get_bool(env, argv[1]);

    bq::_api_string_def snapshot_str_def = { NULL, 0 };
    bq::api::__api_take_snapshot_string(id, use_gmt_time, &snapshot_str_def);
    napi_value out = bq::_make_str_utf8(env, snapshot_str_def.str);
    bq::api::__api_release_snapshot_string(id, &snapshot_str_def);
    return out;
}

// set_console_callback(callback?: function): void
BQ_NAPI_DEF(set_console_callback, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 1) {
        napi_throw_type_error(env, NULL, "function count error");
        return NULL;
    }
    if (console_callback_) {
        napi_delete_reference(env, console_callback_);
        console_callback_ = NULL;
    }
    BQ_NAPI_CALL(env, nullptr, napi_create_reference(env, argv[0], 1, &console_callback_));
    console_msg_buffer_.set_thread_check_enable(false);

    bq::log::register_console_callback(on_console_callback);
    return bq::_make_undefined(env);
}

// set_console_buffer_enable(enable: boolean): void
BQ_NAPI_DEF(set_console_buffer_enable, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "enable required"); return NULL; }
    bool en = bq::_get_bool(env, argv[0]);
    bq::log::set_console_buffer_enable(en);
    return bq::_make_undefined(env);
}

// reset_base_dir(in_sandbox: boolean, dir: string): void
BQ_NAPI_DEF(reset_base_dir, napi_env, env, napi_callback_info, info)
{
    size_t argc = 2; napi_value argv[2] = { 0, 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 2) { napi_throw_type_error(env, NULL, "reset_base_dir invalid parameters count, should be 2"); return NULL; }
    bool in_sandbox = bq::_get_bool(env, argv[0]);
    char* dir = bq::_dup_cstr_from_napi(env, argv[1]);
    bq::api::__api_reset_base_dir(in_sandbox, dir);
    bq::_free_cstr(dir);
    return bq::_make_undefined(env);
}

struct _napi_fetch_ctx {
    napi_env  env;
    napi_value js_cb;
};

static void BQ_STDCALL _napi_console_buffer_fetch_callback(
    void* pass_through_param,
    uint64_t log_id,
    int32_t category_idx,
    int32_t log_level,
    const char* content,
    int32_t length)
{
    (void)length;
    _napi_fetch_ctx* ctx = (_napi_fetch_ctx*)pass_through_param;
    if (!ctx || !ctx->env || !ctx->js_cb) return;

    napi_value argv2[4];
    argv2[0] = bq::_make_u64(ctx->env, log_id);
    argv2[1] = bq::_make_i32(ctx->env, category_idx);
    argv2[2] = bq::_make_i32(ctx->env, log_level);
    argv2[3] = bq::_make_str_utf8(ctx->env, content ? content : "");
    napi_value undefined = bq::_make_undefined(ctx->env);
    napi_call_function(ctx->env, undefined, ctx->js_cb, 4, argv2, NULL);
}

// fetch_and_remove_console_buffer(callback: (log_id, category_idx, level, text) => void): boolean
BQ_NAPI_DEF(fetch_and_remove_console_buffer, napi_env, env, napi_callback_info, info)
{
    size_t argc = 1; napi_value argv[1] = { 0 };
    BQ_NAPI_CALL(env, nullptr, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc < 1) { napi_throw_type_error(env, NULL, "callback required"); return NULL; }

    _napi_fetch_ctx ctx;
    ctx.env = env;
    ctx.js_cb = argv[0];

    bool ok = bq::api::__api_fetch_and_remove_console_buffer(
        _napi_console_buffer_fetch_callback,
        &ctx);

    return bq::_make_bool(env, ok);
}
    



#endif // BQ_NAPI