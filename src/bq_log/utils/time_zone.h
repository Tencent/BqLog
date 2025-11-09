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
 * \file time_zone.h
 *
 * Mode 1: local time, DST supported by OS
 * Mode 2: Fixed time zone offset to UTC time, no DST support.
 *
 * \author pippocao
 * \date 2025
 *
 *
 */

#include "bq_common/bq_common_public_include.h"

namespace bq {
    class time_zone {
    public:
        time_zone(const bq::string& time_zone_str = "localtime");
        time_zone(bool use_local_time, int32_t gmt_offset_hours, int32_t gmt_offset_minutes, int64_t time_zone_diff_to_gmt_ms, const bq::string& time_zone_str);
        ~time_zone();

        void reset();

        /// <summary>
        /// Parse by timezone string
        /// </summary>
        /// <param name="time_zone_str">such as : "localtime", "gmt", "Z", "UTC", "UTC+8", "UTC-11", "utc+11:30"</param>
        void parse_by_string(const bq::string& time_zone_str);

        void restore_by_config(bool use_local_time, int32_t gmt_offset_hours, int32_t gmt_offset_minutes, int64_t time_zone_diff_to_gmt_ms, const bq::string& time_zone_str);

        bool get_tm_by_epoch(uint64_t epoch_ms, struct tm& result) const;

        bq::string get_time_str_by_epoch(uint64_t epoch_ms) const;

        static bq::string get_local_timezone_name();

        bq_forceinline bool is_use_local_time() const { return use_local_time_; }
        bq_forceinline int32_t get_gmt_offset_hours() const { return gmt_offset_hours_; }
        bq_forceinline int32_t get_gmt_offset_minutes() const { return gmt_offset_minutes_; }
        bq_forceinline int32_t get_offset_to_epoch_ms() const
        {
            return (gmt_offset_hours_ * 3600 + gmt_offset_minutes_ * 60) * 1000;
        }
        bq_forceinline const bq::string& get_time_zone_str() const { return time_zone_str_; }
        bq_forceinline int64_t get_time_zone_diff_to_gmt_ms() const { return time_zone_diff_to_gmt_ms_; }
        bq_forceinline void refresh_time_string_cache(uint64_t epoch_ms)
        {
            if (epoch_ms == last_time_epoch_cache_) {
                return;
            }
            inner_refresh_time_string_cache(epoch_ms);
        }
        bq_forceinline const char* get_time_string_cache() const { return time_cache_; }
        bq_forceinline size_t get_time_string_cache_len() const { return time_cache_len_; }

    private:
        void inner_refresh_time_string_cache(uint64_t epoch_ms);

    public:
        static constexpr uint32_t MAX_TIME_STR_LEN = 128;

    private:
        bool use_local_time_;
        int32_t gmt_offset_hours_;
        int32_t gmt_offset_minutes_;
        int64_t time_zone_diff_to_gmt_ms_;
        bq::string time_zone_str_;

        // Cache for time string
        char time_cache_[MAX_TIME_STR_LEN + 1];
        size_t time_cache_len_;
        uint64_t last_time_epoch_cache_ = 0;
    };

}
