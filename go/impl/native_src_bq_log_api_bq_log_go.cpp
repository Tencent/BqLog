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
#include "native_include_bq_log_misc_bq_log_api.h"

#if defined(BQ_GO)
#include "native_src_bq_common_bq_common.h"
#include "native_include_bq_log_bq_log.h"

namespace bq {
    namespace api {
        namespace {
            // Values and string pointers arrive as separate cgo parameters.
            // The descriptor array exists only on the native stack.
            struct go_log_arg {
                uint64_t value;
                const char* string;
            };

            // Matches the existing log argument layout. Return zero for an
            // invalid descriptor before any native write handle is allocated.
            uint32_t go_arg_size(log_arg_type_enum type, const go_log_arg& arg)
            {
                switch (type) {
                case log_arg_type_enum::unsupported_type: // zero-value Go Arg is null
                case log_arg_type_enum::null_type:
                case log_arg_type_enum::bool_type:
                case log_arg_type_enum::int8_type:
                case log_arg_type_enum::uint8_type:
                case log_arg_type_enum::int16_type:
                case log_arg_type_enum::uint16_type:
                    return 4;
                case log_arg_type_enum::int32_type:
                case log_arg_type_enum::uint32_type:
                case log_arg_type_enum::float_type:
                    return 8;
                case log_arg_type_enum::pointer_type:
                case log_arg_type_enum::int64_type:
                case log_arg_type_enum::uint64_type:
                case log_arg_type_enum::double_type:
                    return 12;
                case log_arg_type_enum::string_utf8_type:
                    if (arg.value > UINT32_MAX - 11 || (arg.value != 0 && !arg.string)) {
                        return 0;
                    }
                    return 8 + static_cast<uint32_t>(bq::align_4(arg.value));
                default:
                    return 0;
                }
            }

            uint32_t go_log_write_small(uint64_t log_id, uint8_t level, uint32_t category_index,
                uint32_t format_str_bytes_len, const void* format_str_data,
                uint32_t args_count, uint32_t args_types, const go_log_arg* args)
            {
                // Validate before begin: after allocation, every path must finish.
                uint32_t sizes[4];
                uint64_t total_size = 0;
                for (uint32_t i = 0; i < args_count; ++i) {
                    sizes[i] = go_arg_size(static_cast<log_arg_type_enum>((args_types >> (8 * i)) & 0xFF), args[i]);
                    if (sizes[i] == 0) {
                        return static_cast<uint32_t>(enum_buffer_result_code::err_alloc_size_invalid);
                    }
                    total_size += sizes[i];
                }
                if (total_size + format_str_bytes_len + 1024 > UINT32_MAX) {
                    return static_cast<uint32_t>(enum_buffer_result_code::err_alloc_size_invalid);
                }

                auto handle = __api_log_write_begin(log_id, level, category_index,
                    static_cast<uint8_t>(log_arg_type_enum::string_utf8_type),
                    format_str_bytes_len, format_str_data, static_cast<uint32_t>(total_size));
                if (handle.result != enum_buffer_result_code::success) {
                    return static_cast<uint32_t>(handle.result);
                }
                auto* dst = handle.format_data_addr + bq::align_4(format_str_bytes_len);
                for (uint32_t i = 0; i < args_count; ++i) {
                    auto type = static_cast<log_arg_type_enum>((args_types >> (8 * i)) & 0xFF);
                    if (type == log_arg_type_enum::unsupported_type) {
                        type = log_arg_type_enum::null_type;
                    }
                    memset(dst, 0, 4);
                    dst[0] = static_cast<uint8_t>(type);
                    if (type == log_arg_type_enum::string_utf8_type) {
                        const auto length = static_cast<uint32_t>(args[i].value);
                        memcpy(dst + 4, &length, sizeof(length));
                        if (length != 0) {
                            memcpy(dst + 8, args[i].string, length);
                        }
                        memset(dst + 8 + length, 0, sizes[i] - 8 - length);
                    } else if (sizes[i] == 8) {
                        const auto value = static_cast<uint32_t>(args[i].value);
                        memcpy(dst + 4, &value, sizeof(value));
                    } else if (sizes[i] == 12) {
                        memcpy(dst + 4, &args[i].value, sizeof(args[i].value));
                    } else if (type == log_arg_type_enum::int16_type || type == log_arg_type_enum::uint16_type) {
                        const auto value = static_cast<uint16_t>(args[i].value);
                        memcpy(dst + 2, &value, sizeof(value));
                    } else if (type != log_arg_type_enum::null_type) {
                        dst[2] = static_cast<uint8_t>(args[i].value);
                    }
                    dst += sizes[i];
                }
                __api_log_write_finish(log_id, handle);
                return static_cast<uint32_t>(enum_buffer_result_code::success);
            }
        }

        BQ_API uint32_t __api_go_log_write_1(uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_types, uint64_t value0, const char* string0)
        {
            const go_log_arg args[] = { { value0, string0 } };
            return go_log_write_small(log_id, level, category_index, format_str_bytes_len, format_str_data,
                1, args_types, args);
        }

        BQ_API uint32_t __api_go_log_write_2(uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_types, uint64_t value0, const char* string0, uint64_t value1, const char* string1)
        {
            const go_log_arg args[] = { { value0, string0 }, { value1, string1 } };
            return go_log_write_small(log_id, level, category_index, format_str_bytes_len, format_str_data,
                2, args_types, args);
        }

        BQ_API uint32_t __api_go_log_write_4(uint64_t log_id, uint8_t level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_count, uint32_t args_types, uint64_t value0, const char* string0, uint64_t value1, const char* string1, uint64_t value2, const char* string2, uint64_t value3, const char* string3)
        {
            if (args_count < 3 || args_count > 4) {
                return static_cast<uint32_t>(enum_buffer_result_code::err_alloc_size_invalid);
            }
            const go_log_arg args[] = { { value0, string0 }, { value1, string1 }, { value2, string2 }, { value3, string3 } };
            return go_log_write_small(log_id, level, category_index, format_str_bytes_len, format_str_data,
                args_count, args_types, args);
        }

        // Go wrapper entry point. begin + args copy + finish are fused into a
        // single cgo call because a goroutine may migrate to another OS thread
        // between two cgo calls, which would break the TLS-based thread info
        // captured in __api_log_write_begin.
        BQ_API uint32_t __api_go_log_write(uint64_t log_id, uint8_t log_level, uint32_t category_index, uint32_t format_str_bytes_len, const void* format_str_data, uint32_t args_data_bytes_len, const void* args_data)
        {
            bq::_api_log_write_handle handle = __api_log_write_begin(log_id, log_level, category_index, static_cast<uint8_t>(bq::log_arg_type_enum::string_utf8_type), format_str_bytes_len, format_str_data, args_data_bytes_len);
            if (handle.result != bq::enum_buffer_result_code::success) {
                return static_cast<uint32_t>(handle.result);
            }
            if (args_data_bytes_len > 0 && args_data) {
                memcpy(handle.format_data_addr + bq::align_4(format_str_bytes_len), args_data, args_data_bytes_len);
            }
            __api_log_write_finish(log_id, handle);
            return static_cast<uint32_t>(bq::enum_buffer_result_code::success);
        }
    }
}
#endif
