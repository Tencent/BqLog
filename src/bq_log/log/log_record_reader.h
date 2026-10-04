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
#pragma once
#include "bq_log/log/log_types.h"
#include "bq_log/types/buffer/log_buffer.h"

namespace bq {
    // Expands fast records to the standard layout and attaches the HP block's owner thread.
    class log_record_reader {
        static const log_thread_info* get_block_thread_info(const log_buffer& buffer);

    public:
        static bool is_fast_record(const uint8_t* data, uint32_t size);
        static uint64_t get_timestamp(const uint8_t* data, uint32_t size);
        static bool read(log_buffer& buffer, const uint8_t* data, uint32_t size,
            bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>>& converted_data, log_entry_handle& output);
        static bool make_recovery_error(log_buffer& buffer, const uint8_t* data, uint32_t size,
            bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>>& converted_data, log_entry_handle& output);
    };
}
