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
#include <string.h>
#include "bq_log/log/decoder/appender_decoder_base.h"

static constexpr size_t DECODER_CACHE_READ_DEFAULT_SIZE = 32 * 1024;

bq::appender_decode_result bq::appender_decoder_base::init(const bq::file_handle& file)
{
    file_ = file;
    current_file_size_ = bq::file_manager::instance().get_file_size(file_);
    seek_read_file_absolute(0);
    uint32_t format_version = get_binary_format_version();

    if (current_file_size_ <= sizeof(head_)) {
        bq::util::log_device_console(log_level::error, "decode log file failed, size less than appender head");
        return appender_decode_result::failed_decode_error;
    }
    auto read_handle = read_with_cache(sizeof(head_));
    memcpy(&head_, read_handle.data(), sizeof(head_));

    if (head_.version != format_version) {
        bq::util::log_device_console(log_level::error, "decode log file failed, unsupported binary log format version, tools version:%d, log file version:%d", format_version, head_.version);
        return appender_decode_result::failed_decode_error;
    }
    for (uint32_t i = 0; i < head_.category_count; ++i) {
        uint32_t name_len = 0;
        bq::string name;
        read_handle = read_with_cache(sizeof(name_len));
        if (read_handle.len() < sizeof(name_len)) {
            bq::util::log_device_console(log_level::error, "read category name length failed");
            return appender_decode_result::failed_io_error;
        }
        name_len = *(uint32_t*)read_handle.data();
        if (name_len > 0) {
            name.fill_uninitialized((size_t)name_len);
            read_handle = read_with_cache((size_t)name_len);
            if (read_handle.len() < (size_t)name_len) {
                bq::util::log_device_console(log_level::error, "read category name len mismatch");
                return appender_decode_result::failed_io_error;
            }
            memcpy(&name[0], read_handle.data(), name_len);
        } else if (i != 0) {
            bq::util::log_device_console(log_level::error, "category name length mismatch");
            return appender_decode_result::failed_decode_error;
        }
        category_names_.push_back(bq::move(name));
    }
    return init_private();
}

bq::appender_decode_result bq::appender_decoder_base::decode()
{
    return decode_private();
}

void bq::appender_decoder_base::seek_read_file_absolute(size_t pos)
{
    clear_read_cache();
    file_manager::instance().seek(file_, file_manager::seek_option::begin, (int32_t)pos);
    current_file_cursor_ = pos;
}

void bq::appender_decoder_base::seek_read_file_offset(int32_t offset)
{
    int64_t final_cursor = (int64_t)cache_read_cursor_ + offset;
    if (final_cursor >= 0 && final_cursor <= (int64_t)cache_read_.size()) {
        cache_read_cursor_ += offset;
    } else {
        clear_read_cache();
        file_manager::instance().seek(file_, file_manager::seek_option::current, offset);
    }
    current_file_cursor_ += offset;
}

size_t bq::appender_decoder_base::get_current_file_size()
{
    return current_file_size_;
}

// data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
bq::appender_decoder_base::read_with_cache_handle bq::appender_decoder_base::read_with_cache(size_t size)
{
    auto left_size = cache_read_.size() - cache_read_cursor_;
    if (left_size < size) {
        cache_read_.erase(cache_read_.begin(), cache_read_cursor_);
        auto total_size = bq::max_value(size, DECODER_CACHE_READ_DEFAULT_SIZE);
        auto fill_size = total_size - left_size;
        cache_read_.fill_uninitialized(fill_size);
        auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + left_size, fill_size);
        cache_read_cursor_ = 0;
        if (read_size < fill_size) {
            cache_read_.erase(cache_read_.begin() + left_size + read_size, fill_size - read_size);
        }
    }
    read_with_cache_handle result;
    result.data_ = cache_read_.begin() + cache_read_cursor_;
    result.len_ = bq::min_value(size, cache_read_.size() - cache_read_cursor_);
    cache_read_cursor_ += result.len_;
    current_file_cursor_ += result.len_;
    return result;
}

void bq::appender_decoder_base::clear_read_cache()
{
    cache_read_cursor_ = 0;
    cache_read_.clear();
    if (cache_read_.capacity() > DECODER_CACHE_READ_DEFAULT_SIZE) {
        cache_read_.shrink();
    }
}

bq::appender_decode_result bq::appender_decoder_base::do_decode_by_log_entry_handle(const bq::log_entry_handle& item)
{
    auto layout_result = layout_.do_layout(item, head_.is_gmt, &category_names_);
    if (layout_result != layout::enum_layout_result::finished) {
        bq::util::log_device_console(log_level::error, "decode compressed log file failed, layout error code:%d", (int32_t)layout_result);
        return appender_decode_result::failed_decode_error;
    }
    if (layout_.get_formated_str_len() > 0) {
        decoded_text_.fill_uninitialized((size_t)layout_.get_formated_str_len());
        // todo, use string_view to enhance performance
        memcpy(decoded_text_.begin(), layout_.get_formated_str(), layout_.get_formated_str_len());
    } else {
        decoded_text_.clear();
    }
    return appender_decode_result::success;
}

size_t bq::appender_decoder_base::get_next_read_file_pos() const
{
    return current_file_cursor_;
}
