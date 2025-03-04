#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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
#include <stddef.h>
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/types/buffer/log_buffer_defs.h"
#include "bq_log/types/buffer/siso_ring_buffer.h"

namespace bq {
    BQ_PACK_BEGIN
    class block_node_head {
    public:
        friend class block_list;
        friend void block_head_aligment_check();

        BQ_PACK_BEGIN
        struct pointer_type {
            uint16_t index_;
            uint16_t aba_mark_;

            bq_forceinline pointer_type() : index_((uint16_t)-1), aba_mark_(0) {};
            bq_forceinline pointer_type(const pointer_type& rhs)
                : index_(rhs.index_)
                , aba_mark_(rhs.aba_mark_)
            {
            }
            bq_forceinline pointer_type(uint32_t new_union_value)
            {
                union_value() = new_union_value;
            }
            bq_forceinline uint32_t& union_value() { return *reinterpret_cast<uint32_t*>(&index_); }
            bq_forceinline const uint32_t& union_value() const{ return *reinterpret_cast<const uint32_t*>(&index_); }
            bq_forceinline pointer_type& operator=(const pointer_type& rhs)
            {
                union_value() = rhs.union_value();
                return *this;
            }
            bq_forceinline bool is_empty() const { return index_ == (uint16_t)-1; }
        }
        BQ_PACK_END

        static_assert((size_t)(offsetof(pointer_type, aba_mark_) - offsetof(pointer_type, index_)) == sizeof(pointer_type::index_), "invalid alignment of bq::block_node_head::pointer_type");
        static_assert(sizeof(pointer_type) == sizeof(uint32_t), "invalid size of pointer_type");    
    private:
        // These members are modified in different threads which will leads to "False Share".
        // So we must modify them in a low frequency
        pointer_type next_;
        char padding_inner0_[8 - sizeof(next_)];
        // reserved data. can be cast to any struct, all the bytes will be set to 0 in constructor.
        char misc_data_[56];
        siso_ring_buffer buffer_;
    public:
        /// <summary>
        /// constructor
        /// </summary>
        /// <param name="buffer">buffer for siso_ring_buffer</param>
        /// <param name="buffer_size">buffer size for siso_ring_buffer</param>
        /// <param name="is_memory_mapped">is buffer memory mapped</param>
        block_node_head(void* buffer, size_t buffer_size, bool is_memory_mapped);

        template<typename T>
        bq_forceinline T& get_misc_data()
        {
            static_assert(sizeof(T) <= sizeof(misc_data_), "size of T too large");
            return *((T*)(void*)misc_data_);
        }

        void reset_misc_data();

        bq_forceinline siso_ring_buffer& get_buffer() {return buffer_;}

        static constexpr ptrdiff_t get_buffer_data_offset()
        {
            size_t struct_size = sizeof(block_node_head);
            size_t offset = struct_size + (CACHE_LINE_SIZE - (struct_size % CACHE_LINE_SIZE));
            return (ptrdiff_t)offset;
        }
    } 
    BQ_PACK_END

    bq_forceinline void block_head_aligment_check()
    {
        static_assert(offsetof(block_node_head, next_) == 0, "invalid alignment of bq::block_node_head");
        static_assert((offsetof(block_node_head, misc_data_) % 8 == 0), "invalid alignment of bq::block_node_head");
        static_assert((offsetof(block_node_head, buffer_) == CACHE_LINE_SIZE), "invalid alignment of bq::block_node_head");
    }

        
    BQ_PACK_BEGIN
    class block_list {
        friend struct group_data_head;
    private:
        block_node_head::pointer_type head_;
        char padding_0_[8 - sizeof(head_)];
        uint16_t offset_;
        char padding_1_[8 - sizeof(offset_)];
        uint16_t max_blocks_count_;
        char padding_2_[8 - sizeof(max_blocks_count_)];
        size_t buffer_size_per_block_;
        char padding_3_[40 - sizeof(buffer_size_per_block_)];
    private:
        void reset(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        bool try_recover_from_memory_map(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size);
        void construct_blocks();
    public:
        block_list(uint16_t max_blocks_count, uint8_t* buffers_base_addr, size_t blocks_total_buffer_size, bool is_memory_mapped);

        ~block_list();

        bq_forceinline block_node_head& get_block_head_by_index(uint16_t index) const
        {
            return *(block_node_head*)((uint8_t*)this + (ptrdiff_t)offset_ + (ptrdiff_t)(buffer_size_per_block_ * index));
        }
        bq_forceinline uint16_t get_index_by_block_head(block_node_head* block) const
        {
            if (!block) {
                return (uint16_t)-1;
            }
            return (uint16_t)(((const uint8_t*)block - ((const uint8_t*)this + (ptrdiff_t)offset_)) / buffer_size_per_block_);
        }
        bq_forceinline bool is_include(const block_node_head* block) const
        {
            if (!block) {
                return false;
            }
            return ((const uint8_t*)block >= ((const uint8_t*)this + (ptrdiff_t)offset_)) && ((const uint8_t*)block < ((const uint8_t*)this + (ptrdiff_t)offset_ + (ptrdiff_t)(buffer_size_per_block_ * max_blocks_count_)));
        }

        bq_forceinline block_node_head* first() const
        {
            if (head_.is_empty()) {
                return nullptr;
            }
            return &get_block_head_by_index(head_.index_);
        }

        bq_forceinline block_node_head* next(const block_node_head* current) const
        {
            if (current->next_.is_empty()) {
                return nullptr;
            }
            return &get_block_head_by_index(current->next_.index_);
        }

        bq_forceinline block_node_head* pop()
        {
            while (true) {
                auto head_cpy_union = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).load(bq::platform::memory_order::acquire);
                block_node_head::pointer_type head_cpy(head_cpy_union);
                if (head_cpy.is_empty()) {
                    return nullptr;
                }
                block_node_head* first_node = &get_block_head_by_index(head_cpy.index_);
                uint32_t head_copy_expected_value = head_cpy.union_value();
                head_cpy.union_value() = first_node->next_.union_value();
                ++head_cpy.aba_mark_;
                if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).compare_exchange_strong(head_copy_expected_value, head_cpy.union_value(), bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                    return first_node;
                }
            }
        }

        bq_forceinline void push(block_node_head* new_block_node)
        {
            while (true) {
                auto head_cpy_union = BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).load(bq::platform::memory_order::acquire);
                block_node_head::pointer_type head_cpy(head_cpy_union);
                new_block_node->next_ = head_cpy;
                uint32_t head_copy_expected_value = head_cpy.union_value();
                head_cpy.index_ = get_index_by_block_head(new_block_node);
#if BQ_LOG_BUFFER_DEBUG
                assert(head_cpy.index_ >= 0 && head_cpy.index_ < max_blocks_count_ && "push assert failed, invalid new_block_node!");
#endif
                ++head_cpy.aba_mark_;
                if (BUFFER_ATOMIC_CAST_IGNORE_ALIGNMENT(head_.union_value(), uint32_t).compare_exchange_strong(head_copy_expected_value, head_cpy.union_value(), bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                    break;
                }
            }
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">nullptr means push after head</param>
        /// <param name="new_block_node"></param>
        /// <returns></returns>
        bq_forceinline void push_after_thread_unsafe(block_node_head* prev_block_node, block_node_head* new_block_node)
        {
#if BQ_LOG_BUFFER_DEBUG
            uint16_t new_index = get_index_by_block_head(new_block_node);
            uint16_t prev_index = get_index_by_block_head(prev_block_node);
            assert(new_index >= 0 && new_index < max_blocks_count_ && "push_after_thread_unsafe assert failed, invalid new_block_node!");
            assert((prev_index == (uint16_t)-1 || (prev_index >= 0 && prev_index < max_blocks_count_)) && "push_after_thread_unsafe assert failed, invalid prev_block_node!");
            block_node_head* test_node = first();
            bool found_prev = false;
            while (test_node) {
                assert(test_node != new_block_node);
                found_prev |= (prev_block_node == test_node);
                test_node = next(test_node);
            }
            assert(found_prev || (!prev_block_node));
#endif
            auto& prev_pointer = (prev_block_node ? prev_block_node->next_ : head_);
            new_block_node->next_ = prev_pointer;
            prev_pointer.index_ = (uint16_t)(((const uint8_t*)new_block_node - ((const uint8_t*)this + (ptrdiff_t)offset_)) / buffer_size_per_block_);
        }

        /// <summary>
        /// be care, this is not thread safe!!!
        /// </summary>
        /// <param name="prev_block_node">prev node of `remove_block_node`, nullptr means remove_block_node is head node</param>
        /// <param name="remove_block_node"></param>
        /// <returns></returns>
        bq_forceinline void remove_thread_unsafe(block_node_head* prev_block_node, block_node_head* remove_block_node)
        {
#if BQ_LOG_BUFFER_DEBUG
            uint16_t remove_index = get_index_by_block_head(remove_block_node);
            assert(remove_index >= 0 && remove_index < max_blocks_count_ && "remove assert failed, invalid remove_block_node!");
            uint16_t prev_index = get_index_by_block_head(prev_block_node);
            block_node_head* test_node = first();
            bool found_prev = false;
            bool found_remove = false;
            while (test_node) {
                found_remove |= (remove_block_node == test_node);
                found_prev |= (prev_block_node == test_node);
                test_node = next(test_node);
            }
            assert(found_remove);
            assert(found_prev || !prev_block_node);
#endif
            if (!prev_block_node) {
#if BQ_LOG_BUFFER_DEBUG
                assert(head_.index_ == remove_index && "remove assert failed!");
#endif
                head_ = remove_block_node->next_;
            } else {
#if BQ_LOG_BUFFER_DEBUG
                assert(prev_index >= 0 && prev_index < max_blocks_count_ && "remove assert failed, invalid prev_block_node!");
                assert(prev_block_node->next_.index_ == remove_index);
#endif
                prev_block_node->next_ = remove_block_node->next_;
            }
        }
    } 
    BQ_PACK_END
    static_assert(sizeof(block_list) == CACHE_LINE_SIZE, "invalid block_list size");
}
