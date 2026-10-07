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
#pragma once
/*!
 * This is a high-performance asynchronous buffer that balanced between performance and memory usage.
 *
 * HP = High Performance, LP = Low Performance.
 * In fact, LP is only considered lower performance relative to HP;
 * it is still a high-performance multi-producer single-consumer ring buffer.
 * HP has an independent block for each thread, while LP has multiple threads sharing a single ring buffer.
 *
 * \author pippocao
 * \date 2024/12/17
 */
#include "native_src_bq_common_bq_common.h"
#include "native_src_bq_log_types_buffer_log_buffer_defs.h"
#include "native_src_bq_log_log_log_types.h"
#include "native_src_bq_log_types_buffer_miso_linked_list.h"
#include "native_src_bq_log_types_buffer_siso_ring_buffer.h"
#include "native_src_bq_log_types_buffer_miso_ring_buffer.h"
#include "native_src_bq_log_types_buffer_group_list.h"
#include "native_src_bq_log_types_buffer_oversize_buffer.h"
#include "native_src_bq_log_types_buffer_fast_meta_store.h"

namespace bq {
    // Owner thread of an HP block. From thread_name_len_ on, same layout as _log_entry_ext_head_def plus name.
    struct alignas(8) log_thread_info {
        static constexpr uint8_t MAX_THREAD_NAME_LEN = 16;
        uint64_t thread_id_;
        uint8_t thread_name_len_;
        char thread_name_[MAX_THREAD_NAME_LEN];
    };
    static_assert(sizeof(log_thread_info) == 32, "log_thread_info size");
    static_assert(offsetof(log_thread_info, thread_name_) == offsetof(log_thread_info, thread_name_len_) + sizeof(_log_entry_ext_head_def), "thread info must match ext info layout");

    void init_log_thread_info(log_thread_info& info);

    class alignas(BQ_CACHE_LINE_SIZE) log_buffer {
    public:
#if defined(BQ_MOBILE_PLATFORM)
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 2;
#else
        static constexpr uint16_t BLOCKS_PER_GROUP_NODE = 16;
#endif
#if defined(BQ_UNIT_TEST)
        static constexpr uint16_t MAX_RECOVERY_VERSION_RANGE = 5;
#else
        static constexpr uint16_t MAX_RECOVERY_VERSION_RANGE = 2;
#endif
        static constexpr uint64_t HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL = 1000;
        static constexpr uint64_t OVERSIZE_BUFFER_RECYCLE_INTERVAL_MS = 1000;

    public:
        BQ_PACK_BEGIN
        struct lp_buffer_head_misc {
            uint16_t saved_version_;
        } BQ_PACK_END

            struct destruction_mark {
            bq::platform::spin_lock lock_;
            bool is_destructed_ = false;
        };

        struct oversize_buffer_obj_def {
            bq::oversize_buffer buffer_;
            bq::platform::spin_lock_rw_crazy buffer_lock_;
            uint64_t last_used_epoch_ms_;
            bool is_thread_finished_;
            oversize_buffer_obj_def(uint32_t size, const bq::string& mmap_file_abs_path, bool auto_create)
                : buffer_(size, mmap_file_abs_path, auto_create)
                , last_used_epoch_ms_(0)
                , is_thread_finished_(false)
            {
            }
        };

        struct alignas(BQ_CACHE_LINE_SIZE) log_tls_buffer_info {
#if defined(BQ_JAVA)
            struct java_info {
                jobjectArray buffer_obj_for_lp_buffer_ = NULL; // miso_ring_buffer shared between low frequency threads;
                jobjectArray buffer_obj_for_hp_buffer_ = NULL; // siso_ring_buffer on block_node;
                jobjectArray buffer_obj_for_oversize_buffer_ = NULL; // oversize buffer;
                block_node_head* buffer_ref_block_ = nullptr;
                const uint8_t* buffer_addr_oversize = nullptr;
                uint32_t size_ref_oversize = 0;
                int32_t buffer_offset_ = 0;
            };

#endif
            uint64_t last_update_epoch_ms_ = 0;
            uint64_t update_times_ = 0;
            block_node_head* cur_block_ = nullptr; // nullptr means using lp_buffer.
            log_buffer* buffer_ = nullptr;
            log_buffer_write_handle oversize_parent_handle_;
            oversize_buffer_obj_def* oversize_target_buffer_;
            bq::shared_ptr<destruction_mark> destruction_mark_;
            bool fast_mode_ = false;
#if defined(BQ_JAVA)
            java_info java_;
#endif
            // Fields frequently accessed by write(produce) thread.
            alignas(BQ_CACHE_LINE_SIZE) struct {
                uint32_t current_write_seq_ = 0;
            } wt_data_;
            // Fields frequently accessed by read(consumer) thread.
            alignas(BQ_CACHE_LINE_SIZE) struct {
                uint32_t current_read_seq_ = 0;
            } rt_data_;

            ~log_tls_buffer_info();
        };
        static_assert(sizeof(log_tls_buffer_info) % BQ_CACHE_LINE_SIZE == 0, "log_tls_buffer_info current_read_seq_ must be 64 bytes aligned");

        struct log_tls_info {
#if !defined(BQ_LOG_BUFFER_DEBUG)
        private:
#endif
            bq::hash_map_inline<uint64_t, log_tls_buffer_info*>* log_map_ = nullptr;
            uint64_t cur_log_buffer_id_ = 0;
            log_tls_buffer_info* cur_buffer_info_ = nullptr;
            _api_fast_log_thread_state* fast_state_ = nullptr; // header-side thread local fast path state, see bind_fast_state
            uint64_t fast_state_buffer_id_ = 0; // id of the buffer fast_state_ describes (ids are never reused)
            log_thread_info thread_info_ {};

        public:
            bq_forceinline log_tls_buffer_info& get_buffer_info(const log_buffer* buffer);
            bq_forceinline log_tls_buffer_info& get_buffer_info_directly(const log_buffer* buffer);

            bq_forceinline const log_thread_info& get_thread_info()
            {
                BQ_UNLIKELY_IF(thread_info_.thread_id_ == 0)
                {
                    init_log_thread_info(thread_info_);
                }
                return thread_info_;
            }

            void bind_fast_state(_api_fast_log_thread_state* state);

            bq_forceinline void on_cur_block_changed(uint64_t buffer_id, block_node_head* block)
            {
                BQ_UNLIKELY_IF(fast_state_buffer_id_ == buffer_id)
                {
                    publish_fast_state(block);
                }
            }
            ~log_tls_info();

        private:
            void publish_fast_state(block_node_head* block);
        };

        BQ_PACK_BEGIN
        struct alignas(8) pointer_8_bytes_for_32_bits_system {
            log_tls_buffer_info* ptr;
            uintptr_t dummy;
        } BQ_PACK_END

            BQ_PACK_BEGIN struct alignas(8) pointer_8_bytes_for_64_bits_system {
            log_tls_buffer_info* ptr;
        } BQ_PACK_END

            BQ_PACK_BEGIN struct alignas(8) context_head {
        public:
            uint16_t version_;
            bool is_thread_finished_;
            bool is_external_ref_; // only works in lp_buffer when alloc size is larger than buffer size.
            uint32_t seq_;
            bq::condition_type_t<sizeof(void*) == 8, pointer_8_bytes_for_64_bits_system, pointer_8_bytes_for_32_bits_system> tls_info_; // Only meaningful when get_ver() equals the current version of log_buffer.

            bq_forceinline log_tls_buffer_info* get_tls_info() const
            {
                return tls_info_.ptr;
            }
            bq_forceinline void set_tls_info(log_tls_buffer_info* tls_info)
            {
                tls_info_.ptr = tls_info;
            }
        } BQ_PACK_END static_assert(sizeof(context_head) == 16, "context_head size must be 16");
        static_assert(sizeof(context_head) % 8 == 0, "context_head size must be a multiple of 8");

        BQ_PACK_BEGIN
        struct alignas(8) block_misc_data {
        private:
            alignas(8) char is_removed_place_holder_[sizeof(bq::platform::atomic_trivially_constructible<bool>)];

        public:
            alignas(8) bool need_reallocate_;
            alignas(8) context_head context_;

            alignas(8) log_thread_info thread_info_;
            bq_forceinline bq::platform::atomic_trivially_constructible<bool>& is_removed()
            {
                return *bq::launder(reinterpret_cast<bq::platform::atomic_trivially_constructible<bool>*>(is_removed_place_holder_));
            }
        } BQ_PACK_END static_assert(sizeof(block_misc_data) == 16 + sizeof(context_head) + sizeof(log_thread_info), "invalid block_misc_data size");

    public:
        log_buffer(log_buffer_config& config);

        ~log_buffer();

        // ext_info_size is reserved only outside HP blocks
        bq_forceinline log_buffer_write_handle alloc_write_chunk(uint32_t size, uint32_t ext_info_size, uint64_t current_epoch_ms);

        bq_forceinline log_buffer_write_handle alloc_write_chunk(uint32_t size, uint64_t current_epoch_ms)
        {
            return alloc_write_chunk(size, 0, current_epoch_ms);
        }

        bq::block_node_head* alloc_new_hp_block();

        bq::block_node_head* ensure_fast_hp_block(log_tls_buffer_info& tls_buffer_info);

        const fast_format_meta* register_fast_log_format(const char* format, uint32_t format_size,
            uint8_t level, uint32_t category_idx, uint8_t format_type,
            const uint8_t* arg_types, uint16_t arg_count);

        const fast_meta_head* resolve_fast_log_meta(uint16_t version, uint64_t old_addr);

        bq_forceinline void commit_write_chunk(const log_buffer_write_handle& handle);

        bq_forceinline log_buffer_read_handle read_chunk();

        bq_forceinline void return_read_chunk(const log_buffer_read_handle& handle);

#if defined(BQ_JAVA)
        bq::java_buffer_info get_java_buffer_info(JNIEnv* env, const log_buffer_write_handle& handle);
#endif

        bq_forceinline uint64_t get_id() const
        {
            return id_;
        }

        bq_forceinline uint16_t get_version() const
        {
            return version_;
        }

        bq_forceinline uint16_t get_current_reading_version() const
        {
            return rt_cache_.current_reading_.version_;
        }

        bq_forceinline bool is_current_reading_recovered() const
        {
            return rt_cache_.current_reading_.is_in_recovery_reading_;
        }

        // owner of the last chunk read if it came from an HP block
        bq_forceinline const log_thread_info* get_current_reading_thread_info() const
        {
            const auto& rt_reading = rt_cache_.current_reading_;
            return rt_reading.hp_handle_cache_.result == enum_buffer_result_code::success
                ? &rt_reading.cur_block_->get_misc_data<block_misc_data>().thread_info_
                : nullptr;
        }

        bq_forceinline const log_buffer_config& get_config() const
        {
            return config_;
        }

#if defined(BQ_UNIT_TEST)
        const log_tls_buffer_info& get_buffer_info_for_this_thread() const;
        // fast record with its head filled in, args after the head
        log_buffer_write_handle test_alloc_fast_record(const fast_format_meta* format, uint32_t args_size);

        int32_t get_groups_count() const { return hp_buffer_.get_groups_count(); }
        void garbage_collect() { hp_buffer_.garbage_collect(); }
        size_t get_garbage_count() { return hp_buffer_.get_garbage_count(); }
#endif
    private:
        enum class context_verify_result {
            valid,
            version_pending,
            version_invalid,
            seq_pending,
            seq_invalid,
        };

        enum class read_state {
            lp_buffer_reading,
            hp_block_reading,
            next_block_finding,
            next_group_finding,
            traversal_completed
        };

        bq_forceinline bool is_version_valid(uint16_t version)
        {
            return static_cast<uint16_t>(version_ - version) <= static_cast<uint16_t>(version_ - rt_cache_.current_reading_.version_);
        }

        // every cur_block_ change goes through here to keep the header's fast state in sync
        bq_forceinline void set_cur_block(log_tls_buffer_info& tls_buffer_info, block_node_head* block);

        context_verify_result verify_context(const context_head& context);
        context_verify_result verify_oversize_context(const context_head& parent_context, const context_head& oversize_context);
        void deregister_seq(const context_head& context);
        void prepare_and_fix_recovery_data();
        void clear_recovery_data();

        // For reading thread.
        bool rt_read_from_lp_buffer(log_buffer_read_handle& out_handle);
        log_buffer_read_handle read_chunk_full_impl();
        void return_read_chunk_full_impl(const log_buffer_read_handle& handle);
#if defined(BQ_LOG_BUFFER_DEBUG)
        void debug_check_read_thread();
#endif
        bool rt_try_traverse_to_next_block_in_group(context_verify_result& out_verify_result);
        bool rt_try_traverse_to_next_group();
        void rt_try_traverse_to_next_version();
        void refresh_traverse_end_mark();

        log_buffer_write_handle alloc_write_chunk_full_impl(log_tls_buffer_info& tls_buffer, uint32_t size, uint32_t ext_info_size, uint64_t current_epoch_ms);
        void commit_write_chunk_full_impl(log_tls_buffer_info& tls_buffer_info, const log_buffer_write_handle& handle);

        // For oversize data.
        log_buffer_write_handle wt_alloc_oversize_write_chunk(uint32_t size, uint64_t current_epoch_ms);
        void wt_commit_oversize_write_chunk(const log_buffer_write_handle& oversize_handle);
        bool rt_read_oversize_chunk(const log_buffer_read_handle& parent_handle, log_buffer_read_handle& out_oversize_handle);
        void rt_return_oversize_read_chunk(const log_buffer_read_handle& oversize_handle);
        void rt_recycle_oversize_buffers();

    private:
        friend struct log_tls_info;
        log_buffer_config config_;
        uint64_t id_; // unique id for log_buffer
        group_list hp_buffer_; // high performance buffer, each thread has its own block, but more memory usage.
        miso_ring_buffer lp_buffer_; // used to save memory for low frequency threads.
        uint32_t hp_buffer_max_alloc_size_;
        const uint16_t version_ = 0;
        bq::shared_ptr<destruction_mark> destruction_mark_;
        bq::platform::spin_lock fast_meta_lock_;
        bq::platform::atomic<fast_meta_store*> fast_meta_ptr_;
        bq::unique_ptr<fast_meta_store> fast_meta_store_;

        fast_meta_store* get_fast_meta_store();

        struct alignas(BQ_CACHE_LINE_SIZE) {
            bq::platform::spin_lock_rw_crazy array_lock_;
            bq::array<bq::unique_ptr<oversize_buffer_obj_def>> buffers_array_;
#if defined(BQ_JAVA)
            jobject java_buffer_obj_ = nullptr;
#endif
        } temprorary_oversize_buffer_; // used when allocating a large chunk of data that exceeds the size of lp_buffer or hp_buffer.
        bq::platform::atomic<uint64_t> current_oversize_buffer_index_;

        struct alignas(BQ_CACHE_LINE_SIZE) {
            struct {
                group_list::iterator last_group_; // empty means read from lp_buffer
                group_list::iterator cur_group_;
                block_node_head* last_block_ = nullptr;
                block_node_head* cur_block_ = nullptr;
                uint16_t version_ = 0;
                bool is_in_recovery_reading_ = true;
                bq::array<bq::hash_map<void*, uint32_t>> recovery_records_; // <tls_buffer_info_ptr, seq> for each version, only works when reading recovering data
#ifdef BQ_UNIT_TEST
                bq::array<bq::hash_map<void*, bq::hash_map<uint32_t, uint16_t>>> recovery_seq_records_;
#endif
                read_state state_ = read_state::lp_buffer_reading;
                bool traverse_end_block_is_working_ = false;
                block_node_head* traverse_end_block_ = nullptr;
                siso_ring_buffer::siso_buffer_batch_read_handle hp_handle_cache_;
                log_buffer_read_handle rt_oversize_parent_handle_;
                oversize_buffer_obj_def* rt_oversize_target_buffer_ = nullptr;
            } current_reading_;

            // memory fragmentation optimize
            struct {
                uint32_t left_holes_num_ = 0;
                uint16_t cur_group_using_blocks_num_ = 0;
                bool is_block_marked_removed = false;
                context_verify_result verify_result;
            } mem_optimize_;
        } rt_cache_; // Cache that only access in read(consumer) thread.
#if defined(BQ_LOG_BUFFER_DEBUG)
        alignas(BQ_CACHE_LINE_SIZE) bq::platform::thread::thread_id empty_thread_id_ = 0;
        bq::platform::thread::thread_id read_thread_id_ = 0;
#endif
    };

    bq_forceinline log_buffer_read_handle log_buffer::read_chunk()
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        debug_check_read_thread();
#endif
        auto& hp_handle = rt_cache_.current_reading_.hp_handle_cache_;
        BQ_LIKELY_IF(hp_handle.result == enum_buffer_result_code::success)
        {
            if (hp_handle.has_next()) {
                return hp_handle.next();
            }
            hp_handle.result = enum_buffer_result_code::err_empty_log_buffer;
        }
        return read_chunk_full_impl();
    }

    bq_forceinline void log_buffer::return_read_chunk(const log_buffer_read_handle& handle)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        debug_check_read_thread();
#endif
        auto& rt_reading = rt_cache_.current_reading_;
        BQ_LIKELY_IF(rt_reading.hp_handle_cache_.result == enum_buffer_result_code::success)
        {
#if defined(BQ_LOG_BUFFER_DEBUG)
            assert(rt_reading.hp_handle_cache_.verify_chunk(handle) && "log_buffer::return_read_chunk hp chunk return verify failed");
#endif
            if (!rt_reading.hp_handle_cache_.has_next()) {
#if defined(BQ_LOG_BUFFER_DEBUG)
                assert(rt_reading.cur_block_);
#endif
                rt_reading.cur_block_->get_buffer().return_batch_read_chunks(rt_reading.hp_handle_cache_);
            }
            return;
        }
        return_read_chunk_full_impl(handle);
    }

    BQ_TLS_NON_POD_INLINE(log_buffer::log_tls_info, log_tls_info_)

    bq_forceinline const log_thread_info& get_log_thread_info()
    {
        return log_tls_info__get_direct().get_thread_info();
    }

    bq_forceinline log_buffer_write_handle log_buffer::alloc_write_chunk(uint32_t size, uint32_t ext_info_size, uint64_t current_epoch_ms)
    {
        auto& tls_buffer = log_tls_info__get_direct().get_buffer_info(this);
        block_node_head* block = tls_buffer.cur_block_;
        // HP thread inside its frequency window, block kept: the full path would do just this
        BQ_LIKELY_IF(block && size <= hp_buffer_max_alloc_size_
            && current_epoch_ms < tls_buffer.last_update_epoch_ms_ + HP_BUFFER_CALL_FREQUENCY_CHECK_INTERVAL
            && !block->get_misc_data<block_misc_data>().need_reallocate_)
        {
            log_buffer_write_handle result = block->get_buffer().alloc_write_chunk(size);
            BQ_LIKELY_IF(result.result == enum_buffer_result_code::success)
            {
                if (++tls_buffer.update_times_ >= config_.high_frequency_threshold_per_second) {
                    tls_buffer.last_update_epoch_ms_ = current_epoch_ms;
                    tls_buffer.update_times_ = 0;
                }
                result.ext_info_reserved = false;
                return result;
            }
        }
        return alloc_write_chunk_full_impl(tls_buffer, size, ext_info_size, current_epoch_ms);
    }

    bq_forceinline void log_buffer::commit_write_chunk(const log_buffer_write_handle& handle)
    {
        auto& tls_buffer_info = log_tls_info__get_direct().get_buffer_info_directly(this);
        block_node_head* block = tls_buffer_info.cur_block_;
        BQ_LIKELY_IF(block)
        {
            block->get_buffer().commit_write_chunk(handle);
            return;
        }
        commit_write_chunk_full_impl(tls_buffer_info, handle);
    }

    bq_forceinline log_buffer::log_tls_buffer_info& log_buffer::log_tls_info::get_buffer_info(const log_buffer* buffer)
    {
        if (buffer->id_ == cur_log_buffer_id_) {
            return *cur_buffer_info_;
        }
        if (!log_map_) {
            log_map_ = new bq::hash_map_inline<uint64_t, log_tls_buffer_info*>();
            log_map_->set_expand_rate(4);
        }
        cur_log_buffer_id_ = buffer->id_;
        auto iter = log_map_->find(buffer->id_);
        if (iter == log_map_->end()) {
            iter = log_map_->add(buffer->id_, bq::util::aligned_new<log_tls_buffer_info>(alignof(log_tls_buffer_info)));
            iter->value()->destruction_mark_ = buffer->destruction_mark_;
            iter->value()->buffer_ = const_cast<log_buffer*>(buffer);
        }
        cur_buffer_info_ = iter->value();
        return *cur_buffer_info_;
    }

    bq_forceinline void log_buffer::set_cur_block(log_tls_buffer_info& tls_buffer_info, block_node_head* block)
    {
        tls_buffer_info.cur_block_ = block;
        log_tls_info__get_direct().on_cur_block_changed(id_, block);
    }

    bq_forceinline log_buffer::log_tls_buffer_info& log_buffer::log_tls_info::get_buffer_info_directly(const log_buffer* buffer)
    {
#if defined(BQ_LOG_BUFFER_DEBUG)
        assert(buffer->id_ == cur_log_buffer_id_ && "log_buffer::alloc and log_buffer::commit must use in pair");
#endif
        (void)buffer;
        return *cur_buffer_info_;
    }
}
