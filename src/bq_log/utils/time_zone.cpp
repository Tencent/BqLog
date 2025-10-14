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

#include "bq_common/bq_common.h"
#include "bq_log/utils/time_zone.h"
#ifdef BQ_WIN
#include <timezoneapi.h>
#endif

namespace bq {

#if defined(BQ_WIN)
#define BQ_LOCALTIME(dst, tptr) (localtime_s((dst),(tptr)) == 0)
#define BQ_GMTIME(dst, tptr)    (gmtime_s((dst),(tptr)) == 0)
#else
#define BQ_LOCALTIME(dst, tptr) (localtime_r((tptr),(dst)) != NULL)
#define BQ_GMTIME(dst, tptr)    (gmtime_r((tptr),(dst)) != NULL)
#endif

    time_zone::time_zone(const bq::string& time_zone_str /*= "localtime" */) {
        reset();
        parse_by_string(time_zone_str);
    }


    time_zone::time_zone(bool use_local_time, int32_t gmt_offset_hours, int32_t gmt_offset_minutes, int64_t time_zone_diff_to_gmt_ms, const bq::string& time_zone_str)
    {
        restore_by_config(use_local_time, gmt_offset_hours, gmt_offset_minutes, time_zone_diff_to_gmt_ms, time_zone_str);
    }

    time_zone::~time_zone()
    {
    }

    void time_zone::reset()
    {
        use_local_time_ = true;
        gmt_offset_hours_ = 0;
        gmt_offset_minutes_ = 0;
        time_zone_str_ = "";
        time_zone_diff_to_gmt_ms_ = 0;
    }

    void time_zone::parse_by_string(const bq::string& time_zone_str)
    {
        reset();
        bq::string upper_time_zone = time_zone_str.trim();
        for (auto& c : upper_time_zone) {
            c = (char)toupper(c);
        }

        do {
            if (upper_time_zone.is_empty()) {
                bq::util::log_device_console(bq::log_level::warning, "empty time_zone string, use local time as default.");
                break;
            }
            if (upper_time_zone.is_empty() || upper_time_zone == "LOCAL" || upper_time_zone == "LOCALTIME" || upper_time_zone == "LOCAL_TIME") {
                break;
            }
            if (upper_time_zone == "GMT" || upper_time_zone == "Z" || upper_time_zone == "UTC") {
                use_local_time_ = false;
                break;
            }
            size_t parse_start = 0;
            if (upper_time_zone.begin_with("UTC")) {
                parse_start = 3;
            }
            char* end_ptr = nullptr;
            char* start_ptr = (char*)upper_time_zone.begin() + parse_start;
            int32_t hour = static_cast<int32_t>(strtol(start_ptr, &end_ptr, 10));
            if (end_ptr == start_ptr) {
                bq::util::log_device_console(bq::log_level::warning, "invalid time_zone string %s, use local time as default.", time_zone_str.c_str());
                break;
            }
            if (hour < -12 || hour > 14) {
                bq::util::log_device_console(bq::log_level::warning, "invalid time_zone string %s, hour %d out of range[-12, 14], use local time as default.", time_zone_str.c_str(), hour);
                break;
            }
            int32_t minute = 0;
            if (*end_ptr == ':') {
                start_ptr = end_ptr + 1;
                minute = static_cast<int32_t>(strtol(start_ptr, &end_ptr, 10));
                if (end_ptr == start_ptr) {
                    bq::util::log_device_console(bq::log_level::warning, "invalid time_zone string %s, use local time as default.", time_zone_str.c_str());
                    break;
                }
                if (minute < 0 || minute >= 60) {
                    bq::util::log_device_console(bq::log_level::warning, "invalid time_zone string %s, minute %d out of range[0, 59], use local time as default.", time_zone_str.c_str(), minute);
                    break;
                }
                gmt_offset_minutes_ = minute;
            }
            if (*end_ptr != '\0') {
                bq::util::log_device_console(bq::log_level::warning, "invalid time_zone string %s, use local time as default.", time_zone_str.c_str());
                break;
            }
            use_local_time_ = false;
            gmt_offset_hours_ = hour;
            gmt_offset_minutes_ = minute;
            if (gmt_offset_hours_ < 0) {
                gmt_offset_minutes_ = -gmt_offset_minutes_;
            }
        }while(0);
        

        //generate time_zone_str_ and time_zone_diff_to_gmt_ms_;
        if (use_local_time_) {
            time_zone_str_ = get_local_timezone_name();
            time_t now = time(NULL);
            struct tm lt;
            struct tm gt;
            if (!BQ_LOCALTIME(&lt, &now)) {
                time_zone_diff_to_gmt_ms_ = 0;
            }
            else {
                (void)BQ_GMTIME(&gt, &now);
                time_t local_epoch_sec = mktime(const_cast<struct tm*>(&lt));
                time_t utc_epoch_sec = mktime(const_cast<struct tm*>(&gt));
                double timezone_offset = difftime(local_epoch_sec, utc_epoch_sec);
                time_zone_diff_to_gmt_ms_ = (int64_t)(timezone_offset) * 1000;
            }
        }
        else {
            if (gmt_offset_hours_ == 0 && gmt_offset_minutes_ == 0) {
                time_zone_str_ = "UTC0";
            }
            else if (gmt_offset_minutes_ == 0) {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "UTC%+" PRId32, gmt_offset_hours_);
                time_zone_str_ = buffer;
            }
            else {
                char buffer[16];
                snprintf(buffer, sizeof(buffer), "UTC%+d:%02" PRId32, gmt_offset_hours_, abs(gmt_offset_minutes_));
                time_zone_str_ = buffer;
            }
            time_zone_diff_to_gmt_ms_ = ((int64_t)gmt_offset_hours_ * 3600 + (int64_t)gmt_offset_minutes_ * 60) * 1000;
        }
    }


    void time_zone::restore_by_config(bool use_local_time, int32_t gmt_offset_hours, int32_t gmt_offset_minutes, int64_t time_zone_diff_to_gmt_ms, const bq::string& time_zone_str)
    {
        use_local_time_ = use_local_time;
        gmt_offset_hours_ = gmt_offset_hours;
        gmt_offset_minutes_ = gmt_offset_minutes;
        time_zone_diff_to_gmt_ms_ = time_zone_diff_to_gmt_ms;
        time_zone_str_ = time_zone_str;
    }

    bq::string time_zone::get_local_timezone_name() {
        time_t now = time(NULL);
        struct tm lt;
        if (!BQ_LOCALTIME(&lt, &now)) {
            bq::util::log_device_console(bq::log_level::warning, "failed to fetch localtime.");
            return "localtime";
        }

#ifdef BQ_POSIX
        char tzbuf[128] = { 0 };
        size_t n = strftime(tzbuf, sizeof(tzbuf), "%Z", &lt);
        if (n > 0 && tzbuf[0] != '\0') {
            return tzbuf;
        }
#else
        DYNAMIC_TIME_ZONE_INFORMATION dtzi;
        DWORD id = GetDynamicTimeZoneInformation(&dtzi);
        if (id != TIME_ZONE_ID_INVALID) {
            char tz_utf8[256] = { 0 };
            int32_t utf8_len = WideCharToMultiByte(CP_UTF8, 0, dtzi.TimeZoneKeyName, -1, tz_utf8, sizeof(tz_utf8), nullptr, nullptr);
            if (utf8_len > 0 && tz_utf8[0] != '\0') {
                return tz_utf8;
            }
        }
#endif
        return "localtime";
    }


    bool time_zone::get_tm_by_epoch(uint64_t epoch_ms, struct tm& result) const
    {
        time_t base_sec = static_cast<time_t>(epoch_ms / static_cast<uint64_t>(1000));

        if (use_local_time_) {
            return BQ_LOCALTIME(&result, &base_sec) ? true : false;
        }

        // Fixed UTC offset in seconds (no DST)
        int64_t delta_sec =
            (int64_t)gmt_offset_hours_ * static_cast<int64_t>(3600) +
            (int64_t)gmt_offset_minutes_ * static_cast<int64_t>(60); // minutes are 0 in current parsing

        time_t adjusted = (time_t)((int64_t)base_sec + delta_sec);

        return BQ_GMTIME(&result, &adjusted) ? true : false;
    }


    bq::string time_zone::get_time_str_by_epoch(uint64_t epoch_ms) const
    {
        char time_buffer[128];
        struct tm result;
        get_tm_by_epoch(epoch_ms, result);
        snprintf(time_buffer, sizeof(time_buffer), "%04d-%02d-%02d %02d:%02d:%02d",
            result.tm_year + 1900, result.tm_mon + 1, result.tm_mday, result.tm_hour, result.tm_min, result.tm_sec);
        return time_buffer;
    }

}