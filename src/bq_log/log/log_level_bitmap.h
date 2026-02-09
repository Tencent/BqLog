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
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
namespace bq {
    class log_level_bitmap {
    public:
        log_level_bitmap();
        log_level_bitmap(uint32_t init_bitmap_value);
        log_level_bitmap(const log_level_bitmap& rhs);

    public:
        log_level_bitmap& operator=(const log_level_bitmap& rhs);
        void clear();
        bq_forceinline bool have_level(bq::log_level level) const
        {
            return (bitmap_ & (1U << static_cast<uint32_t>(level))) != 0;
        }
        void add_level(bq::log_level level);
        void add_level(const bq::string& level_string);
        void del_level(bq::log_level level);
        uint32_t* get_bitmap_ptr();

    private:
        uint32_t bitmap_;
    };
}
