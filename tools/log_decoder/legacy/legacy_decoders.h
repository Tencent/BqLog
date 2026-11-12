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

#include "bq_log/log/decoder/appender_decoder_compressed.h"

namespace bq {
    // Format 9/10 segment layer (RSA-wrapped AES key, 32 KiB XOR mask by absolute file position).
    // v10 adds a key fingerprint; v9 flags key presence in the segment header and reuses the mask.
    class appender_decoder_legacy : public appender_decoder_compressed {
    protected:
        virtual bool is_supported_enc_type(appender_file_binary::appender_encryption_type type) const override;

        virtual bool setup_segment(const appender_file_binary::appender_file_segment_head& head, uint64_t seg_start) override;

        virtual bool decrypt_segment_chunk(uint8_t* data, size_t size, uint64_t file_pos) override;

        virtual size_t get_read_front_padding(uint64_t file_pos) const override;

        virtual bool segment_carries_key(const appender_file_binary::appender_file_segment_head& head) const = 0;

        virtual size_t fingerprint_size() const = 0;

    private:
        bq::array<uint8_t> xor_key_blob_;
        bool masked_ = false;
    };

    class appender_decoder_legacy_v9 : public appender_decoder_legacy {
    protected:
        virtual uint32_t get_binary_format_version() const override
        {
            return 9;
        }

        virtual bool segment_carries_key(const appender_file_binary::appender_file_segment_head& head) const override;

        virtual size_t fingerprint_size() const override
        {
            return 0;
        }
    };

    class appender_decoder_legacy_v10 : public appender_decoder_legacy {
    protected:
        virtual uint32_t get_binary_format_version() const override
        {
            return 10;
        }

        virtual bool segment_carries_key(const appender_file_binary::appender_file_segment_head& head) const override
        {
            return head.enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor;
        }

        virtual size_t fingerprint_size() const override
        {
            return sizeof(uint64_t);
        }
    };
}
