/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once
#include "bq_common/bq_common.h"

namespace bq {
    class instance_storage;

    // Per-log directory layout inside the instance: <root>/logs/<escaped log name>/
    // holding every mmap file of that log. Null when recovery is off or no instance
    // could be claimed; all consumers then fall back to heap memory.
    class log_storage {
    public:
        // nullptr when recovery is off or no instance could be claimed: callers use heap memory
        static bq::unique_ptr<log_storage> create(const bq::string& log_name, bool need_recovery);

        // [a-z0-9_-] verbatim, [A-Z] -> ^ + c, everything else -> % + 2 hex digits (uppercase);
        // empty name -> "%". See 04_DESIGN section 1.3.
        static bq::string escape_name(const bq::string& name);

        const bq::string& dir() const { return dir_; } // p<k>/logs/<escaped>
        bq::string lp_path() const; // dir/lp.mmap
        bq::string hp_dir() const; // dir/hp
        bq::string hp_path(uint64_t index) const; // dir/hp/<index>.mmap
        bq::string os_dir() const; // dir/os
        bq::string os_path(uint64_t index) const; // dir/os/<index>.mmap
        bq::string appender_path(const bq::string& appender_name) const; // dir/appenders/<escaped appender>.mmap
        void remove(const bq::string& abs_path) const; // forwards to instance_storage::remove

    private:
        bq::string dir_;
    };
}
