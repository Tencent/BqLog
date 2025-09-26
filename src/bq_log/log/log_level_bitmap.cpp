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
#include "bq_common/bq_common.h"
#include "bq_log/log/log_level_bitmap.h"

namespace bq {
    log_level_bitmap::log_level_bitmap()
        : bitmap_(0)
    {
    }

    log_level_bitmap::log_level_bitmap(uint32_t init_bitmap_value)
        : bitmap_(init_bitmap_value)
    {
    }

    log_level_bitmap::log_level_bitmap(const log_level_bitmap& rhs)
        : bitmap_(rhs.bitmap_)
    {
    }

    bq::log_level_bitmap& log_level_bitmap::operator=(const log_level_bitmap& rhs)
    {
        bitmap_ = rhs.bitmap_;
        return *this;
    }

    void log_level_bitmap::clear()
    {
        bitmap_ = 0;
    }

    void log_level_bitmap::add_level(bq::log_level level)
    {
        bitmap_ |= (1 << (int32_t)level);
    }

    void log_level_bitmap::add_level(const bq::string& level_string)
    {
        if (level_string.equals_ignore_case("all")) {
            bitmap_ = 0xFFFFFFFF;
            return;
        }
        if (level_string.equals_ignore_case("verbose")) {
            add_level(bq::log_level::verbose);
        } else if (level_string.equals_ignore_case("debug")) {
            add_level(bq::log_level::debug);
        } else if (level_string.equals_ignore_case("info")) {
            add_level(bq::log_level::info);
        } else if (level_string.equals_ignore_case("warning")) {
            add_level(bq::log_level::warning);
        } else if (level_string.equals_ignore_case("error")) {
            add_level(bq::log_level::error);
        } else if (level_string.equals_ignore_case("fatal")) {
            add_level(bq::log_level::fatal);
        } else {
            util::log_device_console(bq::log_level::warning, "bq log error, invalid level mask was found:\"%s\"", level_string.c_str());
        }
    }

    void log_level_bitmap::del_level(bq::log_level level)
    {
        bitmap_ &= ~(1U << static_cast<uint32_t>(level));
    }

    uint32_t* log_level_bitmap::get_bitmap_ptr()
    {
        return &bitmap_;
    }
}
