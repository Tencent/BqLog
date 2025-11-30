#pragma once
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
 *
 * appender_file_binary.h
 * -----------------------------------------------------------------------------
 * Overview
 * - Compact binary container used by the appender subsystem.
 * - The file begins with a fixed file header, followed by an encryption header.
 *   If encryption is enabled, keying material immediately follows; otherwise the
 *   payload starts right after the encryption header. The payload itself starts
 *   with a small metadata header.
 *
 * Conventions
 * - All multi-byte integers are little-endian unless stated otherwise.
 * - All structures are packed (BQ_PACK_BEGIN/END). The explicit padding fields
 *   must be written/read as-is; do not rely on compiler-inserted padding.
 * - Versioning is governed by appender_file_header.version. New fields must be
 *   appended; readers should use the version and known struct sizes to validate.
 *
 *
 * Top-level binary layout
 * -----------------------------------------------------------------------------
 *  Offset  Size   Field/Block
 *  ------  -----  -------------------------------------------------------------
 *  0x0000     4   uint32_t version                      (appender_file_header)
 *  0x0004     1   appender_format_type format           (1=raw, 2=compressed)
 *  0x0005     3   char padding[3]
 *
 *  0x0008     1   appender_encryption_type encryption_type  (appender_encryption_header)
 *  0x0009     3   char padding[3]
 *
 *  -- If encryption_type == rsa_aes_xor, the following keying material appears: --
 *  0x000C   256   RSA-2048 ciphertext of AES_256 key (exactly 256 bytes)
 *  0x010C    16   AES IV in plaintext (16 bytes)
 *  0x011C  32768  AES-encrypted XOR key blob (32 KiB)
 *
 *  -- Otherwise (encryption_type == plaintext), the payload starts at 0x000C. --
 *
 *  Payload (always begins with appender_payload_metadata)
 *  Offset  Size   Field/Block
 *  ------  -----  -------------------------------------------------------------
 *    +0x00    1   bool is_gmt                (0 = local time, 1 = GMT/UTC)
 *    +0x01    3   uint8_t magic_number[3]       (should be "2, 2, 7" which is used to check whether decryption is success)
 *    +0x04    4   uint32_t category_count
 *    +0x08   ...  Category data and the rest of the payload (see appender_file_binary::parse_exist_log_file)
 *
 *
 */
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/log_types.h"

namespace bq {
    class appender_file_binary : public appender_file_base {
    public:
        enum class appender_format_type : uint8_t {
            raw = 1,
            compressed
        };
        enum class appender_encryption_type : uint8_t {
            plaintext = 1,
            rsa_aes_xor
        };

        BQ_PACK_BEGIN
        struct appender_file_header {
            uint32_t version;
            appender_format_type format;
            char padding[3];
        } BQ_PACK_END

            BQ_PACK_BEGIN struct appender_encryption_header {
            appender_encryption_type encryption_type;
            char padding[7];
        } BQ_PACK_END

            BQ_PACK_BEGIN struct appender_payload_metadata {
            char magic_number[3];
            bool use_local_time;
            int32_t gmt_offset_hours;
            int32_t gmt_offset_minutes;
            int64_t time_zone_diff_to_gmt_ms;
            char time_zone_str[32];
            uint32_t category_count;
    } BQ_PACK_END public : bq_forceinline static size_t get_xor_key_blob_size()
        {
            return 32 * 1024; // 32 KiB
        }
        bq_forceinline static size_t get_encryption_keys_size()
        {
            return 256 // size of RSA-2048 ciphertext of AES key
                + 16 // size of AES IV in plaintext
                + get_xor_key_blob_size(); // size of AES-encrypted XOR key blob
        }
        bq_forceinline static size_t get_encryption_head_size()
        {
            return sizeof(appender_encryption_header);
        }
        bq_forceinline static size_t get_encryption_base_pos()
        {
            return sizeof(appender_file_header)
                + get_encryption_head_size()
                + get_encryption_keys_size();
        }

    public:
        static void xor_stream_inplace_u64_aligned(uint8_t* buf, size_t len, const uint8_t* key, size_t key_size_pow2, size_t key_stream_offset);

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;
        virtual bool reset_impl(const bq::property_value& config_obj) override;
        virtual bool parse_exist_log_file(parse_file_context& context) override;
        virtual void on_file_open(bool is_new_created) override;
        virtual void flush_cache() override;
        virtual appender_format_type get_appender_format() const = 0;
        virtual uint32_t get_binary_format_version() const = 0;
        virtual void before_recover() override;
        virtual void after_recover() override;

    private:
        appender_encryption_type encryption_type_ = appender_encryption_type::plaintext;
        bq::rsa::public_key rsa_pub_key_;
        bq::array<uint8_t, bq::aligned_allocator<uint8_t, sizeof(uint64_t)>> xor_key_blob_;
        size_t encryption_start_pos_ = 0;
    };
}
