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
#include "bq_log_godot.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include "bq_log_godot_console.h"
#include "bq_log_godot_encoder.h"

namespace bq {

    void bq_log_category::_bind_methods()
    {
        godot::ClassDB::bind_static_method("bq_log_category", godot::D_METHOD("create_category", "index"), &bq_log_category::create_category);
        godot::ClassDB::bind_method(godot::D_METHOD("set_index", "index"), &bq_log_category::set_index);
        godot::ClassDB::bind_method(godot::D_METHOD("get_index"), &bq_log_category::get_index);
    }

    godot::Ref<bq_log_category> bq_log_category::create_category(int32_t index)
    {
        godot::Ref<bq_log_category> cat;
        cat.instantiate();
        cat->set_index(index);
        return cat;
    }

    void bq_log_base::_bind_methods()
    {
        BIND_CONSTANT(LOG_LEVEL_VERBOSE);
        BIND_CONSTANT(LOG_LEVEL_DEBUG);
        BIND_CONSTANT(LOG_LEVEL_INFO);
        BIND_CONSTANT(LOG_LEVEL_WARNING);
        BIND_CONSTANT(LOG_LEVEL_ERROR);
        BIND_CONSTANT(LOG_LEVEL_FATAL);

        BIND_CONSTANT(RECOVERY_READY);
        BIND_CONSTANT(RECOVERY_NO_MEMORY_MAP);
        BIND_CONSTANT(RECOVERY_NO_INSTANCE_LOCK);

        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("get_version"), &bq_log_base::get_version);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("enable_auto_crash_handle"), &bq_log_base::enable_auto_crash_handle);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("get_recovery_capability"), &bq_log_base::get_recovery_capability);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("get_platform_capabilities"), &bq_log_base::get_platform_capabilities);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("get_file_base_dir", "base_dir_type"), &bq_log_base::get_file_base_dir);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("reset_base_dir", "base_dir_type", "dir"), &bq_log_base::reset_base_dir);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("force_flush_all_logs"), &bq_log_base::force_flush_all_logs);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("register_console_callback", "callback"), &bq_log_base::register_console_callback);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("unregister_console_callback", "callback"), &bq_log_base::unregister_console_callback);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("set_console_buffer_enable", "enable"), &bq_log_base::set_console_buffer_enable);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("fetch_and_remove_console_buffer", "callback"), &bq_log_base::fetch_and_remove_console_buffer);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("set_godot_console_redirect_enable", "enable"), &bq_log_base::set_godot_console_redirect_enable);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("is_godot_console_redirect_enabled"), &bq_log_base::is_godot_console_redirect_enabled);
        godot::ClassDB::bind_static_method("bq_log_base", godot::D_METHOD("console", "level", "str"), &bq_log_base::console);

        godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &bq_log_base::is_valid);
        godot::ClassDB::bind_method(godot::D_METHOD("get_id"), &bq_log_base::get_id);
        godot::ClassDB::bind_method(godot::D_METHOD("get_name"), &bq_log_base::get_name);
        godot::ClassDB::bind_method(godot::D_METHOD("reset_config", "config"), &bq_log_base::reset_config);
        godot::ClassDB::bind_method(godot::D_METHOD("set_appender_enable", "appender_name", "enable"), &bq_log_base::set_appender_enable);
        godot::ClassDB::bind_method(godot::D_METHOD("force_flush"), &bq_log_base::force_flush);
        godot::ClassDB::bind_method(godot::D_METHOD("take_snapshot", "time_zone_config"), &bq_log_base::take_snapshot);

        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "verbose", &bq_log_base::verbose, godot::MethodInfo("verbose"));
        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "debug", &bq_log_base::debug, godot::MethodInfo("debug"));
        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "info", &bq_log_base::info, godot::MethodInfo("info"));
        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "warning", &bq_log_base::warning, godot::MethodInfo("warning"));
        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "error", &bq_log_base::error, godot::MethodInfo("error"));
        godot::ClassDB::bind_vararg_method(godot::METHOD_FLAGS_DEFAULT, "fatal", &bq_log_base::fatal, godot::MethodInfo("fatal"));
    }

    void bq_log_base::init_by_id(uint64_t log_id)
    {
        log_id_ = 0u;
        name_ = godot::String();
        categories_name_array_.clear();
        categories_count_ = 0u;
        merged_log_level_bitmap_ = nullptr;
        categories_mask_array_ = nullptr;

        if (log_id == 0u) {
            return;
        }

        bq::_api_string_def name_def {};
        if (!bq::api::__api_get_log_name_by_id(log_id, &name_def)) {
            return;
        }

        log_id_ = log_id;
        name_ = (name_def.str && name_def.len > 0u)
            ? godot::String::utf8(name_def.str, static_cast<int32_t>(name_def.len))
            : godot::String();

        categories_count_ = bq::api::__api_get_log_categories_count(log_id);
        categories_name_array_.resize(static_cast<int32_t>(categories_count_));
        for (uint32_t i = 0u; i < categories_count_; ++i) {
            bq::_api_string_def cat_def {};
            if (bq::api::__api_get_log_category_name_by_index(log_id, i, &cat_def) && cat_def.str && cat_def.len > 0u) {
                categories_name_array_.set(static_cast<int32_t>(i), godot::String::utf8(cat_def.str, static_cast<int32_t>(cat_def.len)));
            } else {
                categories_name_array_.set(static_cast<int32_t>(i), godot::String());
            }
        }

        merged_log_level_bitmap_ = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
        categories_mask_array_ = bq::api::__api_get_log_category_masks_array_by_log_id(log_id);
    }

    bool bq_log_base::is_enabled_for_fast(uint32_t category_index, bq::log_level level) const
    {
        if (log_id_ == 0u || !merged_log_level_bitmap_ || !categories_mask_array_) {
            return false;
        }
        if ((*merged_log_level_bitmap_ & (1u << static_cast<uint32_t>(level))) == 0u) {
            return false;
        }
        if (category_index >= categories_count_ || categories_mask_array_[category_index] == 0u) {
            return false;
        }
        return true;
    }

    godot::Variant bq_log_base::do_log_vararg(
        bq::log_level level,
        const godot::Variant** args,
        GDExtensionInt arg_count,
        GDExtensionCallError& error) const
    {
        error.error = GDEXTENSION_CALL_OK;
        if (log_id_ == 0u || arg_count <= 0 || !args || bq_godot::is_in_console_callback()) {
            return false;
        }

        uint32_t category_index = 0u;
        GDExtensionInt fmt_index = 0;

        if (args[0]->get_type() == godot::Variant::OBJECT) {
            godot::Object* obj = *args[0];
            if (const auto* cat = godot::Object::cast_to<bq_log_category>(obj)) {
                const int32_t idx = cat->get_index();
                if (idx < 0) {
                    return false;
                }
                category_index = static_cast<uint32_t>(idx);
                fmt_index = 1;
            }
        }

        if (fmt_index >= arg_count) {
            return false;
        }

        // O(1) lock-free, zero-allocation fast filter before touching format string or args.
        if (!is_enabled_for_fast(category_index, level)) {
            return false;
        }

        const godot::Variant& fmt_var = *args[fmt_index];
        godot::String format_str;
        if (fmt_var.get_type() == godot::Variant::STRING) {
            format_str = fmt_var;
        } else {
            format_str = fmt_var.stringify();
        }

        const godot::Variant* const* value_args = (fmt_index + 1 < arg_count) ? (args + fmt_index + 1) : nullptr;
        const int64_t value_arg_count = static_cast<int64_t>(arg_count - fmt_index - 1);

        return bq_godot::write_godot_log_entry(
            log_id_,
            level,
            category_index,
            format_str,
            value_args,
            value_arg_count);
    }

    godot::String bq_log_base::get_version()
    {
        const char* ver = bq::api::__api_get_log_version();
        return ver ? godot::String::utf8(ver) : godot::String();
    }

    void bq_log_base::enable_auto_crash_handle()
    {
        bq::api::__api_enable_auto_crash_handler();
    }

    int32_t bq_log_base::get_recovery_capability()
    {
        return static_cast<int32_t>(bq::api::__api_get_recovery_capability());
    }

    int64_t bq_log_base::get_platform_capabilities()
    {
        return static_cast<int64_t>(bq::api::__api_get_platform_capabilities());
    }

    godot::String bq_log_base::get_file_base_dir(int32_t base_dir_type)
    {
        const char* dir = bq::api::__api_get_file_base_dir(base_dir_type);
        return dir ? godot::String::utf8(dir) : godot::String();
    }

    void bq_log_base::reset_base_dir(int32_t base_dir_type, const godot::String& dir)
    {
        const godot::CharString utf8 = dir.utf8();
        bq::api::__api_reset_base_dir(base_dir_type, utf8.get_data());
    }

    void bq_log_base::force_flush_all_logs()
    {
        bq::api::__api_force_flush(0u);
    }

    void bq_log_base::register_console_callback(const godot::Callable& callback)
    {
        bq_godot::register_user_console_callback(callback);
    }

    void bq_log_base::unregister_console_callback(const godot::Callable& callback)
    {
        bq_godot::unregister_user_console_callback(callback);
    }

    void bq_log_base::set_console_buffer_enable(bool enable)
    {
        bq::api::__api_set_console_buffer_enable(enable);
    }

    bool bq_log_base::fetch_and_remove_console_buffer(const godot::Callable& callback)
    {
        return bq_godot::fetch_and_remove_godot_console_buffer(callback);
    }

    void bq_log_base::set_godot_console_redirect_enable(bool enable)
    {
        bq_godot::set_godot_console_redirect_enable(enable);
    }

    bool bq_log_base::is_godot_console_redirect_enabled()
    {
        return bq_godot::is_godot_console_redirect_enabled();
    }

    void bq_log_base::console(int32_t level, const godot::String& str)
    {
        const godot::CharString utf8 = str.utf8();
        bq::api::__api_log_device_console(static_cast<bq::log_level>(level), utf8.get_data());
    }

    bool bq_log_base::reset_config(const godot::String& config)
    {
        if (log_id_ == 0u || config.is_empty()) {
            return false;
        }
        const godot::CharString name_utf8 = name_.utf8();
        const godot::CharString config_utf8 = config.utf8();
        return bq::api::__api_log_reset_config(name_utf8.get_data(), config_utf8.get_data());
    }

    void bq_log_base::set_appender_enable(const godot::String& appender_name, bool enable)
    {
        if (log_id_ == 0u) {
            return;
        }
        const godot::CharString utf8 = appender_name.utf8();
        bq::api::__api_set_appender_enable(log_id_, utf8.get_data(), enable);
    }

    void bq_log_base::force_flush() const
    {
        if (log_id_ == 0u) {
            return;
        }
        bq::api::__api_force_flush(log_id_);
    }

    godot::String bq_log_base::take_snapshot(const godot::String& time_zone_config) const
    {
        if (log_id_ == 0u) {
            return godot::String();
        }
        const godot::CharString tz_utf8 = time_zone_config.utf8();
        bq::_api_string_def snap {};
        bq::api::__api_take_snapshot_string(log_id_, tz_utf8.get_data(), &snap);
        godot::String result = (snap.str && snap.len > 0u)
            ? godot::String::utf8(snap.str, static_cast<int32_t>(snap.len))
            : godot::String();
        bq::api::__api_release_snapshot_string(log_id_, &snap);
        return result;
    }

    godot::Variant bq_log_base::verbose(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::verbose, args, arg_count, error);
    }

    godot::Variant bq_log_base::debug(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::debug, args, arg_count, error);
    }

    godot::Variant bq_log_base::info(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::info, args, arg_count, error);
    }

    godot::Variant bq_log_base::warning(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::warning, args, arg_count, error);
    }

    godot::Variant bq_log_base::error(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::error, args, arg_count, error);
    }

    godot::Variant bq_log_base::fatal(const godot::Variant** args, GDExtensionInt arg_count, GDExtensionCallError& error)
    {
        return do_log_vararg(bq::log_level::fatal, args, arg_count, error);
    }

    void bq_log::_bind_methods()
    {
        godot::ClassDB::bind_static_method("bq_log", godot::D_METHOD("create_log", "name", "config"), &bq_log::create_log);
        godot::ClassDB::bind_static_method("bq_log", godot::D_METHOD("get_log_by_name", "log_name"), &bq_log::get_log_by_name);
    }

    godot::Ref<bq_log> bq_log::create_log(const godot::String& name, const godot::String& config)
    {
        godot::Ref<bq_log> result;
        result.instantiate();
        if (config.is_empty()) {
            return result;
        }
        const godot::CharString name_utf8 = name.utf8();
        const godot::CharString config_utf8 = config.utf8();
        const uint64_t log_id = bq::api::__api_create_log(name_utf8.get_data(), config_utf8.get_data(), 0u, nullptr);
        result->init_by_id(log_id);
        return result;
    }

    godot::Ref<bq_log> bq_log::get_log_by_name(const godot::String& log_name)
    {
        godot::Ref<bq_log> result;
        result.instantiate();
        if (log_name.is_empty()) {
            return result;
        }
        const uint32_t count = bq::api::__api_get_logs_count();
        for (uint32_t i = 0u; i < count; ++i) {
            const uint64_t id = bq::api::__api_get_log_id_by_index(i);
            bq::_api_string_def name_def {};
            if (bq::api::__api_get_log_name_by_id(id, &name_def) && name_def.str) {
                const godot::String candidate = godot::String::utf8(name_def.str, static_cast<int32_t>(name_def.len));
                if (candidate == log_name) {
                    result->init_by_id(id);
                    return result;
                }
            }
        }
        return result;
    }

}
