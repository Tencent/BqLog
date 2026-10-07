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
#include "native_include_bq_common_platform_fast_clock.h"

namespace bq {
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

        template <typename T = void>
        struct module_state {
            static const bool no_reallocate;
            static const uint32_t no_level;
            static uint32_t layout_status; // 0 unknown, 1 ready, 2 unavailable
        };
        template <typename T>
        const bool module_state<T>::no_reallocate = false;
        template <typename T>
        const uint32_t module_state<T>::no_level = 0;
        template <typename T>
        uint32_t module_state<T>::layout_status = 0;

        // Function scope: a TLS static member of a class template gets an init guard on MSVC.
        inline thread_slot& get_thread_slot()
        {
            static BQ_TLS thread_slot slot = { { 0, nullptr, nullptr, &module_state<>::no_reallocate, 0, 0 }, {} };
            return slot;
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
            uint32_t& status = module_state<>::layout_status;
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
        template <typename FILL>
        bq_forceinline int32_t write(const bq::_api_fast_log_site_handle& site, uint32_t args_size, const FILL& fill)
        {
            typedef expected_layout l;
            thread_slot& slot = get_thread_slot();
            const bq::_api_fast_log_thread_state& state = slot.state;
            BQ_UNLIKELY_IF((~state.buffer_key != site.buffer_id) | *state.need_reallocate)
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
            record_head.format_meta_addr = site.format_meta_addr;
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
}
