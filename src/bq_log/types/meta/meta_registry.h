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
#include "bq_log/misc/bq_log_def.h"
#include "bq_log/types/buffer/normal_buffer.h"

#ifndef BQ_LOG_META_SEGMENT_SIZE
#define BQ_LOG_META_SEGMENT_SIZE (1024 * 1024)
#endif

#ifndef BQ_LOG_META_MAX_ENTRIES
#if defined(BQ_OHOS) || defined(BQ_ANDROID) || defined(BQ_IOS)
#define BQ_LOG_META_MAX_ENTRIES (4 * 1024)
#else
#define BQ_LOG_META_MAX_ENTRIES (64 * 1024)
#endif
#endif
#if BQ_LOG_META_MAX_ENTRIES < 1
#error "BQ_LOG_META_MAX_ENTRIES must be at least 1"
#endif

namespace bq {
    class instance_storage;
    struct log_global_vars;

    // Instance-level registry of fast log format metadata. All logs of a process share
    // one arena; recovered fast records resolve their format through the meta record files
    // written under <instance_root>/meta/<run_version>.mmap.
    // The registry lives in log_global_vars and is never destructed (its arena entries
    // are referenced by live records at any time; a small leak is accepted there).
    class meta_registry {
    public:
        static meta_registry& instance();

        // all fast paths (macro and fast_info) register here; returns the arena entry, nullptr on failure
        const fast_format_meta* register_format(const char* format, uint32_t format_size,
            uint8_t level, uint32_t category_idx, uint8_t format_type,
            const uint8_t* arg_types, uint16_t arg_count);

        uint32_t max_entries() const { return max_entries_; }

        // called by log_imp::init for the first recovery log, after instance_storage::acquire()
        bool enable_meta_records();
        bool is_meta_records_enabled() const;

        // resolved records of a previous run
        const fast_format_meta* resolve_recovered(uint16_t version, uint64_t arena_addr);

        // builds the entry bytes for a format registration (fast_format_meta head + format string + arg types, align_8)
        static bool build_format_entry(const char* format, uint32_t format_size,
            uint8_t level, uint32_t category_idx, uint8_t format_type,
            const uint8_t* arg_types, uint16_t arg_count,
            bq::array<uint8_t>& out_bytes);

        // checksum / layout validation of a recovered or live entry
        static bool verify_entry(const fast_meta_head* entry, uint32_t available_size);

#if defined(BQ_UNIT_TEST)
        // registration count including deduped ones (1 per register_format call)
        uint32_t test_register_count() const { return test_register_count_.load_relaxed(); }
        // deduped entries in registration order
        size_t test_entry_count() const;
        void test_set_max_entries(uint32_t max_entries);
        // deletes expired meta record files under meta_dir() (normally done by enable_meta_records)
        void test_cleanup_expired();
        void test_flush_meta_records();
#endif

    private:
        struct arena_block {
            uint8_t* data;
            uint32_t size;
            uint32_t used;
        };

        BQ_PACK_BEGIN
        struct alignas(8) meta_record_head {
            uint64_t magic; // 0x42514C4F474D4554 ("BQLOGMET")
            uint32_t format_version; // 1
            uint16_t run_version;
            uint16_t segment_index;
            uint64_t run_serial;
            uint32_t committed_size; // bytes of entries after the head
            uint8_t reserved[36];
        } BQ_PACK_END
        static_assert(sizeof(meta_record_head) == 64, "meta record head size");
        static_assert(offsetof(meta_record_head, committed_size) == 24, "meta record head layout");

        BQ_PACK_BEGIN
        struct alignas(8) meta_record_entry_head {
            uint64_t arena_addr;
            uint32_t entry_size; // 16 + meta record_size
            uint32_t reserved;
        } BQ_PACK_END
        static_assert(sizeof(meta_record_entry_head) == 16, "meta record entry head size");

        struct recovered_version {
            uint16_t version;
            bool loaded;
            bq::array<bq::unique_ptr<normal_buffer>> segments;
            bq::hash_map<uint64_t, const fast_format_meta*> map;
        };

        meta_registry() = default;
        // log_global_vars constructs the singleton member in place
        friend struct log_global_vars;

        uint8_t* arena_alloc(uint32_t size);
        const fast_format_meta* append_locked(const uint8_t* entry, uint32_t entry_size, uint64_t checksum);
        // writes one entry into the current meta record segments
        bool segment_append_locked(const fast_format_meta* entry);
        // on a run version change (only possible within one process in tests), closes
        // the current segment set and replays every registered entry into fresh
        // segments named for the new run, so records of that run can recover
        bool meta_records_sync_run_locked();
        bool meta_records_open_segment_locked(uint32_t min_capacity);
        void cleanup_expired_locked();
        recovered_version* load_version_locked(uint16_t version);

        bq::platform::spin_lock lock_;
        bq::array<arena_block> blocks_;
        bq::array<const fast_format_meta*> entries_; // registration order, for meta record replay
        bq::hash_map<uint64_t, bq::array<const fast_format_meta*>> index_; // checksum -> candidates
        uint32_t max_entries_ = BQ_LOG_META_MAX_ENTRIES;
        bool capacity_warning_printed_ = false;
        // meta records
        bool meta_records_enabled_ = false;
        uint16_t meta_records_run_version_ = 0; // run version the open segments belong to (0 = none)
        bq::array<bq::unique_ptr<normal_buffer>> meta_records_segments_;
        uint16_t meta_records_next_segment_index_ = 0;
        // recovered versions
        bq::array<bq::unique_ptr<recovered_version>> recovered_;

#if defined(BQ_UNIT_TEST)
        bq::platform::atomic<uint32_t> test_register_count_ { 0 };
#endif
    };
}
