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
#include "bq_log/types/meta/meta_registry.h"
#include "bq_log/global/log_vars.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/storage/instance_storage.h"
#include <errno.h>
#include <stdlib.h>

namespace bq {
    namespace {
        constexpr uint64_t meta_journal_magic = UINT64_C(0x42514C4F474D4554);
        constexpr uint32_t meta_journal_format_version = 1;
        constexpr uint32_t meta_journal_head_size = 64;
        constexpr uint32_t meta_journal_entry_head_size = 16;
        constexpr uint32_t meta_arena_block_size = 64 * 1024;
    }

    meta_registry& meta_registry::instance()
    {
        return log_global_vars::get().meta_registry_inst_;
    }

    bool meta_registry::build_format_entry(const char* format, uint32_t format_size,
        uint8_t level, uint32_t category_idx, uint8_t format_type,
        const uint8_t* arg_types, uint16_t arg_count,
        bq::array<uint8_t>& out_bytes)
    {
        if (!format || (arg_count && !arg_types)
            || static_cast<uint64_t>(format_size) + arg_count + sizeof(fast_format_meta) > UINT32_MAX) {
            return false;
        }
        const uint32_t size = static_cast<uint32_t>(bq::align_8(
            sizeof(fast_format_meta) + static_cast<size_t>(format_size) + arg_count));
        out_bytes.fill_uninitialized(size);
        uint8_t* data = out_bytes.begin();
        if (!data) {
            return false;
        }
        memset(data, 0, size);
        auto& entry = *reinterpret_cast<fast_format_meta*>(data);
        BQ_SUPPRESS_NULL_DEREF_BEGIN();
        entry.head.record_size = size;
        entry.head.kind = fast_meta_kind::format;
        entry.head.ready = 1;
        entry.format_hash = bq::util::get_hash_64(format, format_size);
        entry.category_idx = category_idx;
        entry.format_size = format_size;
        entry.arg_count = arg_count;
        entry.level = level;
        entry.format_type = format_type;
        memcpy(data + sizeof(entry), format, format_size);
        if (arg_count) {
            memcpy(data + sizeof(entry) + format_size, arg_types, arg_count);
        }
        entry.head.checksum = bq::util::get_hash_64(data + sizeof(uint64_t), size - sizeof(uint64_t));
        BQ_SUPPRESS_NULL_DEREF_END();
        return true;
    }

    bool meta_registry::verify_entry(const fast_meta_head* entry, uint32_t available_size)
    {
        if (!entry || available_size < sizeof(fast_meta_head) || entry->ready != 1
            || entry->record_size != available_size || (available_size & 7) != 0
            || entry->kind != fast_meta_kind::format) {
            return false;
        }
        if (available_size < sizeof(fast_format_meta)) {
            return false;
        }
        const auto& format = *reinterpret_cast<const fast_format_meta*>(entry);
        const uint64_t required = sizeof(format) + static_cast<uint64_t>(format.format_size) + format.arg_count;
        return required <= available_size
            && bq::util::get_hash_64(reinterpret_cast<const uint8_t*>(entry) + sizeof(uint64_t),
                   available_size - sizeof(uint64_t))
            == entry->checksum;
    }

    const fast_format_meta* meta_registry::register_format(const char* format, uint32_t format_size,
        uint8_t level, uint32_t category_idx, uint8_t format_type,
        const uint8_t* arg_types, uint16_t arg_count)
    {
#if defined(BQ_UNIT_TEST)
        test_register_count_.fetch_add_relaxed(1);
#endif
        bq::array<uint8_t> bytes;
        if (!build_format_entry(format, format_size, level, category_idx, format_type, arg_types, arg_count, bytes)) {
            return nullptr;
        }
        uint8_t* bytes_data = bytes.begin();
        const auto& entry = *reinterpret_cast<const fast_format_meta*>(bytes_data);
        bq::platform::scoped_spin_lock guard(lock_);
        return append_locked(bytes_data, entry.head.record_size, entry.head.checksum);
    }

    uint8_t* meta_registry::arena_alloc(uint32_t size)
    {
        // entries larger than a fresh block get their own block
        const uint32_t block_size = size > meta_arena_block_size ? static_cast<uint32_t>(bq::align_8(size)) : meta_arena_block_size;
        BQ_LIKELY_IF(!blocks_.is_empty())
        {
            auto& last = blocks_[blocks_.size() - 1];
            const uint32_t aligned_used = static_cast<uint32_t>(bq::align_8(last.used));
            if (last.size - aligned_used >= size) {
                last.used = aligned_used + size;
                return last.data + aligned_used;
            }
        }
        auto* data = static_cast<uint8_t*>(bq::platform::aligned_alloc(8, block_size));
        if (!data) {
            return nullptr;
        }
        arena_block block;
        block.data = data;
        block.size = block_size;
        block.used = size;
        blocks_.push_back(block);
        return data;
    }

    const fast_format_meta* meta_registry::append_locked(const uint8_t* entry, uint32_t entry_size, uint64_t checksum)
    {
        // sync the journal to the current run before dedupe: a dedupe hit must not
        // skip re-journaling existing entries into a new run's segments
        const bool journal_ok = !journal_enabled_ || journal_sync_run_locked();
        // dedupe: same checksum and same bytes
        auto list_iter = index_.find(checksum);
        if (list_iter != index_.end()) {
            for (const auto* e : list_iter->value()) {
                if (e->head.record_size == entry_size && memcmp(e, entry, entry_size) == 0) {
                    return e;
                }
            }
        }
        uint8_t* p = arena_alloc(entry_size);
        if (!p) {
            return nullptr;
        }
        memcpy(p, entry, entry_size);
        auto* e = reinterpret_cast<const fast_format_meta*>(p);
        if (journal_enabled_ && journal_ok && !segment_append_locked(e)) {
            // journal failure must not lose live logging: keep the entry, recovery of this meta is lost
            util::log_device_console(log_level::warning, "fast log meta journal append failed, fast logs of entry %p can not be recovered", static_cast<const void*>(e));
        }
        entries_.push_back(e);
        index_[checksum].push_back(e);
        return e;
    }

    bool meta_registry::journal_open_segment_locked(uint32_t min_capacity)
    {
        auto* inst = instance_storage::get();
        if (!inst || journal_next_segment_index_ == UINT16_MAX) {
            return false;
        }
        uint32_t size = BQ_LOG_META_SEGMENT_SIZE;
        // a single entry larger than a fresh segment gets its own exact-size segment
        const uint32_t need = meta_journal_head_size + min_capacity;
        while (size < need) {
            if (size > (UINT32_MAX >> 1)) {
                size = need;
                break;
            }
            size <<= 1;
        }
        char file_name[32];
        if (journal_next_segment_index_ == 0) {
            snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", journal_run_version_);
        } else {
            snprintf(file_name, sizeof(file_name), "%" PRIu16 "_%" PRIu16 ".mmap", journal_run_version_, journal_next_segment_index_);
        }
        const bq::string path = bq::file_manager::combine_path(inst->meta_dir(), file_name);
        auto segment = bq::make_unique<normal_buffer>(size, path, true);
        BQ_SUPPRESS_NULL_DEREF_BEGIN();
        if (!segment->is_memory_mapped()) {
            return false;
        }
        BQ_SUPPRESS_NULL_DEREF_END();
        auto& head = *reinterpret_cast<meta_journal_head*>(segment->data());
        memset(&head, 0, sizeof(head));
        head.magic = meta_journal_magic;
        head.format_version = meta_journal_format_version;
        head.run_version = journal_run_version_;
        head.segment_index = journal_next_segment_index_;
        head.run_serial = inst->run_serial();
        journal_segments_.push_back(bq::move(segment));
        ++journal_next_segment_index_;
        return true;
    }

    bool meta_registry::journal_sync_run_locked()
    {
        auto* inst = instance_storage::get();
        const uint16_t cur = inst ? inst->run_version() : 0;
        if (journal_run_version_ == cur && !journal_segments_.is_empty()) {
            return true;
        }
        journal_segments_.clear(); // closes the old run's segment files
        journal_next_segment_index_ = 0;
        journal_run_version_ = cur;
        if (journal_segments_.is_empty() && !journal_open_segment_locked(0)) {
            journal_run_version_ = 0;
            return false;
        }
        // replay every registered entry so the new run's journal is complete
        for (const auto* e : entries_) {
            if (!segment_append_locked(e)) {
                return false;
            }
        }
        return true;
    }

    bool meta_registry::segment_append_locked(const fast_format_meta* entry)
    {
        const uint32_t entry_size = static_cast<uint32_t>(meta_journal_entry_head_size + entry->head.record_size);
        BQ_LIKELY_IF(!journal_segments_.is_empty())
        {
            auto& last = journal_segments_[journal_segments_.size() - 1];
            auto& head = *reinterpret_cast<meta_journal_head*>(last->data());
            // max capacity derives from this segment's real size (an oversized
            // single-entry segment is larger than BQ_LOG_META_SEGMENT_SIZE)
            const uint32_t max_committed = static_cast<uint32_t>(last->size() - meta_journal_head_size);
            if (head.committed_size + entry_size > max_committed) {
                if (!journal_open_segment_locked(entry_size)) {
                    return false;
                }
            }
        }
        else if (!journal_open_segment_locked(entry_size))
        {
            return false;
        }
        auto& last = journal_segments_[journal_segments_.size() - 1];
        auto& head = *reinterpret_cast<meta_journal_head*>(last->data());
        if (head.committed_size + entry_size > last->size() - meta_journal_head_size) {
            // own oversized segment should have been opened; refuse rather than overflow
            return false;
        }
        auto& entry_head = *reinterpret_cast<meta_journal_entry_head*>(
            static_cast<uint8_t*>(last->data()) + meta_journal_head_size + head.committed_size);
        entry_head.arena_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry));
        entry_head.entry_size = entry_size;
        entry_head.reserved = 0;
        memcpy(static_cast<uint8_t*>(last->data()) + meta_journal_head_size + head.committed_size
                + meta_journal_entry_head_size,
            entry, entry->head.record_size);
        head.committed_size += entry_size;
        return true;
    }

    bool meta_registry::enable_journal()
    {
        bq::platform::scoped_spin_lock guard(lock_);
        if (journal_enabled_) {
            return true;
        }
        auto* inst = instance_storage::get();
        if (!inst) {
            return false;
        }
        cleanup_expired_locked();
        // sync opens the run's first segment and replays every registered entry
        if (!journal_sync_run_locked()) {
            return false;
        }
        journal_enabled_ = true;
        return true;
    }

    bool meta_registry::is_journal_enabled() const
    {
        return journal_enabled_;
    }

    void meta_registry::cleanup_expired_locked()
    {
        auto* inst = instance_storage::get();
        if (!inst) {
            return;
        }
        const bq::string meta_dir = inst->meta_dir();
        if (!bq::file_manager::is_dir(meta_dir)) {
            return;
        }
        const uint16_t cur = inst->run_version();
        // the current version's segments are owned by this registry: drop them first
        // so the files below can actually be deleted (on Windows a mapped file cannot)
        if (journal_run_version_ == cur) {
            journal_segments_.clear();
            journal_next_segment_index_ = 0;
            journal_run_version_ = 0;
        }
        for (const auto& name : bq::file_manager::get_sub_dirs_and_files_name(meta_dir)) {
            bool parsed = false;
            uint64_t v = 0;
            size_t dot = 0;
            // accepted names: "<v>.mmap" and "<v>_<n>.mmap", v decimal
            if (name.end_with(".mmap") && !name.is_empty()) {
                dot = name.size() - strlen(".mmap");
                if (dot > 0) {
                    bool all_digits = true;
                    for (size_t i = 0; i < dot; ++i) {
                        if (name[i] < '0' || name[i] > '9') {
                            if (name[i] == '_') {
                                break; // "<v>_<n>.mmap": stop at the underscore
                            }
                            all_digits = false;
                            break;
                        }
                    }
                    if (all_digits) {
                        char* end_ptr = nullptr;
                        errno = 0;
                        v = strtoull(name.c_str(), &end_ptr, 10);
                        parsed = errno != ERANGE && end_ptr == name.c_str() + dot && v <= UINT16_MAX;
                    }
                }
            }
            if (!parsed) {
                inst->remove(bq::file_manager::combine_path(meta_dir, name));
                continue;
            }
            const uint16_t version = static_cast<uint16_t>(v);
            if (static_cast<uint16_t>(cur - version) > bq::MAX_RECOVERY_VERSION_RANGE || version == cur) {
                inst->remove(bq::file_manager::combine_path(meta_dir, name));
            }
        }
    }

    meta_registry::recovered_version* meta_registry::load_version_locked(uint16_t version)
    {
        for (auto& rv : recovered_) {
            if (rv->version == version) {
                return rv->loaded ? rv.operator->() : nullptr;
            }
        }
        // normal_buffer opens its mmap file exclusively: release this run's own journal
        // segments before reading the files back (next registration re-opens + replays)
        if (journal_run_version_ == version) {
            journal_segments_.clear();
            journal_next_segment_index_ = 0;
            journal_run_version_ = 0;
        }
        auto item = bq::make_unique<recovered_version>();
        item->version = version;
        item->loaded = false;
        auto* inst = instance_storage::get();
        const bq::string meta_dir = inst ? inst->meta_dir() : bq::string();
        bool load_ok = true;
        if (inst && bq::file_manager::is_dir(meta_dir)) {
            for (uint16_t n = 0; load_ok; ++n) {
                char file_name[32];
                if (n == 0) {
                    snprintf(file_name, sizeof(file_name), "%" PRIu16 ".mmap", version);
                } else {
                    snprintf(file_name, sizeof(file_name), "%" PRIu16 "_%" PRIu16 ".mmap", version, n);
                }
                const bq::string path = bq::file_manager::combine_path(meta_dir, file_name);
                if (!bq::file_manager::is_file(path)) {
                    break;
                }
                const uint64_t file_size = bq::file_manager::get_file_size(path);
                auto segment = bq::make_unique<normal_buffer>(
                    static_cast<size_t>(file_size), path, false);
                if (!segment->is_memory_mapped() || file_size < meta_journal_head_size) {
                    load_ok = false;
                    break;
                }
                const auto& head = *reinterpret_cast<const meta_journal_head*>(segment->data());
                if (head.magic != meta_journal_magic
                    || head.format_version != meta_journal_format_version
                    || head.run_version != version
                    || head.segment_index != n
                    || head.committed_size > file_size - meta_journal_head_size
                    // a segment from a run outside the recovery window is stale
                    || (inst && static_cast<uint16_t>(inst->run_serial() - head.run_serial) > bq::MAX_RECOVERY_VERSION_RANGE)) {
                    load_ok = false;
                    break;
                }
                item->segments.push_back(bq::move(segment));
                auto& loaded = *item->segments[item->segments.size() - 1];
                const auto* cursor = static_cast<const uint8_t*>(loaded.data()) + meta_journal_head_size;
                const auto* end = cursor + head.committed_size;
                while (cursor + meta_journal_entry_head_size <= end) {
                    const auto& entry_head = *reinterpret_cast<const meta_journal_entry_head*>(cursor);
                    if (entry_head.entry_size < meta_journal_entry_head_size + sizeof(fast_format_meta)
                        || (entry_head.entry_size & 7) != 0
                        || cursor + entry_head.entry_size > end) {
                        break;
                    }
                    const auto* meta = reinterpret_cast<const fast_format_meta*>(cursor + meta_journal_entry_head_size);
                    if (verify_entry(&meta->head, entry_head.entry_size - meta_journal_entry_head_size)) {
                        item->map[entry_head.arena_addr] = meta;
                    }
                    // an entry failing verify_entry (e.g. a corrupted meta byte) is
                    // skipped: its records take the recovery error path, the rest of
                    // the segment stays recoverable
                    cursor += entry_head.entry_size;
                }
                if (cursor != end) {
                    // a torn or structurally invalid entry invalidates the rest of this segment
                    // and all later segments
                    load_ok = false;
                    break;
                }
            }
        }
        item->loaded = load_ok;
        recovered_.push_back(bq::move(item));
        return load_ok ? recovered_[recovered_.size() - 1].operator->() : nullptr;
    }

    const fast_format_meta* meta_registry::resolve_recovered(uint16_t version, uint64_t arena_addr)
    {
        bq::platform::scoped_spin_lock guard(lock_);
        auto* rv = load_version_locked(version);
        if (!rv) {
            return nullptr;
        }
        auto hit = rv->map.find(arena_addr);
        return hit != rv->map.end() ? hit->value() : nullptr;
    }

#if defined(BQ_UNIT_TEST)
    size_t meta_registry::test_entry_count() const
    {
        return entries_.size();
    }

    void meta_registry::test_cleanup_expired()
    {
        bq::platform::scoped_spin_lock guard(lock_);
        cleanup_expired_locked();
    }
#endif
}
