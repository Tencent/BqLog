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
#include "bq_log/log/appender/appender_file_binary.h"
#include "bq_log/log/log_imp.h"

namespace bq {
    bool appender_file_binary::init_impl(const bq::property_value& config_obj)
    {
        if (config_obj["pub_key"].is_string()) {
            enc_type_ = appender_encryption_type::rsa_aes_xor;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
        }
        else {
            enc_type_ = appender_encryption_type::plaintext;
        }
        if (!appender_file_base::init_impl(config_obj)) {
            return false;
        }
        return true;
    }

    bool appender_file_binary::reset_impl(const bq::property_value& config_obj)
    {
        if (!appender_file_base::reset_impl(config_obj)) {
            return false;
        }
        rsa::public_key prev_pub_key = rsa_pub_key_;
        auto prev_encryption_type = enc_type_;
        if (config_obj["pub_key"].is_string()) {
            enc_type_ = appender_encryption_type::rsa_aes_xor;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
        }
        return (enc_type_ == prev_encryption_type)
            && (rsa_pub_key_ == prev_pub_key);
    }

    bool appender_file_binary::parse_exist_log_file(parse_file_context& context)
    {
        uint32_t format_version = get_binary_format_version();
        seek_read_file_absolute(0);

        // parse file type
        auto read_handle = appender_file_base::read_with_cache(sizeof(appender_file_header));
        if (read_handle.len() < sizeof(appender_file_header)) {
            context.log_parse_fail_reason("read appender_file_header failed");
            return false;
        }
        appender_file_header file_head;
        memcpy(&file_head, read_handle.data(), sizeof(file_head));
        if (file_head.version != format_version) {
            context.log_parse_fail_reason("format version incompatible");
            return false;
        }
        if (file_head.format != get_appender_format()) {
            context.log_parse_fail_reason("format incompatible");
            return false;
        }
        // parse first segment information
        cur_read_seg_.end_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        if (!read_to_next_segment()) {
            context.log_parse_fail_reason("parse first appender_file_segment_head failed");
            return false;
        }
        if (cur_read_seg_.enc_type_ != appender_encryption_type::plaintext) {
            return false;
        }
        // parse payload
        read_handle = read_with_cache(sizeof(appender_payload_metadata));
        if (read_handle.len() < sizeof(appender_payload_metadata)) {
            context.log_parse_fail_reason("read appender_payload_metadata failed");
            return false;
        }
        appender_payload_metadata payload_metadata;
        memcpy(&payload_metadata, read_handle.data(), sizeof(payload_metadata));
        if (payload_metadata.use_local_time != time_zone_.is_use_local_time()
            || payload_metadata.gmt_offset_hours != time_zone_.get_gmt_offset_hours()
            || payload_metadata.gmt_offset_minutes != time_zone_.get_gmt_offset_minutes()
            || payload_metadata.time_zone_diff_to_gmt_ms != time_zone_.get_time_zone_diff_to_gmt_ms()
            || payload_metadata.time_zone_str != time_zone_.get_time_zone_str()) {
            context.log_parse_fail_reason("timezone miss match");
            return false;
        }
        if (payload_metadata.category_count != parent_log_->get_categories_count()) {
            context.log_parse_fail_reason("category count miss match");
            return false;
        }
        uint32_t category_count = payload_metadata.category_count;
        for (uint32_t i = 0; i < category_count; ++i) {
            read_handle = read_with_cache(sizeof(uint32_t));
            if (read_handle.len() < sizeof(uint32_t)) {
                context.log_parse_fail_reason("read category name length failed");
                return false;
            }
            uint32_t name_len;
            memcpy(&name_len, read_handle.data(), sizeof(name_len));
            if ((size_t)name_len != parent_log_->get_categories_name()[i].size()) {
                context.log_parse_fail_reason("category name length mismatch");
                return false;
            }
            read_handle = read_with_cache((size_t)name_len);
            if (read_handle.len() < (size_t)name_len) {
                context.log_parse_fail_reason("read category name mismatch");
                return false;
            }
            if (memcmp(read_handle.data(), parent_log_->get_categories_name()[i].c_str(), (size_t)name_len) != 0) {
                context.log_parse_fail_reason("category name mismatch");
                return false;
            }
        }
        return true;
    }

    void appender_file_binary::on_file_open(bool is_new_created)
    {
        appender_file_base::on_file_open(is_new_created);
        if (enc_type_ == appender_encryption_type::rsa_aes_xor) {
            assert(is_new_created && "encrypted file must be created new");
        }
        cur_read_seg_.start_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        cur_read_seg_.end_pos = get_current_file_size();
        if (is_new_created) {
            // write file header and initialize encryption information
            appender_file_header file_head;
            file_head.format = get_appender_format();
            file_head.version = get_binary_format_version();
            direct_write(&file_head, sizeof(file_head), bq::file_manager::seek_option::end, 0);
        }
        append_new_segment(appender_segment_type::normal);
        if (is_new_created) {
            appender_payload_metadata payload_matadata;
            payload_matadata.magic_number[0] = 2;
            payload_matadata.magic_number[1] = 2;
            payload_matadata.magic_number[2] = 7;
            payload_matadata.use_local_time = time_zone_.is_use_local_time();
            payload_matadata.gmt_offset_hours = time_zone_.get_gmt_offset_hours();
            payload_matadata.gmt_offset_minutes = time_zone_.get_gmt_offset_minutes();
            payload_matadata.time_zone_diff_to_gmt_ms = time_zone_.get_time_zone_diff_to_gmt_ms();
            snprintf(payload_matadata.time_zone_str, sizeof(payload_matadata.time_zone_str), "%s", time_zone_.get_time_zone_str().c_str());
            payload_matadata.category_count = parent_log_->get_categories_count();
            auto handle = alloc_write_cache(sizeof(payload_matadata));
            memcpy(handle.data(), &payload_matadata, handle.allcoated_len());
            return_write_cache(handle);
            for (uint32_t i = 0; i < payload_matadata.category_count; ++i) {
                const bq::string& category_name = parent_log_->get_categories_name()[i];
                uint32_t name_len = (uint32_t)category_name.size();
                handle = alloc_write_cache(sizeof(name_len) + static_cast<size_t>(name_len));
                memcpy(handle.data(), &name_len, sizeof(name_len));
                memcpy(handle.data() + static_cast<ptrdiff_t>(sizeof(name_len)), category_name.c_str(), static_cast<size_t>(name_len));
                return_write_cache(handle);
            }
            mark_write_finished();
        }
    }

    void appender_file_binary::flush_cache()
    {
        if (xor_key_blob_.is_empty() || get_pendding_flush_written_size() == 0) {
            appender_file_base::flush_cache();
            return;
        }

#ifndef NDEBUG
        assert((get_xor_key_blob_size() & (get_xor_key_blob_size() - 1)) == 0 && "get_xor_key_blob_size() must be power of two");
#endif
        vernam::vernam_encrypt_32bytes_aligned(
            get_cache_write_ptr_base(),
            get_pendding_flush_written_size(),
            xor_key_blob_.begin(),
            get_xor_key_blob_size(),
            get_current_file_size());
        appender_file_base::flush_cache();
        update_write_cache_padding();
        if (enc_type_ == appender_encryption_type::rsa_aes_xor && get_pendding_flush_written_size() > 0) {
            //revert
            vernam::vernam_encrypt_32bytes_aligned(
                get_cache_write_ptr_base(),
                get_pendding_flush_written_size(),
                xor_key_blob_.begin(),
                get_xor_key_blob_size(),
                get_current_file_size());
        }
    }

    bool appender_file_binary::seek_read_file_absolute(size_t pos)
    {
        if (appender_file_base::seek_read_file_absolute(pos)) {
            
            return true;
        }
        return false;
    }

    void appender_file_binary::seek_read_file_offset(int32_t offset)
    {
        appender_file_base::seek_read_file_offset(offset);
        if (cur_read_seg_.start_pos > get_read_file_pos() || cur_read_seg_.end_pos < get_read_file_pos()) {
            read_to_correct_segment();
        }
    }

    bool appender_file_binary::on_appender_file_recovery_begin() {
        if (!appender_file_base::on_appender_file_recovery_begin()) {
            return false;
        }
        append_new_segment(appender_segment_type::recovery_by_appender);
        return true;
    }

    void appender_file_binary::on_appender_file_recovery_end() {
        appender_file_base::on_appender_file_recovery_end();
    }

    void appender_file_binary::on_log_item_recovery_begin(bq::log_entry_handle& read_handle) {
        appender_file_base::on_log_item_recovery_begin(read_handle);
        append_new_segment(appender_segment_type::recovery_by_log_buffer);
    }
    void appender_file_binary::on_log_item_recovery_end() {
        appender_file_base::on_log_item_recovery_end();
        append_new_segment(appender_segment_type::normal);
    }

    appender_file_base::read_with_cache_handle appender_file_binary::read_with_cache(size_t size)
    {
        uint64_t current_read_pos = static_cast<uint64_t>(get_read_file_pos());
        assert(((current_read_pos >= cur_read_seg_.start_pos) && (current_read_pos <= cur_read_seg_.end_pos)) && "read beyond file size");
        while (current_read_pos == cur_read_seg_.end_pos)
        {
            if (!read_to_next_segment()) {
                return appender_file_base::read_with_cache(0);
            }
            current_read_pos = static_cast<uint64_t>(get_read_file_pos());
        }
        if (current_read_pos + static_cast<uint64_t>(size) > cur_read_seg_.end_pos) {
            size = static_cast<size_t>(cur_read_seg_.end_pos - current_read_pos);
        }
        return appender_file_base::read_with_cache(size);
    }

    bool appender_file_binary::read_to_correct_segment() {
        auto current_file_read_pos_backup = get_read_file_pos();
        cur_read_seg_.end_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        bool found_segment = false;
        while (read_to_next_segment()) {
            if (current_file_read_pos_backup >= cur_read_seg_.start_pos && current_file_read_pos_backup <= cur_read_seg_.end_pos) {
                seek_read_file_absolute(current_file_read_pos_backup);
                found_segment = true;
                break;
            }
        }
        return found_segment;
    }

    bool appender_file_binary::read_to_next_segment()
    {
        auto new_seg_start_pos = cur_read_seg_.end_pos;
        if (new_seg_start_pos == UINT64_MAX) {
            return false;
        }
        if (!seek_read_file_absolute(static_cast<size_t>(new_seg_start_pos))) {
            return false;
        }
        auto read_handle = appender_file_base::read_with_cache(sizeof(appender_file_segment_head));
        if (read_handle.len() < sizeof(appender_file_segment_head)) {
            return false;
        }
        appender_file_segment_head seg_head;
        memcpy(&seg_head, read_handle.data(), sizeof(seg_head));
        if (enc_type_ != seg_head.enc_type) {
            return false;
        }
        if (seg_head.next_seg_pos < new_seg_start_pos + sizeof(appender_file_segment_head)) {
            bq::util::log_device_console(bq::log_level::error, "file format of segment start pos: %" PRIu64 ", invalid segment end pos:%" PRIu64 ", file path:%s"
                , new_seg_start_pos
                , seg_head.next_seg_pos
                , get_file_handle().abs_file_path().c_str());
            return false;
        }
        cur_read_seg_.start_pos = new_seg_start_pos;
        cur_read_seg_.end_pos = seg_head.next_seg_pos;
        cur_read_seg_.enc_type_ = seg_head.enc_type;
        return true;
    }

    void appender_file_binary::append_new_segment(appender_file_binary::appender_segment_type type)
    {
        assert((get_written_size() == 0 || type == appender_segment_type::recovery_by_appender) && "Can't append new segment when written cache is not empty!");
        clear_read_cache();
        cur_read_seg_.end_pos = static_cast<uint64_t>(sizeof(appender_file_header));
        bool has_segment = false;
        while (read_to_next_segment()) {
            has_segment = true;
            // skip to the final_segment
        }
        if (has_segment) {
            uint64_t new_seg_start_pos = static_cast<uint64_t>(get_current_file_size());
            //record the start of current segment on prev segment's head
            direct_write(&new_seg_start_pos, sizeof(new_seg_start_pos), bq::file_manager::seek_option::begin,
                static_cast<int64_t>(cur_read_seg_.start_pos) + static_cast<int64_t>(BQ_POD_RUNTIME_OFFSET_OF(appender_file_segment_head, next_seg_pos)));
        }
        size_t prev_file_size = get_current_file_size();
        appender_file_segment_head new_segment;
        new_segment.enc_type = enc_type_;
        new_segment.next_seg_pos = UINT64_MAX;
        new_segment.seg_type = type;
        direct_write(&new_segment, sizeof(new_segment), bq::file_manager::seek_option::end, 0);

        if (enc_type_ == appender_encryption_type::rsa_aes_xor) {
            aes aes_obj(bq::aes::enum_cipher_mode::AES_CBC, bq::aes::enum_key_bits::AES_256);
            auto aes_key = aes_obj.generate_key();
            if (aes_key.size() != aes_obj.get_key_size()) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl generate AES key failed");
                assert(false);
            }
            if (aes_key[0] == 0) {
                bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));
                // make sure the first byte is not zero, because padding is not implemented in BqLog RSA encryption.
                aes_key[0] = static_cast<uint8_t>((bq::util::rand() % UINT8_MAX) + 1);
            }
            auto aes_iv = aes_obj.generate_iv();
            if (aes_iv.size() != aes_obj.get_iv_size()) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl generate AES iv failed");
                assert(false);
            }
            bq::aes::key_type ciphertext_aes_key;
            if (!bq::rsa::encrypt(rsa_pub_key_, aes_key, ciphertext_aes_key) || ciphertext_aes_key.size() != rsa_pub_key_.n_.size()) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl RSA encrypt AES key failed");
                assert(false);
            }
            decltype(xor_key_blob_) xor_key_blob_plaintext;
            xor_key_blob_plaintext.fill_uninitialized(get_xor_key_blob_size());
            bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));
            uint64_t* xor_key_blob_64 = reinterpret_cast<uint64_t*>(&xor_key_blob_plaintext[0]);
            for (size_t i = 0; i < xor_key_blob_plaintext.size() / sizeof(uint64_t); ++i) {
                xor_key_blob_64[i] = bq::util::rand64();
            }

            bq::array<uint8_t> xor_key_blob_ciphertext;
            if (!aes_obj.encrypt(aes_key, aes_iv, xor_key_blob_plaintext, xor_key_blob_ciphertext)) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl AES encrypt VERNAM key blob failed");
                assert(false);
            }
            direct_write(static_cast<const uint8_t*>(ciphertext_aes_key.begin()), ciphertext_aes_key.size(), bq::file_manager::seek_option::current, 0);
            direct_write(static_cast<const uint8_t*>(aes_iv.begin()), aes_iv.size(), bq::file_manager::seek_option::current, 0);
            direct_write(static_cast<const uint8_t*>(xor_key_blob_ciphertext.begin()), xor_key_blob_ciphertext.size(), bq::file_manager::seek_option::current, 0);
            xor_key_blob_ = bq::move(xor_key_blob_plaintext);

            //make sure segment head size is aligned by DEFAULT_ALIGNMENT when vernam is enabled
            size_t current_file_size = get_current_file_size();
            size_t aligned_offset = (current_file_size - prev_file_size) & (appender_file_base::DEFAULT_BUFFER_ALIGNMENT - 1);
            if (aligned_offset != 0) {
                char padding_buff[appender_file_base::DEFAULT_BUFFER_ALIGNMENT] = { 0 };
                direct_write(padding_buff, appender_file_base::DEFAULT_BUFFER_ALIGNMENT - aligned_offset, bq::file_manager::seek_option::current, 0);
            }
            assert(((get_current_file_size() - prev_file_size) & (appender_file_base::DEFAULT_BUFFER_ALIGNMENT - 1)) == 0);
        }
        update_write_cache_padding();
    }


    void appender_file_binary::update_write_cache_padding()
    {
        if (enc_type_ != appender_encryption_type::rsa_aes_xor || xor_key_blob_.is_empty()) {
            set_cache_write_padding(0);
        }
        else {
            auto file_pos = get_current_file_size();
            auto enc_start_pos = static_cast<size_t>(file_pos & (xor_key_blob_.size() - 1));
            size_t target_align = enc_start_pos & (appender_file_base::DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1));
            size_t current_align = get_cache_write_head_size() & (appender_file_base::DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1));
            uint8_t target_padding = (target_align + appender_file_base::DEFAULT_BUFFER_ALIGNMENT - current_align) & (appender_file_base::DEFAULT_BUFFER_ALIGNMENT - static_cast<size_t>(1));
            set_cache_write_padding(target_padding);
        }
    }
}
