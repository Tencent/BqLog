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
    // The v9/v10 segment layer. Both versions wrap an AES-256 key with RSA, use that key to decrypt
    // a 32 KiB XOR blob, and mask the payload with the blob addressed by absolute file position.
    // They differ in two places, and both are handled by the two pure virtuals below:
    //   - v10 puts an 8-byte public-key fingerprint in front of the wrapped key, v9 does not;
    //   - v9 signals key presence with a byte in the segment header and lets later segments of the
    //     same file reuse the blob, while v10 either has the blob or is not encrypted at all.
    //
    // Nothing here is part of the runtime: it exists so the standalone tool can read files written
    // before format 11, and it is compiled into that tool only.
    class appender_decoder_legacy : public appender_decoder_compressed {
    protected:
        virtual bool is_supported_enc_type(appender_file_binary::appender_encryption_type type) const override;

        virtual bool setup_segment(const appender_file_binary::appender_file_segment_head& head, uint64_t seg_start) override;

        virtual bool decrypt_segment_chunk(uint8_t* data, size_t size, uint64_t file_pos) override;

        // The XOR routine needs the buffer and the file offset to agree modulo the vector width, so
        // the record layer reserves that many bytes in front of the chunk (see the base class).
        virtual size_t get_read_front_padding(uint64_t file_pos) const override;

        virtual bool segment_carries_key(const appender_file_binary::appender_file_segment_head& head) const = 0;

        virtual size_t fingerprint_size() const = 0;

    private:
        bq::array<uint8_t> xor_key_blob_;
        uint64_t payload_base_ = 0;
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
            // Every encrypted segment of a v10 file carries its own wrapped key; the flag that used
            // to say so was removed from the header in v10 and its byte became padding.
            return head.enc_type == appender_file_binary::appender_encryption_type::rsa_aes_xor;
        }

        virtual size_t fingerprint_size() const override
        {
            return sizeof(uint64_t);
        }
    };
}
