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

#include <stdint.h>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include "bq_log/bq_log.h"

namespace bq {

    class bq_log_category : public godot::RefCounted {
        GDCLASS(bq_log_category, godot::RefCounted)

    private:
        int32_t index_ = 0;

    protected:
        static void _bind_methods();

    public:
        bq_log_category() = default;
        explicit bq_log_category(int32_t index)
            : index_(index)
        {
        }

        static godot::Ref<bq_log_category> create_category(int32_t index);

        void set_index(int32_t index) { index_ = index; }
        int32_t get_index() const { return index_; }
    };

    class bq_log_base : public godot::RefCounted {
        GDCLASS(bq_log_base, godot::RefCounted)

    public:
        enum log_level_const {
            LOG_LEVEL_VERBOSE = static_cast<int32_t>(bq::log_level::verbose),
            LOG_LEVEL_DEBUG = static_cast<int32_t>(bq::log_level::debug),
            LOG_LEVEL_INFO = static_cast<int32_t>(bq::log_level::info),
            LOG_LEVEL_WARNING = static_cast<int32_t>(bq::log_level::warning),
            LOG_LEVEL_ERROR = static_cast<int32_t>(bq::log_level::error),
            LOG_LEVEL_FATAL = static_cast<int32_t>(bq::log_level::fatal)
        };

        enum recovery_capability_const {
            RECOVERY_READY = static_cast<int32_t>(bq::recovery_capability::ready),
            RECOVERY_NO_MEMORY_MAP = static_cast<int32_t>(bq::recovery_capability::no_memory_map),
            RECOVERY_NO_INSTANCE_LOCK = static_cast<int32_t>(bq::recovery_capability::no_instance_lock)
        };

    protected:
        uint64_t log_id_ = 0u;
        godot::String name_;
        godot::PackedStringArray categories_name_array_;
        uint32_t categories_count_ = 0u;
        const uint32_t* merged_log_level_bitmap_ = nullptr;
        const uint8_t* categories_mask_array_ = nullptr;

        static void _bind_methods();

        void init_by_id(uint64_t log_id);
        bool is_enabled_for_fast(uint32_t category_index, bq::log_level level) const;
        godot::Variant do_log_vararg(
            bq::log_level level,
            const godot::Variant** args,
            GDExtensionInt arg_count,
            GDExtensionCallError& error) const;

    public:
        bq_log_base() = default;

        static godot::String get_version();
        static void enable_auto_crash_handle();
        static int32_t get_recovery_capability();
        static int64_t get_platform_capabilities();
        static godot::String get_file_base_dir(int32_t base_dir_type);
        static void reset_base_dir(int32_t base_dir_type, const godot::String& dir);
        static void force_flush_all_logs();
        static void register_console_callback(const godot::Callable& callback);
        static void unregister_console_callback(const godot::Callable& callback);
        static void set_console_buffer_enable(bool enable);
        static bool fetch_and_remove_console_buffer(const godot::Callable& callback);
        static void set_godot_console_redirect_enable(bool enable);
        static bool is_godot_console_redirect_enabled();
        static void console(int32_t level, const godot::String& str);

        bool is_valid() const { return log_id_ != 0u; }
        int64_t get_id() const { return static_cast<int64_t>(log_id_); }
        godot::String get_name() const { return name_; }
        bool reset_config(const godot::String& config);
        void set_appender_enable(const godot::String& appender_name, bool enable);
        void force_flush() const;
        godot::String take_snapshot(const godot::String& time_zone_config) const;

        godot::Variant verbose(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
        godot::Variant debug(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
        godot::Variant info(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
        godot::Variant warning(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
        godot::Variant error(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
        godot::Variant fatal(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error);
    };

    class bq_log : public bq_log_base {
        GDCLASS(bq_log, bq_log_base)

    protected:
        static void _bind_methods();

    public:
        bq_log() = default;

        static godot::Ref<bq_log> create_log(const godot::String& name, const godot::String& config);
        static godot::Ref<bq_log> get_log_by_name(const godot::String& log_name);
    };

}
