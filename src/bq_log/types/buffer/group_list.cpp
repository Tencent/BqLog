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
 */
/*!
 * \class bq::group_list
 *
 * A singly linked list, where each item is referred to as a "Group,"
 * containing a pool of group_list::group::group_node siso_buffer instances. Each siso_buffer serves a single thread
 * , aimed at reducing inter-thread data storms caused by the MESI protocol, thereby enhancing performance.
 *
 * \author pippocao
 * \date 2024/12/17
 */

#include "bq_log/types/buffer/group_list.h"

namespace bq {

    group_data_head::group_data_head(uint16_t max_blocks_count, uint8_t* group_data_addr, size_t group_data_size, bool is_memory_recovery)
        : used_(block_list_type::list_used, max_blocks_count, group_data_addr, group_data_size, is_memory_recovery)
        , free_(block_list_type::list_free, max_blocks_count, group_data_addr, group_data_size, false)
        , stage_(block_list_type::list_stage, max_blocks_count, group_data_addr, group_data_size, is_memory_recovery)
    {
        assert(((uintptr_t)group_data_addr % BQ_CACHE_LINE_SIZE) == 0);
        bq::hash_map<uint8_t*, bool> used_map;
        bq::hash_map<uint8_t*, bool> stage_map;
        if (is_memory_recovery) {
            block_node_head* current = used_.first();
            while (current) {
                used_map[(uint8_t*)current] = true;
                current = used_.next(current);
            }
            current = stage_.first();
            while (current) {
                if (used_map.find((uint8_t*)current) != used_map.end()) {
                    bq::util::log_device_console(bq::log_level::warning, "found block_node_head existed in both used list and stage list during recovery, break link from stage list");
                    block_node_head* last = nullptr;
                    block_node_head* next = stage_.first();
                    bool break_link = false;
                    while (next) {
                        if (next == current) {
                            stage_.remove_thread_unsafe(last, next);
                            break_link = true;
                            break;
                        }
                        last = next;
                        next = stage_.next(next);
                    }
                    assert(break_link && "failed to break link from stage list during recovery");
                    break;
                }
                stage_map[(uint8_t*)current] = true;
                current = stage_.next(current);
            }
            used_.recovery_blocks();
            stage_.recovery_blocks();
        }

        for (uint16_t i = 0; i < max_blocks_count; ++i) {
            uint8_t* block_head_addr = (uint8_t*)(&stage_.get_block_head_by_index(i));
            if ((used_map.find(block_head_addr) == used_map.end())
                && (stage_map.find(block_head_addr) == stage_map.end())) {
                new ((void*)block_head_addr, bq::enum_new_dummy::dummy) block_node_head(block_list_type::list_none, block_head_addr + block_node_head::get_buffer_data_offset(), (size_t)(group_data_size / max_blocks_count) - (size_t)block_node_head::get_buffer_data_offset(), false);
                block_node_head* block = (block_node_head*)block_head_addr;
                free_.push(block);
            }
        }
    }

    size_t group_node::get_group_meta_size(const log_buffer_config& config)
    {
        static_assert(sizeof(decltype(config.calculate_check_sum())) <= BQ_CACHE_LINE_SIZE, "group meta data size overflow");
        return BQ_CACHE_LINE_SIZE;
    }

    size_t group_node::get_group_data_size(const log_buffer_config& config, uint16_t max_block_count_per_group)
    {
        size_t data_per_block = static_cast<size_t>(block_node_head::get_buffer_data_offset()) + siso_ring_buffer::calculate_min_size_of_memory(config.default_buffer_size);
        size_t total_size_of_block_datas = data_per_block * max_block_count_per_group + sizeof(group_data_head);
        return total_size_of_block_datas;
    }

    create_memory_map_result group_node::create_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group, uint64_t index)
    {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "_%" PRIu64 "", index);
        bq::string path = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config.log_name + "/hp/" + config.log_name + tmp + ".mmap", 0);
        size_t meta_size = get_group_meta_size(config);
        size_t data_size = get_group_data_size(config, max_block_count_per_group);
        size_t desired_size = meta_size + data_size;
        buffer_entity_ = bq::make_unique<normal_buffer>(desired_size, config.need_recovery ? path : "", true);
        return buffer_entity_->get_mmap_result();
    }

    bool group_node::try_recover_from_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group)
    {
        size_t meta_size = get_group_meta_size(config);
        size_t data_size = get_group_data_size(config, max_block_count_per_group);

        auto mapped_data_addr = static_cast<uint8_t*>(buffer_entity_->data());
        if (*(uint64_t*)mapped_data_addr != config.calculate_check_sum()) {
            bq::util::log_device_console(bq::log_level::warning, "recover from memory map verify failed, create new memory map, log_name:%s", config.log_name.c_str());
            return false;
        }

        new ((void*)(mapped_data_addr + meta_size), bq::enum_new_dummy::dummy) group_data_head(
            max_block_count_per_group, mapped_data_addr + meta_size + sizeof(group_data_head), data_size - sizeof(group_data_head), config.need_recovery);
        head_ptr_ = (group_data_head*)(mapped_data_addr + meta_size);
        return true;
    }

    void group_node::init_memory_map(const log_buffer_config& config, uint16_t max_block_count_per_group)
    {
        size_t meta_size = get_group_meta_size(config);
        size_t data_size = get_group_data_size(config, max_block_count_per_group);
        memset(buffer_entity_->data(), 0, buffer_entity_->size());
        auto mapped_data_addr = static_cast<uint8_t*>(buffer_entity_->data());
        *(uint64_t*)mapped_data_addr = config.calculate_check_sum();
        new ((void*)(mapped_data_addr + meta_size), bq::enum_new_dummy::dummy) group_data_head(
            max_block_count_per_group, mapped_data_addr + meta_size + sizeof(group_data_head), data_size - sizeof(group_data_head), config.need_recovery);
        head_ptr_ = (group_data_head*)(mapped_data_addr + meta_size);
    }

    void group_node::init_memory(const log_buffer_config& config, uint16_t max_block_count_per_group)
    {
        size_t desired_size = get_group_data_size(config, max_block_count_per_group);
        new (buffer_entity_->data(), bq::enum_new_dummy::dummy) group_data_head(max_block_count_per_group, static_cast<uint8_t*>(buffer_entity_->data()) + sizeof(group_data_head), desired_size - sizeof(group_data_head), false);
        head_ptr_ = static_cast<group_data_head*>(buffer_entity_->data());
    }

    group_node::group_node(class group_list* parent_list, uint16_t max_block_count_per_group, uint64_t index)
    {
        parent_list_ = parent_list;
        // This high-frequency memory should be kept from being swapped to the swap partition or LLC as much as possible,
        // so having a memory map as a backing mechanism is a relatively cost-effective solution.
        const auto& config = parent_list->get_config();
        auto mmap_create_result = create_memory_map(config, max_block_count_per_group, index);
        if (create_memory_map_result::failed == mmap_create_result) {
            init_memory(config, max_block_count_per_group);
        } else if (mmap_create_result == create_memory_map_result::new_created) {
            init_memory_map(config, max_block_count_per_group);
        } else if (mmap_create_result == create_memory_map_result::use_existed) {
            if (!try_recover_from_memory_map(config, max_block_count_per_group)) {
                init_memory_map(config, max_block_count_per_group);
            }
        }
    }

    group_node::~group_node()
    {
        if (get_data_head().used_.first() || get_data_head().stage_.first()) {
            buffer_entity_->set_delete_mmap_when_destruct(false);
        }
        head_ptr_ = nullptr;
    }

    group_list::group_list(const log_buffer_config& config, uint16_t max_block_count_per_group)
        : config_(config)
        , max_block_count_per_group_(max_block_count_per_group)
        , current_group_index_(0)
    {
        bq::string memory_map_folder = TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + config_.log_name + "/hp", 0);
        if (!config_.need_recovery) {
            if (bq::file_manager::is_dir(memory_map_folder) || bq::file_manager::is_file(memory_map_folder))
                bq::file_manager::remove_file_or_dir(memory_map_folder);
            return;
        }
        if (bq::file_manager::is_file(memory_map_folder)) {
            bq::file_manager::remove_file_or_dir(memory_map_folder);
        }
        if (!bq::file_manager::is_dir(memory_map_folder)) {
            bq::file_manager::create_directory(memory_map_folder);
        }
        bq::array<bq::string> sub_names = bq::file_manager::get_sub_dirs_and_files_name(memory_map_folder);
        for (const bq::string& file_name : sub_names) {
            bq::string full_path = bq::file_manager::combine_path(memory_map_folder, file_name);
            if (!file_name.end_with(".mmap") || !file_name.begin_with(config_.log_name + "_")) {
                bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file:%s", full_path.c_str());
                bq::file_manager::remove_file_or_dir(full_path);
                continue;
            }
            char* end_ptr = nullptr;
            errno = 0;
            bq::string file_name_cpy = file_name.substr(config_.log_name.size() + 1);
            uint64_t u64_value = strtoull(file_name_cpy.c_str(), &end_ptr, 10);
            if (errno == ERANGE
                || end_ptr != file_name_cpy.c_str() + (file_name_cpy.size() - strlen(".mmap"))) {
                bq::util::log_device_console(bq::log_level::warning, "remove invalid mmap file:%s", full_path.c_str());
                bq::file_manager::remove_file_or_dir(full_path);
                continue;
            }
            if (u64_value > current_group_index_.load(bq::platform::memory_order::relaxed)) {
                current_group_index_.store(u64_value, bq::platform::memory_order::seq_cst);
            }
            auto* new_node = bq::util::aligned_new<group_node>(BQ_CACHE_LINE_SIZE, this, max_block_count_per_group_, u64_value);
            if (new_node) {
                new_node->get_next_ptr().node_ = head_.node_;
            } else {
                assert(false && "group_list alloc group_node failed");
            }
            head_.node_ = new_node;
        }
    }

    group_list::~group_list()
    {
        while (auto iter = first(group_list::lock_type::write_lock)) {
            delete_and_unlock_thread_unsafe(iter);
        }
        while (auto node = pool_.pop()) {
            bq::util::aligned_delete(node);
        }
    }

    block_node_head* group_list::alloc_new_block(const void* misc_data_src, size_t misc_data_size)
    {
        // try alloc from current exist groups
        auto iter = first(lock_type::read_lock);
        while (iter) {
            block_node_head* result = iter.value().get_data_head().free_.pop();
            if (result) {
                result->set_misc_data(misc_data_src, misc_data_size);
                result->get_buffer().set_thread_check_enable(true);
                iter.value().get_data_head().stage_.push(result);
                next(iter, lock_type::no_lock);
                return result;
            }
            iter = next(iter, group_list::lock_type::read_lock);
        }

        // alloc new group
        head_.lock_.write_lock();
        // double check
        block_node_head* result = nullptr;
        group_node* node = nullptr;
        if (!head_.is_empty()) {
            node = head_.node_;
            result = node->get_data_head().free_.pop();
        }
        if (!result) {
            node = pool_.pop();
            if (!node) {
                node = bq::util::aligned_new<group_node>(BQ_CACHE_LINE_SIZE, this, max_block_count_per_group_, current_group_index_.add_fetch(1, bq::platform::memory_order::relaxed));
#if defined(BQ_UNIT_TEST)
                if (node && node->get_memory_map_status() == create_memory_map_result::use_existed) {
                    bq::util::log_device_console(bq::log_level::error, "group index:");
                    assert(false && "must use new memory map");
                }
#endif
            }
            result = node->get_data_head().free_.pop();
            node->get_next_ptr().node_ = head_.node_;
            head_.node_ = node;
#if defined(BQ_UNIT_TEST)
            groups_count_.fetch_add_seq_cst(1);
#endif
        }
        result->set_misc_data(misc_data_src, misc_data_size);
        result->get_buffer().set_thread_check_enable(true);
        node->get_data_head().stage_.push(result);
        head_.lock_.write_unlock();
        return result;
    }

    void group_list::recycle_block_thread_unsafe(iterator group, block_node_head* prev_block, block_node_head* recycle_block)
    {
        recycle_block->get_buffer().set_thread_check_enable(false);
        bool remove_result = group.value().get_data_head().used_.remove_thread_unsafe(prev_block, recycle_block);
        if (remove_result) {
            group.value().get_data_head().free_.push(recycle_block);
        }
        else {
#ifdef BQ_UNIT_TEST
            bq::util::log_device_console(bq::log_level::error, "free list:");
            group.value().get_data_head().free_.debug_output();
            bq::util::log_device_console(bq::log_level::error, "used list:");
            group.value().get_data_head().used_.debug_output();
            bq::util::log_device_console(bq::log_level::error, "stage list:");
            group.value().get_data_head().stage_.debug_output();
            assert(false && "recycle_block_thread_unsafe failed, block not found in used list");    
#endif
        }
    }

    void group_list::garbage_collect()
    {
        uint64_t current_epoch_ms = bq::platform::high_performance_epoch_ms();
        while (true) {
            group_node* candidiate = pool_.evict([](const group_node* node, void* user_data) {
                uint64_t current_epoch_ms_local = *(uint64_t*)user_data;
                return current_epoch_ms_local >= node->get_in_pool_epoch_ms() + GROUP_NODE_GC_LIFE_TIME_MS;
            },
                &current_epoch_ms);
            if (candidiate) {
                bq::util::aligned_delete(candidiate);
            } else {
                break;
            }
        }
    }

    size_t group_list::get_garbage_count()
    {
        return pool_.size();
    }
}