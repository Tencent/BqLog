#pragma once
/*
 * Copyright (C) 2024 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_types.h"
#include "bq_log/log/log_level_bitmap.h"
#include "bq_log/log/log_worker.h"
#include "bq_log/types/ring_buffer.h"

namespace bq {
    constexpr uint64_t log_id_magic_number = 0x24FE284C23EA5821;
    class log_imp {
        friend class log_manager;
        friend const uint8_t* bq::api::__api_get_log_category_masks_array_by_log_id(uint64_t log_id);
        friend const uint32_t* bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(uint64_t log_id);
        friend const uint32_t* bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(uint64_t log_id);

    public:
        log_imp();
        ~log_imp();
        bool init(const bq::string& name, const property_value& config, const bq::array<bq::string>& category_names);
        bool reset_config(const property_value& config);
        void log(const log_entry_handle& handle);

        const bq::string& take_snapshot_string(bool use_gmt_time);
        void release_snapshot_string();

        const bq::string& get_name() const;
        uint32_t get_categories_count() const;
        const bq::string& get_category_name_by_index(uint32_t index) const;
        const bq::array<bq::string>& get_categories_name() const;

        void set_appenders_enable(const bq::string& appender_name, bool enable);

        inline ring_buffer& get_ring_buffer() const
        {
            return *ring_buffer_;
        }

        inline bq::log_reliable_level get_reliable_level() const
        {
            return reliable_level_;
        }

        inline uint64_t id() const
        {
            return id_;
        }

        inline log_thread_mode get_thread_mode() const
        {
            return thread_mode_;
        }

        void set_config(const bq::string& config);

        bq::string& get_config();

        void process(bool is_force_flush);

        void sync_process();

        const layout& get_layout() const;

    private:
        bool add_appender(const string& name, const bq::property_value& jobj);
        void refresh_merged_log_level_bitmap();
        void set_thread_mode(log_thread_mode thread_mode);
        void flush_appenders_cache();
        void flush_appenders_io();
        void clear();
        void process_log_chunk(bq::log_entry_handle& read_handle);

    private:
        uint64_t id_;
        log_thread_mode thread_mode_;
        log_worker worker_;
        layout layout_;
        bq::string name_;
        bq::platform::spin_lock spin_lock_;
        log_level_bitmap merged_log_level_bitmap_;
        log_level_bitmap print_stack_level_bitmap_;
        bq::log_reliable_level reliable_level_;
        ring_buffer* ring_buffer_;
        class log_snapshot* snapshot_;
        uint64_t last_log_entry_epoch_ms_;
        uint64_t last_flush_io_epoch_ms_;
        bq::array_inline<appender_base*> appenders_list_;
        bq::array<bq::string> categories_name_array_;
        bq::array_inline<uint8_t> categories_mask_array_;

        bq::string last_config_;
    };
}
