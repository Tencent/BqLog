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

bq::appender_decode_result bq::appender_decoder_base::init(const bq::file_handle& file, const bq::string& private_key_str)
{
    file_ = file;
    current_file_size_ = bq::file_manager::instance().get_file_size(file_);
    seek_read_file_absolute(0);
    uint32_t format_version = get_binary_format_version();

    if (current_file_size_ <= sizeof(file_head_) + sizeof(enc_head_) + sizeof(payload_metadata_)) {
        bq::util::log_device_console(log_level::error, "decode log file failed, too small size");
        return appender_decode_result::failed_decode_error;
    }
    auto read_handle = read_with_cache(sizeof(file_head_) + sizeof(enc_head_));
    memcpy(&file_head_, read_handle.data(), sizeof(file_head_));
    memcpy(&enc_head_, read_handle.data() + sizeof(file_head_), sizeof(enc_head_));

    if (file_head_.version != format_version) {
        bq::util::log_device_console(log_level::error, "decode log file failed, unsupported binary log format version, tools version:%d, log file version:%d", format_version, file_head_.version);
        return appender_decode_result::failed_decode_error;
    }
    if (enc_head_.encryption_type == appender_file_binary::appender_encryption_type::rsa_aes_xor) {
        bq::rsa::private_key private_key;
        if (!bq::rsa::parse_private_key_pem(private_key_str, private_key)) {
            bq::util::log_device_console(log_level::error, "decode log file failed, invalid private key");
            return appender_decode_result::failed_decode_error;
        }
        if (private_key.n_.size() != 256) {
            bq::util::log_device_console(log_level::error, "decode log file failed, invalid private key size, only rsa 2048 supported");
            return appender_decode_result::failed_decode_error;
        }
        read_handle = read_with_cache(appender_file_binary::get_encryption_info_size());
        if (read_handle.len() < appender_file_binary::get_encryption_info_size()) {
            bq::util::log_device_console(log_level::error, "decode log file failed, read encryption info failed");
            return appender_decode_result::failed_decode_error;
        }
        bq::aes::key_type aes_key;
        bq::aes::iv_type aes_iv;
        bq::aes::key_type aes_key_ciphertext;
        bq::aes aes_obj(bq::aes::enum_cipher_mode::AES_CBC, bq::aes::enum_key_bits::AES_256);
        size_t aes_key_ciphertext_size = private_key.n_.size();
        size_t aes_iv_size = aes_obj.get_iv_size();
        aes_key_ciphertext.insert_batch(aes_key_ciphertext.end(), read_handle.data(), aes_key_ciphertext_size);
        aes_iv.insert_batch(aes_iv.end(), read_handle.data() + aes_key_ciphertext_size, aes_iv_size);
        if (!rsa::decrypt(private_key, aes_key_ciphertext, aes_key)) {
            bq::util::log_device_console(log_level::error, "decode log file failed, decrypt aes key failed");
            return appender_decode_result::failed_decode_error;
        }
        xor_key_blob_.clear();
        xor_key_blob_.fill_uninitialized(appender_file_binary::get_xor_key_blob_size());
        if (!aes_obj.decrypt(aes_key, aes_iv
                            , read_handle.data() + aes_key_ciphertext_size + aes_iv_size
                            , appender_file_binary::get_xor_key_blob_size()
                            , xor_key_blob_.begin()
                            , appender_file_binary::get_xor_key_blob_size())) {
            bq::util::log_device_console(log_level::error, "decode log file failed, decrypt XOR key failed");
            return appender_decode_result::failed_decode_error;
        }
    }
    read_handle = read_with_cache(sizeof(payload_metadata_));
    memcpy(&payload_metadata_, read_handle.data(), sizeof(payload_metadata_));
    if (payload_metadata_.magic_number[0] != 2 || payload_metadata_.magic_number[1] != 2 || payload_metadata_.magic_number[2] != 7) {
        if (xor_key_blob_.is_empty()) {
            bq::util::log_device_console(log_level::error, "decode log file failed, magic number mismatch");
        }
        else {
            bq::util::log_device_console(log_level::error, "decode log file failed, magic number mismatch, please check your private key");
        }
        return appender_decode_result::failed_decode_error;
    }

    for (uint32_t i = 0; i < payload_metadata_.category_count; ++i) {
        uint32_t name_len = 0;
        bq::string name;
        read_handle = read_with_cache(sizeof(name_len));
        if (read_handle.len() < sizeof(name_len)) {
            bq::util::log_device_console(log_level::error, "read category name length failed");
            return appender_decode_result::failed_io_error;
        }
        name_len = *(const uint32_t*)read_handle.data();
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
    int64_t final_cache_cursor = (int64_t)cache_read_cursor_ + offset;
    if (final_cache_cursor >= 0 && final_cache_cursor <= (int64_t)cache_read_.size()) {
        cache_read_cursor_ = static_cast<size_t>(final_cache_cursor);
    } else {
        seek_read_file_absolute(static_cast<size_t>(static_cast<size_t_to_int_t>(current_file_cursor_) + offset));
    }
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
        auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + static_cast<ptrdiff_t>(left_size), fill_size);
        cache_read_cursor_ = 0;
        if (read_size < fill_size) {
            cache_read_.erase(cache_read_.begin() + static_cast<ptrdiff_t>(left_size + read_size), fill_size - read_size);
        }
        if (!xor_key_blob_.is_empty() && read_size > 0) {
            bq::appender_file_binary::xor_stream_inplace_u64_aligned(cache_read_.begin() + static_cast<ptrdiff_t>(left_size)
                                        , read_size
                                        , xor_key_blob_.begin()
                                        , xor_key_blob_.size()
                                        , current_file_cursor_ - bq::appender_file_binary::get_encryption_base_pos());
        }
        current_file_cursor_ += read_size;
    }
    read_with_cache_handle result;
    result.data_ = cache_read_.begin() + static_cast<ptrdiff_t>(cache_read_cursor_);
    result.len_ = bq::min_value(size, cache_read_.size() - cache_read_cursor_);
    cache_read_cursor_ += result.len_;
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
    auto layout_result = layout_.do_layout(item, payload_metadata_.is_gmt, &category_names_);
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
