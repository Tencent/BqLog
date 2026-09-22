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
#include "bq_log/misc/bq_log_api.h"

#if defined(BQ_GO)
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"

namespace bq {
    namespace api {
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
