/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

// CPython C Extension bindings for BqLog core.
// Type convention: only fixed-width integer types (int32_t, uint32_t, int64_t, uint64_t, uint8_t)
// are used throughout. No bare int / long / unsigned int.

#include "native_src_bq_log_api_bq_log_python_api.h"
#if defined(BQ_PYTHON)

// Use Python Stable ABI (abi3) targeting Python 3.7+.
#define Py_LIMITED_API 0x03070000

#if defined(BQ_MSVC) && defined(_DEBUG)
#if _DEBUG
#define _BQ_PYTHON_HAD_DEBUG
#undef _DEBUG
#endif
#endif

#include <Python.h>

#if defined(_BQ_PYTHON_HAD_DEBUG)
#define _DEBUG 1
#undef _BQ_PYTHON_HAD_DEBUG
#endif

#include <string.h>
#include <stdlib.h>
#include "native_src_bq_common_bq_common.h"
#include "native_src_bq_common_platform_python_misc.h"
#include "native_include_bq_log_bq_log.h"
#include "native_src_bq_log_log_log_manager.h"

// ----------------------------- console callback -----------------------------

static PyObject* py_console_callback = nullptr;

// Called from C++ worker thread. Must acquire GIL before accessing any Python state.
static void BQ_STDCALL on_console_callback(uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length)
{
    // Acquire GIL first — this serializes against register/unregister
    // which also run under GIL. Do NOT check py_console_callback before
    // acquiring the GIL: that would be a TOCTOU race (the pointer could be
    // cleared and DECREF'd between the check and PyGILState_Ensure).
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* cb = py_console_callback;
    if (!cb) {
        PyGILState_Release(gstate);
        return;
    }
    // INCREF to prevent the callback from being freed if another thread
    // unregisters it while we are executing (after we release the GIL or
    // if __api_unregister_console_callbacks doesn't provide a barrier).
    Py_INCREF(cb);
    PyObject* args = Py_BuildValue("(Kiisi)",
        (unsigned long long)log_id,
        (int)category_idx,
        (int)log_level,
        content,
        (int)length);
    if (args) {
        PyObject* ret = PyObject_CallObject(cb, args);
        Py_XDECREF(ret);
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        Py_DECREF(args);
    }
    Py_DECREF(cb);
    PyGILState_Release(gstate);
}

// ----------------------------- helper: encode Python args into log buffer -----------------------------

static const uint8_t TYPE_NULL    = static_cast<uint8_t>(bq::log_arg_type_enum::null_type);
static const uint8_t TYPE_BOOL    = static_cast<uint8_t>(bq::log_arg_type_enum::bool_type);
static const uint8_t TYPE_INT64   = static_cast<uint8_t>(bq::log_arg_type_enum::int64_type);
static const uint8_t TYPE_DOUBLE  = static_cast<uint8_t>(bq::log_arg_type_enum::double_type);
static const uint8_t TYPE_STR16   = static_cast<uint8_t>(bq::log_arg_type_enum::string_utf16_type);

static inline uint32_t align4(uint32_t v) { return (v + 3u) & ~3u; }

// Pre-calculate args size and cache converted values
struct py_arg_cache {
    enum arg_kind { ARG_NULL, ARG_BOOL, ARG_INT64, ARG_DOUBLE, ARG_STR16 };
    arg_kind kind;
    union {
        bool bool_val;
        int64_t int64_val;
        double double_val;
    };
    char16_t* str_data;
    uint32_t str_byte_len; // byte length of UTF-16 data (excluding null terminator)
};

static bool prepare_arg(PyObject* arg, py_arg_cache* cache) {
    cache->str_data = nullptr;
    cache->str_byte_len = 0;

    if (arg == Py_None) {
        cache->kind = py_arg_cache::ARG_NULL;
        return true;
    }

    if (PyBool_Check(arg)) {
        cache->kind = py_arg_cache::ARG_BOOL;
        cache->bool_val = (arg == Py_True);
        return true;
    }

    if (PyLong_Check(arg)) {
        int32_t overflow = 0;
        int64_t val = PyLong_AsLongLongAndOverflow(arg, &overflow);
        if (overflow == 0 && !PyErr_Occurred()) {
            cache->kind = py_arg_cache::ARG_INT64;
            cache->int64_val = val;
            return true;
        }
        PyErr_Clear();
        // Overflow: fall through to string conversion
    }

    if (PyFloat_Check(arg)) {
        cache->kind = py_arg_cache::ARG_DOUBLE;
        cache->double_val = PyFloat_AsDouble(arg);
        return true;
    }

    // For strings and all other types, convert to UTF-16 string
    PyObject* str_obj = arg;
    bool need_decref = false;
    if (!PyUnicode_Check(arg)) {
        str_obj = PyObject_Str(arg);
        if (!str_obj) {
            PyErr_Clear();
            cache->kind = py_arg_cache::ARG_NULL;
            return true;
        }
        need_decref = true;
    }

    // PyUnicode_AsUTF16String returns bytes with 2-byte BOM prefix
    PyObject* utf16_bytes = PyUnicode_AsUTF16String(str_obj);
    if (need_decref) {
        Py_DECREF(str_obj);
    }
    if (!utf16_bytes) {
        PyErr_Clear();
        cache->kind = py_arg_cache::ARG_NULL;
        return true;
    }

    const char* raw = PyBytes_AsString(utf16_bytes);
    Py_ssize_t raw_len = PyBytes_Size(utf16_bytes);
    // Skip BOM (2 bytes)
    const char* data_start = raw + 2;
    Py_ssize_t data_len = raw_len - 2;

    cache->kind = py_arg_cache::ARG_STR16;
    cache->str_byte_len = static_cast<uint32_t>(data_len);
    if (data_len > 0) {
        cache->str_data = static_cast<char16_t*>(malloc(static_cast<size_t>(data_len)));
        if (cache->str_data) {
            memcpy(cache->str_data, data_start, static_cast<size_t>(data_len));
        } else {
            cache->str_byte_len = 0;
        }
    }

    Py_DECREF(utf16_bytes);
    return true;
}

static uint32_t calc_arg_storage_size(const py_arg_cache* cache) {
    // Storage layout per BqLog binary format (INCLUDE_TYPE_INFO=true).
    // Each arg's storage is 4-byte aligned (align_4).
    //   null:   4 bytes type header only                            -> align4(4)  = 4
    //   bool:   2 bytes type header + 1 byte value = 3             -> align4(3)  = 4
    //   int64:  4 bytes type header + 8 bytes value = 12           -> align4(12) = 12
    //   double: 4 bytes type header + 8 bytes value = 12           -> align4(12) = 12
    //   str16:  4 bytes type header + 4 bytes length + data bytes  -> align4(8 + str_byte_len)
    switch (cache->kind) {
    case py_arg_cache::ARG_NULL:
        return 4u;
    case py_arg_cache::ARG_BOOL:
        return 4u;
    case py_arg_cache::ARG_INT64:
        return 12u;
    case py_arg_cache::ARG_DOUBLE:
        return 12u;
    case py_arg_cache::ARG_STR16:
        return align4(8u + cache->str_byte_len);
    }
    return 0u;
}

static void write_arg_to_buffer(const py_arg_cache* cache, uint8_t* dest) {
    // Binary format matches bq_log_wrapper_tools.h _type_copy<true>
    switch (cache->kind) {
    case py_arg_cache::ARG_NULL: {
        // null: just 4 bytes type header, byte 0 = type
        dest[0] = TYPE_NULL;
        dest[1] = 0; dest[2] = 0; dest[3] = 0;
        break;
    }
    case py_arg_cache::ARG_BOOL: {
        // pod size<=2: 2-byte type header + 1 byte data (total 3, storage 4 with padding)
        dest[0] = TYPE_BOOL;
        dest[1] = 0; // padding
        dest[2] = cache->bool_val ? 1u : 0u;
        dest[3] = 0; // padding to align 4
        break;
    }
    case py_arg_cache::ARG_INT64: {
        // pod size>2: 4-byte type header + 8 bytes data
        dest[0] = TYPE_INT64;
        dest[1] = 0; dest[2] = 0; dest[3] = 0;
        memcpy(dest + 4, &cache->int64_val, 8);
        break;
    }
    case py_arg_cache::ARG_DOUBLE: {
        // pod size>2: 4-byte type header + 8 bytes data
        dest[0] = TYPE_DOUBLE;
        dest[1] = 0; dest[2] = 0; dest[3] = 0;
        memcpy(dest + 4, &cache->double_val, 8);
        break;
    }
    case py_arg_cache::ARG_STR16: {
        // string: 4-byte type header + 4-byte uint32 length + string data
        dest[0] = TYPE_STR16;
        dest[1] = 0; dest[2] = 0; dest[3] = 0;
        memcpy(dest + 4, &cache->str_byte_len, 4);
        if (cache->str_byte_len > 0u && cache->str_data) {
            memcpy(dest + 8, cache->str_data, cache->str_byte_len);
        }
        break;
    }
    }
}

static void free_arg_cache(py_arg_cache* cache) {
    if (cache->kind == py_arg_cache::ARG_STR16 && cache->str_data) {
        free(cache->str_data);
        cache->str_data = nullptr;
    }
}

// ----------------------------- exported Python functions -----------------------------

// get_log_version() -> str
static PyObject* py_get_log_version(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    const char* ver = bq::api::__api_get_log_version();
    return PyUnicode_FromString(ver ? ver : "");
}

// enable_auto_crash_handler() -> None
static PyObject* py_enable_auto_crash_handler(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    bq::api::__api_enable_auto_crash_handler();
    Py_RETURN_NONE;
}

// create_log(name: str, config: str, categories_count: int, categories: list[str] | None) -> int
static PyObject* py_create_log(PyObject* self, PyObject* args) {
    (void)self;
    const char* log_name = nullptr;
    const char* config = nullptr;
    uint32_t categories_count = 0;
    PyObject* categories_list = nullptr;

    if (!PyArg_ParseTuple(args, "ssIO", &log_name, &config, &categories_count, &categories_list)) {
        return nullptr;
    }

    const char** category_names = nullptr;
    // Temporary array to hold PyObject* references that must stay alive
    // until __api_create_log returns.
    PyObject** utf8_objs = nullptr;

    if (categories_list && categories_list != Py_None && PyList_Check(categories_list)) {
        Py_ssize_t list_len = PyList_Size(categories_list);
        if (categories_count > static_cast<uint32_t>(list_len)) {
            categories_count = static_cast<uint32_t>(list_len);
        }
        if (categories_count > 0u) {
            category_names = static_cast<const char**>(
                malloc(sizeof(const char*) * static_cast<size_t>(categories_count)));
            utf8_objs = static_cast<PyObject**>(
                calloc(static_cast<size_t>(categories_count), sizeof(PyObject*)));
            if (category_names && utf8_objs) {
                for (uint32_t i = 0; i < categories_count; ++i) {
                    PyObject* item = PyList_GetItem(categories_list, static_cast<Py_ssize_t>(i));
                    if (PyUnicode_Check(item)) {
                        // PyUnicode_AsUTF8 is not in Stable ABI before 3.13;
                        // use PyUnicode_AsEncodedString + PyBytes_AsString instead.
                        PyObject* utf8_bytes = PyUnicode_AsEncodedString(item, "utf-8", "strict");
                        if (utf8_bytes) {
                            category_names[i] = PyBytes_AsString(utf8_bytes);
                            utf8_objs[i] = utf8_bytes; // prevent premature free
                        } else {
                            category_names[i] = "";
                            PyErr_Clear();
                        }
                    } else {
                        category_names[i] = "";
                    }
                }
            } else {
                categories_count = 0;
            }
        }
    }

    uint64_t log_id = bq::api::__api_create_log(log_name, config, categories_count, category_names);

    // Release temporary utf8 bytes objects
    if (utf8_objs) {
        for (uint32_t i = 0; i < categories_count; ++i) {
            Py_XDECREF(utf8_objs[i]);
        }
        free(utf8_objs);
    }
    if (category_names) {
        free(const_cast<char**>(category_names));
    }

    return PyLong_FromUnsignedLongLong(log_id);
}

// log_reset_config(log_name: str, config: str) -> bool
static PyObject* py_log_reset_config(PyObject* self, PyObject* args) {
    (void)self;
    const char* log_name = nullptr;
    const char* config = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &log_name, &config)) {
        return nullptr;
    }
    bool result = bq::api::__api_log_reset_config(log_name, config);
    return PyBool_FromLong(result ? 1L : 0L);
}

// log_write(log_id: int, level: int, category_idx: int, format_str: str, *args) -> bool
static PyObject* py_log_write(PyObject* self, PyObject* args) {
    (void)self;
    Py_ssize_t nargs = PyTuple_Size(args);
    if (nargs < 4) {
        PyErr_SetString(PyExc_TypeError,
            "log_write requires at least 4 arguments: log_id, level, category_idx, format_str");
        return nullptr;
    }

    uint64_t log_id = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 0));
    if (PyErr_Occurred()) return nullptr;

    int64_t level_raw = PyLong_AsLongLong(PyTuple_GetItem(args, 1));
    if (PyErr_Occurred()) return nullptr;
    uint8_t level = static_cast<uint8_t>(level_raw);

    uint64_t cat_raw = PyLong_AsUnsignedLongLong(PyTuple_GetItem(args, 2));
    if (PyErr_Occurred()) return nullptr;
    uint32_t category_idx = static_cast<uint32_t>(cat_raw);

    PyObject* format_str_obj = PyTuple_GetItem(args, 3);

    // Convert format string to UTF-16
    PyObject* format_utf16 = PyUnicode_AsUTF16String(format_str_obj);
    if (!format_utf16) {
        return nullptr;
    }
    const char* fmt_raw = PyBytes_AsString(format_utf16);
    Py_ssize_t fmt_raw_len = PyBytes_Size(format_utf16);
    // Skip BOM (2 bytes)
    const char* fmt_data = fmt_raw + 2;
    uint32_t fmt_byte_len = static_cast<uint32_t>(fmt_raw_len - 2);

    // Prepare extra args
    Py_ssize_t extra_args_count = nargs - 4;
    py_arg_cache* arg_caches = nullptr;
    uint32_t total_args_size = 0;

    if (extra_args_count > 0) {
        arg_caches = static_cast<py_arg_cache*>(
            calloc(static_cast<size_t>(extra_args_count), sizeof(py_arg_cache)));
        if (!arg_caches) {
            Py_DECREF(format_utf16);
            return PyErr_NoMemory();
        }
        for (Py_ssize_t i = 0; i < extra_args_count; ++i) {
            prepare_arg(PyTuple_GetItem(args, 4 + i), &arg_caches[i]);
            total_args_size += calc_arg_storage_size(&arg_caches[i]);
        }
    }

    // All Python objects have been converted to C types above.
    // Release the GIL for the entire begin→write→finish sequence so that
    // concurrent Python threads can proceed.  The ring buffer's alloc and
    // commit must happen on the same OS thread *without* re-acquiring the
    // GIL in between, otherwise another Python thread could alloc before
    // this thread's commit, corrupting the cursor invariants.
    //
    // NOTE: Py_BEGIN_ALLOW_THREADS / Py_END_ALLOW_THREADS expand to a
    // brace-delimited scope, so all variables used across the boundary
    // must be declared beforehand.
    bq::_api_log_write_handle write_handle;
    bool write_ok = false;

    Py_BEGIN_ALLOW_THREADS

    write_handle = bq::api::__api_log_write_begin(
        log_id,
        level,
        category_idx,
        static_cast<uint8_t>(bq::log_arg_type_enum::string_utf16_type),
        fmt_byte_len,
        nullptr, // zero-copy: we write format data manually
        total_args_size
    );

    if (write_handle.result == bq::enum_buffer_result_code::success) {
        // Write format string data
        if (fmt_byte_len > 0u) {
            memcpy(write_handle.format_data_addr, fmt_data, fmt_byte_len);
        }

        // Write args data
        if (extra_args_count > 0 && arg_caches) {
            uint8_t* args_addr = write_handle.format_data_addr + align4(fmt_byte_len);
            for (Py_ssize_t i = 0; i < extra_args_count; ++i) {
                write_arg_to_buffer(&arg_caches[i], args_addr);
                args_addr += calc_arg_storage_size(&arg_caches[i]);
                free_arg_cache(&arg_caches[i]);
            }
            free(arg_caches);
            arg_caches = nullptr;
        }

        bq::api::__api_log_write_finish(log_id, write_handle);
        write_ok = true;
    }

    Py_END_ALLOW_THREADS

    // Cleanup (back under GIL)
    Py_DECREF(format_utf16);
    if (arg_caches) {
        for (Py_ssize_t i = 0; i < extra_args_count; ++i) {
            free_arg_cache(&arg_caches[i]);
        }
        free(arg_caches);
    }
    if (write_ok) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

// set_appender_enable(log_id: int, appender_name: str, enable: bool) -> None
static PyObject* py_set_appender_enable(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    const char* appender_name = nullptr;
    int32_t enable = 0;
    if (!PyArg_ParseTuple(args, "Ksi", &log_id_ull, &appender_name, &enable)) {
        return nullptr;
    }
    bq::api::__api_set_appender_enable(static_cast<uint64_t>(log_id_ull), appender_name, enable != 0);
    Py_RETURN_NONE;
}

// get_logs_count() -> int
static PyObject* py_get_logs_count(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    uint32_t count = bq::api::__api_get_logs_count();
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(count));
}

// get_log_id_by_index(index: int) -> int
static PyObject* py_get_log_id_by_index(PyObject* self, PyObject* args) {
    (void)self;
    uint32_t index = 0;
    if (!PyArg_ParseTuple(args, "I", &index)) {
        return nullptr;
    }
    uint64_t id = bq::api::__api_get_log_id_by_index(index);
    return PyLong_FromUnsignedLongLong(id);
}

// get_log_name_by_id(log_id: int) -> str | None
static PyObject* py_get_log_name_by_id(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    if (!PyArg_ParseTuple(args, "K", &log_id_ull)) {
        return nullptr;
    }
    bq::_api_string_def name_def = { nullptr, 0 };
    if (!bq::api::__api_get_log_name_by_id(static_cast<uint64_t>(log_id_ull), &name_def)) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name_def.str ? name_def.str : "");
}

// get_log_categories_count(log_id: int) -> int
static PyObject* py_get_log_categories_count(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    if (!PyArg_ParseTuple(args, "K", &log_id_ull)) {
        return nullptr;
    }
    uint32_t count = bq::api::__api_get_log_categories_count(static_cast<uint64_t>(log_id_ull));
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(count));
}

// get_log_category_name_by_index(log_id: int, index: int) -> str | None
static PyObject* py_get_log_category_name_by_index(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    uint32_t index = 0;
    if (!PyArg_ParseTuple(args, "KI", &log_id_ull, &index)) {
        return nullptr;
    }
    bq::_api_string_def name_def = { nullptr, 0 };
    if (!bq::api::__api_get_log_category_name_by_index(static_cast<uint64_t>(log_id_ull), index, &name_def)) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name_def.str ? name_def.str : "");
}

// get_log_merged_log_level_bitmap_by_log_id(log_id: int) -> int
static PyObject* py_get_log_merged_log_level_bitmap_by_log_id(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    if (!PyArg_ParseTuple(args, "K", &log_id_ull)) {
        return nullptr;
    }
    const uint32_t* bitmap = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(
        static_cast<uint64_t>(log_id_ull));
    if (!bitmap) {
        return PyLong_FromUnsignedLong(0UL);
    }
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(*bitmap));
}

// get_log_category_masks_array_by_log_id(log_id: int, category_count: int) -> list[int]
static PyObject* py_get_log_category_masks_array_by_log_id(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    uint32_t category_count = 0;
    if (!PyArg_ParseTuple(args, "KI", &log_id_ull, &category_count)) {
        return nullptr;
    }
    const uint8_t* masks = bq::api::__api_get_log_category_masks_array_by_log_id(
        static_cast<uint64_t>(log_id_ull));
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(category_count));
    if (!list) return nullptr;
    for (uint32_t i = 0; i < category_count; ++i) {
        PyList_SetItem(list, static_cast<Py_ssize_t>(i),
            PyLong_FromLong(masks ? static_cast<long>(masks[i]) : 0L));
    }
    return list;
}

// get_log_print_stack_level_bitmap_by_log_id(log_id: int) -> int
static PyObject* py_get_log_print_stack_level_bitmap_by_log_id(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    if (!PyArg_ParseTuple(args, "K", &log_id_ull)) {
        return nullptr;
    }
    const uint32_t* bitmap = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(
        static_cast<uint64_t>(log_id_ull));
    if (!bitmap) {
        return PyLong_FromUnsignedLong(0UL);
    }
    return PyLong_FromUnsignedLong(static_cast<unsigned long>(*bitmap));
}

// log_device_console(level: int, content: str) -> None
static PyObject* py_log_device_console(PyObject* self, PyObject* args) {
    (void)self;
    int32_t level = 0;
    const char* content = nullptr;
    if (!PyArg_ParseTuple(args, "is", &level, &content)) {
        return nullptr;
    }
    bq::api::__api_log_device_console(static_cast<bq::log_level>(level), content);
    Py_RETURN_NONE;
}

// force_flush(log_id: int) -> None
static PyObject* py_force_flush(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    if (!PyArg_ParseTuple(args, "K", &log_id_ull)) {
        return nullptr;
    }
    Py_BEGIN_ALLOW_THREADS
    bq::api::__api_force_flush(static_cast<uint64_t>(log_id_ull));
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

// get_file_base_dir(base_dir_type: int) -> str
static PyObject* py_get_file_base_dir(PyObject* self, PyObject* args) {
    (void)self;
    int32_t base_dir_type = 0;
    if (!PyArg_ParseTuple(args, "i", &base_dir_type)) {
        return nullptr;
    }
    const char* path = bq::api::__api_get_file_base_dir(base_dir_type);
    return PyUnicode_FromString(path ? path : "");
}

// reset_base_dir(base_dir_type: int, dir: str) -> None
static PyObject* py_reset_base_dir(PyObject* self, PyObject* args) {
    (void)self;
    int32_t base_dir_type = 0;
    const char* dir = nullptr;
    if (!PyArg_ParseTuple(args, "is", &base_dir_type, &dir)) {
        return nullptr;
    }
    bq::api::__api_reset_base_dir(base_dir_type, dir);
    Py_RETURN_NONE;
}

// log_decoder_create(path: str, priv_key: str) -> tuple(int, int)
static PyObject* py_log_decoder_create(PyObject* self, PyObject* args) {
    (void)self;
    const char* path = nullptr;
    const char* priv_key = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &path, &priv_key)) {
        return nullptr;
    }
    uint32_t handle = 0;
    bq::appender_decode_result result;
    Py_BEGIN_ALLOW_THREADS
    result = bq::api::__api_log_decoder_create(path, priv_key, &handle);
    Py_END_ALLOW_THREADS
    return Py_BuildValue("(iI)", static_cast<int>(result), handle);
}

// log_decoder_decode(handle: int) -> tuple(int, str)
static PyObject* py_log_decoder_decode(PyObject* self, PyObject* args) {
    (void)self;
    uint32_t handle = 0;
    if (!PyArg_ParseTuple(args, "I", &handle)) {
        return nullptr;
    }
    bq::_api_string_def text = { nullptr, 0 };
    bq::appender_decode_result result;
    Py_BEGIN_ALLOW_THREADS
    result = bq::api::__api_log_decoder_decode(handle, &text);
    Py_END_ALLOW_THREADS
    const char* text_str = (result == bq::appender_decode_result::success && text.str) ? text.str : "";
    return Py_BuildValue("(is)", static_cast<int>(result), text_str);
}

// log_decoder_destroy(handle: int) -> None
static PyObject* py_log_decoder_destroy(PyObject* self, PyObject* args) {
    (void)self;
    uint32_t handle = 0;
    if (!PyArg_ParseTuple(args, "I", &handle)) {
        return nullptr;
    }
    Py_BEGIN_ALLOW_THREADS
    bq::api::__api_log_decoder_destroy(handle);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

// log_decode(in_path: str, out_path: str, priv_key: str) -> bool
static PyObject* py_log_decode(PyObject* self, PyObject* args) {
    (void)self;
    const char* in_path = nullptr;
    const char* out_path = nullptr;
    const char* priv_key = nullptr;
    if (!PyArg_ParseTuple(args, "sss", &in_path, &out_path, &priv_key)) {
        return nullptr;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
    ok = bq::api::__api_log_decode(in_path, out_path, priv_key);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ok ? 1L : 0L);
}

// register_console_callback(callback) -> None
static PyObject* py_register_console_callback(PyObject* self, PyObject* args) {
    (void)self;
    PyObject* callback = nullptr;
    if (!PyArg_ParseTuple(args, "O", &callback)) {
        return nullptr;
    }
    if (callback == Py_None) {
        if (py_console_callback) {
            bq::api::__api_unregister_console_callbacks(on_console_callback);
            Py_DECREF(py_console_callback);
            py_console_callback = nullptr;
        }
    } else if (PyCallable_Check(callback)) {
        if (py_console_callback) {
            bq::api::__api_unregister_console_callbacks(on_console_callback);
            Py_DECREF(py_console_callback);
        }
        py_console_callback = callback;
        Py_INCREF(py_console_callback);
        bq::api::__api_register_console_callbacks(on_console_callback);
    } else {
        PyErr_SetString(PyExc_TypeError, "callback must be callable or None");
        return nullptr;
    }
    Py_RETURN_NONE;
}

// unregister_console_callback() -> None
static PyObject* py_unregister_console_callback(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    if (py_console_callback) {
        bq::api::__api_unregister_console_callbacks(on_console_callback);
        Py_DECREF(py_console_callback);
        py_console_callback = nullptr;
    }
    Py_RETURN_NONE;
}

// set_console_buffer_enable(enable: bool) -> None
static PyObject* py_set_console_buffer_enable(PyObject* self, PyObject* args) {
    (void)self;
    int32_t enable = 0;
    if (!PyArg_ParseTuple(args, "p", &enable)) {
        return nullptr;
    }
    bq::api::__api_set_console_buffer_enable(enable != 0);
    Py_RETURN_NONE;
}

// Console buffer fetch callback context
struct py_fetch_ctx {
    PyObject* py_callback;
};

// Console buffer fetch callback — called SYNCHRONOUSLY by
// __api_fetch_and_remove_console_buffer on the calling thread,
// which already holds the GIL.  Do NOT wrap the call site in
// Py_BEGIN_ALLOW_THREADS or this will crash.
static void BQ_STDCALL py_console_buffer_fetch_callback(
    void* pass_through_param,
    uint64_t log_id,
    int32_t category_idx,
    bq::log_level log_level,
    const char* content,
    int32_t length)
{
    py_fetch_ctx* ctx = static_cast<py_fetch_ctx*>(pass_through_param);
    if (!ctx || !ctx->py_callback) return;

    PyObject* cb_args = Py_BuildValue("(Kiisi)",
        (unsigned long long)log_id,
        (int)category_idx,
        (int)log_level,
        content,
        (int)length);
    if (cb_args) {
        PyObject* ret = PyObject_CallObject(ctx->py_callback, cb_args);
        Py_XDECREF(ret);
        if (PyErr_Occurred()) {
            PyErr_Clear();
        }
        Py_DECREF(cb_args);
    }
}

// fetch_and_remove_console_buffer(callback) -> bool
static PyObject* py_fetch_and_remove_console_buffer(PyObject* self, PyObject* args) {
    (void)self;
    PyObject* callback = nullptr;
    if (!PyArg_ParseTuple(args, "O", &callback)) {
        return nullptr;
    }
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return nullptr;
    }

    py_fetch_ctx ctx;
    ctx.py_callback = callback;

    bool ok = bq::api::__api_fetch_and_remove_console_buffer(py_console_buffer_fetch_callback, &ctx);
    return PyBool_FromLong(ok ? 1L : 0L);
}

// take_snapshot_string(log_id: int, time_zone_config: str) -> str
static PyObject* py_take_snapshot_string(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    const char* tz_config = nullptr;
    if (!PyArg_ParseTuple(args, "Ks", &log_id_ull, &tz_config)) {
        return nullptr;
    }
    bq::_api_string_def snapshot_def = { nullptr, 0 };
    uint64_t log_id = static_cast<uint64_t>(log_id_ull);
    Py_BEGIN_ALLOW_THREADS
    bq::api::__api_take_snapshot_string(log_id, tz_config, &snapshot_def);
    Py_END_ALLOW_THREADS
    PyObject* result = PyUnicode_FromString(snapshot_def.str ? snapshot_def.str : "");
    bq::api::__api_release_snapshot_string(log_id, &snapshot_def);
    return result;
}

// get_stack_trace(skip_frame_count: int) -> str
static PyObject* py_get_stack_trace(PyObject* self, PyObject* args) {
    (void)self;
    uint32_t skip = 0;
    if (!PyArg_ParseTuple(args, "I", &skip)) {
        return nullptr;
    }
    bq::_api_string_def trace_def = { nullptr, 0 };
    bq::api::__api_get_stack_trace(&trace_def, skip);
    return PyUnicode_FromString(trace_def.str ? trace_def.str : "");
}

// is_enable_for(log_id: int, category_idx: int, level: int) -> bool
static PyObject* py_is_enable_for(PyObject* self, PyObject* args) {
    (void)self;
    unsigned long long log_id_ull = 0;
    uint32_t category_idx = 0;
    int32_t level = 0;
    if (!PyArg_ParseTuple(args, "KIi", &log_id_ull, &category_idx, &level)) {
        return nullptr;
    }
    bq::log_imp* log_impl = bq::log_manager::get_log_by_id(static_cast<uint64_t>(log_id_ull));
    if (!log_impl || !log_impl->is_enable_for(category_idx, static_cast<bq::log_level>(level))) {
        Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

// uninit() -> None
// Called during interpreter shutdown. Must unregister the console callback
// to prevent the C++ worker thread from calling into Python after Py_Finalize.
static PyObject* py_uninit(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    // Unregister console callback first (blocks until in-flight callbacks drain)
    if (py_console_callback) {
        bq::api::__api_unregister_console_callbacks(on_console_callback);
        Py_DECREF(py_console_callback);
        py_console_callback = nullptr;
    }
    bq::api::__api_force_flush(0);
    Py_RETURN_NONE;
}

// ----------------------------- module definition -----------------------------

static PyMethodDef bqlog_methods[] = {
    {"get_log_version", py_get_log_version, METH_NOARGS, "Get BqLog version string"},
    {"enable_auto_crash_handler", py_enable_auto_crash_handler, METH_NOARGS, "Enable auto crash handler"},
    {"create_log", py_create_log, METH_VARARGS, "Create a log object"},
    {"log_reset_config", py_log_reset_config, METH_VARARGS, "Reset log configuration"},
    {"log_write", py_log_write, METH_VARARGS, "Write a log entry"},
    {"set_appender_enable", py_set_appender_enable, METH_VARARGS, "Enable/disable an appender"},
    {"get_logs_count", py_get_logs_count, METH_NOARGS, "Get the count of log objects"},
    {"get_log_id_by_index", py_get_log_id_by_index, METH_VARARGS, "Get log id by index"},
    {"get_log_name_by_id", py_get_log_name_by_id, METH_VARARGS, "Get log name by id"},
    {"get_log_categories_count", py_get_log_categories_count, METH_VARARGS, "Get log categories count"},
    {"get_log_category_name_by_index", py_get_log_category_name_by_index, METH_VARARGS, "Get log category name by index"},
    {"get_log_merged_log_level_bitmap_by_log_id", py_get_log_merged_log_level_bitmap_by_log_id, METH_VARARGS, "Get merged log level bitmap"},
    {"get_log_category_masks_array_by_log_id", py_get_log_category_masks_array_by_log_id, METH_VARARGS, "Get category masks array"},
    {"get_log_print_stack_level_bitmap_by_log_id", py_get_log_print_stack_level_bitmap_by_log_id, METH_VARARGS, "Get print stack level bitmap"},
    {"log_device_console", py_log_device_console, METH_VARARGS, "Output to device console"},
    {"force_flush", py_force_flush, METH_VARARGS, "Force flush log buffer"},
    {"get_file_base_dir", py_get_file_base_dir, METH_VARARGS, "Get file base directory"},
    {"reset_base_dir", py_reset_base_dir, METH_VARARGS, "Reset base directory"},
    {"log_decoder_create", py_log_decoder_create, METH_VARARGS, "Create a log decoder"},
    {"log_decoder_decode", py_log_decoder_decode, METH_VARARGS, "Decode a log entry"},
    {"log_decoder_destroy", py_log_decoder_destroy, METH_VARARGS, "Destroy a log decoder"},
    {"log_decode", py_log_decode, METH_VARARGS, "Decode a log file to text file"},
    {"register_console_callback", py_register_console_callback, METH_VARARGS, "Register console callback"},
    {"unregister_console_callback", py_unregister_console_callback, METH_NOARGS, "Unregister console callback"},
    {"set_console_buffer_enable", py_set_console_buffer_enable, METH_VARARGS, "Enable/disable console buffer"},
    {"fetch_and_remove_console_buffer", py_fetch_and_remove_console_buffer, METH_VARARGS, "Fetch and remove console buffer entry"},
    {"take_snapshot_string", py_take_snapshot_string, METH_VARARGS, "Take snapshot string"},
    {"get_stack_trace", py_get_stack_trace, METH_VARARGS, "Get stack trace"},
    {"is_enable_for", py_is_enable_for, METH_VARARGS, "Check if log level is enabled for category"},
    {"uninit", py_uninit, METH_NOARGS, "Uninitialize BqLog (flush all)"},
    {nullptr, nullptr, 0, nullptr}
};

// ----------------------------- auto-registration -----------------------------

// Register all BqLog Python methods into the shared module.
// Other components (e.g. cloud-control modules) can do the same
// with their own static python_method_register instances.
static bq::platform::python_method_register _bqlog_method_register(bqlog_methods);

#endif // BQ_PYTHON
