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
#include "bq_log/log/decoder/appender_decoder_manager.h"

#include "bq_log/global/vars.h"
#include "bq_log/log/decoder/appender_decoder_raw.h"
#include "bq_log/log/decoder/appender_decoder_compressed.h"
#include "bq_log/log/appender/appender_file_binary.h"

bq::appender_decoder_manager::appender_decoder_manager()
    :
#if !BQ_TOOLS
    mutex_(true)
    ,
#endif
    idx_seq_(0)
{
}

bq::appender_decoder_manager::~appender_decoder_manager()
{
}

bq::appender_decoder_manager& bq::appender_decoder_manager::instance()
{
    return *log_global_vars::get().appender_decoder_manager_inst_;
}

bq::appender_decode_result bq::appender_decoder_manager::create_decoder(const bq::string& path, uint32_t& out_handle)
{
    string path_tmp = TO_ABSOLUTE_PATH(path, false);
    auto handle = bq::file_manager::instance().open_file(path_tmp, file_open_mode_enum::read);
    if (!handle.is_valid()) {
        bq::util::log_device_console(log_level::error, "decode log file :%s open failed, error code:%d", path.c_str(), bq::file_manager::get_and_clear_last_file_error());
        return appender_decode_result::failed_io_error;
    }
    bq::appender_file_binary::_binary_appender_head_def head;
    auto read_size = bq::file_manager::instance().read_file(handle, &head, sizeof(head), file_manager::seek_option::begin, 0);
    if (read_size != sizeof(head)) {
        bq::util::log_device_console(log_level::error, "decode log file :%s failed, size less than appender head", path.c_str());
        return appender_decode_result::failed_decode_error;
    }
    bq::unique_ptr<appender_decoder_base> decoder(nullptr);
    if (head.format == bq::appender_file_binary::appender_format_type::raw) {
        decoder = bq::make_unique<bq::appender_decoder_raw>();
    } else if (head.format == bq::appender_file_binary::appender_format_type::compressed) {
        decoder = bq::make_unique<bq::appender_decoder_compressed>();
    } else {
        bq::util::log_device_console(log_level::error, "decode log file :%s failed, unrecognized format", path.c_str());
        return appender_decode_result::failed_decode_error;
    }
    bq::appender_decode_result result = decoder->init(handle);
    if (result != appender_decode_result::success) {
        return result;
    }
    out_handle = idx_seq_.add_fetch_seq_cst(1);
#if !BQ_TOOLS
    bq::platform::scoped_mutex lock(mutex_);
#endif
    decoders_map_.add(out_handle, bq::move(decoder));

    return result;
}

void bq::appender_decoder_manager::destroy_decoder(uint32_t handle)
{
#if !BQ_TOOLS
    bq::platform::scoped_mutex lock(mutex_);
#endif
    decoders_map_.erase(handle);
}

bq::appender_decode_result bq::appender_decoder_manager::decode_single_item(uint32_t handle, const bq::string*& out_decoded_log_text)
{
#if !BQ_TOOLS
    bq::platform::scoped_mutex lock(mutex_);
#endif
    auto iter = decoders_map_.find(handle);
    if (iter != decoders_map_.end()) {
        auto result = iter->value()->decode();
        if (result == appender_decode_result::success) {
            out_decoded_log_text = &iter->value()->get_decoded_log_text();
        }
        return result;
    }
    return appender_decode_result::failed_invalid_handle;
}
