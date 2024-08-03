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
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "bq_common/bq_common.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/utils/log_utils.h"

namespace bq {
    static constexpr size_t CACHE_READ_DEFAULT_SIZE = 32 * 1024;
    static constexpr size_t CACHE_WRITE_DEFAULT_SIZE = 64 * 1024;

    appender_file_base::~appender_file_base()
    {
        file_manager::instance().close_file(file_);
    }

    bool appender_file_base::is_output_file_in_sandbox() const
    {
        return is_in_sandbox_;
    }

    void appender_file_base::flush_cache()
    {
        if (file_) {
            size_t real_write_size = 0;
            size_t need_write_size = cache_write_.size();
            for (int32_t i = 0; i < 10; ++i) {
                // try 10 times;
                real_write_size += file_manager::instance().write_file(file_, cache_write_.begin() + real_write_size, need_write_size - real_write_size);
                if (real_write_size >= need_write_size) {
                    break;
                }
                bq::platform::thread::yield();
            }
            cache_write_.clear();
            if (cache_write_.capacity() > (CACHE_WRITE_DEFAULT_SIZE << 1)) {
                cache_write_.shrink();
            }
            if (real_write_size != need_write_size) {
                int32_t errid = file_manager::instance().get_and_clear_last_file_error();

                if (errid != 0 && errid != 28) {
                    char error_text[256] = { 0 };
                    auto epoch = bq::platform::high_performance_epoch_ms();
                    const struct tm* timeptr = is_gmt_time_ ? bq::util::get_gmt_time_by_epoch_unsafe(epoch) : bq::util::get_local_time_by_epoch_unsafe(epoch);
                    snprintf(error_text, sizeof(error_text), "%s %d-%02d-%02d %02d:%02d:%02d appender_file_base write_file ecode:%d, trying open new file real_write_size:%d,need_write_size:%d\n",
                        is_gmt_time_ ? "UTC0" : "LOCAL",
                        timeptr->tm_year + 1900, timeptr->tm_mon + 1, timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec,
                        errid, (int32_t)real_write_size, (int32_t)need_write_size);
                    string path = TO_ABSOLUTE_PATH("bqLog/write_file_error.log", true);
                    bq::file_manager::append_all_text(path, error_text);
                    bq::util::log_device_console_plain_text(log_level::warning, error_text);
                }
                open_new_indexed_file_by_name();
            }
        }
    }

    void appender_file_base::flush_io()
    {
        if (file_) {
            if (!file_manager::instance().flush_file(file_)) {
                int32_t errid = file_manager::instance().get_and_clear_last_file_error();
                if (errid != 0) {
                    char ids[32] = { 0 };
                    snprintf(ids, 32, "%d", errid);
                    string path = TO_ABSOLUTE_PATH("bqLog/flush_file_error.log", true);
                    bq::file_manager::write_all_text(path, ids);
                }
            }
        }
    }

    bool appender_file_base::init_impl(const bq::property_value& config_obj)
    {
        if (!config_obj["file_name"].is_string()) {
            util::log_device_console(bq::log_level::error, "init appender \"%s\" failed, can not find valid \"file_name\" token", ((string)config_obj["name"]).c_str());
            return false;
        }
        if (config_obj["is_in_sandbox"].is_bool()) {
            is_in_sandbox_ = (bool)config_obj["is_in_sandbox"];
        }

        config_file_name_ = ((string)config_obj["file_name"]).trim();

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
            capacity_limit = (uint64_t)config_obj["capacity_limit"];
        }

        // Calculate time difference from UTC time to local time.
        if (!is_gmt_time_) {
            uint64_t epoch = bq::platform::high_performance_epoch_ms();
            const struct tm* local = bq::util::get_local_time_by_epoch_unsafe(epoch);
            time_t local_time = mktime(const_cast<struct tm*>(local));
            const struct tm* utc0 = bq::util::get_gmt_time_by_epoch_unsafe(epoch);
            time_t utc_time = mktime(const_cast<struct tm*>(utc0));
            double timezone_offset = difftime(local_time, utc_time);
            time_zone_diff_to_gmt_ms_ = (int64_t)(timezone_offset) * 1000;
        } else {
            time_zone_diff_to_gmt_ms_ = 0;
        }
        return true;
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
            cache_read_cursor_ += offset;
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
            auto read_size = file_manager::instance().read_file(file_, cache_read_.begin() + left_size, fill_size);
            cache_read_cursor_ = 0;
            if (read_size < fill_size) {
                cache_read_.erase(cache_read_.begin() + left_size + read_size, fill_size - read_size);
            }
        }
        bq::appender_file_base::read_with_cache_handle result;
        result.data_ = cache_read_.begin() + cache_read_cursor_;
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

    appender_file_base::write_with_cache_handle appender_file_base::write_with_cache_alloc(size_t size)
    {
#ifndef NDEBUG
        assert(!cache_write_already_allocated_ && "duplicate call write_with_cache_alloc in log file appender");
        cache_write_already_allocated_ = true;
#endif
        cache_write_.fill_uninitialized(size);
        write_with_cache_handle result_handle;
        result_handle.data_ = (uint8_t*)cache_write_.end() - size;
        result_handle.alloc_len_ = size;
        result_handle.used_len_ = size;
        return result_handle;
    }

    void appender_file_base::write_with_cache_commit(const appender_file_base::write_with_cache_handle& handle)
    {
#ifndef NDEBUG
        assert(cache_write_already_allocated_ && "call write_with_cache_commit without calling write_with_cache_alloc in log file appender");
        assert(handle.used_len_ <= handle.alloc_len_ && "used data length greater than allocated length in log file appender");
        cache_write_already_allocated_ = false;
#endif
        if (handle.used_len_ < handle.alloc_len_) {
            cache_write_.pop_back(handle.alloc_len_ - handle.used_len_);
        }
        current_file_size_ += handle.used_len_;

        if (parent_log_->get_reliable_level() < log_reliable_level::high) {
            if (cache_write_.size() >= CACHE_WRITE_DEFAULT_SIZE) {
                flush_cache();
            }
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
                bq::util::log_device_console(bq::log_level::error, "need_create_new_file");
            }
        }
        if (need_create_new_file) {
            auto current_time_epoch = (handle.get_log_head()).timestamp_epoch;
            uint64_t ms_per_day = 60 * 60 * 24 * 1000;
            current_file_expire_time_epoch_ms_ = current_time_epoch + time_zone_diff_to_gmt_ms_;
            // get next day ms
            uint64_t check_ms_ = (current_file_expire_time_epoch_ms_ / ms_per_day + 1) * ms_per_day;
            current_file_expire_time_epoch_ms_ = current_file_expire_time_epoch_ms_ - (current_file_expire_time_epoch_ms_ % ms_per_day) + ms_per_day;
            if (check_ms_ != current_file_expire_time_epoch_ms_) {
                bq::util::log_device_console(bq::log_level::error, "exception: set expire_time in refresh_file_handle!!!");
            }
            current_file_expire_time_epoch_ms_ -= time_zone_diff_to_gmt_ms_;
            flush_cache();
            flush_io();
            clear_read_cache();
            open_new_indexed_file_by_name();
        }
    }

    bool appender_file_base::open_file_with_write_exclusive(const bq::string& file_path)
    {
        string path = TO_ABSOLUTE_PATH(file_path, is_in_sandbox_);
        file_ = file_manager::instance().open_file(path, file_open_mode_enum::auto_create | file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
        if (!file_) {
            return false;
        }
        current_file_size_ = file_manager::instance().get_file_size(file_);
        return true;
    }

    void appender_file_base::open_new_indexed_file_by_name()
    {
        bool is_prev_file_exist = file_;
        file_manager::instance().close_file(file_);
        clear_all_expired_files();
        clear_all_limit_files();

        auto dir_name = bq::file_manager::get_directory_from_path(config_file_name_);
        auto file_name = bq::file_manager::get_file_name_from_path(config_file_name_);

        char time_str_buf[128];
        auto epoch = bq::platform::high_performance_epoch_ms();
        const struct tm* timeptr = is_gmt_time_ ? bq::util::get_gmt_time_by_epoch_unsafe(epoch) : bq::util::get_local_time_by_epoch_unsafe(epoch);
        strftime(time_str_buf, sizeof(time_str_buf), "_%Y%m%d_", timeptr);

        int32_t max_index = 0;

        bq::string file_prefix = file_name + time_str_buf;
        const bq::string ext_name_with_dot = get_file_ext_name();

        string path = TO_ABSOLUTE_PATH(dir_name, is_in_sandbox_);
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
        if (is_prev_file_exist || max_index == 0) {
            // open a new file
            max_index++;
        } else {
            // can we reuse prev file
        }

        string full_path = TO_ABSOLUTE_PATH(dir_name, is_in_sandbox_);
        bq::file_manager::create_directory(full_path);
        bool need_open_new_file = true;
        while (need_open_new_file) {
            char idx_buff[32];
            snprintf(idx_buff, sizeof(idx_buff), "%d", max_index++);
            bq::string file_relative_path = config_file_name_ + time_str_buf + idx_buff + ext_name_with_dot;
            bq::string absolute_file_path = TO_ABSOLUTE_PATH(file_relative_path, is_in_sandbox_);
            parse_file_context parse_context(file_relative_path);

            need_open_new_file = !open_file_with_write_exclusive(file_relative_path) || is_file_oversize();
            if (!need_open_new_file) {
                need_open_new_file |= (current_file_size_ > 0 && !parse_exist_log_file(parse_context));
            }
        }
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

        bq::string path = TO_ABSOLUTE_PATH(dir_name, is_in_sandbox_);
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
        if (capacity_limit == 0) {
            return;
        }
        auto dir_name = bq::file_manager::get_directory_from_path(config_file_name_);
        auto file_name = bq::file_manager::get_file_name_from_path(config_file_name_);
        bq::string file_prefix = file_name + "_";
        bq::string path = TO_ABSOLUTE_PATH(dir_name, is_in_sandbox_);
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
        if (file_sum_size >= capacity_limit) {
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
                if (file_sum_size < capacity_limit) {
                    break;
                }
            }
        }
    }

    void appender_file_base::parse_file_context::log_parse_fail_reason(const char* msg) const
    {
        bq::util::log_device_console(log_level::error, "failed to parse log file :\"%s\" at offset %zu, msg: %s", file_name_.c_str(), parsed_size, msg);
    }

}
