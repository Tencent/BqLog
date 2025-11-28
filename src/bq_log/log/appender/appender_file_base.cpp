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
#include "bq_log/log/appender/appender_file_base.h"
#if defined(BQ_WIN)
#include "bq_common/platform/win64_includes_begin.h"
#endif
#include "bq_common/bq_common.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/utils/log_utils.h"

namespace bq {
    static constexpr size_t CACHE_READ_DEFAULT_SIZE = 32 * 1024;
    static constexpr size_t CACHE_WRITE_DEFAULT_SIZE = 64 * 1024;

    appender_file_base::~appender_file_base()
    {
        file_manager::instance().close_file(file_);
        clean_recovery_context();
        if (!need_recovery_) {

            if (head_) {
                bq::platform::aligned_free(head_);
            }
        }
        head_ = nullptr;
        cache_write_ = nullptr;
    }

    void appender_file_base::flush_cache()
    {
        if (!file_) {
            return;
        }
        size_t real_write_size = 0;
        size_t need_write_size = head_->cache_write_finished_cursor_;
        int32_t error_code = bq::platform::write_file(file_.platform_handle(), (const void*)cache_write_, need_write_size, real_write_size);
        if (real_write_size == need_write_size) {
            if(head_->write_cache_size_ > (CACHE_WRITE_DEFAULT_SIZE << 1)) {
                resize_write_cache(CACHE_WRITE_DEFAULT_SIZE >> 1);
            }
        } else {
            memcpy(cache_write_, cache_write_ + static_cast<ptrdiff_t>(real_write_size), need_write_size - real_write_size);
        }
        head_->cache_write_finished_cursor_ -= real_write_size;
        cache_write_cursor_ -= real_write_size;
        current_file_size_ += real_write_size;
        if (error_code != 0 && error_code !=
#if defined(BQ_WIN)
                ERROR_DISK_FULL
#else
                ENOSPC
#endif
        ) {
            char error_text[256] = { 0 };
            auto epoch = bq::platform::high_performance_epoch_ms();
            struct tm time_st;
            time_zone_.get_tm_by_epoch(epoch, time_st);
            snprintf(error_text, sizeof(error_text), "%s %d-%02d-%02d %02d:%02d:%02d appender_file_base write_file error code:%" PRId32 ", trying open new file real_write_size : %" PRIu64 ", need_write_size : %" PRIu64 "\n",
                time_zone_.get_time_zone_str().c_str(),
                time_st.tm_year + 1900, time_st.tm_mon + 1, time_st.tm_mday, time_st.tm_hour, time_st.tm_min, time_st.tm_sec,
                error_code, static_cast<uint64_t>(real_write_size), static_cast<uint64_t>(need_write_size));
            string path = TO_ABSOLUTE_PATH("bqLog/write_file_error.log", 0);
            bq::file_manager::append_all_text(path, error_text);
            bq::util::log_device_console_plain_text(log_level::warning, error_text);
            open_new_indexed_file_by_name();
        }
    }

    void appender_file_base::flush_io()
    {
        if (file_) {
            if (!file_manager::instance().flush_file(file_)) {
                int32_t err_code = file_manager::instance().get_and_clear_last_file_error();
                if (err_code != 0) {
                    char ids[32] = { 0 };
                    snprintf(ids, 32, "%d", err_code);
                    string path = TO_ABSOLUTE_PATH("bqLog/flush_file_error.log", 0);
                    bq::file_manager::write_all_text(path, ids);
                    bq::util::log_device_console(log_level::warning, "appender_file_base::flush_io error, file_path:%s, error code:%d", file_.abs_file_path().c_str(), err_code);
                }
            }
        }
    }

    bool appender_file_base::init_impl(const bq::property_value& config_obj)
    {
        set_basic_configs(config_obj);
        need_recovery_ = parent_log_->get_buffer().get_config().need_recovery;
        if (need_recovery_) {
            bool recover_result = try_recover();
            bq::file_manager::instance().close_file(file_);
            if (!recover_result) {
                clean_recovery_context();
            }
        }
        return !config_file_name_.is_empty();
    }

    bool appender_file_base::reset_impl(const bq::property_value& config_obj)
    {
        bq::string prev_config_file_name = config_file_name_;
        auto prev_base_dir_type = base_dir_type_;
        set_basic_configs(config_obj);
        return (prev_config_file_name == config_file_name_) && (prev_base_dir_type == base_dir_type_);
    }

    void appender_file_base::log_impl(const log_entry_handle& handle)
    {
        refresh_file_handle(handle);
    }

    void appender_file_base::on_file_open(bool is_new_created)
    {
        (void)is_new_created;
    }

    void appender_file_base::seek_read_file_absolute(size_t pos)
    {
        clear_read_cache();
        file_manager::instance().seek(file_, file_manager::seek_option::begin, (int32_t)pos);
    }

    void appender_file_base::seek_read_file_offset(int32_t offset)
    {
        int64_t final_cursor = (int64_t)cache_read_cursor_ + offset;
        if (final_cursor >= 0 && final_cursor <= (int64_t)cache_read_.size()) {
            cache_read_cursor_ = static_cast<size_t>(static_cast<size_t_to_int_t>(cache_read_cursor_) + offset);
        } else {
            assert(false && "not implemented");
            // TODO: The following implementation is incorrect.
            // Given that this scenario is unlikely to occur at the moment, we can wait until it arises to implement a solution.
            // This situation could be somewhat complex, as it might need additional variables to keep track of the current actual cursor position within the entire file.
            // Subsequently, we would need to add an offset to this variable to get the final absolute file position,
            // and then call `clear_read_cache()` and `file_manager::instance().seek`.

            /*clear_read_cache();
            file_manager::instance().seek(file_, file_manager::seek_option::current, offset);*/
        }
    }

    bq::appender_file_base::read_with_cache_handle appender_file_base::read_with_cache(size_t size)
    {
        auto left_size = cache_read_.size() - cache_read_cursor_;
        if (left_size < size) {
            cache_read_.erase(cache_read_.begin(), cache_read_cursor_);
            auto total_size = bq::max_value(size, CACHE_READ_DEFAULT_SIZE);
            auto fill_size = total_size - left_size;
            cache_read_.fill_uninitialized(fill_size);
            auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + static_cast<ptrdiff_t>(left_size), fill_size);
            cache_read_cursor_ = 0;
            if (read_size < fill_size) {
                cache_read_.erase(cache_read_.begin() + static_cast<ptrdiff_t>(left_size + read_size), fill_size - read_size);
            }
        }
        bq::appender_file_base::read_with_cache_handle result;
        result.data_ = cache_read_.begin() + static_cast<ptrdiff_t>(cache_read_cursor_);
        result.len_ = bq::min_value(size, cache_read_.size() - cache_read_cursor_);
        cache_read_cursor_ += result.len_;
        return result;
    }

    void appender_file_base::clear_read_cache()
    {
        cache_read_cursor_ = 0;
        cache_read_.clear();
        if (cache_read_.capacity() > CACHE_READ_DEFAULT_SIZE) {
            cache_read_.shrink();
        }
    }

    appender_file_base::write_with_cache_handle appender_file_base::alloc_write_cache(size_t size)
    {
#ifndef NDEBUG
        assert(!cache_write_already_allocated_ && "duplicate call alloc_write_cache in log file appender");
        cache_write_already_allocated_ = true;
#endif
        uint64_t need_cache_size = cache_write_cursor_ + static_cast<uint64_t>(size);
        if (need_cache_size > get_cache_total_size()) {
            uint64_t new_cache_size = bq::roundup_pow_of_two(need_cache_size);
            resize_write_cache(static_cast<size_t>(new_cache_size));
        }

        write_with_cache_handle result_handle;
        result_handle.data_ = cache_write_ + static_cast<ptrdiff_t>(cache_write_cursor_);
        result_handle.alloc_len_ = size;
        result_handle.used_len_ = size;
        return result_handle;
    }

    void appender_file_base::return_write_cache(const appender_file_base::write_with_cache_handle& handle)
    {
#ifndef NDEBUG
        assert(cache_write_already_allocated_ && "call return_write_cache without calling alloc_write_cache in log file appender");
        assert(handle.used_len_ <= handle.alloc_len_ && "used data length greater than allocated length in log file appender");
        cache_write_already_allocated_ = false;
#endif
        cache_write_cursor_ += static_cast<uint64_t>(handle.used_len_);
    }


    void appender_file_base::mark_write_finished() {
#ifndef NDEBUG
        assert(!cache_write_already_allocated_ && "mark_write_finished must be called after return_write_cache()");
#endif
        head_->cache_write_finished_cursor_ = cache_write_cursor_;
        if (cache_write_cursor_ >= CACHE_WRITE_DEFAULT_SIZE) {
            flush_cache();
        }
    }

    size_t appender_file_base::get_current_file_size()
    {
        return current_file_size_;
    }

    void appender_file_base::refresh_file_handle(const log_entry_handle& handle)
    {
        bool need_create_new_file = (!file_) || is_file_oversize();
        if (!need_create_new_file) {
            auto current_time_epoch = (handle.get_log_head()).timestamp_epoch;
            if (current_time_epoch > current_file_expire_time_epoch_ms_) {
                need_create_new_file = true;
                bq::util::log_device_console(bq::log_level::info, "need_create_new_file");
            }
        }
        if (need_create_new_file) {
            auto current_time_epoch = (handle.get_log_head()).timestamp_epoch;
            uint64_t ms_per_day = 60 * 60 * 24 * 1000;
            current_file_expire_time_epoch_ms_ = static_cast<uint64_t>(static_cast<int64_t>(current_time_epoch) + time_zone_.get_time_zone_diff_to_gmt_ms());
            // get next day ms
            uint64_t check_ms_ = (current_file_expire_time_epoch_ms_ / ms_per_day + 1) * ms_per_day;
            current_file_expire_time_epoch_ms_ = current_file_expire_time_epoch_ms_ - (current_file_expire_time_epoch_ms_ % ms_per_day) + ms_per_day;
            if (check_ms_ != current_file_expire_time_epoch_ms_) {
                bq::util::log_device_console(bq::log_level::error, "exception: set expire_time in refresh_file_handle!!!");
            }
            current_file_expire_time_epoch_ms_ = static_cast<uint64_t>(static_cast<int64_t>(current_file_expire_time_epoch_ms_) - time_zone_.get_time_zone_diff_to_gmt_ms());
            if (file_) {
                flush_cache();
                flush_io();
            }
            clear_read_cache();
            open_new_indexed_file_by_name();
        }
    }

    bool appender_file_base::open_file_with_write_exclusive(const bq::string& file_path)
    {
        string path = TO_ABSOLUTE_PATH(file_path, base_dir_type_);
        file_ = file_manager::instance().open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        if (!file_) {
            return false;
        }
        current_file_size_ = file_manager::instance().get_file_size(file_);
        return true;
    }

    void appender_file_base::resize_write_cache(size_t new_size)
    {
        uint64_t cache_write_finished_cursor_backup = head_ ? head_->cache_write_finished_cursor_ : 0;
        do {
            if (need_recovery_) {
                bool need_recalc_head_size = (!head_) || head_->file_path_size_ != file_.abs_file_path().size();
                assert((!need_recalc_head_size || !head_ || head_->cache_write_finished_cursor_ == 0) && "need_recalc_head_size but need flush data exist");
                bq::memory_map::release_memory_map(memory_map_handle_);
                if (!memory_map_file_) {
                    bq::string mmap_file_path = get_mmap_file_path();
                    memory_map_file_ = file_manager::instance().open_file(mmap_file_path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
                    if (!memory_map_file_) {
                        bq::util::log_device_console(bq::log_level::error, "%s failed to open for resize write cache!", mmap_file_path.c_str());
                        need_recovery_ = false; 
                        break;
                    }
                }
                size_t head_size = static_cast<size_t>(calculate_real_mmap_head_size(file_.abs_file_path().size()));
                size_t mmap_size = new_size + head_size;
                auto new_file_size = bq::memory_map::get_min_size_of_memory_map_file(0, mmap_size);
                if (!bq::file_manager::instance().truncate_file(memory_map_file_, new_file_size)) {
                    bq::util::log_device_console(bq::log_level::error, "%s failed to truncate for resize write cache!, new size:%" PRIu64 ", error code:%" PRId32
                        , memory_map_file_.abs_file_path().c_str(), static_cast<uint64_t>(new_file_size), bq::file_manager::get_and_clear_last_file_error());
                    need_recovery_ = false;
                    break;
                }
                memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, new_file_size);
                if (!memory_map_handle_.has_been_mapped()) {
                    bq::util::log_device_console(bq::log_level::error, "%s failed to map for resize write cache!", memory_map_file_.abs_file_path().c_str());
                    need_recovery_ = false;
                    break;
                }
                head_ = static_cast<mmap_head*>(memory_map_handle_.get_mapped_data());
                head_->file_path_size_ = static_cast<uint64_t>(file_.abs_file_path().size());
                new_size = memory_map_handle_.get_mapped_size() - head_size;
                memcpy(head_->file_path_, file_.abs_file_path().c_str(), file_.abs_file_path().size());
            }
        }while(false);

        if (!need_recovery_) {
            bq::file_manager::instance().close_file(memory_map_file_);
            void* new_buffer = bq::platform::aligned_alloc(BQ_CACHE_LINE_SIZE, new_size + static_cast<size_t>(calculate_real_mmap_head_size(0)));
            if (head_) {
                memcpy(new_buffer, head_, static_cast<size_t>(calculate_real_mmap_head_size(0) + head_->write_cache_size_));
                bq::platform::aligned_free(head_);
            }
            head_ = static_cast<mmap_head*>(new_buffer);
            head_->file_path_size_ = 0;
        }

        head_->cache_write_finished_cursor_ = cache_write_finished_cursor_backup;
        head_->write_cache_size_ = static_cast<uint64_t>(new_size);
        cache_write_ = reinterpret_cast<uint8_t*>(head_) + calculate_real_mmap_head_size(file_.abs_file_path().size());
    }


    bq::string appender_file_base::get_mmap_file_path() const
    {
        return TO_ABSOLUTE_PATH("bqlog_mmap/mmap_" + parent_log_->get_name() + "/appenders/" + get_name() + ".mmap", 0);
    }

    uint64_t appender_file_base::calculate_real_mmap_head_size(size_t file_path_size) const
    {
        if (!need_recovery_) {
            return static_cast<uint64_t>(sizeof(mmap_head));
        }
        mmap_head tmp_head;
        return static_cast<uint64_t>(reinterpret_cast<const char*>(&tmp_head.file_path_) - reinterpret_cast<const char*>(&tmp_head))
                + static_cast<uint64_t>(bq::align_8(file_path_size));
    }

    void appender_file_base::clean_recovery_context() {
        if (memory_map_file_) {
            bq::string abs_mmap_file_path = memory_map_file_.abs_file_path();
            bq::file_manager::instance().close_file(memory_map_file_);
            bq::memory_map::release_memory_map(memory_map_handle_);
            bq::file_manager::remove_file_or_dir(abs_mmap_file_path);
        }
        head_ = nullptr;
        cache_write_ = nullptr;
        cache_write_cursor_ = 0;
    }

    void appender_file_base::set_basic_configs(const bq::property_value& config_obj)
    {
        config_file_name_.clear();
        if (!config_obj["file_name"].is_string()) {
            util::log_device_console(bq::log_level::error, "init appender \"%s\" failed, can not find valid \"file_name\" token", ((string)config_obj["name"]).c_str());
            return;
        } else {
            config_file_name_ = ((string)config_obj["file_name"]).trim();
        }

        if (config_obj["base_dir_type"].is_integral()) {
            base_dir_type_ = static_cast<int32_t>((bq::property_value::integral_type)config_obj["base_dir_type"]);
        } else {
            base_dir_type_ = 0;
        }

        if (config_obj["always_create_new_file"].is_bool()) {
            always_create_new_file_ = (bool)config_obj["always_create_new_file"];
        } else {
            always_create_new_file_ = false;
        }

        if (config_obj["max_file_size"].is_integral()) {
            max_file_size_ = (size_t)(int64_t)config_obj["max_file_size"];
        } else {
            max_file_size_ = 0;
        }

        if (config_obj["expire_time_seconds"].is_integral()) {
            expire_time_ms_ = (uint64_t)config_obj["expire_time_seconds"] * 1000;
        } else if (config_obj["expire_time_days"].is_integral()) {
            expire_time_ms_ = (uint64_t)config_obj["expire_time_days"] * 3600 * 24 * 1000;
        } else {
            expire_time_ms_ = 0;
        }

        if (config_obj["capacity_limit"].is_integral()) {
            capacity_limit_ = (uint64_t)config_obj["capacity_limit"];
        } else {
            capacity_limit_ = 0;
        }
    }

    bool appender_file_base::try_recover() {
        bq::string mmap_file_path = get_mmap_file_path();
        memory_map_file_ = file_manager::instance().open_file(mmap_file_path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write);
        if (!memory_map_file_) {
            bq::util::log_device_console(bq::log_level::warning, "%s failed to open, give up recovery!", mmap_file_path.c_str());
            return false;
        }
        memory_map_handle_ = bq::memory_map::create_memory_map(memory_map_file_, 0, bq::file_manager::instance().get_file_size(memory_map_file_));
        if (!memory_map_handle_.has_been_mapped()) {
            bq::util::log_device_console(bq::log_level::warning, "%s failed to map, give up recovery!", mmap_file_path.c_str());
            return false;
        }
        head_ = static_cast<mmap_head*>(memory_map_handle_.get_mapped_data());
        if (memory_map_handle_.get_mapped_size() <= (static_cast<size_t>(reinterpret_cast<const char*>(head_->file_path_) - reinterpret_cast<const char*>(head_)))) {
            bq::util::log_device_console(bq::log_level::warning, "%s too small, give up recovery!", mmap_file_path.c_str());
            return false;
        }
        cache_write_ = reinterpret_cast<uint8_t*>(head_) + calculate_real_mmap_head_size(head_->file_path_size_);
        uint64_t max_cache_write_size = static_cast<uint64_t>(memory_map_handle_.get_mapped_size()) - calculate_real_mmap_head_size(head_->file_path_size_);
        if (head_->write_cache_size_ > max_cache_write_size 
            || head_->cache_write_finished_cursor_ > max_cache_write_size
            || head_->cache_write_finished_cursor_ > head_->write_cache_size_) {
            bq::util::log_device_console(bq::log_level::warning, "%s invalid mmap head data, give up recovery! max size:%" PRIu64 ", recovered size:%" PRIu64 ", recovered cursor:%" PRIu64, mmap_file_path.c_str(), max_cache_write_size, head_->write_cache_size_, head_->cache_write_finished_cursor_);
            return false;
        }
        bq::string current_file_path;
        current_file_path.insert_batch(current_file_path.end(), head_->file_path_, static_cast<size_t>(head_->file_path_size_));
        file_ = file_manager::instance().open_file(current_file_path, file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        if (!file_) {
            bq::util::log_device_console(bq::log_level::warning, "%s failed to open log file %s, give up recovery!", mmap_file_path.c_str(), current_file_path.c_str());
            return false;
        }
        cache_write_cursor_ = head_->cache_write_finished_cursor_;
        flush_cache(); 
        return true;
    }

    void appender_file_base::open_new_indexed_file_by_name()
    {
        bool is_prev_file_exist = file_;
        file_manager::instance().close_file(file_);
        clean_recovery_context();
        clear_all_expired_files();
        clear_all_limit_files();

        auto dir_name = bq::file_manager::get_directory_from_path(config_file_name_);
        auto file_name = bq::file_manager::get_file_name_from_path(config_file_name_);

        char time_str_buf[128];
        auto epoch = bq::platform::high_performance_epoch_ms();
        struct tm time_st;
        time_zone_.get_tm_by_epoch(epoch, time_st);
        strftime(time_str_buf, sizeof(time_str_buf), "_%Y%m%d_", &time_st);

        int32_t max_index = 0;

        bq::string file_prefix = file_name + time_str_buf;
        const bq::string ext_name_with_dot = get_file_ext_name();

        string path = TO_ABSOLUTE_PATH(dir_name, base_dir_type_);
        bq::array<bq::string> exist_files = bq::file_manager::get_sub_dirs_and_files_name(path);
        for (const bq::string& name : exist_files) {
            bq::string full_name_in_absolute_path = bq::file_manager::combine_path(path, name);
            if (!bq::file_manager::is_file(full_name_in_absolute_path)) {
                continue;
            }
            if (name.begin_with(file_prefix) && name.end_with(ext_name_with_dot)) {
                bq::string idx_str = name.substr(file_prefix.size());
                idx_str = idx_str.substr(0, idx_str.size() - ext_name_with_dot.size());
                int32_t idx = atoi(idx_str.c_str());
                if (idx != 0 && idx > max_index) {
                    max_index = idx;
                }
            }
        }
        if (is_prev_file_exist || max_index == 0 || always_create_new_file_) {
            // open a new file
            max_index++;
        } else {
            // can we reuse prev file
        }

        string full_path = TO_ABSOLUTE_PATH(dir_name, base_dir_type_);
        bq::file_manager::create_directory(full_path);
        bool need_open_new_file = true;
        while (need_open_new_file) {
            char idx_buff[32];
            snprintf(idx_buff, sizeof(idx_buff), "%d", max_index++);
            bq::string file_relative_path = config_file_name_ + time_str_buf + idx_buff + ext_name_with_dot;
            bq::string absolute_file_path = TO_ABSOLUTE_PATH(file_relative_path, base_dir_type_);
            parse_file_context parse_context(absolute_file_path);

            need_open_new_file = !open_file_with_write_exclusive(absolute_file_path) || is_file_oversize();
            if (!need_open_new_file) {
                need_open_new_file |= (current_file_size_ > 0 && !parse_exist_log_file(parse_context));
            }
        }
#ifdef BQ_UNIT_TEST
        printf("open log appender file : %s\n", file_.abs_file_path().c_str());
#endif
        file_manager::instance().seek(file_, file_manager::seek_option::end, 0);
        on_file_open(current_file_size_ == 0);
    }

    bool appender_file_base::is_file_oversize()
    {
        if (max_file_size_ > 0) {
            if (current_file_size_ >= max_file_size_) {
                return true;
            }
        }
        return false;
    }

    void appender_file_base::clear_all_expired_files()
    {
        if (expire_time_ms_ == 0) {
            return;
        }
        auto dir_name = bq::file_manager::get_directory_from_path(config_file_name_);
        auto file_name = bq::file_manager::get_file_name_from_path(config_file_name_);
        bq::string file_prefix = file_name + "_";

        bq::string path = TO_ABSOLUTE_PATH(dir_name, base_dir_type_);
        bq::array<bq::string> exist_files = bq::file_manager::get_sub_dirs_and_files_name(path);
        uint64_t current_epoch_ms = bq::platform::high_performance_epoch_ms();
        for (const bq::string& name : exist_files) {
            bq::string full_name_in_absolute_path = bq::file_manager::combine_path(path, name);
            if (!bq::file_manager::is_file(full_name_in_absolute_path)) {
                continue;
            }
            if (!name.begin_with(file_prefix) || !name.end_with(get_file_ext_name())) {
                continue;
            }
            uint64_t last_m_time_ms = bq::file_manager::get_file_last_modified_epoch_ms(full_name_in_absolute_path);
            if (last_m_time_ms != 0 && last_m_time_ms + expire_time_ms_ <= current_epoch_ms) {
                bq::file_manager::instance().remove_file_or_dir(full_name_in_absolute_path);
            }
        }
    }

    void appender_file_base::clear_all_limit_files()
    {
        if (capacity_limit_ == 0) {
            return;
        }
        auto dir_name = bq::file_manager::get_directory_from_path(config_file_name_);
        auto file_name = bq::file_manager::get_file_name_from_path(config_file_name_);
        bq::string file_prefix = file_name + "_";
        bq::string path = TO_ABSOLUTE_PATH(dir_name, base_dir_type_);
        bq::array<bq::string> exist_files = bq::file_manager::get_sub_dirs_and_files_name(path);

        bq::array<bq::tuple<uint64_t, bq::string, uint64_t>> sort_list;
        uint64_t file_sum_size = 0;
        for (const bq::string& name : exist_files) {
            bq::string full_name_in_absolute_path = bq::file_manager::combine_path(path, name);
            if (!bq::file_manager::is_file(full_name_in_absolute_path)) {
                continue;
            }
            if (!name.begin_with(file_prefix) || !name.end_with(get_file_ext_name())) {
                continue;
            }
            uint64_t last_m_time_ms = bq::file_manager::get_file_last_modified_epoch_ms(full_name_in_absolute_path);
            size_t file_size = 0;
            int32_t file_size_result = bq::platform::get_file_size(full_name_in_absolute_path.c_str(), file_size);
            if (file_size_result != 0) {
                bq::util::log_device_console(log_level::error, "failed to get size of file:%s, error code:%d", full_name_in_absolute_path.c_str(), file_size_result);
                continue;
            }
            file_sum_size += static_cast<uint64_t>(file_size);
            sort_list.push_back(bq::make_tuple(last_m_time_ms, full_name_in_absolute_path, (uint64_t)file_size));
        }
        if (file_sum_size >= capacity_limit_) {
            if (sort_list.size() > 0) {
                qsort(&sort_list[0], sort_list.size(), sizeof(typename decltype(sort_list)::value_type), [](void const* v1, void const* v2) {
                    typename decltype(sort_list)::value_type const* value1 = (typename decltype(sort_list)::value_type const*)v1;
                    typename decltype(sort_list)::value_type const* value2 = (typename decltype(sort_list)::value_type const*)v2;
                    if (bq::get<0>(*value1) < bq::get<0>(*value2)) {
                        return -1;
                    } else if (bq::get<0>(*value1) == bq::get<0>(*value2)) {
                        return 0;
                    }
                    return 1;
                });
            }
            for (auto& cell : sort_list) {
                file_manager::instance().remove_file_or_dir(bq::get<1>(cell));
                file_sum_size -= bq::get<2>(cell);
                if (file_sum_size < capacity_limit_) {
                    break;
                }
            }
        }
    }

    void appender_file_base::parse_file_context::log_parse_fail_reason(const char* msg) const
    {
        bq::util::log_device_console(log_level::error, "failed to parse log file :\"%s\" at offset %" PRIu64 ", msg: %s", file_name_.c_str(), static_cast<uint64_t>(parsed_size), msg);
    }

}

#if defined(BQ_WIN)
#include "bq_common/platform/win64_includes_end.h"
#endif