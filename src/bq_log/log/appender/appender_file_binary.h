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
 * - Binary container used by the appender subsystem.
 * - The file consists of a global File Header followed by a linked list of Segments.
 * - Each Segment contains a Segment Header, optional Encryption Info, and Data.
 *
 * Top-level binary layout
 * -----------------------------------------------------------------------------
 *  [File Header]
 *       |
 *  [Segment 1] -> [Segment 2] -> ... -> [Segment N]
 *
 * 1. File Header (Fixed 8 bytes at file offset 0)
 *    Offset  Size  Field
 *    ------  ----  -----------------------------------------------------------
 *    0x0000     4  uint32_t version                      (appender_file_header)
 *    0x0004     1  appender_format_type format           (1=raw, 2=compressed)
 *    0x0005     3  char padding[3]
 *
 * 2. Segment Structure
 *    Each segment begins with a Segment Header, followed by Encryption Info (if enabled),
 *    and then the Segment Payload.
 *
 *    A. Segment Header (12 bytes)
 *       Offset  Size  Field
 *       ------  ----  -------------------------------------------------------
 *       +0x00      8  uint64_t next_seg_pos  (Abs offset of next segment, or UINT64_MAX)
 *       +0x08      1  appender_segment_type seg_type
 *       +0x09      1  appender_encryption_type enc_type
 *       +0x0A      1  bool has_key (Whether encryption keys are present)
 *       +0x0B      1  char padding[2]
 *
 *    B. Encryption Keys (Optional, Present only if enc_type == rsa_aes_xor and only has_key is true in Segment Header)
 *       Offset  Size       Field
 *       ------  ---------  ---------------------------------------------------
 *       +0x00   256        RSA-2048 ciphertext of AES_256 key
 *       +0x100  16         AES IV in plaintext
 *       +0x110  32768      AES-encrypted XOR key blob (32 KiB)
 *
 *    C. Segment Payload (Optional, The content depends on whether it is the First Segment or a subsequent one.)
 *
 *       [First Segment Payload]
 *       Starts with Metadata, followed by Log Entries.
 *
 *       Metadata Header (appender_payload_metadata):
 *       Offset  Size  Field
 *       ------  ----  -------------------------------------------------------
 *       +0x00      3  char magic_number[3] ("2, 2, 7")
 *       +0x03      1  bool use_local_time
 *       +0x04      4  int32_t gmt_offset_hours
 *       +0x08      4  int32_t gmt_offset_minutes
 *       +0x0C      4  int32_t time_zone_diff_to_gmt_ms
 *       +0x10     32  char time_zone_str[32]
 *       +0x30      4  uint32_t category_count
 *
 *       Category Definitions (Repeated category_count times):
 *       +0x00      4  uint32_t name_len
 *       +0x04   name_len  char name_str[]
 *
 *       [Subsequent Segments Payload]
 *       Contains only Log Entries (no Metadata).
 *
 * Conventions
 * - All multi-byte integers are little-endian unless stated otherwise.
 * - All structures are packed (BQ_PACK_BEGIN/END). The explicit padding fields
 *   must be written/read as-is; do not rely on compiler-inserted padding.
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
        enum class appender_segment_type : uint8_t {
            normal = 1,
            recovery_by_appender,
            recovery_by_log_buffer
        };

        BQ_PACK_BEGIN
        struct appender_file_header {
            uint32_t version;
            appender_format_type format;
            char padding[3];
        } BQ_PACK_END
        static_assert(sizeof(appender_file_header) == 8, "appender_file_header size error");

        BQ_PACK_BEGIN
        struct appender_file_segment_head {
            uint64_t next_seg_pos;
            appender_segment_type seg_type;
            appender_encryption_type enc_type;
            bool has_key;
            char padding[1];
        }
        BQ_PACK_END
        static_assert(sizeof(appender_file_segment_head) == 12, "appender_file_header size error");

        //Only exist in first segment
        BQ_PACK_BEGIN 
        struct appender_payload_metadata {
            char magic_number[3];
            bool use_local_time;
            int32_t gmt_offset_hours;
            int32_t gmt_offset_minutes;
            int32_t time_zone_diff_to_gmt_ms;
            char time_zone_str[32];
            uint32_t category_count;
        } 
        BQ_PACK_END

        struct seg_info {
            uint64_t start_pos;
            uint64_t end_pos;
            appender_encryption_type enc_type_;
        };

    public: 
        bq_forceinline static constexpr size_t get_xor_key_blob_size()
        {
            return 32 * 1024; // 32 KiB
        }

        bq_forceinline static constexpr size_t get_encryption_keys_size()
        {
            return 256 // size of RSA-2048 ciphertext of AES key
                + 16 // size of AES IV in plaintext
                + get_xor_key_blob_size(); // size of AES-encrypted XOR key blob
        }

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;
        virtual bool reset_impl(const bq::property_value& config_obj) override;
        virtual bool parse_exist_log_file(parse_file_context& context) override;
        virtual void on_file_open(bool is_new_created) override;
        virtual void flush_write_cache() override;
        virtual bool seek_read_file_absolute(size_t pos) override;
        virtual void seek_read_file_offset(int32_t offset) override;
        virtual appender_format_type get_appender_format() const = 0;
        virtual uint32_t get_binary_format_version() const = 0;
        virtual bool on_appender_file_recovery_begin() override;
        virtual void on_appender_file_recovery_end() override;
        virtual void on_log_item_recovery_begin(bq::log_entry_handle& read_handle) override;
        virtual void on_log_item_recovery_end() override;
        virtual void on_log_item_new_begin(bq::log_entry_handle& read_handle) override;
        virtual read_with_cache_handle read_with_cache(size_t size) override;

    private:
        bool read_to_correct_segment();
        bool read_to_next_segment();
        void append_new_segment(appender_segment_type type);
        void update_write_cache_padding();
    private:
        bq::rsa::public_key rsa_pub_key_;
        seg_info cur_read_seg_;
        appender_encryption_type enc_type_;
        bq::array<uint8_t, bq::aligned_allocator<uint8_t, appender_file_base::DEFAULT_BUFFER_ALIGNMENT>> xor_key_blob_;
    };
}
