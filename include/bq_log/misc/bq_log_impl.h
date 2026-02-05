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
 * \author pippocao
 * \date 2022.08.03
 */
#include <stdint.h>
#include <string.h>
#include "bq_log/bq_log.h"

namespace bq {
    namespace impl {
        template <bool INCLUDE_TYPE_INFO, typename First>
        inline void _do_log_args_fill(uint8_t* data_addr, const bq::tools::size_seq<INCLUDE_TYPE_INFO, First>& seq, const First& first)
        {
            size_t data_size = seq.get_element().get_value();
            bq::tools::_type_copy<INCLUDE_TYPE_INFO>(first, data_addr, data_size);
        }

        template <bool INCLUDE_TYPE_INFO, typename First, typename... Args>
        inline void _do_log_args_fill(uint8_t* data_addr, const bq::tools::size_seq<INCLUDE_TYPE_INFO, First, Args...>& seq, const First& first, const Args&... rest)
        {
            size_t data_size = seq.get_element().get_value();
            bq::tools::_type_copy<INCLUDE_TYPE_INFO>(first, data_addr, data_size);
            size_t alloc_size = seq.get_element().get_aligned_value();
            data_addr += alloc_size;
            _do_log_args_fill<INCLUDE_TYPE_INFO>(data_addr, seq.get_next(), rest...);
        }
    }

    inline bq::string log::get_version()
    {
        return api::__api_get_log_version();
    }

    inline void log::enable_auto_crash_handle()
    {
        api::__api_enable_auto_crash_handler();
    }

    inline log log::create_log(const bq::string& log_name, const bq::string& config_content)
    {
        uint64_t log_id = api::__api_create_log(log_name.c_str(), config_content.c_str(), 0, nullptr);
        log result = get_log_by_id(log_id);
        return result;
    }

    inline log::log(const bq::log& rhs)
        : merged_log_level_bitmap_(rhs.merged_log_level_bitmap_)
        , categories_mask_array_(rhs.categories_mask_array_)
        , print_stack_level_bitmap_(rhs.print_stack_level_bitmap_)
        , log_id_(rhs.log_id_)
        , name_(rhs.name_)
        , categories_name_array_(rhs.categories_name_array_)
    {
    }

    inline log& log::operator=(const bq::log& rhs)
    {
        merged_log_level_bitmap_ = rhs.merged_log_level_bitmap_;
        categories_mask_array_ = rhs.categories_mask_array_;
        print_stack_level_bitmap_ = rhs.print_stack_level_bitmap_;
        log_id_ = rhs.log_id_;
        name_ = rhs.name_;
        categories_name_array_ = rhs.categories_name_array_;
        return *this;
    }

    inline bool log::reset_config(const bq::string& config_content)
    {
        bool result = api::__api_log_reset_config(name_.c_str(), config_content.c_str());
        return result;
    }

    inline void log::set_appender_enable(const bq::string& appender_name, bool enable)
    {
        api::__api_set_appender_enable(log_id_, appender_name.c_str(), enable);
    }

    inline void log::force_flush()
    {
        api::__api_force_flush(log_id_);
    }

    inline bq::string log::get_file_base_dir(int32_t base_dir_type)
    {
        return api::__api_get_file_base_dir(base_dir_type);
    }

    inline void log::reset_base_dir(int32_t base_dir_type, const bq::string& dir)
    {
        bq::api::__api_reset_base_dir(base_dir_type, dir.c_str());
    }

    inline log log::get_log_by_id(uint64_t log_id)
    {
        log log;
        bq::_api_string_def log_name_tmp;
        if (!bq::api::__api_get_log_name_by_id(log_id, &log_name_tmp)) {
            return log;
        }
        log.log_id_ = log_id;
        log.name_ = log_name_tmp.str;
        log.merged_log_level_bitmap_ = bq::api::__api_get_log_merged_log_level_bitmap_by_log_id(log_id);
        log.categories_mask_array_ = bq::api::__api_get_log_category_masks_array_by_log_id(log_id);
        log.print_stack_level_bitmap_ = bq::api::__api_get_log_print_stack_level_bitmap_by_log_id(log_id);
        uint32_t category_count = bq::api::__api_get_log_categories_count(log_id);
        log.categories_name_array_.set_capacity(category_count);
        for (uint32_t i = 0; i < category_count; ++i) {
            bq::_api_string_def category_name_tmp;
            bq::api::__api_get_log_category_name_by_index(log_id, i, &category_name_tmp);
            const char* name_str = category_name_tmp.str; // avoid directly reference packed field
            log.categories_name_array_.push_back(name_str);
        }
        return log;
    }

    inline log log::get_log_by_name(const bq::string& log_name)
    {
        uint32_t log_count = bq::api::__api_get_logs_count();
        for (uint32_t i = 0; i < log_count; ++i) {
            auto id = bq::api::__api_get_log_id_by_index(i);
            bq::_api_string_def log_name_tmp;
            if (bq::api::__api_get_log_name_by_id(id, &log_name_tmp)) {
                if (log_name == log_name_tmp.str) {
                    return get_log_by_id(id);
                }
            }
        }
        log log;
        return log;
    }

    inline void log::force_flush_all_logs()
    {
        api::__api_force_flush(0);
    }

    inline void log::register_console_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::api::__api_register_console_callbacks(callback);
    }

    inline void log::unregister_console_callback(bq::type_func_ptr_console_callback callback)
    {
        bq::api::__api_unregister_console_callbacks(callback);
    }

    inline void log::set_console_buffer_enable(bool enable)
    {
        bq::api::__api_set_console_buffer_enable(enable);
    }

    inline void BQ_STDCALL fetch_and_remove_console_buffer_callback_wrapper(void* pass_through_param, uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length)
    {
        bq::type_func_ptr_console_callback real_callback = (bq::type_func_ptr_console_callback)pass_through_param;
        real_callback(log_id, category_idx, log_level, content, length);
    }

    inline bool log::fetch_and_remove_console_buffer(bq::type_func_ptr_console_callback on_console_callback)
    {
        return bq::api::__api_fetch_and_remove_console_buffer(fetch_and_remove_console_buffer_callback_wrapper, (const void*)on_console_callback);
    }

    template <typename STR>
    inline bq::enable_if_t<bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, char*>::value || bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, const char*>::value> log::console(bq::log_level level, const STR& str)
    {
        bq::api::__api_log_device_console(level, str);
    }

    template <typename STR>
    inline bq::enable_if_t<!(bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, char*>::value || bq::is_same<bq::decay_t<bq::remove_cv_t<STR>>, const char*>::value)> log::console(bq::log_level level, const STR& str)
    {
        bq::api::__api_log_device_console(level, str.c_str());
    }

    inline bool log::is_enable_for(uint32_t category_index, bq::log_level level) const
    {
        return ((*merged_log_level_bitmap_ & (1U << (int32_t)level)) != 0) && categories_mask_array_[category_index];
    }

    inline bq::string log::take_snapshot(const bq::string& time_zone_config) const
    {
        bq::_api_string_def snapshot_def;
        bq::api::__api_take_snapshot_string(log_id_, time_zone_config.c_str(), &snapshot_def);
        bq::string result;
        result.insert_batch(result.begin(), snapshot_def.str, snapshot_def.len);
        bq::api::__api_release_snapshot_string(log_id_, &snapshot_def);
        return result;
    }

    template <typename STR>
    bq_forceinline bq::tuple<const char*, uint32_t> get_stack_trace()
    {
        if (bq::tools::_get_log_param_type_enum<STR>() == log_arg_type_enum::string_utf8_type) {
            bq::_api_string_def str;
            bq::api::__api_get_stack_trace(&str, 0);
            const char* trace_str = str.str;
            uint32_t trace_len = str.len;
            return bq::make_tuple(trace_str, trace_len);
        } else {
            bq::_api_u16string_def str;
            bq::api::__api_get_stack_trace_utf16(&str, 0);
            const char* trace_str = (const char*)str.str;
            uint32_t trace_len = str.len << 1;
            return bq::make_tuple(trace_str, trace_len);
        }
    }

    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::do_log(uint32_t category_index, bq::log_level level, const STR& log_format_content) const
    {
        if (!is_enable_for(category_index, level)) {
            return false;
        }
        bool should_print_stack = (*print_stack_level_bitmap_ & static_cast<uint32_t>(1 << (int32_t)level));
        bq::tuple<const char*, uint32_t> stack_info = should_print_stack ? get_stack_trace<STR>() : bq::make_tuple((const char*)nullptr, (uint32_t)0);
        size_t format_size = bq::tools::_serialize_str_helper_by_type<STR>::get_storage_data_size(log_format_content);
        size_t total_format_data_size = format_size + bq::get<1>(stack_info);
        bq::_api_log_write_handle handle;
        const void* format_data_ptr = should_print_stack ? nullptr : bq::tools::_serialize_str_helper_by_type<STR>::get_storage_data_addr(log_format_content);
        handle = bq::api::__api_log_write_begin(log_id_,
                static_cast<uint8_t>(level),
                category_index,
                static_cast<uint8_t>(is_bq_log_format<STR>::arg_type),
                static_cast<uint32_t>(total_format_data_size),
                format_data_ptr,
                0);
        if (handle.result != bq::enum_buffer_result_code::success) {
            return false;
        }
        if (!format_data_ptr) {
            //Ugly hack
            bq::tools::_type_copy<false>(log_format_content, handle.format_data_addr - sizeof(uint32_t), total_format_data_size + sizeof(uint32_t));
            memcpy(handle.format_data_addr + format_size, bq::get<0>(stack_info), bq::get<1>(stack_info));
            *reinterpret_cast<uint32_t*>(handle.format_data_addr - sizeof(uint32_t)) = static_cast<uint32_t>(total_format_data_size);
        }
        bq::api::__api_log_write_finish(log_id_, handle);
        return true;
    }

    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::do_log(uint32_t category_index, bq::log_level level, const STR& log_format_content, const Args&... args) const
    {
        if (!is_enable_for(category_index, level)) {
            return false;
        }
        bool should_print_stack = (*print_stack_level_bitmap_ & static_cast<uint32_t>(1 << (int32_t)level));
        bq::tuple<const char*, uint32_t> stack_info = should_print_stack ? get_stack_trace<STR>() : bq::make_tuple((const char*)nullptr, (uint32_t)0);
        size_t format_size = bq::tools::_serialize_str_helper_by_type<STR>::get_storage_data_size(log_format_content);
        size_t total_format_data_size = format_size + bq::get<1>(stack_info);
        auto args_size_seq = bq::tools::make_size_seq<true>(args...);
        size_t total_args_size = args_size_seq.get_total();
        bq::_api_log_write_handle handle;
        const void* format_data_ptr = should_print_stack ? nullptr : bq::tools::_serialize_str_helper_by_type<STR>::get_storage_data_addr(log_format_content);
        handle = bq::api::__api_log_write_begin(log_id_,
            static_cast<uint8_t>(level),
            category_index,
            static_cast<uint8_t>(is_bq_log_format<STR>::arg_type),
            static_cast<uint32_t>(total_format_data_size),
            format_data_ptr,
            static_cast<uint32_t>(total_args_size));
        if (handle.result != bq::enum_buffer_result_code::success) {
            return false;
        }
        if (!format_data_ptr) {
            //Ugly hack
            bq::tools::_type_copy<false>(log_format_content, handle.format_data_addr - sizeof(uint32_t), total_format_data_size + sizeof(uint32_t));
            memcpy(handle.format_data_addr + format_size, bq::get<0>(stack_info), bq::get<1>(stack_info));
            *reinterpret_cast<uint32_t*>(handle.format_data_addr - sizeof(uint32_t)) = static_cast<uint32_t>(total_format_data_size);
        }
        uint8_t* log_args_addr = handle.format_data_addr + bq::align_4(total_format_data_size);
        bq::impl::_do_log_args_fill(log_args_addr, args_size_seq, args...);
        bq::api::__api_log_write_finish(log_id_, handle);
        return true;
    }

    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::verbose(const STR& log_content) const
    {
        return do_log(0, log_level::verbose, log_content);
    }
    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::debug(const STR& log_content) const
    {
        return do_log(0, log_level::debug, log_content);
    }
    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::info(const STR& log_content) const
    {
        return do_log(0, log_level::info, log_content);
    }
    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::warning(const STR& log_content) const
    {
        return do_log(0, log_level::warning, log_content);
    }
    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::error(const STR& log_content) const
    {
        return do_log(0, log_level::error, log_content);
    }
    template <typename STR>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::fatal(const STR& log_content) const
    {
        return do_log(0, log_level::fatal, log_content);
    }

    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::verbose(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::verbose, log_content, args...);
    }
    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::debug(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::debug, log_content, args...);
    }
    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::info(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::info, log_content, args...);
    }
    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::warning(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::warning, log_content, args...);
    }
    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::error(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::error, log_content, args...);
    }
    template <typename STR, typename... Args>
    inline bq::enable_if_t<log::is_bq_log_format<STR>::value, bool> log::fatal(const STR& log_content, const Args&... args) const
    {
        return do_log(0, log_level::fatal, log_content, args...);
    }

    inline category_log::category_log()
        : log()
    {
    }

    inline category_log::category_log(const log& child_inst)
        : log(child_inst)
    {
    }

    inline size_t category_log::get_categories_count() const
    {
        return categories_name_array_.size();
    }

    inline const bq::array<bq::string>& category_log::get_categories_name_array() const
    {
        return categories_name_array_;
    }

    namespace tools {
        inline log_decoder::log_decoder(const bq::string& log_file_path, const bq::string& priv_key)
        {
            result_ = bq::api::__api_log_decoder_create(log_file_path.c_str(), priv_key.c_str(), &handle_);
            if (result_ != appender_decode_result::success) {
                handle_ = 0xFFFFFFFF;
            }
        }

        inline log_decoder::~log_decoder()
        {
            bq::api::__api_log_decoder_destroy(handle_);
        }

        inline bq::appender_decode_result log_decoder::decode()
        {
            if (result_ != bq::appender_decode_result::success) {
                return result_;
            }
            bq::_api_string_def text;
            decode_text_.clear();
            result_ = bq::api::__api_log_decoder_decode(handle_, &text);
            if (result_ == bq::appender_decode_result::success) {
                decode_text_.insert_batch(decode_text_.begin(), text.str, (size_t)text.len);
            }
            return result_;
        }

        inline bq::appender_decode_result log_decoder::get_last_decode_result() const
        {
            return result_;
        }

        inline const bq::string& log_decoder::get_last_decoded_log_entry() const
        {
            return decode_text_;
        }

        inline bool log_decoder::decode_file(const bq::string& log_file_path, const bq::string& output_file, const bq::string& priv_key)
        {
            return bq::api::__api_log_decode(log_file_path.c_str(), output_file.c_str(), priv_key.c_str());
        }
    }

}
