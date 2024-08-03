#pragma once
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

// this class is not thread safe.
// write_data() and take_snapshot() can not be called in the same time
#include "bq_common/bq_common.h"
#include "bq_log/misc/bq_log_def.h"
#include "bq_log/types/ring_buffer.h"
#include "bq_log/log/layout.h"

namespace bq {
    class log_snapshot {
    public:
        log_snapshot(class log_imp* parent_log);

        log_snapshot(const log_snapshot& rhs) = delete;

        ~log_snapshot();

        void reset_buffer_size(uint32_t buffer_size);

        void write_data(const uint8_t* data, uint32_t data_size);

        const bq::string& take_snapshot_string(bool use_gmt_time);

        // take_snapshot_string and release_snapshot_string must be called in pair, or the lock will not be released
        void release_snapshot_string();

    private:
        uint32_t buffer_size_;
        ring_buffer* snapshot_buffer_;
        bq::string snapshot_text_[2];
        uint32_t snapshot_text_index_;
        bool snapshot_text_continuous_;

        bq::platform::spin_lock lock_;

        layout snapshot_layout_;
        class log_imp* parent_log_;
    };
}
