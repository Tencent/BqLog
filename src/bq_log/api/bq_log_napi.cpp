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

 // N-API bindings for BqLog core. Module init/registration has been moved to
 // src/bq_common/platform/napi_misc.cpp as requested.


#include <stddef.h>
#include "bq_log/bq_log.h"
#include "bq_log/global/vars.h"

#if defined(BQ_NAPI)
#include "bq_common/bq_common.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/types/buffer/log_buffer.h"

namespace bq {
    BQ_TLS_NON_POD(bq::u16string, _tls_utf16_str);
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
    static inline napi_value _make_i64(napi_env env, int64_t x) {
        napi_value v; napi_create_bigint_int64(env, x, &v); return v;
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

    static inline bq::napi_str_result<bq::u16string, 256> read_utf16_str_tls(napi_env env, napi_value v, size_t u16string_bytes) {
        auto& tls_str = bq::_tls_utf16_str.get();
        if (!v) {
            tls_str.clear();
            return bq::napi_str_result<bq::u16string, 256>(tls_str);
        }
        if (tls_str.is_empty()) {
            tls_str.fill_uninitialized(10);
        }
        size_t len16 = (u16string_bytes >> 1);
        if (u16string_bytes == SIZE_MAX) {
            napi_get_value_string_utf16(env, v, nullptr, 0, &len16);
        }
        tls_str.clear();
        if (len16 > 0) {
            tls_str.fill_uninitialized(len16);
            size_t got16 = 0;
            napi_get_value_string_utf16(env, v, tls_str.begin(), len16 + 1, &got16);
            assert(got16 == len16);
        }
        return bq::napi_str_result<bq::u16string, 256>(tls_str);
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

    static inline napi_value _make_external_arraybuffer(napi_env env, void* data, size_t byte_length) {
        napi_value ab = NULL;
        napi_create_external_arraybuffer(env, data, byte_length, NULL, NULL, &ab);
        return ab;
    }
}
extern "C" {
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
    static bq::miso_ring_buffer console_msg_buffer_(bq::log_buffer_config({"napi_console_cb_msg", bq::array<bq::string>(), 1024 * 8, false, bq::log_memory_policy::block_when_full, 0}));


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
        const char* ver = bq::log::get_version().c_str();
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
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
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
                bq::_free_cstr((char*)category_names[i]);
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
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 2) { napi_throw_type_error(env, NULL, "log_name and config required"); return NULL; }

        char* log_name = bq::_dup_cstr_from_napi(env, argv[0]);
        char* log_config = bq::_dup_cstr_from_napi(env, argv[1]);
        bq::api::__api_log_reset_config(log_name ? log_name : "", log_config ? log_config : "");
        bq::_free_cstr(log_name);
        bq::_free_cstr(log_config);
        return bq::_make_undefined(env);
    }

    // TLS for a single in-flight write session
    static BQ_TLS struct {
        bq::_api_log_buffer_chunk_write_handle write_handle_;
        bq::log_buffer::napi_buffer_info napi_info_;
    } _tls_write_handle_;

    // __api_log_buffer_alloc
    // N-API: log_buffer_alloc(log_id: bigint, length: number, level: number, category_index: number, format_content_utf16: string, utf16_str_bytes_len: number): any[] (length 2)
    BQ_NAPI_DEF(log_buffer_alloc, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 6; napi_value argv[6] = { 0,0,0,0,0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 6) { napi_throw_type_error(env, NULL, "log_id, length, level, category_index, format_content, utf16_str_bytes_len required"); return NULL; }

        uint64_t log_id = bq::_get_u64_from_bigint(env, argv[0]);
        uint32_t length = (uint32_t)bq::_get_u64_from_bigint(env, argv[1]);
        int32_t  level = bq::_get_i32(env, argv[2]);
        uint32_t category_index = bq::_get_u32(env, argv[3]);

        auto handle = bq::api::__api_log_buffer_alloc(static_cast<uint64_t>(log_id), static_cast<uint32_t>(length));
        if (handle.result != bq::enum_buffer_result_code::success) {
            bq::api::__api_log_buffer_commit(static_cast<uint64_t>(log_id), handle);
            return NULL;
        }
        // fetch utf16 format string
        uint32_t utf16_str_bytes_len = (uint32_t)bq::_get_u64_from_bigint(env, argv[5]); // bytes length from caller
        auto utf16_str = bq::read_utf16_str_tls(env, argv[4], static_cast<size_t>(utf16_str_bytes_len));
        _tls_write_handle_.write_handle_ = handle;

        bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
        head->category_idx = (uint32_t)category_index;
        head->level = (uint8_t)level;
        head->log_format_str_type = (uint16_t)bq::log_arg_type_enum::string_utf16_type;

        bq::tools::size_seq<false, const char16_t*> seq;
        seq.get_element().value = (uint32_t)sizeof(uint32_t) + (uint32_t)utf16_str_bytes_len;
        head->log_args_offset = (uint32_t)(sizeof(bq::_log_entry_head_def) + seq.get_total());
        uint8_t* log_format_content_addr = handle.data_addr + sizeof(bq::_log_entry_head_def);
        bq::impl::_do_log_args_fill<false>(log_format_content_addr, seq, (const char16_t*)utf16_str.str().begin());


        bq::log_imp* log = bq::log_manager::get_log_by_id(static_cast<uint64_t>(log_id));
        bq::log_buffer_write_handle inner_handle;
        inner_handle.data_addr = handle.data_addr;
        inner_handle.result = handle.result;
        if (log) {
            auto& log_buffer = log->get_buffer();
            _tls_write_handle_.napi_info_ = log_buffer.get_napi_buffer_info(env, inner_handle);
        }
        *_tls_write_handle_.napi_info_.offset_store_ += (int32_t)head->log_args_offset;
        return _tls_write_handle_.napi_info_.buffer_array_obj_;
    }

    // __api_log_arg_push_utf16_string
    // N-API: log_arg_push_utf16_string(log_id: bigint, offset: number, arg_str: string, arg_utf16_bytes_len: number): void
    BQ_NAPI_DEF(log_arg_push_utf16_string, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 4; napi_value argv[4] = { 0,0,0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 4) { napi_throw_type_error(env, NULL, "log_id, offset, arg_str, arg_utf16_bytes_len required"); return NULL; }

        (void)bq::_get_u64_from_bigint(env, argv[0]); // unused here
        uint32_t offset = bq::_get_u32(env, argv[1]);

        uint32_t arg_utf16_bytes_len = (uint32_t)bq::_get_u64_from_bigint(env, argv[3]);
        auto utf16_str = bq::read_utf16_str_tls(env, argv[2], static_cast<size_t>(arg_utf16_bytes_len));

        bq::tools::size_seq<true, const char16_t*> seq;
        seq.get_element().value = sizeof(uint32_t) + sizeof(uint32_t) + (size_t)arg_utf16_bytes_len;
        uint8_t* write_addr = const_cast<uint8_t*>(_tls_write_handle_.napi_info_.buffer_base_addr_) + (ptrdiff_t)offset;
        bq::impl::_do_log_args_fill<true>(write_addr, seq, (const char16_t*)utf16_str.str().begin());

        return bq::_make_undefined(env);
    }

    // __api_log_buffer_commit
    // N-API: log_buffer_commit(log_id: bigint): void
    BQ_NAPI_DEF(log_buffer_commit, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
        uint64_t log_id = bq::_get_u64_from_bigint(env, argv[0]);
        bq::api::__api_log_buffer_commit(static_cast<uint64_t>(log_id), _tls_write_handle_.write_handle_);
        return bq::_make_undefined(env);
    }

    // set_appenders_enable(log_id: bigint, appender_name: string, enable: boolean): void
    BQ_NAPI_DEF(set_appenders_enable, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 3; napi_value argv[3] = { 0,0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 3) { napi_throw_type_error(env, NULL, "id, appender_name, enable required"); return NULL; }

        uint64_t log_id = bq::_get_u64_from_bigint(env, argv[0]);
        char* appender_name = bq::_dup_cstr_from_napi(env, argv[1]);
        bool enable = bq::_get_bool(env, argv[2]);

        bq::api::__api_set_appenders_enable(log_id, appender_name ? appender_name : "", enable);
        bq::_free_cstr(appender_name);
        return bq::_make_undefined(env);
    }

    // get_logs_count(): bigint
    BQ_NAPI_DEF(get_logs_count, napi_env, env, napi_callback_info, info)
    {
        (void)info;
        uint64_t c = bq::api::__api_get_logs_count();
        return bq::_make_u64(env, c);
    }

    // get_log_id_by_index(index: number): bigint
    BQ_NAPI_DEF(get_log_id_by_index, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "index required"); return NULL; }
        uint32_t idx = bq::_get_u32(env, argv[0]);
        uint64_t id = bq::api::__api_get_log_id_by_index(idx);
        return bq::_make_u64(env, id);
    }

    // get_log_name_by_id(log_id: bigint): string|null
    BQ_NAPI_DEF(get_log_name_by_id, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }

        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        bq::_api_string_def name_def;
        if (!bq::api::__api_get_log_name_by_id(id, &name_def)) {
            return bq::_make_undefined(env);
        }
        return bq::_make_str_utf8(env, name_def.str);
    }

    // get_log_categories_count(log_id: bigint): bigint
    BQ_NAPI_DEF(get_log_categories_count, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        uint32_t count = bq::api::__api_get_log_categories_count(id);
        return bq::_make_u64(env, (uint64_t)count);
    }

    // get_log_category_name_by_index(log_id: bigint, index: number): string|null
    BQ_NAPI_DEF(get_log_category_name_by_index, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 2; napi_value argv[2] = { 0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 2) { napi_throw_type_error(env, NULL, "log_id and index required"); return NULL; }

        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        uint32_t index = bq::_get_u32(env, argv[1]);
        bq::_api_string_def name_def;
        if (!bq::api::__api_get_log_category_name_by_index(id, index, &name_def)) {
            return bq::_make_undefined(env);
        }
        return bq::_make_str_utf8(env, name_def.str);
    }

    // get_log_merged_log_level_bitmap_by_log_id(log_id: bigint): ArrayBuffer (size 4)
    BQ_NAPI_DEF(get_log_merged_log_level_bitmap_by_log_id, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        const uint32_t* bitmap_ptr = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(id);
        return bq::_make_external_arraybuffer(env, (void*)bitmap_ptr, sizeof(uint32_t));
    }

    // get_log_category_masks_array_by_log_id(log_id: bigint): ArrayBuffer (size = categories_count bytes)
    BQ_NAPI_DEF(get_log_category_masks_array_by_log_id, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }

        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        uint32_t count = bq::api::__api_get_log_categories_count(id);
        const uint8_t* mask_array_ptr = bq::api::__api_get_log_category_masks_array_by_log_id(id);
        return bq::_make_external_arraybuffer(env, (void*)mask_array_ptr, (size_t)count * sizeof(uint8_t));
    }

    // get_log_print_stack_level_bitmap_by_log_id(log_id: bigint): ArrayBuffer (size 4)
    BQ_NAPI_DEF(get_log_print_stack_level_bitmap_by_log_id, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        const uint32_t* bitmap_ptr = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(id);
        return bq::_make_external_arraybuffer(env, (void*)bitmap_ptr, sizeof(uint32_t));
    }

    // log_device_console(level: number, content: string): void
    BQ_NAPI_DEF(log_device_console, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 2; napi_value argv[2] = { 0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
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
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "log_id required"); return NULL; }
        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        bq::api::__api_force_flush(id);
        return bq::_make_undefined(env);
    }

    // get_file_base_dir(in_sand_box: boolean): string
    BQ_NAPI_DEF(get_file_base_dir, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "in_sand_box required"); return NULL; }
        bool in_sand_box = bq::_get_bool(env, argv[0]);
        const char* path = bq::api::__api_get_file_base_dir(in_sand_box);
        return bq::_make_str_utf8(env, path);
    }

    // log_decoder_create(path: string, priv_key: string): bigint (negative int64 on error)
    BQ_NAPI_DEF(log_decoder_create, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 2; napi_value argv[2] = { 0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 2) { napi_throw_type_error(env, NULL, "path and priv_key required"); return NULL; }

        char* path = bq::_dup_cstr_from_napi(env, argv[0]);
        char* priv_key = bq::_dup_cstr_from_napi(env, argv[1]);

        uint32_t handle = 0;
        bq::appender_decode_result result = bq::api::__api_log_decoder_create(path ? path : "", priv_key ? priv_key : "", &handle);

        bq::_free_cstr(path);
        bq::_free_cstr(priv_key);

        if (result != bq::appender_decode_result::success) {
            // negative error code in int64
            return bq::_make_i64(env, (int64_t)result * (int64_t)(-1));
        }
        else {
            return bq::_make_i64(env, (int64_t)handle);
        }
    }

    // log_decoder_decode(handle: bigint): { code: number, text: string }
    BQ_NAPI_DEF(log_decoder_decode, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "handle required"); return NULL; }

        uint64_t handle = bq::_get_u64_from_bigint(env, argv[0]);
        bq::_api_string_def text = { 0 };
        bq::appender_decode_result result = bq::api::__api_log_decoder_decode((uint32_t)handle, &text);

        napi_value obj = NULL; napi_create_object(env, &obj);
        napi_value v_code = bq::_make_i32(env, (int32_t)result);
        napi_value v_text = bq::_make_str_utf8(env, (result == bq::appender_decode_result::success) ? text.str : "");
        napi_set_named_property(env, obj, "code", v_code);
        napi_set_named_property(env, obj, "text", v_text);
        return obj;
    }

    // log_decoder_destroy(handle: bigint): void
    BQ_NAPI_DEF(log_decoder_destroy, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "handle required"); return NULL; }
        uint64_t handle = bq::_get_u64_from_bigint(env, argv[0]);
        bq::api::__api_log_decoder_destroy((uint32_t)handle);
        return bq::_make_undefined(env);
    }

    // log_decode(in_path: string, out_path: string, priv_key: string): boolean
    BQ_NAPI_DEF(log_decode, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 3; napi_value argv[3] = { 0,0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
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
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 2) { napi_throw_type_error(env, NULL, "log_id and use_gmt_time required"); return NULL; }

        uint64_t id = bq::_get_u64_from_bigint(env, argv[0]);
        bool use_gmt_time = bq::_get_bool(env, argv[1]);

        bq::_api_string_def snapshot_str_def = { 0 };
        bq::api::__api_take_snapshot_string(id, use_gmt_time, &snapshot_str_def);
        napi_value out = bq::_make_str_utf8(env, snapshot_str_def.str);
        bq::api::__api_release_snapshot_string(id, &snapshot_str_def);
        return out;
    }

    // set_console_callback(enable: boolean, callback?: function): void
    BQ_NAPI_DEF(set_console_callback, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 2; napi_value argv[2] = { 0,0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 2) { 
            napi_throw_type_error(env, NULL, "function count error"); 
            return NULL; 
        }
        if (console_callback_) {
            napi_delete_reference(env, console_callback_);
            console_callback_ = NULL;
        }
        bool enable = bq::_get_bool(env, argv[0]);
        if (!enable) {
            return bq::_make_undefined(env);
        }
        BQ_NAPI_CALL(env, napi_create_reference(env, argv[1], 1, &console_callback_));
        console_msg_buffer_.set_thread_check_enable(false);

        bq::log::register_console_callback(on_console_callback);
        return bq::_make_undefined(env);
    }

    // set_console_buffer_enable(enable: boolean): void
    BQ_NAPI_DEF(set_console_buffer_enable, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 1; napi_value argv[1] = { 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "enable required"); return NULL; }
        bool en = bq::_get_bool(env, argv[0]);
        bq::log::set_console_buffer_enable(en);
        return bq::_make_undefined(env);
    }

    // reset_base_dir(in_sandbox: boolean, dir: string): void
    BQ_NAPI_DEF(reset_base_dir, napi_env, env, napi_callback_info, info)
    {
        size_t argc = 2; napi_value argv[2] = { 0, 0 };
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
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
        BQ_NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
        if (argc < 1) { napi_throw_type_error(env, NULL, "callback required"); return NULL; }

        _napi_fetch_ctx ctx;
        ctx.env = env;
        ctx.js_cb = argv[0];

        bool ok = bq::api::__api_fetch_and_remove_console_buffer(
            _napi_console_buffer_fetch_callback,
            &ctx);

        return bq::_make_bool(env, ok);
    }

} // extern "C"

#endif // BQ_NAPI