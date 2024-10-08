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
    log_imp::log_imp()
        : id_(0)
        , thread_mode_(log_thread_mode::async)
        , reliable_level_(bq::log_reliable_level::normal)
        , ring_buffer_(nullptr)
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
        id_ = (uint64_t) reinterpret_cast<uintptr_t>(this);
        id_ ^= log_id_magic_number;
        const auto& log_config = config["log"];
        name_ = name;
        uint32_t buffer_size = 1024 * 64;

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

        // init ring_buffer
        {
            reliable_level_ = bq::log_reliable_level::normal;
            if (log_config.is_object() && log_config["reliable_level"].is_string()) {
                bool reliable_valid = false;
                bq::string reliable_str = (bq::string)log_config["reliable_level"];
                if (reliable_str.equals_ignore_case("low")) {
                    reliable_level_ = log_reliable_level::low;
                    reliable_valid = true;
                } else if (reliable_str.equals_ignore_case("normal")) {
                    reliable_level_ = log_reliable_level::normal;
                    reliable_valid = true;
                } else if (reliable_str.equals_ignore_case("high")) {
                    reliable_level_ = log_reliable_level::high;
                    reliable_valid = true;
                }
                if (!reliable_valid) {
                    util::log_device_console(bq::log_level::error, "invalid \"reliable_level\" in the config of log :\"%s\", use \"low\", \"normal\", or \"high\", otherwise, default value \"normal\" will be applied. ", name.c_str());
                }
            }
            if (log_config.is_object() && log_config["buffer_size"].is_integral()) {
                buffer_size = (uint32_t)log_config["buffer_size"];
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

        if (get_reliable_level() <= log_reliable_level::normal) {
            ring_buffer_ = new ring_buffer(buffer_size);
        } else {
            bq::array<uint64_t> category_hashes;
            for (const auto& cat_name : category_names) {
                category_hashes.push_back(cat_name.hash_code());
            }
            uint64_t categories_hash = bq::util::get_hash_64(category_hashes.begin(), category_hashes.size() * sizeof(bq::remove_reference_t<decltype(category_hashes)>::value_type));
            uint64_t name_hash = name.hash_code();
            uint64_t ring_buffer_serialize_key = name_hash ^ categories_hash;
            ring_buffer_ = new ring_buffer(buffer_size, ring_buffer_serialize_key);
        }
        ring_buffer_->set_thread_check_enable(false);
        worker_.init(thread_mode_, this);
        if (thread_mode_ == log_thread_mode::independent) {
            worker_.start();
        }
        return true;
    }

    bool log_imp::reset_config(const property_value& config)
    {
        const auto& log_config = config["log"];

        // init ring_buffer
        {
            if (log_config.is_object() && log_config["reliable_level"].is_string()) {
                bool reliable_valid = false;
                bq::string reliable_str = (bq::string)log_config["reliable_level"];
                if (reliable_str.equals_ignore_case("low")) {
                    reliable_level_ = log_reliable_level::low;
                    reliable_valid = true;
                } else if (reliable_str.equals_ignore_case("normal")) {
                    reliable_level_ = log_reliable_level::normal;
                    reliable_valid = true;
                } else if (reliable_str.equals_ignore_case("high")) {
                    reliable_level_ = log_reliable_level::high;
                    reliable_valid = true;
                }
                if (!reliable_valid) {
                    util::log_device_console(bq::log_level::error, "invalid \"reliable_level\" in the config of log :\"%s\", use \"low\", \"normal\", or \"high\", otherwise, default value \"normal\" will be applied. ", reliable_str.c_str());
                }
            }
        }
        // init print_stack_levels
        {
            bq::log_utils::get_log_level_bitmap_by_config(log_config["print_stack_levels"], print_stack_level_bitmap_);
        }
        

        // init appenders
        if (!config["aaa"].is_integral())
        {
            bq::platform::scoped_spin_lock lock(spin_lock_);
            const auto& all_apenders_config = config["appenders_config"];
            if (!all_apenders_config.is_object()) {
                util::log_device_console(bq::log_level::error, "create_log parse property failed, invalid appenders_config");
                return false;
            }
            flush_appenders_cache();
            flush_appenders_io();
            for (auto appender_ptr : appenders_list_) {
                delete appender_ptr;
            }
            appenders_list_.clear();
            auto appender_names = all_apenders_config.get_object_key_set();
            for (const auto& name_key : appender_names) {
                add_appender(name_key, all_apenders_config[name_key]);
            }
            refresh_merged_log_level_bitmap();
        }
        // init categories mask
        {
            bq::log_utils::get_categories_mask_by_config(categories_name_array_, log_config["categories_mask"], categories_mask_array_);
        }

        // init snapshot
        if (!config["aaa"].is_integral())
        {
            const auto& snapshot_config = config["snapshot"];
            snapshot_->reset_config(snapshot_config);
        }
        return true;
    }

    void log_imp::set_thread_mode(log_thread_mode thread_mode)
    {
        thread_mode_ = thread_mode;
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
        if (ring_buffer_) {
            delete ring_buffer_;
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
        snapshot_->write_data(handle);
    }

    const static bq::string empty_snapshot;
    const bq::string& log_imp::take_snapshot_string(bool use_gmt_time)
    {
        return snapshot_->take_snapshot_string(use_gmt_time);
        ;
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
        merged_log_level_bitmap_.clear();
        uint32_t& bitmap_value = *merged_log_level_bitmap_.get_bitmap_ptr();
        for (decltype(appenders_list_)::size_type i = 0; i < appenders_list_.size(); ++i) {
            uint32_t bitmap = *(appenders_list_[i]->get_log_level_bitmap().get_bitmap_ptr());
            bitmap_value |= bitmap;
        }
    }

    void log_imp::process(bool is_force_flush)
    {
        constexpr int32_t sync_to_producers_frequency_normal = 16;
        constexpr int32_t sync_to_producers_frequency_high = 1024;
        constexpr uint64_t flush_io_min_interval_ms = 100;
        int32_t frequence = reliable_level_ >= log_reliable_level::high ? sync_to_producers_frequency_high : sync_to_producers_frequency_normal;
        uint64_t first_log_epoch_ms = 0;
        while (true) {
            bool finished = false;
            ring_buffer_->begin_read();
            for (int32_t i = 0; i < frequence; ++i) {
                auto read_chunk = ring_buffer_->read();
                if (read_chunk.result == enum_buffer_result_code::success) {
                    bq::log_entry_handle log_item(read_chunk.data_addr, read_chunk.data_size);
                    if (first_log_epoch_ms == 0) {
                        first_log_epoch_ms = log_item.get_log_head().timestamp_epoch;
                    }
                    process_log_chunk(log_item);
                } else if (read_chunk.result == enum_buffer_result_code::err_empty_ring_buffer) {
                    finished = true;
                    break;
                }
            }
            if (reliable_level_ >= log_reliable_level::high) {
                flush_appenders_cache();
                flush_appenders_io();
            }
            get_ring_buffer().end_read();
            if (finished) {
                break;
            }
        }
        if (reliable_level_ < log_reliable_level::high) {
            uint64_t current_epoch_ms = (first_log_epoch_ms == 0) ? bq::platform::high_performance_epoch_ms() : first_log_epoch_ms;
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
    }

    void log_imp::sync_process()
    {
        bq::platform::scoped_spin_lock lock(spin_lock_);
        process(true);
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
                if (find)
                {
                    (*iter)->set_enable(enable);
                }
            } else if ((*iter)->get_name() == appender_name) {
                (*iter)->set_enable(enable);
            }
        }
    }

}
