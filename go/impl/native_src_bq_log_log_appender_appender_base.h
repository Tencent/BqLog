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
#include "native_src_bq_log_log_log_types.h"
#include "native_src_bq_log_log_layout.h"
#include "native_src_bq_log_log_log_level_bitmap.h"
#include "native_src_bq_log_utils_time_zone.h"

namespace bq {
    class log_imp;
    class appender_base {
        friend class log_imp;

    public:
        enum appender_type {
            console,
            text_file,
            raw_file,
            compressed_file,
            type_count
        };

        appender_base();
        virtual ~appender_base();

    public:
        static bq::string get_config_name_by_type(const appender_type type);
        virtual void set_enable(bool enable);
        virtual bool get_enable();
        void clear();
        bool init(const bq::string& name, const bq::property_value& config_obj, const log_imp* parent_log);
        bool reset(const bq::property_value& config_obj);
        bool log(const log_entry_handle& handle);

        inline log_level_bitmap get_log_level_bitmap() const
        {
            return log_level_bitmap_;
        }

        inline const bq::string& get_name() const
        {
            return name_;
        }

        inline appender_type get_type() const
        {
            return type_;
        }

    private:
        void set_basic_configs(const bq::property_value& config_obj);

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) = 0;

        virtual bool reset_impl(const bq::property_value& config_obj) = 0;

        virtual bool log_impl(const log_entry_handle& handle) = 0;

        // Per-entry segment-boundary hooks. Called by process_log_chunk()
        // BEFORE log_impl on the entry that triggers a recovery / new-segment
        // transition. Subclasses use them to insert a marker (binary segment
        // header, "===== RECOVER START =====\n", etc.) into the output stream.
        // Return false to signal that the segment marker could NOT be emitted
        // (e.g. disk-full, refresh_file_handle failed, file_ stays closed) so
        // subclasses can bail BEFORE mutating subclass-side state (xor_key_blob_,
        // cache padding, segment counters, etc.). The caller in log_imp.cpp
        // ignores the return value today; this is purely a chain-of-responsibility
        // signal between base and derived hooks.
        virtual bool on_log_item_recovery_begin(bq::log_entry_handle& read_handle)
        {
            (void)read_handle;
            return true;
        }

        virtual void on_log_item_recovery_end() { }

        virtual bool on_log_item_new_begin(bq::log_entry_handle& read_handle)
        {
            (void)read_handle;
            return true;
        }

    protected:
        time_zone time_zone_;
        const log_imp* parent_log_;
        layout* layout_ptr_;
        appender_type type_;
        bool appenders_enable = true;
        bq::array<bq::string> categories_mask_config_;
        bq::array_inline<uint8_t> categories_mask_array_;

    private:
        log_level_bitmap log_level_bitmap_;
        bq::string name_;
    };
}
