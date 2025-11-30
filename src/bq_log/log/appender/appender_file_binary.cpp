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
            encryption_type_ = appender_encryption_type::rsa_aes_xor;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
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
        auto prev_encryption_type = encryption_type_;
        if (config_obj["pub_key"].is_string()) {
            encryption_type_ = appender_encryption_type::rsa_aes_xor;
            bq::string pub_key_str = config_obj["pub_key"];
            if (!rsa::parse_public_key_ssh(pub_key_str, rsa_pub_key_)) {
                return false;
            }
        }
        return (encryption_type_ == prev_encryption_type)
            && (prev_pub_key == rsa_pub_key_);
    }

    bool appender_file_binary::parse_exist_log_file(parse_file_context& context)
    {
        uint32_t format_version = get_binary_format_version();
        seek_read_file_absolute(0);

        // parse file type
        auto read_handle = read_with_cache(sizeof(appender_file_header));
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
        context.parsed_size += sizeof(file_head);

        // parse encryption information
        read_handle = read_with_cache(sizeof(appender_encryption_header));
        if (read_handle.len() < sizeof(appender_encryption_header)) {
            context.log_parse_fail_reason("read appender_encryption_header failed");
            return false;
        }
        appender_encryption_header enc_head;
        memcpy(&enc_head, read_handle.data(), sizeof(enc_head));
        if (enc_head.encryption_type != appender_encryption_type::plaintext
            || enc_head.encryption_type != encryption_type_) {
            // can not parse encryption type, it's not an error.
            return false;
        }
        context.parsed_size += sizeof(enc_head);

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
        context.parsed_size += sizeof(payload_metadata);
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
            context.parsed_size += sizeof(name_len);
            read_handle = read_with_cache((size_t)name_len);
            if (read_handle.len() < (size_t)name_len) {
                context.log_parse_fail_reason("read category name mismatch");
                return false;
            }
            if (memcmp(read_handle.data(), parent_log_->get_categories_name()[i].c_str(), (size_t)name_len) != 0) {
                context.log_parse_fail_reason("category name mismatch");
                return false;
            }
            context.parsed_size += (size_t)name_len;
        }
        return true;
    }

    void appender_file_binary::on_file_open(bool is_new_created)
    {
        appender_file_base::on_file_open(is_new_created);
        encryption_start_pos_ = 0;
        if (encryption_type_ == appender_encryption_type::rsa_aes_xor) {
            assert(is_new_created && "encrypted file must be created new");
        }
        if (!is_new_created) {
            return;
        }

        // write file header and initialize encryption information
        appender_file_header file_head;
        file_head.format = get_appender_format();
        file_head.version = get_binary_format_version();
        auto handle = alloc_write_cache(sizeof(file_head));
        memcpy(handle.data(), &file_head, handle.allcoated_len());
        return_write_cache(handle);

        appender_encryption_header enc_head;
        enc_head.encryption_type = encryption_type_;
        handle = alloc_write_cache(sizeof(enc_head));
        memcpy(handle.data(), &enc_head, handle.allcoated_len());
        return_write_cache(handle);

        if (encryption_type_ == appender_encryption_type::rsa_aes_xor) {
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
            uint32_t* xor_key_blob_32 = reinterpret_cast<uint32_t*>(&xor_key_blob_plaintext[0]);
            for (size_t i = 0; i < xor_key_blob_plaintext.size() / sizeof(uint32_t); ++i) {
                xor_key_blob_32[i] = bq::util::rand();
            }

            bq::array<uint8_t> xor_key_blob_ciphertext;
            if (!aes_obj.encrypt(aes_key, aes_iv, xor_key_blob_plaintext, xor_key_blob_ciphertext)) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl AES encrypt XOR key blob failed");
                assert(false);
            }

            handle = alloc_write_cache(ciphertext_aes_key.size());
            memcpy(handle.data(), ciphertext_aes_key.begin(), ciphertext_aes_key.size());
            return_write_cache(handle);

            handle = alloc_write_cache(aes_iv.size());
            memcpy(handle.data(), aes_iv.begin(), aes_iv.size());
            return_write_cache(handle);

            handle = alloc_write_cache(xor_key_blob_ciphertext.size());
            memcpy(handle.data(), xor_key_blob_ciphertext.begin(), xor_key_blob_ciphertext.size());
            return_write_cache(handle);

            xor_key_blob_ = bq::move(xor_key_blob_plaintext);
        }

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
        handle = alloc_write_cache(sizeof(payload_matadata));
        memcpy(handle.data(), &payload_matadata, handle.allcoated_len());
        return_write_cache(handle);
        for (uint32_t i = 0; i < payload_matadata.category_count; ++i) {
            const bq::string& category_name = parent_log_->get_categories_name()[i];
            uint32_t name_len = (uint32_t)category_name.size();
            handle = alloc_write_cache(sizeof(name_len) + name_len);
            memcpy(handle.data(), &name_len, sizeof(name_len));
            memcpy(handle.data() + sizeof(name_len), category_name.c_str(), name_len);
            return_write_cache(handle);
        }
        mark_write_finished();
    }

    void appender_file_binary::flush_cache()
    {
        if (xor_key_blob_.is_empty() || get_pendding_flush_size() == 0) {
            appender_file_base::flush_cache();
            return;
        }

#ifndef NDEBUG
        assert(get_encryption_base_pos() % 8 == 0 && "invalid encrypt appender alignment");
        assert((get_xor_key_blob_size() & (get_xor_key_blob_size() - 1)) == 0 && "get_xor_key_blob_size() must be power of two");
#endif
        encryption_start_pos_ = bq::max_value(encryption_start_pos_, get_encryption_base_pos());
        size_t need_encrypt_size = get_current_file_size() + get_pendding_flush_size() - encryption_start_pos_;
        size_t xor_key_blob_start_pos = encryption_start_pos_ - get_encryption_base_pos();
        xor_stream_inplace_u64_aligned(
            get_cache_write_ptr_base() + get_pendding_flush_size() - static_cast<ptrdiff_t>(need_encrypt_size),
            need_encrypt_size,
            xor_key_blob_.begin(),
            get_xor_key_blob_size(),
            xor_key_blob_start_pos);
        encryption_start_pos_ += need_encrypt_size;
        appender_file_base::flush_cache();
    }

    void appender_file_binary::before_recover()
    {
        uint8_t magic_number[256] = {0};
        bq::file_manager::instance().write_file(get_file_handle(), magic_number, sizeof(magic_number), bq::file_manager::seek_option::end, 0);

        appender_encryption_header enc_head;
        enc_head.encryption_type = encryption_type_;
        bq::file_manager::instance().write_file(get_file_handle(), &enc_head, sizeof(enc_head));

        if (enc_head.encryption_type == appender_encryption_type::rsa_aes_xor) {
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
            uint32_t* xor_key_blob_32 = reinterpret_cast<uint32_t*>(&xor_key_blob_plaintext[0]);
            for (size_t i = 0; i < xor_key_blob_plaintext.size() / sizeof(uint32_t); ++i) {
                xor_key_blob_32[i] = bq::util::rand();
            }

            bq::array<uint8_t> xor_key_blob_ciphertext;
            if (!aes_obj.encrypt(aes_key, aes_iv, xor_key_blob_plaintext, xor_key_blob_ciphertext)) {
                bq::util::log_device_console(bq::log_level::error, "appender_file_binary::init_impl AES encrypt XOR key blob failed");
                assert(false);
            }
            bq::file_manager::instance().write_file(get_file_handle(), ciphertext_aes_key.begin(), ciphertext_aes_key.size());
            bq::file_manager::instance().write_file(get_file_handle(), aes_iv.begin(), aes_iv.size());
            bq::file_manager::instance().write_file(get_file_handle(), xor_key_blob_ciphertext.begin(), xor_key_blob_ciphertext.size());
            xor_key_blob_ = bq::move(xor_key_blob_plaintext);
        }
        uint64_t recover_block_data_size = static_cast<uint64_t>(get_pendding_flush_size());
        bq::file_manager::instance().write_file(get_file_handle(), &recover_block_data_size, sizeof(recover_block_data_size));
    }

    void appender_file_binary::after_recover()
    {
        xor_key_blob_.clear();
    }

    void appender_file_binary::xor_stream_inplace_u64_aligned(uint8_t* buf, size_t len, const uint8_t* key, size_t key_size_pow2, size_t key_stream_offset)
    {
        if (len == 0) {
            return;
        }

#ifndef NDEBUG
        assert((key_size_pow2 & (key_size_pow2 - 1)) == 0 && "key_size_pow2 must be power of two");
        assert((key_size_pow2 & 7u) == 0 && "key_size_pow2 should be multiple of 8 for u64 path");
#endif
        const size_t key_mask = key_size_pow2 - 1;
        size_t i = 0;
        size_t head = (8 - (key_stream_offset & 7)) & 7;
        head = bq::min_value(head, len);
        for (; i < head; ++i) {
            buf[i] ^= key[(key_stream_offset + i) & key_mask];
        }
#ifndef NDEBUG
        assert((key_stream_offset + i) % 8 == 0 && "xor_stream_inplace_u64_aligned: alignment process error");
#endif
        size_t n64 = (len - i) / 8;
        if (n64) {
            uint64_t* p64 = reinterpret_cast<uint64_t*>(buf + i);
            const uint64_t* k64 = reinterpret_cast<const uint64_t*>(key);
            const size_t key64_mask = (key_size_pow2 >> 3) - 1;
            size_t key64_idx = ((key_stream_offset + i) >> 3);
            for (size_t j = 0; j < n64; ++j) {
                p64[j] ^= k64[(key64_idx + j) & key64_mask];
            }
            i += n64 * 8;
        }
        for (; i < len; ++i) {
            buf[i] ^= key[(key_stream_offset + i) & key_mask];
        }
    }
}
