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
#include "bq_log/log/decoder/appender_decoder_helper.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/log/appender/appender_file_raw.h"


bool bq::appender_decoder_helper::decode(const bq::string& in_file_path, const bq::string& out_file_path, const bq::string& priv_key)
{
    uint32_t handle = 0;
    auto result = bq::api::__api_log_decoder_create(in_file_path.c_str(), priv_key.c_str(), &handle);
    if (result != bq::appender_decode_result::success) {
        bq::util::log_device_console(log_level::error, "create decoder failed:%d %s", result, in_file_path.c_str());
        return false;
    }

    bq::file_handle output_handle;
    bq::file_manager& file_manager_cache = bq::file_manager::instance();
    string path = TO_ABSOLUTE_PATH(out_file_path, 1);
    output_handle = file_manager_cache.open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
    if (output_handle) {
        file_manager_cache.seek(output_handle, bq::file_manager::seek_option::end, 0);
    } else {
        bq::util::log_device_console(log_level::error, "create out file failed");
        return false;
    }

    while (true) {
        bq::_api_string_def log_text;
        result = bq::api::__api_log_decoder_decode(handle, &log_text);
        if (result == bq::appender_decode_result::eof) {
            break;
        } else if (result != bq::appender_decode_result::success) {
            bq::util::log_device_console(log_level::error, "create decoder failed, result : ");
            return false;
        } else {
            if (output_handle) {
                file_manager_cache.write_file(output_handle, log_text.str, log_text.len);
                file_manager_cache.write_file(output_handle, "\n", 1);
            } else {
            }
        }
    }
    if (output_handle) {
        file_manager_cache.flush_file(output_handle);
    }
    return true;
}
