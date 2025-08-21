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
#include "bq_log/log/log_imp.h"
#include "bq_log/log/log_snapshot.h"
#include "bq_log/log/log_types.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/appender/appender_file_raw.h"
#include "bq_log/log/appender/appender_file_text.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "bq_log/utils/log_utils.h"

namespace bq {

    class sync_buffer {
    private:
        bq::array<uint8_t, bq::aligned_allocator<uint8_t, 8>> buffer_;
        size_t default_buffer_size_;

    public:
        sync_buffer()
            : default_buffer_size_(16 * 1024)
        {
        }
        ~sync_buffer()
        {
        }
        void set_default_buffer_size(size_t default_buffer_size)
        {
            default_buffer_size_ = default_buffer_size;
        }
        bq_forceinline uint8_t* alloc_data(uint32_t size)
        {
            buffer_.fill_uninitialized(static_cast<size_t>(size));
            return buffer_.begin();
        }
        const bq_forceinline uint8_t* get_aligned_data() const
        {
            return buffer_.begin();
        }
        bq_forceinline void recycle_data()
        {
            if (buffer_.capacity() > default_buffer_size_) {
                buffer_.clear();
                buffer_.fill_uninitialized(default_buffer_size_);
                buffer_.shrink();
                buffer_.clear();
            } else {
                buffer_.clear();
            }
        }
        bq_forceinline bool is_empty() const
        {
            return buffer_.is_empty();
        }
        bq_forceinline uint32_t get_used_data_size() const
        {
            return static_cast<uint32_t>(buffer_.size());
        }
    };
    BQ_TLS_NON_POD(sync_buffer, sync_buffer_);

    log_imp::log_imp()
        : id_(0)
        , thread_mode_(log_thread_mode::async)
        , buffer_(nullptr)
        , snapshot_(nullptr)
        , last_log_entry_epoch_ms_(0)
        , last_flush_io_epoch_ms_(0)
    {
    }

    log_imp::~log_imp()
    {
        clear();
    }

    bool log_imp::init(const bq::string& name, const property_value& config, const bq::array<bq::string>& category_names)
    {
        id_ = (uint64_t)reinterpret_cast<uintptr_t>(this);
        id_ ^= log_id_magic_number;
        const auto& log_config = config["log"];
        name_ = name;

        bq::platform::scoped_spin_lock lock(spin_lock_);

        // init thread_mode
        const auto& thread_mode_config = log_config["thread_mode"];
        if (thread_mode_config.is_string()) {
            if ("sync" == (bq::string)thread_mode_config) {
                thread_mode_ = log_thread_mode::sync;
            } else if ("async" == (bq::string)thread_mode_config) {
                thread_mode_ = log_thread_mode::async;
            } else if ("independent" == (bq::string)thread_mode_config) {
                thread_mode_ = log_thread_mode::independent;
            } else {
                util::log_device_console(bq::log_level::warning, "unrecognized thread mode:%s, use async instead.", ((bq::string)thread_mode_config).c_str());
            }
        }

        categories_name_array_ = category_names;
        // init categories mask
        {
            categories_mask_array_.fill_uninitialized(categories_name_array_.size());
            bq::log_utils::get_categories_mask_by_config(categories_name_array_, log_config["categories_mask"], categories_mask_array_);
        }

        // init print_stack_levels
        {
            bq::log_utils::get_log_level_bitmap_by_config(log_config["print_stack_levels"], print_stack_level_bitmap_);
        }

        // init appenders
        {
            const auto& all_apenders_config = config["appenders_config"];
            if (!all_apenders_config.is_object()) {
                util::log_device_console(bq::log_level::error, "create_log parse property failed, invalid appenders_config");
                return false;
            }

            auto appender_names = all_apenders_config.get_object_key_set();
            for (const auto& name_key : appender_names) {
                add_appender(name_key, all_apenders_config[name_key]);
            }
            refresh_merged_log_level_bitmap();
        }

        // init snapshot
        {
            const auto& snapshot_config = config["snapshot"];
            snapshot_ = new log_snapshot(this, snapshot_config);
        }

        {
            log_buffer_config buffer_config;
            buffer_config.log_name = name_;
            buffer_config.log_categories_name = categories_name_array_;
            if (log_config.is_object()) {
                if (log_config["buffer_size"].is_integral()) {
                    buffer_config.default_buffer_size = (uint32_t)log_config["buffer_size"];
                }
                if (log_config["recovery"].is_bool()) {
                    buffer_config.need_recovery = (bool)log_config["recovery"];
                }
                if (log_config["buffer_policy_when_full"].is_string()) {
                    if (((bq::string)log_config["buffer_policy"]).equals_ignore_case("discard")) {
                        buffer_config.policy = log_memory_policy::discard_when_full;
                    } else if (((bq::string)log_config["buffer_policy"]).equals_ignore_case("block")) {
                        buffer_config.policy = log_memory_policy::block_when_full;
                    } else if (((bq::string)log_config["buffer_policy"]).equals_ignore_case("expand")) {
                        buffer_config.policy = log_memory_policy::auto_expand_when_full;
                    }
                }
                if (log_config["high_perform_mode_freq_threshold_per_second"].is_integral()) {
                    buffer_config.high_frequency_threshold_per_second = (uint64_t)(int64_t)log_config["high_perform_mode_freq_threshold_per_second"];
                    if (0 == buffer_config.high_frequency_threshold_per_second) {
                        buffer_config.high_frequency_threshold_per_second = UINT64_MAX;
                    }
                }
            }
            buffer_ = bq::util::aligned_new<bq::log_buffer>(CACHE_LINE_SIZE, buffer_config);
        }
        worker_.init(thread_mode_, this);
        if (thread_mode_ == log_thread_mode::independent) {
            worker_.start();
        }
        return true;
    }

    bool log_imp::reset_config(const property_value& config)
    {
        const auto& log_config = config["log"];

        // init print_stack_levels
        {
            bq::log_utils::get_log_level_bitmap_by_config(log_config["print_stack_levels"], print_stack_level_bitmap_);
        }

        // init or reset appenders
        {
            bq::platform::scoped_spin_lock lock(spin_lock_);
            const auto& all_apenders_config = config["appenders_config"];
            if (!all_apenders_config.is_object()) {
                util::log_device_console(bq::log_level::error, "create_log parse property failed, invalid appenders_config");
                return false;
            }
            flush_appenders_cache();
            flush_appenders_io();
            auto appender_names = all_apenders_config.get_object_key_set();
            decltype(appenders_list_) tmp_list = bq::move(appenders_list_);
            assert(appenders_list_.size() == 0);
            for (const auto& name_key : appender_names) {
                auto old_iter = tmp_list.find_if([&name_key](const appender_base* appender) {
                    return appender->get_name() == name_key;
                    });
                if (old_iter != tmp_list.end()) {
                    if ((*old_iter)->reset(all_apenders_config[name_key])) {
                        appenders_list_.push_back(*old_iter);
                        tmp_list.erase(old_iter);
                        continue;
                    }
                }
                add_appender(name_key, all_apenders_config[name_key]);
            }
            for (auto appender_delete : tmp_list) {
                delete appender_delete;
            }
            refresh_merged_log_level_bitmap();
        }
        // init categories mask
        {
            bq::log_utils::get_categories_mask_by_config(categories_name_array_, log_config["categories_mask"], categories_mask_array_);
        }

        // init snapshot
        {
            const auto& snapshot_config = config["snapshot"];
            snapshot_->reset_config(snapshot_config);
        }
        return true;
    }

    void log_imp::set_config(const bq::string& config)
    {
        last_config_ = config;
    }

    bq::string& log_imp::get_config()
    {
        return last_config_;
    }

    bool log_imp::add_appender(const string& name, const bq::property_value& jobj)
    {
        appender_base* appender_ptr = nullptr;
        const auto& type_obj = jobj["type"];
        if (!type_obj.is_string()) {
            util::log_device_console(bq::log_level::error, "add_appender failed, appender \"%s\" has invalid \"type\" config", name.c_str());
            return false;
        }
        bq::string type_str = ((string)type_obj).trim();
        for (int32_t i = 0; i < (int32_t)appender_base::appender_type::type_count; ++i) {
            if (type_str.equals_ignore_case(appender_base::get_config_name_by_type((appender_base::appender_type)i))) {

                break;
            }
        }
        if (type_str.equals_ignore_case(appender_base::get_config_name_by_type(appender_base::appender_type::console))) {
            appender_ptr = new appender_console();
        } else if (type_str.equals_ignore_case(appender_base::get_config_name_by_type(appender_base::appender_type::text_file))) {
            appender_ptr = new appender_file_text();
        } else if (type_str.equals_ignore_case(appender_base::get_config_name_by_type(appender_base::appender_type::raw_file))) {
            appender_ptr = new appender_file_raw();
        } else if (type_str.equals_ignore_case(appender_base::get_config_name_by_type(appender_base::appender_type::compressed_file))) {
            appender_ptr = new appender_file_compressed();
        } else {
            util::log_device_console(bq::log_level::error, "bq log error: invalid Appender type : %s", type_str.c_str());
            return false;
        }
        bq::scoped_obj<appender_base> appender(appender_ptr);
        if (!appender->init(name, jobj, this)) {
            return false;
        }
        appenders_list_.push_back(appender.transfer());
        return true;
    }

    void log_imp::clear()
    {
        for (auto appender_ptr : appenders_list_) {
            delete appender_ptr;
        }
        appenders_list_.clear();
        categories_name_array_.clear();
        categories_name_array_.clear();
        name_.clear();
        merged_log_level_bitmap_.clear();
        if (buffer_) {
            bq::util::aligned_delete(buffer_);
        }
        delete snapshot_;
        snapshot_ = nullptr;
        id_ = 0;
    }

    void log_imp::process_log_chunk(bq::log_entry_handle& read_handle)
    {
        auto& head = read_handle.get_log_head();

        // Due to the high concurrency of our ring_buffer,
        // we cannot guarantee that the order of log entries matches the sequence of system time retrieval for each entry.
        // To avoid timestamp regression in such scenarios, we've implemented a minor safeguard.
        if (head.timestamp_epoch > last_log_entry_epoch_ms_) {
            last_log_entry_epoch_ms_ = head.timestamp_epoch;
        } else {
            head.timestamp_epoch = last_log_entry_epoch_ms_;
        }
        log(read_handle);
    }

    void log_imp::log(const log_entry_handle& handle)
    {
        auto category_idx = handle.get_log_head().category_idx;
        if (categories_mask_array_.size() <= category_idx || categories_mask_array_[category_idx] == 0) {
            return;
        }
        for (decltype(appenders_list_)::size_type i = 0; i < appenders_list_.size(); i++) {
            appenders_list_[i]->log(handle);
        }
        if (snapshot_->is_enable()) {
            snapshot_->write_data(handle);
        }
    }

    const bq::string& log_imp::take_snapshot_string(bool use_gmt_time)
    {
        return snapshot_->take_snapshot_string(use_gmt_time);
    }

    void log_imp::release_snapshot_string()
    {
        snapshot_->release_snapshot_string();
    }

    const bq::string& log_imp::get_name() const
    {
        return name_;
    }

    void log_imp::refresh_merged_log_level_bitmap()
    {
        log_level_bitmap tmp;
        uint32_t& bitmap_value = *tmp.get_bitmap_ptr();
        for (decltype(appenders_list_)::size_type i = 0; i < appenders_list_.size(); ++i) {
            uint32_t bitmap = *(appenders_list_[i]->get_log_level_bitmap().get_bitmap_ptr());
            bitmap_value |= bitmap;
        }
        // make sure atomic
        merged_log_level_bitmap_ = tmp;
    }

    void log_imp::process(bool is_force_flush)
    {
        constexpr uint64_t flush_io_min_interval_ms = 100;
        uint64_t current_epoch_ms = 0;
        while (true) {
            auto read_chunk = buffer_->read_chunk();
            scoped_log_buffer_handle<log_buffer> scoped_read_chunk(*buffer_, read_chunk);
            if (read_chunk.result == enum_buffer_result_code::success) {
                bq::log_entry_handle log_item(read_chunk.data_addr, read_chunk.data_size);
                current_epoch_ms = log_item.get_log_head().timestamp_epoch;
                process_log_chunk(log_item);
            } else if (read_chunk.result == enum_buffer_result_code::err_empty_log_buffer) {
                break;
            }
        }
        if (0 == current_epoch_ms) {
            current_epoch_ms = bq::platform::high_performance_epoch_ms();
        }
        if (is_force_flush) {
            flush_appenders_cache();
            last_flush_io_epoch_ms_ = current_epoch_ms;
        } else {
            if (current_epoch_ms > last_flush_io_epoch_ms_ + flush_io_min_interval_ms) {
                flush_appenders_cache();
                last_flush_io_epoch_ms_ = current_epoch_ms;
            }
        }
    }

    void log_imp::sync_process(bool is_force_flush)
    {
        bq::platform::scoped_spin_lock lock(spin_lock_);
        uint64_t current_epoch_ms = 0;
        if (!sync_buffer_.get().is_empty()) {
            bq::log_entry_handle log_item(sync_buffer_.get().get_aligned_data(), sync_buffer_.get().get_used_data_size());
            current_epoch_ms = log_item.get_log_head().timestamp_epoch;
            process_log_chunk(log_item);
            sync_buffer_.get().recycle_data();
        } else {
            current_epoch_ms = bq::platform::high_performance_epoch_ms();
        }
        constexpr uint64_t flush_io_min_interval_ms = 100;

        if (is_force_flush) {
            flush_appenders_cache();
            last_flush_io_epoch_ms_ = current_epoch_ms;
        } else {
            if (current_epoch_ms > last_flush_io_epoch_ms_ + flush_io_min_interval_ms) {
                flush_appenders_cache();
                last_flush_io_epoch_ms_ = current_epoch_ms;
            }
        }
    }

    uint8_t* log_imp::get_sync_buffer(uint32_t data_size)
    {
        return sync_buffer_.get().alloc_data(data_size);
    }

    const bq::layout& log_imp::get_layout() const
    {
        return layout_;
    }

    void log_imp::flush_appenders_cache()
    {
        for (decltype(appenders_list_)::size_type i = 0; i < appenders_list_.size(); ++i) {
            switch (appenders_list_[i]->get_type()) {
            case appender_base::appender_type::raw_file:
            case appender_base::appender_type::text_file:
            case appender_base::appender_type::compressed_file:
                static_cast<bq::appender_file_base*>(appenders_list_[i])->flush_cache();
                break;
            default:
                break;
            }
        }
    }

    void log_imp::flush_appenders_io()
    {
        for (decltype(appenders_list_)::size_type i = 0; i < appenders_list_.size(); ++i) {
            switch (appenders_list_[i]->get_type()) {
            case appender_base::appender_type::raw_file:
            case appender_base::appender_type::text_file:
            case appender_base::appender_type::compressed_file:
                static_cast<bq::appender_file_base*>(appenders_list_[i])->flush_io();
                break;
            default:
                break;
            }
        }
    }

    uint32_t log_imp::get_categories_count() const
    {
        return (uint32_t)categories_name_array_.size();
    }

    const bq::string& log_imp::get_category_name_by_index(uint32_t index) const
    {
        return categories_name_array_[index];
    }

    const bq::array<bq::string>& log_imp::get_categories_name() const
    {
        return categories_name_array_;
    }

    void log_imp::set_appenders_enable(const bq::string& appender_name, bool enable)
    {
        bq::platform::scoped_spin_lock lock(spin_lock_);
        auto star_list = appender_name.split("*");
        bool vague = (appender_name.find("*") != string::npos);
        for (auto iter = appenders_list_.begin(); iter != appenders_list_.end(); ++iter) {
            if (vague) {
                bool find = true;
                string temp_cell = (*iter)->get_name();
                for (auto& star : star_list) {
                    size_t index = temp_cell.find(star);
                    if (index == string::npos) {
                        find = false;
                        break;
                    } else {
                        size_t end_pos = index + star.size();
                        if (end_pos < temp_cell.size())
                            temp_cell = temp_cell.substr(end_pos, temp_cell.size() - end_pos);
                        else
                            temp_cell = "";
                    }
                }
                if (find) {
                    (*iter)->set_enable(enable);
                }
            } else if ((*iter)->get_name() == appender_name) {
                (*iter)->set_enable(enable);
            }
        }
    }

}
