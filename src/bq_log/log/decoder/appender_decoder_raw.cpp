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
#include "bq_log/log/decoder/appender_decoder_raw.h"
#include "bq_log/log/appender/appender_file_raw.h"

bq::appender_decode_result bq::appender_decoder_raw::init_private()
{
    return appender_decode_result::success;
}

bq::appender_decode_result bq::appender_decoder_raw::decode_private()
{
    decoded_text_.clear();
    auto read_handle = read_with_cache(sizeof(uint32_t));
    if (read_handle.len() == 0) {
        return appender_decode_result::eof;
    }
    if (read_handle.len() != sizeof(uint32_t)) {
        bq::util::log_device_console(log_level::error, "decode raw log file failed, read item size failed");
        return appender_decode_result::failed_io_error;
    }
    uint32_t item_size = *(const uint32_t*)read_handle.data();
    read_handle = read_with_cache(item_size);
    if (read_handle.len() < (size_t)item_size) {
        bq::util::log_device_console(log_level::error, "decode raw log file failed, read item failed, need read size:%d", item_size);
        return appender_decode_result::failed_io_error;
    }
    bq::log_entry_handle item(read_handle.data(), item_size);
    return do_decode_by_log_entry_handle(item);
}

uint32_t bq::appender_decoder_raw::get_binary_format_version() const
{
    return appender_file_raw::format_version;
}
