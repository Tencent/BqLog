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
#include "bq_common/bq_common.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/miso_ring_buffer.h"
#include "bq_log/types/buffer/normal_buffer.h"

#ifndef BQ_LOG_FAST_META_SEGMENT_SIZE
#define BQ_LOG_FAST_META_SEGMENT_SIZE (64 * 1024)
#endif

namespace bq {
    BQ_PACK_BEGIN
    struct alignas(8) fast_meta_segment_head {
        uint64_t magic;
        uint64_t base_addr;
        uint64_t log_checksum;
        uint32_t format_version;
        uint32_t segment_id;
        uint16_t buffer_version;
        uint8_t reserved[14];
    } BQ_PACK_END static_assert(sizeof(fast_meta_segment_head) == 48, "fast meta segment head size");
    static_assert(offsetof(fast_meta_segment_head, base_addr) == 8, "fast meta segment base alignment");
    static_assert(offsetof(fast_meta_segment_head, log_checksum) == 16, "fast meta segment checksum alignment");

    BQ_PACK_BEGIN
    struct alignas(8) fast_meta_oversize_marker {
        fast_meta_head head;
        uint64_t old_addr;
        uint32_t normal_id;
        uint32_t data_size;
    } BQ_PACK_END static_assert(sizeof(fast_meta_oversize_marker) == 32, "fast meta oversize marker size");

    BQ_PACK_BEGIN
    struct alignas(8) fast_meta_normal_head {
        uint64_t magic;
        uint64_t base_addr;
        uint64_t log_checksum;
        uint32_t format_version;
        uint32_t normal_id;
        uint32_t data_size;
        uint16_t buffer_version;
        uint8_t reserved[26];
    } BQ_PACK_END static_assert(sizeof(fast_meta_normal_head) == 64, "fast meta normal head size");
    static_assert(offsetof(fast_meta_normal_head, base_addr) == 8, "fast meta normal base alignment");

    class fast_meta_store {
        struct segment {
            bq::unique_ptr<miso_ring_buffer> buffer_;
            uint16_t version_;
            uint32_t id_;
        };

        struct normal_entry {
            bq::unique_ptr<normal_buffer> buffer_;
            uint16_t version_;
            uint32_t id_;
        };

        static constexpr uint64_t segment_magic = UINT64_C(0xb09fa57e3ac10003);
        static constexpr uint64_t normal_magic = UINT64_C(0xb09fa57e3ac20003);
        static constexpr uint32_t meta_format_version = 3;

        log_buffer_config config_;
        uint64_t log_checksum_;
        uint16_t version_;
        bq::string folder_;
        bq::platform::spin_lock lock_;
        bq::platform::atomic<segment*> active_segment_;
        bq::array<bq::unique_ptr<segment>> segments_;
        bq::array<bq::unique_ptr<normal_entry>> normal_entries_;
        bq::array<uint16_t> loaded_versions_;
        uint32_t next_segment_id_ = 0;
        uint32_t next_normal_id_ = 0;

        bq::string segment_path(uint16_t version, uint32_t id) const;
        bq::string normal_path(uint16_t version, uint32_t id) const;
        segment* create_segment();
        segment* load_segment(uint16_t version, uint32_t id);
        void load_version(uint16_t version);
        const fast_meta_head* append(const uint8_t* data, uint32_t size);
        const fast_meta_head* append_normal(const uint8_t* data, uint32_t size);
        const fast_meta_head* resolve_in_segment(segment& item, uint64_t old_addr);
        const fast_meta_head* resolve_in_normal(normal_entry& item, uint64_t old_addr);
        bool verify(const fast_meta_head* entry, uint32_t available_size) const;

    public:
        fast_meta_store(const log_buffer_config& config, uint16_t version);

        const fast_format_meta* register_format(const char* format, uint32_t format_size,
            uint8_t level, uint32_t category_idx, uint8_t format_type,
            const uint8_t* arg_types, uint16_t arg_count);
        const fast_meta_head* resolve(uint16_t version, uint64_t old_addr);
        void release_version(uint16_t version);
        void release_recovered_versions();
    };
}
