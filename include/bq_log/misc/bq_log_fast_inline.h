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
#include "bq_common/bq_common_public_include.h"

namespace bq {
    BQ_MODULE_EXPORT_BEGIN
    namespace fast_inline {
        struct expected_layout {
            static constexpr uint32_t head_wt_reading_cursor_cache = 128;
            static constexpr uint32_t head_wt_writing_cursor_cache = 132;
            static constexpr uint32_t head_writing_cursor = 384;
            static constexpr uint32_t head_reading_cursor = 256;
            static constexpr uint32_t chunk_data_offset = 8;
            static constexpr uint32_t block_size_log2 = 3;
        };

        inline bool is_expected_layout(const bq::_api_fast_log_layout& layout)
        {
            typedef expected_layout e;
            return sizeof(void*) == 8 && layout.layout_version == BQ_FAST_LOG_LAYOUT_VERSION && layout.struct_size >= sizeof(layout)
                && layout.head_wt_reading_cursor_cache == e::head_wt_reading_cursor_cache
                && layout.head_wt_writing_cursor_cache == e::head_wt_writing_cursor_cache
                && layout.head_writing_cursor == e::head_writing_cursor && layout.head_reading_cursor == e::head_reading_cursor
                && layout.chunk_data_offset == e::chunk_data_offset && layout.block_size_log2 == e::block_size_log2;
        }

        struct thread_slot {
            bq::_api_fast_log_thread_state state;
            bq::platform::fast_clock_thread_cache clock_cache;
        };

        /// Singletons of one compiled module (an executable, DLL, .so or .dylib), not of the process:
        /// a process can hold several copies of BqLog and they must not share any of this. A template
        /// keeps it header-only under C++11 (no multiply-defined symbol) and still gives each module
        /// its own copy, which is what the address comparison against no_level relies on.
        template <typename T = void>
        struct fast_inline_state {
            /// the constant false that a thread slot's need_reallocate starts out pointing at
            static const bool allocate_mark;
            static const uint32_t no_level;
            static uint32_t layout_status; // 0 unknown, 1 ready, 2 unavailable
            // cold path only: serializes first-time site initialization; 0 = free
            static bq::platform::atomic_trivially_constructible<uint32_t> site_init_locks[32];
        };
        template <typename T>
        const bool fast_inline_state<T>::allocate_mark = false;
        template <typename T>
        const uint32_t fast_inline_state<T>::no_level = 0;
        template <typename T>
        uint32_t fast_inline_state<T>::layout_status = 0;
        template <typename T>
        bq::platform::atomic_trivially_constructible<uint32_t> fast_inline_state<T>::site_init_locks[32];

        // Function scope: a TLS static member of a class template gets an init guard on MSVC.
        inline thread_slot& get_thread_slot()
        {
            static BQ_TLS thread_slot slot = { { 0, nullptr, nullptr, &fast_inline_state<>::allocate_mark, 0, 0 }, {} };
            return slot;
        }

        // A minimal spin lock for the public header: spin_lock and its cpu_relax live in src/, which
        // user code cannot reach, so the pause below is a copy of bq::platform::thread::cpu_relax().
        // The lock is hashed by site address, so concurrent initializations of different sites
        // rarely share one.
        bq_forceinline void site_init_pause()
        {
#if defined(BQ_MSVC) && defined(BQ_X86)
            _mm_pause();
#elif defined(BQ_MSVC) && defined(BQ_ARM)
            __yield();
#elif defined(BQ_MSVC)
            _ReadWriteBarrier();
#elif defined(BQ_ARM)
            __asm__ __volatile__("yield");
#else
            __asm__ __volatile__("pause");
#endif
        }

        class site_init_guard {
            bq::platform::atomic_trivially_constructible<uint32_t>& lock_;

        public:
            explicit site_init_guard(const void* site)
                : lock_(fast_inline_state<>::site_init_locks[((reinterpret_cast<uintptr_t>(site) >> 3) ^ (reinterpret_cast<uintptr_t>(site) >> 9)) & 31U])
            {
                while (lock_.exchange_acquire(1U) != 0U) {
                    while (lock_.load_relaxed() != 0U) {
                        site_init_pause();
                    }
                }
            }

            ~site_init_guard()
            {
                lock_.store_release(0U);
            }

            site_init_guard(const site_init_guard&) = delete;
            site_init_guard& operator=(const site_init_guard&) = delete;
        };

        // level_word is the publication field of a fast site: the other fields are plain stores before it
        // and may only be read after an acquire load of it returned the initialized value.
        bq_forceinline const uint32_t* load_level_word_acquire(const bq::_api_fast_log_site_handle& site)
        {
            return BQ_PACK_ACCESS_BY_TYPE(site.level_word, bq::platform::atomic<const uint32_t*>).load_acquire();
        }

        bq_forceinline void store_level_word_release(bq::_api_fast_log_site_handle& site, const uint32_t* word)
        {
            BQ_PACK_ACCESS_BY_TYPE(site.level_word, bq::platform::atomic<const uint32_t*>).store_release(word);
        }

        typedef bq::platform::atomic_trivially_constructible<uint32_t> cursor;

        bq_forceinline cursor& cursor_at(uint8_t* head, uint32_t offset)
        {
            return *reinterpret_cast<cursor*>(head + offset);
        }

        bq_forceinline void store_u32(uint8_t* address, uint32_t value)
        {
            memcpy(address, &value, sizeof(value));
        }

        inline void refresh_thread_slot()
        {
            uint32_t& status = fast_inline_state<>::layout_status;
            if (status == 0) {
                bq::_api_fast_log_layout layout = {};
                status = (bq::api::__api_fast_log_get_layout(&layout, BQ_FAST_LOG_LAYOUT_VERSION) && is_expected_layout(layout)) ? 1U : 2U;
            }
            if (status != 1) {
                return;
            }
            bq::api::__api_fast_log_bind_thread_state(&get_thread_slot().state);
        }

        // 0 written, 1 written and the consumer should be woken, -1 take the slow path
        //
        // META says where the record's meta address comes from: the macro path already holds it in a
        // register (by value), the fast_info path reads it from the site only when the head is
        // stored, which keeps fewer values live across the resync check.
        struct fast_meta_by_value {
            uint64_t addr;
            bq_forceinline uint64_t get() const { return addr; }
        };

        // fast_info: the site publishes meta_addr | (enabled level mask) in one field, and the
        // record head stores that raw value (the level bits survive; every consumer masks them off
        // with fast_meta_addr_mask()). The field is read again here, at head store time, rather than
        // reusing the load that gated the branch: the two reads of one atomic object must agree
        // (read-read coherence), so the second load cannot observe an address different from the
        // one the branch accepted.
        struct fast_meta_by_site_v {
            const uint64_t* v;
            bq_forceinline uint64_t get() const
            {
                return BQ_PACK_ACCESS_BY_TYPE(*v, bq::platform::atomic<uint64_t>).load_relaxed();
            }
        };

        // Like fast_meta_by_ptr, but holds the site instead of a pointer to one of its packed
        // members: the macro's site is a packed struct, so &site.format_meta_addr warns under
        // -Waddress-of-packed-member. Reading the field through the site pointer keeps the load
        // late (at head store time) exactly as by_ptr does.
        struct fast_meta_by_site {
            const bq::_api_fast_log_site_handle* site;
            bq_forceinline uint64_t get() const
            {
                return BQ_PACK_ACCESS_BY_TYPE(site->format_meta_addr, bq::platform::atomic<uint64_t>).load_relaxed();
            }
        };

        template <typename META, typename FILL>
        bq_forceinline int32_t write(uint64_t buffer_id, const META& meta, uint32_t args_size, const FILL& fill)
        {
            typedef expected_layout l;
            thread_slot& slot = get_thread_slot();
            const bq::_api_fast_log_thread_state& state = slot.state;
            BQ_UNLIKELY_IF((~state.buffer_key != buffer_id) | *state.need_reallocate)
            {
                return -1;
            }
            // before the cursors: fewer live values across resync
            uint64_t epoch_ms;
            BQ_UNLIKELY_IF(!bq::platform::fast_clock_read_epoch_ms(slot.clock_cache, epoch_ms))
            {
                return -1;
            }
            uint8_t* head = state.siso_head;
            uint8_t* blocks = state.siso_units;
            const uint32_t count = state.unit_count;
            // separate loads: a fused ldp stalls on the previous record's cursor store
            const uint32_t write_cursor = cursor_at(head, l::head_wt_writing_cursor_cache).load_relaxed();
            const uint32_t read_cursor_cache = cursor_at(head, l::head_wt_reading_cursor_cache).load_relaxed();
            const uint32_t record_size = static_cast<uint32_t>(sizeof(bq::log_head_fast_def)) + args_size;
            const uint32_t need = (record_size + l::chunk_data_offset + (1U << l::block_size_log2) - 1) >> l::block_size_log2;
            const uint32_t index = write_cursor & (count - 1);
            uint8_t* chunk = blocks + (static_cast<size_t>(index) << l::block_size_log2);
            uint8_t* record = chunk + l::chunk_data_offset;
            uint32_t used = need;
            BQ_UNLIKELY_IF(count - index < need)
            {
                // as siso_ring_buffer::alloc_write_chunk: the head keeps the tail blocks, the record starts at block 0
                used = count - index + ((record_size + (1U << l::block_size_log2) - 1) >> l::block_size_log2);
                record = blocks;
            }
            BQ_UNLIKELY_IF(read_cursor_cache + count - write_cursor < used)
            {
                return -1;
            }
            store_u32(chunk, used);
            store_u32(chunk + sizeof(uint32_t), record_size);
            bq::log_head_fast_def record_head;
            record_head.timestamp_epoch = bq::log_head_base_def::set_fast(epoch_ms);
            record_head.format_meta_addr = meta.get();
            memcpy(record, &record_head, sizeof(record_head));
            fill(record + sizeof(bq::log_head_fast_def));
            const uint32_t new_cursor = write_cursor + used;
            cursor_at(head, l::head_wt_writing_cursor_cache).store_relaxed(new_cursor);
            cursor_at(head, l::head_writing_cursor).store_release(new_cursor);
            // same edge-triggered rule as siso_ring_buffer::alloc_write_chunk
            BQ_UNLIKELY_IF(static_cast<uint32_t>(read_cursor_cache + state.half_unit_count - write_cursor - 1) < used)
            {
                const uint32_t read_cursor = cursor_at(head, l::head_reading_cursor).load_acquire();
                cursor_at(head, l::head_wt_reading_cursor_cache).store_relaxed(read_cursor);
                return ((new_cursor - read_cursor) << 1) >= count ? 1 : 0;
            }
            return 0;
        }
    }
    BQ_MODULE_EXPORT_END
}
