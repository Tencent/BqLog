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
 */
/*!
 * \class bq::block_list
 *
 * A lock free singly linked list.
 *
 * \author pippocao
 * \date 2024/12/06
 */
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    BQ_PACK_BEGIN
    class alignas(8) block_node_head {
    public:
        friend class block_list;

        BQ_PACK_BEGIN
        struct pointer_type {
        private:
            char value[4];

        public:
            bq_forceinline uint32_t& union_value() { return *reinterpret_cast<uint32_t*>(value); }
            bq_forceinline const uint32_t& union_value() const { return *reinterpret_cast<const uint32_t*>(value); }
            bq_forceinline uint16_t& index() { return *reinterpret_cast<uint16_t*>(value); }
            bq_forceinline const uint16_t& index() const { return *reinterpret_cast<const uint16_t*>(value); }
            bq_forceinline uint16_t& aba_mark() { return *reinterpret_cast<uint16_t*>(value + 2); }
            bq_forceinline const uint16_t& aba_mark() const { return *reinterpret_cast<const uint16_t*>(value + 2); }
            bq_forceinline static bool is_empty_index(uint16_t index) { return index == (uint16_t)-1; }
            bq_forceinline bool is_empty() const { return is_empty_index(index()); }
        } BQ_PACK_END

            static_assert(sizeof(pointer_type) == sizeof(uint32_t), "invalid size of pointer_type");

    private:
        // These members are modified in different threads which will leads to "False Share".
        // So we must modify them in a low frequency
        pointer_type next_;
        block_list_type type_;
        char padding_[8 - sizeof(next_) - sizeof(type_)];
        // reserved data. can be cast to any struct, all the bytes will be set to 0 when new created.
        alignas(8) char misc_data_[56];
        // only POD field can be used in packed struct, so we can't use siso_ring_buffer directly.
        char buffer_[sizeof(siso_ring_buffer)];

    public:
        /// <summary>
        /// constructor
        /// </summary>
        /// <param name="type"></param>
        /// <param name="buffer">buffer for siso_ring_buffer</param>
        /// <param name="buffer_size">buffer size for siso_ring_buffer</param>
        /// <param name="is_memory_recovery">is buffer memory mapped</param>
        block_node_head(block_list_type type, void* buffer, size_t buffer_size, bool is_memory_recovery);

        ~block_node_head();

        static void alignment_assert();

        template <typename T>
        bq_forceinline T& get_misc_data()
        {
            static_assert(sizeof(T) <= sizeof(misc_data_), "size of T too large");
            return *reinterpret_cast<T*>((void*)misc_data_);
        }

        void set_misc_data(const void* data_src, size_t data_size);

        bq_forceinline siso_ring_buffer& get_buffer() { return *(siso_ring_buffer*)buffer_; }

        static constexpr ptrdiff_t get_buffer_data_offset()
        {
            return (ptrdiff_t)((sizeof(block_node_head) + BQ_CACHE_LINE_SIZE - 1) - ((sizeof(block_node_head) + BQ_CACHE_LINE_SIZE - 1) % BQ_CACHE_LINE_SIZE));
        }

        bq_forceinline block_list_type get_type() const { return type_; }

        bq_forceinline void set_type(block_list_type type) { type_ = type; }
    } BQ_PACK_END

        BQ_PACK_BEGIN class alignas(BQ_CACHE_LINE_SIZE) block_list {
        friend struct group_data_head;

    private:
        alignas(8) block_node_head::pointer_type head_;
        alignas(8) uint16_t offset_;
        alignas(8) uint16_t max_blocks_count_;
        alignas(8) size_t buffer_size_per_block_;
        alignas(8) const uint8_t* data_range_start_;
        alignas(8) const uint8_t* data_range_end_;
        alignas(8) block_list_type type_;
    private:
        void reset(uint16_t max_blocks_count, const uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        bool try_recover_from_memory_map(uint16_t max_blocks_count, const uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        void recovery_blocks();

    public:
        block_list(block_list_type type, uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size, bool is_memory_recovery);

        ~block_list();

        bq_forceinline block_list_type get_type() const { return type_; }

        bq_forceinline block_node_head& get_block_head_by_index(uint16_t index)
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(index < max_blocks_count_ && "invalid block index!");
#endif
            return *reinterpret_cast<block_node_head*>(reinterpret_cast<uint8_t*>(this) + static_cast<ptrdiff_t>(*bq::launder(&offset_)) + static_cast<ptrdiff_t>(*bq::launder(&buffer_size_per_block_) * index));
        }

        bq_forceinline uint16_t get_index_by_block_head(const block_node_head* block) const
        {
            if (!block) {
                return static_cast<uint16_t>(-1);
            }
            ptrdiff_t diff = reinterpret_cast<const uint8_t*>(block) - (reinterpret_cast<const uint8_t*>(this) + static_cast<ptrdiff_t>(*bq::launder(&offset_)));
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(((static_cast<size_t>(diff) % buffer_size_per_block_) == 0) && "invalid block node head address");
#endif
            return static_cast<uint16_t>(static_cast<size_t>(diff) / buffer_size_per_block_);
        }

        bq_forceinline bool is_range_include(const block_node_head* block) const
        {
            if (!block) {
                return false;
            }
            return (reinterpret_cast<const uint8_t*>(block) >= data_range_start_)
                && (reinterpret_cast<const uint8_t*>(block) < data_range_end_);
        }

#if defined(BQ_LOG_BUFFER_DEBUG) || defined(BQ_UNIT_TEST)
        bq_forceinline bool is_include(const block_node_head* block_node)
        {
            if (!block_node) {
                return false;
            }
            uint16_t index = get_index_by_block_head(block_node);
            if (index >= max_blocks_count_) {
                return false;
            }
            block_node_head* test_node = first();
            while (test_node) {
                if (block_node == test_node) {
                    return true;
                }
                test_node = next(test_node);
            }
            return false;
        }

        bq_forceinline void debug_output() {
            auto block_iter = first();
            while (block_iter) {
                printf("=>%" PRIu16, get_index_by_block_head(block_iter));
                block_iter = next(block_iter);
            }
            printf("\n");
            for (uint16_t i = 0; i < max_blocks_count_; ++i) {
                auto& block_head = get_block_head_by_index(i);
                printf("->%" PRIu8, static_cast<uint8_t>(block_head.get_type()));
            }
            fflush(stdout);
        }
#endif

        bq_forceinline block_node_head* first()
        {
            auto index_tmp = head_.index();
            if (block_node_head::pointer_type::is_empty_index(index_tmp)) {
                return nullptr;
            }
            return &get_block_head_by_index(index_tmp);
        }

        bq_forceinline block_node_head* next(const block_node_head* current)
        {
            auto index_tmp = current->next_.index();
            if (block_node_head::pointer_type::is_empty_index(index_tmp)) {
                return nullptr;
            }
            return &get_block_head_by_index(index_tmp);
        }

        bq_forceinline block_node_head* pop()
        {
            auto head_cpy = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).load_acquire();
            auto& head_cpy_ref = *reinterpret_cast<block_node_head::pointer_type*>(reinterpret_cast<char*>(&head_cpy));
            block_node_head::pointer_type head_desired;
            block_node_head* first_node = nullptr;
            do {
                if (head_cpy_ref.is_empty()) {
                    return nullptr;
                }
                first_node = &get_block_head_by_index(head_cpy_ref.index());
                head_desired.index() = first_node->next_.index();
                head_desired.aba_mark() = static_cast<uint16_t>(head_cpy_ref.aba_mark() + 1);
            } while (!BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).compare_exchange_strong(head_cpy, head_desired.union_value(), bq::platform::memory_order::release, bq::platform::memory_order::acquire));
            return first_node;
        }

        bq_forceinline void push(block_node_head* new_block_node)
        {
#if defined(BQ_LOG_BUFFER_DEBUG) || defined(BQ_UNIT_TEST)
            if (is_include(new_block_node)) {
                bq::util::log_device_console(bq::log_level::error, "push failed, new_block_node already in the list!");
                assert(false && "push failed, new_block_node already in the list!");
            }
            uint16_t new_index = get_index_by_block_head(new_block_node);
            assert(new_index < max_blocks_count_ && "push assert failed, invalid new_block_node!");
#endif
            new_block_node->set_type(type_);
            auto head_cpy = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).load_acquire();
            auto& head_cpy_ref = *reinterpret_cast<block_node_head::pointer_type*>(reinterpret_cast<char*>(&head_cpy));
            block_node_head::pointer_type head_desired;
            do {
                new_block_node->next_.index() = head_cpy_ref.index();
                head_desired.index() = get_index_by_block_head(new_block_node);
                head_desired.aba_mark() = static_cast<uint16_t>(head_cpy_ref.aba_mark() + 1);
            } while (!BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).compare_exchange_strong(head_cpy, head_desired.union_value(), bq::platform::memory_order::release, bq::platform::memory_order::acquire));
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">nullptr means push after head</param>
        /// <param name="new_block_node"></param>
        /// <returns></returns>
        bq_forceinline void push_after_thread_unsafe(block_node_head* prev_block_node, block_node_head* new_block_node)
        {
#if defined(BQ_LOG_BUFFER_DEBUG) || defined(BQ_UNIT_TEST)
            if (prev_block_node && prev_block_node->get_type() != type_) {
                bq::util::log_device_console(bq::log_level::error, "push_after_thread_unsafe failed, prev_block_node type mismatch!, expected:%" PRIu8 ", but:%" PRIu8, static_cast<uint8_t>(type_), static_cast<uint8_t>(prev_block_node->get_type()));
                assert(false && "push_after_thread_unsafe failed, prev_block_node type mismatch!");
            }
            if (new_block_node->get_type() == type_) {
                bq::util::log_device_console(bq::log_level::error, "push_after_thread_unsafe failed, new_block_node already in the list!, type:%" PRIu8, static_cast<uint8_t>(new_block_node->get_type()));
                assert(false && "push_after_thread_unsafe failed, new_block_node already in the list!");
            }
            uint16_t new_index = get_index_by_block_head(new_block_node);
            uint16_t prev_index = get_index_by_block_head(prev_block_node);
            assert(new_index < max_blocks_count_ && "push_after_thread_unsafe assert failed, invalid new_block_node!");
            assert((prev_index == (uint16_t)-1 || (prev_index < max_blocks_count_)) && "push_after_thread_unsafe assert failed, invalid prev_block_node!");
            block_node_head* test_node = first();
            bool found_prev = false;
            while (test_node) {
                assert(test_node != new_block_node);
                found_prev |= (prev_block_node == test_node);
                test_node = next(test_node);
            }
            assert(found_prev || (!prev_block_node));
#endif
            new_block_node->set_type(type_);
            auto& prev_pointer = (prev_block_node ? prev_block_node->next_ : head_);
            new_block_node->next_ = prev_pointer;
            prev_pointer.index() = static_cast<uint16_t>(((const uint8_t*)new_block_node - ((const uint8_t*)this + static_cast<ptrdiff_t>(offset_))) / static_cast<ptrdiff_t>(buffer_size_per_block_));
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">prev node of `remove_block_node`, nullptr means remove_block_node is head node</param>
        /// <param name="remove_block_node"></param>
        /// <returns>true if remove successfully</returns>
        bq_forceinline bool remove_thread_unsafe(block_node_head* prev_block_node, block_node_head* remove_block_node)
        {
#if defined(BQ_UNIT_TEST) || defined(BQ_LOG_BUFFER_DEBUG)
            //make sure remove_block_node is in the list
            if (!is_include(remove_block_node)) {
                return false;
            }
            if (prev_block_node) {
                if (!is_include(prev_block_node)) {
                    return false;
                }
                if (prev_block_node->get_type() != type_) {
                    bq::util::log_device_console(bq::log_level::error, "remove_thread_unsafe failed, prev_block_node type mismatch!, expected:%" PRIu8 ", but:%" PRIu8, static_cast<uint8_t>(type_), static_cast<uint8_t>(prev_block_node->get_type()));
                    assert(false && "remove_thread_unsafe failed, prev_block_node type mismatch!");
                }
            }
            if (remove_block_node->get_type() != type_) {
                bq::util::log_device_console(bq::log_level::error, "remove_thread_unsafe failed, remove_block_node type mismatch!, expected:%" PRIu8 ", but:%" PRIu8, static_cast<uint8_t>(type_), static_cast<uint8_t>(remove_block_node->get_type()));
                assert(false && "remove_thread_unsafe failed, prev_block_node type mismatch!");
            }
            bool check_prev = true;
            if (prev_block_node) {
                auto next_block = next(prev_block_node);
                if (next_block != remove_block_node) {
                    check_prev = false;
                }
            }
            else {
                if (first() != remove_block_node) {
                    check_prev = false;
                }
            }
            if (!check_prev) {
                return false;
            }
#endif
            if (!prev_block_node) {
                head_ = remove_block_node->next_;
            }
            else {
                prev_block_node->next_ = remove_block_node->next_;
            }
            return true;
        }
    } BQ_PACK_END static_assert(sizeof(block_list) == BQ_CACHE_LINE_SIZE, "invalid block_list size");
}
