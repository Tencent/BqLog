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
#include "bq_log/types/buffer/fast_meta_store.h"
#include <stdlib.h>

namespace bq {
    namespace {
        struct fast_meta_find_context {
            const uint8_t* target;
            const fast_meta_head* entry;
            uint32_t size;
        };

        void find_fast_meta(uint8_t* data, uint32_t size, void* context)
        {
            auto& search = *static_cast<fast_meta_find_context*>(context);
            if (data == search.target) {
                search.entry = reinterpret_cast<const fast_meta_head*>(data);
                search.size = size;
            }
        }

        struct fast_meta_marker_context {
            uint64_t old_addr;
            uint32_t normal_id;
            uint32_t size;
            bool found;
        };

        void find_fast_meta_marker(uint8_t* data, uint32_t size, void* context)
        {
            if (size != sizeof(fast_meta_oversize_marker)) {
                return;
            }
            auto& search = *static_cast<fast_meta_marker_context*>(context);
            const auto& marker = *reinterpret_cast<const fast_meta_oversize_marker*>(data);
            if (marker.head.kind == fast_meta_kind::oversize && marker.head.ready == 1
                && marker.old_addr == search.old_addr && marker.normal_id == search.normal_id
                && marker.data_size == search.size) {
                const uint64_t checksum = bq::util::get_hash_64(
                    data + sizeof(uint64_t), size - sizeof(uint64_t));
                search.found = checksum == marker.head.checksum;
            }
        }
    }

    fast_meta_store::fast_meta_store(const log_buffer_config& config, uint16_t version)
        : config_(config)
        , log_checksum_(config.calculate_check_sum())
        , version_(version)
        , folder_(TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config.log_name + "/fast_meta", 0))
        , active_segment_(nullptr)
    {
        config_.default_buffer_size = BQ_LOG_FAST_META_SEGMENT_SIZE;
    }

    bq::string fast_meta_store::segment_path(uint16_t version, uint32_t id) const
    {
        char file_name[48];
        snprintf(file_name, sizeof(file_name), "%" PRIu16 "_%" PRIu32 ".mmap", version, id);
        return bq::file_manager::combine_path(folder_, file_name);
    }

    bq::string fast_meta_store::normal_path(uint16_t version, uint32_t id) const
    {
        char file_name[48];
        snprintf(file_name, sizeof(file_name), "%" PRIu16 "_%" PRIu32 ".large.mmap", version, id);
        return bq::file_manager::combine_path(folder_, file_name);
    }

    fast_meta_store::segment* fast_meta_store::create_segment()
    {
        if (next_segment_id_ == UINT32_MAX) {
            return nullptr;
        }
        if (config_.need_recovery && !bq::file_manager::is_dir(folder_)) {
            bq::file_manager::create_directory(folder_);
        }
        auto item = bq::make_unique<segment>();
        item->version = version_;
        item->id = next_segment_id_;
        item->buffer = bq::make_unique<miso_ring_buffer>(config_,
            config_.need_recovery ? segment_path(version_, item->id) : bq::string());
        if (config_.need_recovery && !item->buffer->is_memory_mapped()) {
            return nullptr;
        }
        auto& head = item->buffer->get_mmap_misc_data<fast_meta_segment_head>();
        memset(&head, 0, sizeof(head));
        head.magic = segment_magic;
        head.base_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(item->buffer->get_buffer_addr()));
        head.log_checksum = log_checksum_;
        head.format_version = meta_format_version;
        head.segment_id = item->id;
        head.buffer_version = version_;
        segment* result = item.operator->();
        segments_.push_back(bq::move(item));
        ++next_segment_id_;
        active_segment_.store_release(result);
        return result;
    }

    fast_meta_store::segment* fast_meta_store::load_segment(uint16_t version, uint32_t id)
    {
        const bq::string path = segment_path(version, id);
        if (!bq::file_manager::is_file(path)) {
            return nullptr;
        }
        auto item = bq::make_unique<segment>();
        item->version = version;
        item->id = id;
        item->buffer = bq::make_unique<miso_ring_buffer>(config_, path);
        if (!item->buffer->is_memory_mapped()
            || item->buffer->get_memory_map_buffer_state() != memory_map_buffer_state::recover_from_memory_map) {
            return nullptr;
        }
        const auto& head = item->buffer->get_mmap_misc_data<fast_meta_segment_head>();
        if (head.magic != segment_magic || head.log_checksum != log_checksum_
            || head.format_version != meta_format_version || head.segment_id != id
            || head.buffer_version != version || (head.base_addr & 7) != 0) {
            return nullptr;
        }
        segment* result = item.operator->();
        segments_.push_back(bq::move(item));
        return result;
    }

    const fast_meta_head* fast_meta_store::append(const uint8_t* data, uint32_t size)
    {
        if (size > BQ_LOG_FAST_META_SEGMENT_SIZE / 2 - BQ_CACHE_LINE_SIZE) {
            return append_normal(data, size);
        }
        for (;;) {
            segment* item = active_segment_.load_acquire();
            if (!item) {
                bq::platform::scoped_spin_lock guard(lock_);
                item = active_segment_.load_acquire();
                if (!item) {
                    item = create_segment();
                }
            }
            if (!item) {
                return nullptr;
            }
            auto handle = item->buffer->alloc_write_chunk(size);
            if (handle.result == enum_buffer_result_code::success) {
                memcpy(handle.data_addr, data, size);
                item->buffer->commit_write_chunk(handle);
                return reinterpret_cast<const fast_meta_head*>(handle.data_addr);
            }
            if (handle.result != enum_buffer_result_code::err_not_enough_space
                && handle.result != enum_buffer_result_code::err_alloc_size_invalid) {
                return nullptr;
            }
            bq::platform::scoped_spin_lock guard(lock_);
            if (active_segment_.load_acquire() == item && !create_segment()) {
                return nullptr;
            }
        }
    }

    const fast_meta_head* fast_meta_store::append_normal(const uint8_t* data, uint32_t size)
    {
        uint64_t old_addr = 0;
        uint32_t normal_id = 0;
        {
            bq::platform::scoped_spin_lock guard(lock_);
            if (next_normal_id_ == UINT32_MAX) {
                return nullptr;
            }
            if (config_.need_recovery && !bq::file_manager::is_dir(folder_)) {
                bq::file_manager::create_directory(folder_);
            }
            auto item = bq::make_unique<normal_entry>();
            item->version = version_;
            item->id = next_normal_id_;
            const bq::string path = config_.need_recovery
                ? normal_path(version_, item->id)
                : bq::string();
            item->buffer = bq::make_unique<normal_buffer>(sizeof(fast_meta_normal_head) + size, path, true);
            if (!item->buffer->is_valid()
                || (config_.need_recovery && !item->buffer->is_memory_mapped())) {
                return nullptr;
            }
            auto& head = *reinterpret_cast<fast_meta_normal_head*>(item->buffer->data());
            memset(&head, 0, sizeof(head));
            head.magic = normal_magic;
            head.base_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(item->buffer->data()));
            head.log_checksum = log_checksum_;
            head.format_version = meta_format_version;
            head.normal_id = item->id;
            head.data_size = size;
            head.buffer_version = version_;
            auto* entry = reinterpret_cast<uint8_t*>(item->buffer->data()) + sizeof(head);
            memcpy(entry, data, size);
            old_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry));
            normal_id = item->id;
            normal_entries_.push_back(bq::move(item));
            ++next_normal_id_;
        }
        fast_meta_oversize_marker marker = {};
        marker.head.record_size = sizeof(marker);
        marker.head.kind = fast_meta_kind::oversize;
        marker.head.ready = 1;
        marker.old_addr = old_addr;
        marker.normal_id = normal_id;
        marker.data_size = size;
        marker.head.checksum = bq::util::get_hash_64(
            reinterpret_cast<const uint8_t*>(&marker) + sizeof(uint64_t),
            sizeof(marker) - sizeof(uint64_t));
        return append(reinterpret_cast<const uint8_t*>(&marker), sizeof(marker))
            ? reinterpret_cast<const fast_meta_head*>(static_cast<uintptr_t>(old_addr))
            : nullptr;
    }

    const fast_format_meta* fast_meta_store::register_format(const char* format, uint32_t format_size,
        uint8_t level, uint32_t category_idx, uint8_t format_type,
        const uint8_t* arg_types, uint16_t arg_count)
    {
        if (!format || (arg_count && !arg_types)
            || static_cast<uint64_t>(format_size) + arg_count + sizeof(fast_format_meta) > UINT32_MAX) {
            return nullptr;
        }
        const uint32_t size = static_cast<uint32_t>(bq::align_8(
            sizeof(fast_format_meta) + static_cast<size_t>(format_size) + arg_count));
        bq::array<uint8_t> bytes;
        bytes.fill_uninitialized(size);
        memset(&bytes[0], 0, size);
        auto& entry = *reinterpret_cast<fast_format_meta*>(&bytes[0]);
        entry.head.record_size = size;
        entry.head.kind = fast_meta_kind::format;
        entry.head.ready = 1;
        entry.format_hash = bq::util::get_hash_64(format, format_size);
        entry.category_idx = category_idx;
        entry.format_size = format_size;
        entry.arg_count = arg_count;
        entry.level = level;
        entry.format_type = format_type;
        memcpy(&bytes[sizeof(entry)], format, format_size);
        if (arg_count) {
            memcpy(&bytes[sizeof(entry) + format_size], arg_types, arg_count);
        }
        entry.head.checksum = bq::util::get_hash_64(
            &bytes[sizeof(uint64_t)], size - sizeof(uint64_t));
        return reinterpret_cast<const fast_format_meta*>(append(&bytes[0], size));
    }

    bool fast_meta_store::verify(const fast_meta_head* entry, uint32_t available_size) const
    {
        if (!entry || available_size < sizeof(fast_meta_head) || entry->ready != 1
            || entry->record_size != available_size || (available_size & 7) != 0) {
            return false;
        }
        uint64_t required = 0;
        if (entry->kind == fast_meta_kind::format) {
            if (available_size < sizeof(fast_format_meta)) {
                return false;
            }
            const auto& format = *reinterpret_cast<const fast_format_meta*>(entry);
            required = sizeof(format) + static_cast<uint64_t>(format.format_size) + format.arg_count;
        } else if (entry->kind == fast_meta_kind::oversize) {
            required = sizeof(fast_meta_oversize_marker);
        } else {
            return false;
        }
        return required <= available_size
            && bq::util::get_hash_64(reinterpret_cast<const uint8_t*>(entry) + sizeof(uint64_t),
                   available_size - sizeof(uint64_t))
            == entry->checksum;
    }

    const fast_meta_head* fast_meta_store::resolve_in_segment(segment& item, uint64_t old_addr)
    {
        const auto& head = item.buffer->get_mmap_misc_data<fast_meta_segment_head>();
        if (old_addr < head.base_addr || old_addr - head.base_addr >= BQ_LOG_FAST_META_SEGMENT_SIZE) {
            return nullptr;
        }
        const auto* target = item.buffer->get_buffer_addr() + (old_addr - head.base_addr);
        fast_meta_find_context context = { target, nullptr, 0 };
        item.buffer->data_traverse(find_fast_meta, &context);
        return verify(context.entry, context.size) ? context.entry : nullptr;
    }

    const fast_meta_head* fast_meta_store::resolve_in_normal(normal_entry& item, uint64_t old_addr)
    {
        const auto& head = *reinterpret_cast<const fast_meta_normal_head*>(item.buffer->data());
        if (head.magic != normal_magic || head.log_checksum != log_checksum_
            || head.format_version != meta_format_version || head.buffer_version != item.version
            || head.normal_id != item.id || head.data_size > item.buffer->size() - sizeof(head)
            || old_addr != head.base_addr + sizeof(head)) {
            return nullptr;
        }
        fast_meta_marker_context marker = { old_addr, item.id, head.data_size, false };
        for (auto& segment_item : segments_) {
            if (segment_item->version == item.version) {
                segment_item->buffer->data_traverse(find_fast_meta_marker, &marker);
            }
        }
        const auto* entry = reinterpret_cast<const fast_meta_head*>(
            static_cast<const uint8_t*>(item.buffer->data()) + sizeof(head));
        return marker.found && verify(entry, head.data_size) ? entry : nullptr;
    }

    void fast_meta_store::load_version(uint16_t version)
    {
        if (!config_.need_recovery || version == version_) {
            return;
        }
        for (uint16_t loaded : loaded_versions_) {
            if (loaded == version) {
                return;
            }
        }
        loaded_versions_.push_back(version);
        if (!bq::file_manager::is_dir(folder_)) {
            return;
        }
        for (const auto& file_name : bq::file_manager::get_sub_dirs_and_files_name(folder_)) {
            char* separator = nullptr;
            const unsigned long file_version = strtoul(file_name.c_str(), &separator, 10);
            if (separator == file_name.c_str() || *separator != '_'
                || file_version != version) {
                continue;
            }
            char* suffix = nullptr;
            const unsigned long file_id = strtoul(separator + 1, &suffix, 10);
            if (suffix == separator + 1 || file_id > UINT32_MAX) {
                continue;
            }
            const uint32_t id = static_cast<uint32_t>(file_id);
            char segment_name[48];
            char normal_name[48];
            snprintf(segment_name, sizeof(segment_name), "%" PRIu16 "_%" PRIu32 ".mmap", version, id);
            snprintf(normal_name, sizeof(normal_name), "%" PRIu16 "_%" PRIu32 ".large.mmap", version, id);
            if (file_name == segment_name) {
                load_segment(version, id);
            } else if (file_name == normal_name) {
                const bq::string path = normal_path(version, id);
                auto item = bq::make_unique<normal_entry>();
                item->version = version;
                item->id = id;
                item->buffer = bq::make_unique<normal_buffer>(
                    bq::file_manager::get_file_size(path), path, false);
                if (item->buffer->is_memory_mapped()) {
                    normal_entries_.push_back(bq::move(item));
                }
            }
        }
    }

    const fast_meta_head* fast_meta_store::resolve(uint16_t version, uint64_t old_addr)
    {
        bq::platform::scoped_spin_lock guard(lock_);
        load_version(version);
        for (auto& item : segments_) {
            if (item->version == version) {
                const auto* entry = resolve_in_segment(*item, old_addr);
                if (entry && entry->kind != fast_meta_kind::oversize) {
                    return entry;
                }
            }
        }
        for (auto& item : normal_entries_) {
            if (item->version == version) {
                const auto* entry = resolve_in_normal(*item, old_addr);
                if (entry && entry->kind != fast_meta_kind::oversize) {
                    return entry;
                }
            }
        }
        return nullptr;
    }

    void fast_meta_store::release_version(uint16_t version)
    {
        if (!config_.need_recovery || version == version_) {
            return;
        }
        bq::platform::scoped_spin_lock guard(lock_);
        for (size_t i = 0; i < segments_.size();) {
            if (segments_[i]->version == version) {
                segments_.erase(segments_.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        for (size_t i = 0; i < normal_entries_.size();) {
            if (normal_entries_[i]->version == version) {
                normal_entries_.erase(normal_entries_.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        if (bq::file_manager::is_dir(folder_)) {
            char prefix[24];
            snprintf(prefix, sizeof(prefix), "%" PRIu16 "_", version);
            for (const auto& file_name : bq::file_manager::get_sub_dirs_and_files_name(folder_)) {
                if (file_name.begin_with(prefix) && file_name.end_with(".mmap")) {
                    bq::file_manager::remove_file_or_dir(
                        bq::file_manager::combine_path(folder_, file_name));
                }
            }
        }
    }

    void fast_meta_store::release_recovered_versions()
    {
        if (!config_.need_recovery || !bq::file_manager::is_dir(folder_)) {
            return;
        }
        bq::platform::scoped_spin_lock guard(lock_);
        char prefix[24];
        snprintf(prefix, sizeof(prefix), "%" PRIu16 "_", version_);
        for (const auto& file_name : bq::file_manager::get_sub_dirs_and_files_name(folder_)) {
            if (!file_name.begin_with(prefix) && file_name.end_with(".mmap")) {
                bq::file_manager::remove_file_or_dir(
                    bq::file_manager::combine_path(folder_, file_name));
            }
        }
    }
}
