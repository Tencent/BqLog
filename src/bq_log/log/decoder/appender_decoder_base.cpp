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
#include "bq_log/log/decoder/appender_decoder_base.h"
#include "bq_log/utils/time_zone.h"
#include "bq_log/global/log_vars.h"

static constexpr size_t DECODER_CACHE_READ_DEFAULT_SIZE = 32 * 1024;

namespace bq {
    appender_decode_result appender_decoder_base::init(const file_handle& file, const string& private_key_str)
    {
        file_ = file;
        current_file_size_ = file_manager::instance().get_file_size(file_);
        seek_read_file_absolute(0);
        uint32_t format_version = get_binary_format_version();

        if (current_file_size_ <= sizeof(file_head_) + sizeof(appender_file_binary::appender_file_segment_head) + sizeof(payload_metadata_)) {
            util::log_device_console(log_level::error, "decode log file failed, too small size");
            return appender_decode_result::failed_decode_error;
        }
        read_from_file_directly(&file_head_, sizeof(file_head_));
        if (file_head_.version != format_version) {
            util::log_device_console(log_level::error, "decode log file failed, unsupported binary log format version, tools version:%d, log file version:%d", format_version, file_head_.version);
            return appender_decode_result::failed_decode_error;
        }
        if (!private_key_str.trim().is_empty()) {
            if (!rsa::parse_private_key_pem(private_key_str.trim(), private_key_)) {
                util::log_device_console(log_level::error, "decode log file failed, invalid private key");
                return appender_decode_result::failed_decode_error;
            }
            if (private_key_.n_.size() != 256) {
                util::log_device_console(log_level::error, "decode log file failed, invalid private key size, only rsa 2048 supported");
                return appender_decode_result::failed_decode_error;
            }
        }
        cur_read_seg_.end_pos = sizeof(file_head_);
        appender_decode_result resut = read_to_next_segment();
        if (resut != appender_decode_result::success) {
            util::log_device_console(log_level::error, "decode log file failed, segment parse failed");
            return resut;
        }
        auto read_handle = read_with_cache(sizeof(payload_metadata_));
        memcpy(&payload_metadata_, read_handle.data(), sizeof(payload_metadata_));
        if (payload_metadata_.magic_number[0] != 2 || payload_metadata_.magic_number[1] != 2 || payload_metadata_.magic_number[2] != 7) {
            if (cur_read_seg_.xor_key_blob.is_empty()) {
                util::log_device_console(log_level::error, "decode log file failed, magic number mismatch");
            }
            else {
                util::log_device_console(log_level::error, "decode log file failed, magic number mismatch, please check your private key");
            }
            return appender_decode_result::failed_decode_error;
        }

        for (uint32_t i = 0; i < payload_metadata_.category_count; ++i) {
            uint32_t name_len = 0;
            string name;
            read_handle = read_with_cache(sizeof(name_len));
            if (read_handle.len() < sizeof(name_len)) {
                util::log_device_console(log_level::error, "read category name length failed");
                return appender_decode_result::failed_io_error;
            }
            name_len = *(const uint32_t*)read_handle.data();
            if (name_len > 0) {
                name.fill_uninitialized((size_t)name_len);
                read_handle = read_with_cache((size_t)name_len);
                if (read_handle.len() < (size_t)name_len) {
                    util::log_device_console(log_level::error, "read category name len mismatch");
                    return appender_decode_result::failed_io_error;
                }
                memcpy(&name[0], read_handle.data(), name_len);
            }
            else if (i != 0) {
                util::log_device_console(log_level::error, "category name length mismatch");
                return appender_decode_result::failed_decode_error;
            }
            category_names_.push_back(move(name));
        }
        return init_private();
    }

    appender_decode_result appender_decoder_base::decode()
    {
        return decode_private();
    }

    bool appender_decoder_base::seek_read_file_absolute(size_t pos)
    {
        if (current_file_cursor_ == pos) {
            return true;
        }
        bool result = file_manager::instance().seek(file_, file_manager::seek_option::begin, (int32_t)pos);
        if (result) {
            clear_read_cache();
            current_file_cursor_ = pos;
        }
        return result;
    }

    bool appender_decoder_base::seek_read_file_offset(int32_t offset)
    {
        int64_t final_cache_cursor = static_cast<int64_t>(cache_read_cursor_) + offset;
        if (final_cache_cursor >= 0 && final_cache_cursor <= static_cast<int64_t>(cache_read_.size())) {
            cache_read_cursor_ = static_cast<size_t>(final_cache_cursor);
            return true;
        }
        else {
            return seek_read_file_absolute(static_cast<size_t>(static_cast<size_t_to_int_t>(current_file_cursor_) + offset));
        }
    }

    size_t appender_decoder_base::get_current_file_size()
    {
        return current_file_size_;
    }

    // data() returned by read_with_cache_handle will be invalid after next calling of "read_with_cache"
    appender_decoder_base::read_with_cache_handle appender_decoder_base::read_with_cache(size_t size)
    {
        auto left_size = cache_read_.size() - cache_read_cursor_;
        if (left_size < size) {
            if (left_size == 0) {
                while (static_cast<uint64_t>(current_file_cursor_) == cur_read_seg_.end_pos) {
                    auto result = read_to_next_segment();
                    if (result != appender_decode_result::success) {
                        read_with_cache_handle empty_handle;
                        empty_handle.data_ = nullptr;
                        empty_handle.len_ = 0;
                        return empty_handle;
                    }
                }
            }
            clear_read_cache();
            if (left_size != 0) {
                size_t adjusted_file_cursor = static_cast<size_t>(static_cast<int64_t>(current_file_cursor_) - static_cast<int64_t>(left_size));
                seek_read_file_absolute(adjusted_file_cursor);
            }
            size_t read_offset = 0;
            if (!cur_read_seg_.xor_key_blob.is_empty()) {
                size_t file_pos_alignment = current_file_cursor_ % appender_file_base::DEFAULT_BUFFER_ALIGNMENT;
                read_offset = file_pos_alignment;
            }
            auto total_size = bq::max_value(size + read_offset, DECODER_CACHE_READ_DEFAULT_SIZE);
            cache_read_.clear();
            cache_read_.fill_uninitialized(total_size);
            auto expected_read_size = total_size - read_offset;
            uint64_t seg_left_size = cur_read_seg_.end_pos - static_cast<uint64_t>(current_file_cursor_);
            if (static_cast<uint64_t>(expected_read_size) > seg_left_size) {
                expected_read_size = static_cast<size_t>(seg_left_size);
            }
            auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + static_cast<ptrdiff_t>(read_offset), expected_read_size);
            current_file_cursor_ += read_size;
            cache_read_cursor_ = read_offset;  
            
            if (read_size < total_size - read_offset) {
                cache_read_.erase(cache_read_.begin() + static_cast<ptrdiff_t>(read_offset + read_size), total_size - read_offset - read_size);
            }
            
            if (!cur_read_seg_.xor_key_blob.is_empty() && read_size > 0) {
                size_t file_offset_start = current_file_cursor_ - read_size;
                vernam::vernam_encrypt_32bytes_aligned(
                    cache_read_.begin() + static_cast<ptrdiff_t>(read_offset),
                    read_size,
                    cur_read_seg_.xor_key_blob.begin(),
                    cur_read_seg_.xor_key_blob.size(),
                    file_offset_start);
            }
        }
        read_with_cache_handle result;
        result.data_ = cache_read_.begin() + static_cast<ptrdiff_t>(cache_read_cursor_);
        result.len_ = bq::min_value(size, cache_read_.size() - cache_read_cursor_);
        cache_read_cursor_ += result.len_;
        return result;
    }

    size_t appender_decoder_base::read_from_file_directly(void* dst, size_t size)
    {
        auto read_size = file_manager::instance().read_file(file_, dst, size);
        current_file_cursor_ += read_size;
        return read_size;
    }

    void appender_decoder_base::clear_read_cache()
    {
        cache_read_cursor_ = 0;
        cache_read_.clear();
        if (cache_read_.capacity() > DECODER_CACHE_READ_DEFAULT_SIZE) {
            cache_read_.shrink();
        }
    }

    appender_decode_result appender_decoder_base::do_decode_by_log_entry_handle(const log_entry_handle& item)
    {
        time_zone time_zone_tmp(payload_metadata_.use_local_time, payload_metadata_.gmt_offset_hours, payload_metadata_.gmt_offset_minutes, payload_metadata_.time_zone_diff_to_gmt_ms, payload_metadata_.time_zone_str);
        auto layout_result = layout_.do_layout(item, time_zone_tmp, &category_names_);
        if (layout_result != layout::enum_layout_result::finished) {
            util::log_device_console(log_level::error, "decode compressed log file failed, layout error code:%d", (int32_t)layout_result);
            return appender_decode_result::failed_decode_error;
        }
        if (layout_.get_formated_str_len() > 0) {
            decoded_text_.insert_batch(decoded_text_.end(), layout_.get_formated_str(), layout_.get_formated_str_len());
            layout_.tidy_memory();
        }
        return appender_decode_result::success;
    }

    appender_decode_result appender_decoder_base::read_to_next_segment()
    {
        auto new_seg_start_pos = cur_read_seg_.end_pos;
        if (!seek_read_file_absolute(static_cast<size_t>(new_seg_start_pos))) {
            return appender_decode_result::eof;
        }
        bq::appender_file_binary::appender_file_segment_head seg_head;
        auto read_size = read_from_file_directly(&seg_head, sizeof(seg_head));
        if (read_size < sizeof(seg_head)) {
            return appender_decode_result::eof;
        }
        if (seg_head.next_seg_pos < current_file_cursor_) {
            bq::util::log_device_console(bq::log_level::error, "file format of segment start pos: %" PRIu64 ", invalid segment end pos:%" PRIu64
                , new_seg_start_pos
                , seg_head.next_seg_pos);
            return appender_decode_result::eof;
        }
        if (seg_head.enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor && seg_head.has_key) {
            if (private_key_.n_.size() != 256) {
                bq::util::log_device_console(bq::log_level::error, "It's an encrypted log file, private key is not specified");
                return appender_decode_result::failed_decode_error;
            }
            bq::array<uint8_t> enc_data;
            enc_data.fill_uninitialized(appender_file_binary::get_encryption_keys_size());
            read_size = read_from_file_directly(static_cast<uint8_t*>(enc_data.begin()), enc_data.size());
            if (read_size < enc_data.size()) {
                util::log_device_console(log_level::error, "decode log file failed, read encryption info failed");
                return appender_decode_result::failed_decode_error;
            }
            aes::key_type aes_key;
            aes::iv_type aes_iv;
            aes::key_type aes_key_ciphertext;
            aes aes_obj(aes::enum_cipher_mode::AES_CBC, aes::enum_key_bits::AES_256);
            size_t aes_key_ciphertext_size = private_key_.n_.size();
            size_t aes_iv_size = aes_obj.get_iv_size();
            aes_key_ciphertext.insert_batch(aes_key_ciphertext.end(), static_cast<uint8_t*>(enc_data.begin()), aes_key_ciphertext_size);
            aes_iv.insert_batch(aes_iv.end(), static_cast<uint8_t*>(enc_data.begin()) + aes_key_ciphertext_size, aes_iv_size);
            if (!rsa::decrypt(private_key_, aes_key_ciphertext, aes_key)) {
                util::log_device_console(log_level::error, "decode log file failed, decrypt aes key failed");
                return appender_decode_result::failed_decode_error;
            }
            cur_read_seg_.xor_key_blob.clear();
            cur_read_seg_.xor_key_blob.fill_uninitialized(appender_file_binary::get_xor_key_blob_size());
            if (!aes_obj.decrypt(aes_key, aes_iv, static_cast<uint8_t*>(enc_data.begin()) + aes_key_ciphertext_size + aes_iv_size, appender_file_binary::get_xor_key_blob_size(), cur_read_seg_.xor_key_blob.begin(), appender_file_binary::get_xor_key_blob_size())) {
                util::log_device_console(log_level::error, "decode log file failed, decrypt VERNAM key failed");
                return appender_decode_result::failed_decode_error;
            }
        }
        auto last_seg_type = (new_seg_start_pos == sizeof(file_head_) ? appender_file_binary::appender_segment_type::normal : cur_read_seg_.seg_type);
        cur_read_seg_.enc_type = seg_head.enc_type;
        cur_read_seg_.start_pos = new_seg_start_pos;
        cur_read_seg_.end_pos = seg_head.next_seg_pos;
        cur_read_seg_.seg_type = seg_head.seg_type;

        bool is_last_seg_recover = (last_seg_type == appender_file_binary::appender_segment_type::recovery_by_appender
            || last_seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer);
        bool is_cur_seg_recover = (cur_read_seg_.seg_type == appender_file_binary::appender_segment_type::recovery_by_appender
            || cur_read_seg_.seg_type == appender_file_binary::appender_segment_type::recovery_by_log_buffer);
        if (is_last_seg_recover != is_cur_seg_recover) {
            if (is_cur_seg_recover) {
                decoded_text_.insert_batch(decoded_text_.end(), log_global_vars::get().log_recover_start_str_, strlen(log_global_vars::get().log_recover_start_str_));
                decoded_text_.push_back('\n');
            }
            else if (is_last_seg_recover) {
                decoded_text_.insert_batch(decoded_text_.end(), log_global_vars::get().log_recover_end_str_, strlen(log_global_vars::get().log_recover_end_str_));
                decoded_text_.push_back('\n');
            }
        }
        return appender_decode_result::success;
    }

}
