/* Copyright (C) 2025 Tencent.
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
#include "bq_log/log/decoder/appender_decoder_base.h"
#include "bq_log/log/appender/appender_file_compressed.h"

namespace bq {
    class appender_decoder_compressed : public appender_decoder_base {
        struct decoder_log_template {
            uint32_t category_idx = (uint32_t)-1;
            bq::log_level level = bq::log_level::log_level_max;
            uint64_t epoch_ms = (uint64_t)(-1);
            bq::string fmt_string;
        };
        struct decoder_thread_info_template {
            uint64_t thread_id;
            bq::string thread_name;
        };

    protected:
        virtual appender_decode_result init_private() override;

        virtual appender_decode_result decode_private() override;

        virtual uint32_t get_binary_format_version() const override;

        // Format v11 segment layer: RSA-KEM to wrap the per-segment seed, HKDF-SHA256 to derive
        // (K, N), ChaCha addressed by the offset from the payload base.
        virtual bool is_supported_enc_type(appender_file_binary::appender_encryption_type type) const override;

        virtual bool setup_segment(const appender_file_binary::appender_file_segment_head& head, uint64_t seg_start) override;

        virtual bool decrypt_segment_chunk(uint8_t* data, size_t size, uint64_t file_pos) override;

    private:
        bq::tuple<appender_decode_result, appender_file_compressed::item_type, appender_decoder_base::read_with_cache_handle> read_item_data();

        appender_decode_result parse_log_entry(const appender_decoder_base::read_with_cache_handle& read_handle);

        appender_decode_result parse_formate_template(const appender_decoder_base::read_with_cache_handle& read_handle, appender_file_compressed::template_sub_type sub_type);

        appender_decode_result parse_thread_info_template(const appender_decoder_base::read_with_cache_handle& read_handle);

    private:
        uint64_t last_log_entry_epoch_;
        bq::array<decoder_log_template> log_templates_array_;
        bq::hash_map<uint64_t, decoder_thread_info_template> thread_info_templates_map_;
        bq::array<uint8_t> raw_data_;

        // The current segment's key material and the position its keystream counts from. Both are
        // replaced, and wiped, by setup_segment for every segment.
        appender_file_binary::segment_key seg_key_ {};
        uint8_t seg_chacha_rounds_ = 0;
        uint64_t seg_payload_base_ = 0;
        appender_file_binary::appender_encryption_type seg_enc_type_ = appender_file_binary::appender_encryption_type::plaintext;
    };
}
