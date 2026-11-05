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
/*!
 * appender_file_binary.h
 * -----------------------------------------------------------------------------
 * Overview
 * - Binary container used by the appender subsystem.
 * - The file consists of a global File Header followed by a linked list of Segments.
 * - Each Segment contains a Segment Header, optional Encryption Metadata, and Data.
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
 *    0x0000     4  uint32_t version                      (format_version)
 *    0x0004     1  appender_format_type format           (2=compressed)
 *    0x0005     3  char padding[3]
 *
 * 2. Segment Structure
 *    Each segment begins with a Segment Header, followed by Encryption Metadata (if enabled),
 *    and then the Segment Payload.
 *
 *    A. Segment Header (12 bytes)
 *       Offset  Size  Field
 *       ------  ----  -------------------------------------------------------
 *       +0x00      8  uint64_t next_seg_pos  (Abs offset of next segment, or UINT64_MAX)
 *       +0x08      1  appender_segment_type seg_type
 *       +0x09      1  appender_encryption_type enc_type
 *       +0x0A      2  char padding[2]
 *
 *    B. Encryption Metadata (268 bytes, present only if enc_type == rsa_kem_chacha)
 *       Offset  Size  Field
 *       ------  ----  -------------------------------------------------------
 *       +0x00      8  uint64_t rsa_key_fingerprint
 *       +0x08      1  uint8_t  chacha_rounds         (written as 8; 8/12/20 accepted on read)
 *       +0x09      3  char     reserved[3]           (written as 0)
 *       +0x0C    256  uint8_t  kem_ciphertext[256]   (I2OSP(r^e mod n, 256), big endian)
 *       The payload starts at segment +0x118 and is ChaCha ciphertext addressed by its offset from there.
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
#pragma once
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/log_types.h"

namespace bq {
    class appender_file_binary : public appender_file_base {
    public:
        virtual ~appender_file_binary();

        enum class appender_format_type : uint8_t {
            compressed = 2
        };
        enum class appender_encryption_type : uint8_t {
            plaintext = 1,
            rsa_aes_xor = 2, // read only by the standalone decoder
            rsa_kem_chacha = 3
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
            char padding[2];
        } BQ_PACK_END
        static_assert(sizeof(appender_file_segment_head) == 12, "appender_file_segment_head size error");

        static constexpr uint8_t default_chacha_rounds = 8;
        static constexpr size_t kem_ciphertext_size = 256;

        // 2^32 ChaCha blocks of 64 bytes; one byte more would reuse keystream.
        bq_forceinline static constexpr uint64_t max_segment_payload_size()
        {
            return static_cast<uint64_t>(1) << 38;
        }

        // Written into the file header and bound into the segment key derivation.
        static constexpr uint32_t format_version = 11;

        bq_forceinline static constexpr bool is_supported_chacha_rounds(uint8_t rounds)
        {
            return rounds == 8 || rounds == 12 || rounds == 20;
        }

        // Follows appender_file_segment_head when enc_type == rsa_kem_chacha.
        BQ_PACK_BEGIN
        struct appender_segment_encryption_head {
            uint64_t rsa_key_fingerprint;
            uint8_t chacha_rounds;
            char reserved[3];
            uint8_t kem_ciphertext[kem_ciphertext_size];
        } BQ_PACK_END
        static_assert(sizeof(appender_segment_encryption_head) == 268, "appender_segment_encryption_head size error");
        static_assert(offsetof(appender_segment_encryption_head, rsa_key_fingerprint) == 0x00, "rsa_key_fingerprint must sit at segment +0x0C");
        static_assert(offsetof(appender_segment_encryption_head, chacha_rounds) == 0x08, "chacha_rounds must sit at segment +0x14");
        static_assert(offsetof(appender_segment_encryption_head, reserved) == 0x09, "reserved must sit at segment +0x15");
        static_assert(offsetof(appender_segment_encryption_head, kem_ciphertext) == 0x0C, "kem_ciphertext must sit at segment +0x18");

        static constexpr size_t encrypted_payload_base_offset = sizeof(appender_file_segment_head) + sizeof(appender_segment_encryption_head);
        static constexpr size_t plaintext_payload_base_offset = sizeof(appender_file_segment_head);
        static_assert(encrypted_payload_base_offset == 280, "encrypted payload must start at segment +0x118");

        // Only exist in first segment
        BQ_PACK_BEGIN
        struct appender_payload_metadata {
            char magic_number[3];
            bool use_local_time;
            int32_t gmt_offset_hours;
            int32_t gmt_offset_minutes;
            int32_t time_zone_diff_to_gmt_ms;
            char time_zone_str[32];
            uint32_t category_count;
        } BQ_PACK_END

        struct seg_info {
            uint64_t start_pos;
            uint64_t end_pos;
            appender_encryption_type enc_type_;
        };

        struct segment_topology_info {
            bool has_segment = false;
            bool all_segments_are_plaintext = true;
            bool has_encrypted_segment = false;
            uint64_t rsa_key_fingerprint = 0;
            uint64_t last_segment_start_pos = 0;
        };

    public:
        struct segment_key {
            uint8_t key[chacha20::key_size];
            uint8_t nonce[chacha20::nonce_size];
        };

        /// <summary>
        /// HKDF-SHA256 over the KEM shared secret, bound to every field of the encryption head.
        /// next_seg_pos and seg_type are left out because they are rewritten after the segment is created.
        /// </summary>
        static bool derive_segment_key(const appender_segment_encryption_head& head,
            const uint8_t* shared_secret, size_t shared_secret_size, segment_key& out);

        /// <summary>
        /// Checks the key fingerprint, decapsulates the KEM ciphertext and derives the segment key.
        /// </summary>
        static bool derive_segment_key_from_head(const appender_segment_encryption_head& head,
            const rsa::private_key& key, segment_key& out, uint8_t& rounds);

        /// <summary>
        /// XORs the keystream into the payload in place; payload_offset counts from the payload start.
        /// Applying it twice restores the input.
        /// </summary>
        static void transform_segment_payload(uint8_t* data, size_t size, uint64_t payload_offset,
            const segment_key& key, uint8_t rounds);

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;
        virtual bool reset_impl(const bq::property_value& config_obj) override;
        virtual bool parse_exist_log_file(parse_file_context& context) override;
        virtual void on_file_open(bool is_new_created) override;
        virtual void on_flush_transform(uint8_t* data, size_t size, uint64_t file_pos) override;
        virtual bool on_flush_before_transform(size_t pending_size, uint64_t file_pos) override;
        virtual bool seek_read_file_absolute(uint64_t pos) override;
        virtual void seek_read_file_offset(int32_t offset) override;
        virtual appender_format_type get_appender_format() const = 0;
        virtual uint32_t get_binary_format_version() const = 0;
        virtual bool on_appender_file_recovery_begin() override;
        virtual void on_appender_file_recovery_end() override;
        virtual bool on_log_item_recovery_begin(bq::log_entry_handle& read_handle) override;
        virtual void on_log_item_recovery_end() override;
        virtual bool on_log_item_new_begin(bq::log_entry_handle& read_handle) override;
        virtual read_with_cache_handle read_with_cache(size_t size) override;

    private:
        bool read_to_correct_segment();
        bool read_to_next_segment();
        bool scan_segment_topology(segment_topology_info& result);
        bool append_new_segment(appender_segment_type type);

    private:
        bq::rsa::public_key rsa_pub_key_;
        uint64_t rsa_key_fingerprint_ = 0;
        seg_info cur_read_seg_;
        appender_encryption_type enc_type_;
        bool current_file_is_new_created_ = false;
        bool warned_weak_key_source_ = false;
        segment_key cur_seg_key_ {};
        uint64_t cur_seg_payload_base_ = 0;
        uint8_t cur_seg_rounds_ = 0;
    };
}
